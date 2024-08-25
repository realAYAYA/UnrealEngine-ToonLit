// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimInstanceLocatorEditor.h"
#include "UniversalObjectLocatorFragmentTypeHandle.h"
#include "UniversalObjectLocators/AnimInstanceLocatorFragment.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorEditor.h"
#include "IUniversalObjectLocatorEditorModule.h"
#include "IUniversalObjectLocatorCustomization.h"
#include "ISequencerModule.h"
#include "SceneOutlinerDragDrop.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "PropertyCustomizationHelpers.h"

#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"


#define LOCTEXT_NAMESPACE "AnimInstanceLocatorEditor"

namespace UE::UniversalObjectLocator
{

class SAnimInstanceLocatorEditorUI : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SAnimInstanceLocatorEditorUI){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<IUniversalObjectLocatorCustomization> InCustomization)
	{
		WeakCustomization = InCustomization;

		TSharedRef<SObjectPropertyEntryBox> EditWidget = SNew(SObjectPropertyEntryBox)
		.ObjectPath(InCustomization.ToSharedRef(), &IUniversalObjectLocatorCustomization::GetPathToObject)
		.AllowedClass(USkeletalMeshComponent::StaticClass())
		.OnObjectChanged(this, &SAnimInstanceLocatorEditorUI::OnSetObject)
		.AllowClear(true)
		.DisplayUseSelected(true)
		.DisplayBrowse(true)
		.DisplayThumbnail(true);

		float MinWidth = 100.f;
		float MaxWidth = 500.f;
		EditWidget->GetDesiredWidth(MinWidth, MaxWidth);

		ChildSlot
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			[
				SNew(SBox)
				.MinDesiredWidth(MinWidth)
				.MaxDesiredWidth(MaxWidth)
				[
					EditWidget
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SAnimInstanceLocatorEditorUI::CreateAnimInstanceTypeMenuContent)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &SAnimInstanceLocatorEditorUI::GetCurrentAnimInstanceTypeText)
				]
			]
		];
	}

