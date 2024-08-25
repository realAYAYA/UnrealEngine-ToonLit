// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalObjectLocatorCustomization.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorFragmentType.h"
#include "UniversalObjectLocatorEditor.h"
#include "UniversalObjectLocatorEditorModule.h"

#include "Containers/Array.h"
#include "Internationalization/Internationalization.h"
#include "Modules/ModuleManager.h"

#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SDropTarget.h"

#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"

#include "Async/Async.h"


#define LOCTEXT_NAMESPACE "FUniversalObjectLocatorCustomization"

namespace UE::UniversalObjectLocator
{

TSharedRef<IPropertyTypeCustomization> FUniversalObjectLocatorCustomization::MakeInstance()
{
	return MakeShared<FUniversalObjectLocatorCustomization>();
}

void FUniversalObjectLocatorCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = StructPropertyHandle;
	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();

	FSceneOutlinerInitializationOptions InitOptions;
	{
		InitOptions.bShowHeaderRow = false;
	}

	bool bDisplayThumbnail = true;

	TArray<FString> AllowedTypes;
	if (PropertyHandle->HasMetaData("AllowedLocators"))
	{
		PropertyHandle->GetMetaData("AllowedLocators").ParseIntoArray(AllowedTypes, TEXT(","));
	}

	FUniversalObjectLocatorEditorModule& Module = FModuleManager::Get().LoadModuleChecked<FUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	for (TPair<FName, TSharedPtr<ILocatorEditor>> Pair : Module.LocatorEditors)
	{
		if (AllowedTypes.Num() == 0 || AllowedTypes.Contains(Pair.Key.ToString()))
		{
			ApplicableLocators.Add(Pair.Key, Pair.Value);
		}
	}

	Content = SNew(SBox);
	HeaderRow
	.NameContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]

		// Type combo box
		+ SHorizontalBox::Slot()
		.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
		.AutoWidth()
		[
			SNew(SComboButton)
			.Visibility(ApplicableLocators.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed)
			.OnGetMenuContent(this, &FUniversalObjectLocatorCustomization::GetUserExposedFragmentTypeList)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FUniversalObjectLocatorCustomization::GetCurrentFragmentTypeText)
			]
		]
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &FUniversalObjectLocatorCustomization::HandleIsDragAllowed)
		.OnDropped(this, &FUniversalObjectLocatorCustomization::HandleDrop)
		[
			Content.ToSharedRef()
		]
	];

	Rebuild();
}

void FUniversalObjectLocatorCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Allow the custom locator editor to customize children if desired
	FUniversalObjectLocatorEditorModule& Module = FModuleManager::Get().LoadModuleChecked<FUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	TSharedPtr<ILocatorEditor> LocatorEditor = ApplicableLocators.FindRef(GetCachedData().LocatorEditorType);
	if (LocatorEditor)
	{
		LocatorEditor->CustomizeChildren(StructPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
}

void FUniversalObjectLocatorCustomization::Rebuild() const
{
	FUniversalObjectLocatorEditorModule& Module = FModuleManager::Get().LoadModuleChecked<FUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	TSharedPtr<ILocatorEditor> LocatorEditor = ApplicableLocators.FindRef(GetCachedData().LocatorEditorType);
	if (LocatorEditor)
	{
		TSharedPtr<SWidget> EditUI = LocatorEditor->MakeEditUI(SharedThis(const_cast<FUniversalObjectLocatorCustomization*>(this)));
		if (EditUI)
		{
			TSharedPtr<SBox> BoxWidget = Content;
			
			// Delay reconstruction until next frame since this can be called from inside the previous EditUI,
			// leading to destruction of the UI while it is still running
			AsyncTask(ENamedThreads::GameThread, [BoxWidget, EditUI, this] {
				BoxWidget->SetContent(EditUI.ToSharedRef());
			});
			return;
		}
	}

	Content->SetContent(SNullWidget::NullWidget);
}

UObject* FUniversalObjectLocatorCustomization::GetSingleObject() const
{
	return GetCachedData().WeakObject.Get();
}

void FUniversalObjectLocatorCustomization::SetValue(FUniversalObjectLocator&& InNewValue) const
{
	PropertyHandle->NotifyPreChange();

	PropertyHandle->EnumerateRawData([&InNewValue](void* RawData, int32 Index, int32 Num){
		FUniversalObjectLocator* Reference = static_cast<FUniversalObjectLocator*>(RawData);
		if (Index == Num-1)
		{
			*Reference = MoveTemp(InNewValue);
		}
		else
		{
			*Reference = InNewValue;
		}
		return true;
	});

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();

	// Clear the cache to force cache to rebuild
	CachedData.PropertyValue.Reset();

	Rebuild();
}

FString FUniversalObjectLocatorCustomization::GetPathToObject() const
{
	return GetCachedData().ObjectPath;
}

TSharedPtr<IPropertyHandle> FUniversalObjectLocatorCustomization::GetProperty() const
{
	return PropertyHandle;
}

TSharedRef<SWidget> FUniversalObjectLocatorCustomization::GetUserExposedFragmentTypeList()
{
	struct FMenuData
	{
		TSharedPtr<ILocatorEditor> LocatorEditor;
		FText DisplayText;
		FText DisplayTooltip;
		FSlateIcon DisplayIcon;
		FName LocatorEditorType;
	};

	TArray<FMenuData> MenuData;

	for (TPair<FName, TSharedPtr<ILocatorEditor>> Pair : ApplicableLocators)
	{
		MenuData.Add(FMenuData{
			Pair.Value,
			Pair.Value->GetDisplayText(),
			Pair.Value->GetDisplayTooltip(),
			Pair.Value->GetDisplayIcon(),
			Pair.Key
		});
	}

	if (MenuData.Num() == 0)
	{
		// 
		return SNullWidget::NullWidget;
	}

	Algo::SortBy(MenuData, &FMenuData::DisplayText, FText::FSortPredicate(ETextComparisonLevel::Default));

	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, nullptr);

	FCanExecuteAction AlwaysExecute = FCanExecuteAction::CreateLambda([]{ return true; });

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FragmentTypeTypeHeader", "Change this locator's to reference a different type:"));
	for (const FMenuData& Item : MenuData)
	{
		MenuBuilder.AddMenuEntry(
			Item.DisplayText,
			Item.DisplayTooltip,
			Item.DisplayIcon,
			FUIAction(
				FExecuteAction::CreateSP(this, &FUniversalObjectLocatorCustomization::ChangeEditorType, Item.LocatorEditorType),
				AlwaysExecute,
				FIsActionChecked::CreateSP(this, &FUniversalObjectLocatorCustomization::CompareCurrentEditorType, Item.LocatorEditorType)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();


	return MenuBuilder.MakeWidget();
}

void FUniversalObjectLocatorCustomization::ChangeEditorType(FName InNewLocatorEditorType)
{
	PropertyHandle->NotifyPreChange();

	PropertyHandle->EnumerateRawData(
		[InNewLocatorEditorType, this](void* Data, int32 DataIndex, int32 Num)
		{
			FUniversalObjectLocator* Ref = static_cast<FUniversalObjectLocator*>(Data);
			Ref->Reset();

			TSharedPtr<ILocatorEditor> LocatorEditor = ApplicableLocators.FindRef(InNewLocatorEditorType);
			if (LocatorEditor)
			{
				*Ref = LocatorEditor->MakeDefaultLocator();
			}
			return true;
		}
	);

	CachedData.LocatorEditorType = InNewLocatorEditorType;

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();

	PropertyUtilities->ForceRefresh();

	Rebuild();

}

bool FUniversalObjectLocatorCustomization::CompareCurrentEditorType(FName InLocatorEditorType) const
{
	return CachedData.LocatorEditorType == InLocatorEditorType;
}

FText FUniversalObjectLocatorCustomization::GetCurrentFragmentTypeText() const
{
	if (CachedData.LocatorEditorType != NAME_None)
	{
		return FText::FromName(CachedData.LocatorEditorType);
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	FText NoneText = LOCTEXT("NoValues", "None");
	if (RawData.Num() == 0)
	{
		return NoneText;
	}

	const FFragmentType* CommonFragmentType = nullptr;
	for (void* Ptr : RawData)
	{
		FUniversalObjectLocator* Reference    = static_cast<FUniversalObjectLocator*>(RawData[0]);
		const FFragmentType*     FragmentType = Reference->GetLastFragmentType();

		if (CommonFragmentType && FragmentType != CommonFragmentType)
		{
			return LOCTEXT("MultipleTypes", "<multiple>");
		}
		CommonFragmentType = FragmentType;
	}
	return CommonFragmentType ? CommonFragmentType->DisplayText : NoneText;
}

bool FUniversalObjectLocatorCustomization::HandleIsDragAllowed(TSharedPtr<FDragDropOperation> InDragOperation)
{
	for (TPair<FName, TSharedPtr<ILocatorEditor>> Pair : ApplicableLocators)
	{
		if (Pair.Value->IsDragSupported(InDragOperation, nullptr))
		{
			return true;
		}
	}

	return false;
}

FReply FUniversalObjectLocatorCustomization::HandleDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> DragOperation = DragDropEvent.GetOperation();

	UObject* ResolvedObject = nullptr;

	for (TPair<FName, TSharedPtr<ILocatorEditor>> Pair : ApplicableLocators)
	{
		ResolvedObject = Pair.Value->ResolveDragOperation(DragOperation, nullptr);
		if (ResolvedObject)
		{
			break;
		}
	}

	if (!ResolvedObject)
	{
		return FReply::Unhandled();
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	for (void* Ptr : RawData)
	{
		FUniversalObjectLocator* Reference = static_cast<FUniversalObjectLocator*>(Ptr);
		Reference->Reset(ResolvedObject);
	}

	return FReply::Handled();
}

void FUniversalObjectLocatorCustomization::SetActor(AActor* NewObject)
{
	FUniversalObjectLocator NewRef(NewObject);

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	for (void* Ptr : RawData)
	{
		FUniversalObjectLocator* Reference = static_cast<FUniversalObjectLocator*>(RawData[0]);
		*Reference = NewRef;
	}
}

FUniversalObjectLocator* FUniversalObjectLocatorCustomization::GetSinglePropertyValue()
{
	const FUniversalObjectLocator* ConstValue = static_cast<const FUniversalObjectLocatorCustomization*>(this)->GetSinglePropertyValue();
	return const_cast<FUniversalObjectLocator*>(ConstValue);
}

const FUniversalObjectLocator* FUniversalObjectLocatorCustomization::GetSinglePropertyValue() const
{
	const FUniversalObjectLocator* Value = nullptr;

	PropertyHandle->EnumerateConstRawData(
		[&Value](const void* Data, int32 DataIndex, int32 Num)
		{
			if (Num == 1)
			{
				Value = static_cast<const FUniversalObjectLocator*>(Data);
			}
			return false;
		}
	);

	return Value;
}

const FUniversalObjectLocatorCustomization::FCachedData& FUniversalObjectLocatorCustomization::GetCachedData() const
{

	const FUniversalObjectLocator* SingleValue = nullptr;

	bool bNeedsUpdate = false;
	if (CachedData.LocatorEditorType == NAME_None || !CachedData.PropertyValue.IsSet())
	{
		// Set the LocatorEditorType type based on the current values
		TOptional<FName> CommonLocatorEditorType;

		PropertyHandle->EnumerateConstRawData(
			[this, &SingleValue, &CommonLocatorEditorType](const void* Data, int32 DataIndex, int32 Num)
			{
				const FUniversalObjectLocator* Value = static_cast<const FUniversalObjectLocator*>(Data);
				if (Num == 1)
				{
					SingleValue = Value;
				}

				const FFragmentType* FragmentTypePtr = Value->GetLastFragmentType();
				if (!CommonLocatorEditorType)
				{
					if (FragmentTypePtr)
					{
						CommonLocatorEditorType = FragmentTypePtr->PrimaryEditorType;
					}
					else
					{
						// If we find any value that is effectively null, we choose the most applicable locator
						return false;
					}
				}
				else if (!FragmentTypePtr || FragmentTypePtr->PrimaryEditorType != CommonLocatorEditorType.GetValue())
				{
					CommonLocatorEditorType = NAME_None;
				}
				else
				{
					CommonLocatorEditorType = FragmentTypePtr->PrimaryEditorType;
				}
				return true;
			}
		);

		if (CommonLocatorEditorType)
		{
			if (CommonLocatorEditorType.GetValue() != NAME_None)
			{
				CachedData.LocatorEditorType = CommonLocatorEditorType.GetValue();
			}
		}
		else if (CachedData.LocatorEditorType == NAME_None)
		{
			for (const TPair<FName, TSharedPtr<ILocatorEditor>>& Pair : ApplicableLocators)
			{
				CachedData.LocatorEditorType = Pair.Key;
				break;
			}
		}

		if (SingleValue)
		{
			if (!CachedData.PropertyValue.IsSet() || *SingleValue != CachedData.PropertyValue.GetValue())
			{
				CachedData.PropertyValue = *SingleValue;
				bNeedsUpdate = true;
			}
		}
		else if (CachedData.PropertyValue.IsSet())
		{
			CachedData.PropertyValue.Reset();
			bNeedsUpdate = true;
		}

	}

	if (bNeedsUpdate)
	{
		if (CachedData.PropertyValue.IsSet())
		{
			CachedData.WeakObject = CachedData.PropertyValue->SyncFind();
		}
		else
		{
			CachedData.WeakObject = nullptr;
		}

		if (UObject* Resolved = CachedData.WeakObject.Get())
		{
			CachedData.ObjectPath = FSoftObjectPath(Resolved).ToString();
		}
		else
		{
			CachedData.ObjectPath.Empty();
		}

		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);

		bool bAnyFragmentType = false;
		const FFragmentType* CommonFragmentType = nullptr;
		TOptional<FFragmentTypeHandle> CommonFragmentTypeHandle;
		for (void* Ptr : RawData)
		{
			FUniversalObjectLocator* Locator      = static_cast<FUniversalObjectLocator*>(Ptr);
			const FFragmentType*     FragmentType = Locator->GetLastFragmentType();

			if (CommonFragmentType && FragmentType != CommonFragmentType)
			{
				CommonFragmentType = nullptr;
				CommonFragmentTypeHandle.Reset();
				break;
			}

			if (FragmentType)
			{
				bAnyFragmentType = true;
			}
			CommonFragmentType = FragmentType;
			CommonFragmentTypeHandle = Locator->GetLastFragmentTypeHandle();
		}

		FText NoneText = LOCTEXT("NoValues", "None");
		if (CommonFragmentType != nullptr)
		{
			CachedData.FragmentTypeText = CommonFragmentType->DisplayText;
			CachedData.FragmentType     = CommonFragmentTypeHandle;
		}
		else if (!bAnyFragmentType)
		{
			CachedData.FragmentTypeText = NoneText;
		}
		else
		{
			CachedData.FragmentTypeText = LOCTEXT("MultipleTypes", "<multiple>");
		}

		Rebuild();
	}

	return CachedData;
}

} // namespace UE::UniversalObjectLocator

#undef LOCTEXT_NAMESPACE