private:

	void OnSetObject(const FAssetData& InNewObject)
	{
		TSharedPtr<IUniversalObjectLocatorCustomization> Customization = WeakCustomization.Pin();
		if (!Customization)
		{
			return;
		}

		UObject* Object  = InNewObject.FastGetAsset(true);

		if (Object)
		{
			FUniversalObjectLocator NewRef(Object);
			Customization->SetValue(MoveTemp(NewRef));
		}
	}

	TSharedRef<SWidget> CreateAnimInstanceTypeMenuContent()
	{
		const bool bCloseAfterSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterSelection, nullptr);

		FCanExecuteAction AlwaysExecute = FCanExecuteAction::CreateLambda([]{ return true; });

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Menu_AnimInstanceLabel", "Anim Instance"),
			LOCTEXT("Menu_AnimInstanceTooltip", "Bind to the Anim Instance on the selected component"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAnimInstanceLocatorEditorUI::ChangeType, EAnimInstanceLocatorFragmentType::AnimInstance),
				AlwaysExecute,
				FIsActionChecked::CreateSP(this, &SAnimInstanceLocatorEditorUI::CompareCurrentType, EAnimInstanceLocatorFragmentType::AnimInstance)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Menu_PPAnimInstanceLabel", "Post Process Anim Instance"),
			LOCTEXT("Menu_PPAnimInstanceTooltip", "Bind to the Post Process Anim Instance on the selected component"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAnimInstanceLocatorEditorUI::ChangeType, EAnimInstanceLocatorFragmentType::PostProcessAnimInstance),
				AlwaysExecute,
				FIsActionChecked::CreateSP(this, &SAnimInstanceLocatorEditorUI::CompareCurrentType, EAnimInstanceLocatorFragmentType::PostProcessAnimInstance)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
		return MenuBuilder.MakeWidget();
	}

	FText GetCurrentAnimInstanceTypeText() const
	{
		TOptional<EAnimInstanceLocatorFragmentType> CommonType = GetCurrentType();
		if (!CommonType)
		{
			return LOCTEXT("AnimInstanceMixedLabel", "None");
		}

		if (CommonType.GetValue() == EAnimInstanceLocatorFragmentType::AnimInstance)
		{
			return LOCTEXT("AnimInstanceLabel", "Anim Instance");
		}
		return LOCTEXT("PostProcessAnimInstanceLabel", "Post Process Anim Instance");
	}

	void ChangeType(EAnimInstanceLocatorFragmentType InType)
	{
		TSharedPtr<IUniversalObjectLocatorCustomization> Customization = WeakCustomization.Pin();
		if (Customization)
		{
			TSharedPtr<IPropertyHandle>    Property          = Customization->GetProperty();
			const FStructProperty*         RawProperty       = CastFieldChecked<const FStructProperty>(Property->GetProperty());

			Property->NotifyPreChange();
			Property->EnumerateRawData([this, InType](void* RawData, const int32, const int32){

				FUniversalObjectLocatorFragment* Locator = static_cast<FUniversalObjectLocator*>(RawData)->GetLastFragment();

				FAnimInstanceLocatorFragment* Payload = nullptr;
				if (Locator && Locator->TryGetPayloadAs(FAnimInstanceLocatorFragment::FragmentType, Payload))
				{
					Payload->Type = InType;
				}

				// Continue to the next property value
				return true;
			});

			Property->NotifyPostChange(EPropertyChangeType::ValueSet);
			Property->NotifyFinishedChangingProperties();
		}
	}

	bool CompareCurrentType(EAnimInstanceLocatorFragmentType InType) const
	{
		TOptional<EAnimInstanceLocatorFragmentType> Type = GetCurrentType();
		return Type.IsSet() && InType == Type.GetValue();
	}

	TOptional<EAnimInstanceLocatorFragmentType> GetCurrentType() const
	{
		TOptional<EAnimInstanceLocatorFragmentType> CommonType;

		TSharedPtr<IUniversalObjectLocatorCustomization> Customization = WeakCustomization.Pin();
		if (Customization)
		{
			TSharedPtr<IPropertyHandle> Property    = Customization->GetProperty();
			const FStructProperty*      RawProperty = CastFieldChecked<const FStructProperty>(Property->GetProperty());

			Property->EnumerateRawData([this, &CommonType](void* RawData, const int32, const int32){

				FUniversalObjectLocatorFragment* LastFragment = static_cast<FUniversalObjectLocator*>(RawData)->GetLastFragment();

				FAnimInstanceLocatorFragment* Payload = nullptr;
				if (LastFragment && LastFragment->TryGetPayloadAs(FAnimInstanceLocatorFragment::FragmentType, Payload))
				{
					if (!CommonType.IsSet())
					{
						CommonType = Payload->Type;
					}
					else if (CommonType.GetValue() != Payload->Type)
					{
						// Different type - common type is undefined
						CommonType.Reset();
						// Stop iterating and return an unset optional
						return false;
					}
				}

				// Continue to the next property value
				return true;
			});
		}

		return CommonType;
	}

	TWeakPtr<IUniversalObjectLocatorCustomization> WeakCustomization;
	EAnimInstanceLocatorFragmentType CurrentType;
};

bool FAnimInstanceLocatorEditor::IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FActorDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FSceneOutlinerDragDropOp>())
	{
		FSceneOutlinerDragDropOp* SceneOutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(DragOperation.Get());
		ActorDrag = SceneOutlinerOp->GetSubOp<FActorDragDropOp>();
	}
	else if (DragOperation->IsOfType<FActorDragDropOp>())
	{
		ActorDrag = StaticCastSharedPtr<FActorDragDropOp>(DragOperation);
	}		

	if (ActorDrag)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : ActorDrag->Actors)
		{
			if (WeakActor.Get())
			{
				return true;
			}
		}
	}

	return false;
}

UObject* FAnimInstanceLocatorEditor::ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FActorDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FSceneOutlinerDragDropOp>())
	{
		FSceneOutlinerDragDropOp* SceneOutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(DragOperation.Get());
		ActorDrag = SceneOutlinerOp->GetSubOp<FActorDragDropOp>();
	}
	else if (DragOperation->IsOfType<FActorDragDropOp>())
	{
		ActorDrag = StaticCastSharedPtr<FActorDragDropOp>(DragOperation);
	}

	if (ActorDrag)
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : ActorDrag->Actors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				return Actor;
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FAnimInstanceLocatorEditor::MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization)
{
	return SNew(SAnimInstanceLocatorEditorUI, Customization);
}

FText FAnimInstanceLocatorEditor::GetDisplayText() const
{
	return LOCTEXT("AnimInstanceLocatorName", "Anim Instance");
}
FText FAnimInstanceLocatorEditor::GetDisplayTooltip() const
{
	return LOCTEXT("AnimInstanceLocatorTooltip", "Change this to a reference to an Anim Instance");
}
FSlateIcon FAnimInstanceLocatorEditor::GetDisplayIcon() const
{
	return FSlateIcon();
}

} // namespace UE::UniversalObjectLocator


#undef LOCTEXT_NAMESPACE
