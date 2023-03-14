// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraAnimationSequenceCustomization.h"
#include "CineCameraActor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styles/TemplateSequenceEditorStyle.h"
#include "TemplateSequence.h"

#define LOCTEXT_NAMESPACE "CameraAnimationSequenceCustomization"

void FCameraAnimationSequenceCustomization::RegisterSequencerCustomization(FSequencerCustomizationBuilder& Builder)
{
	FTemplateSequenceCustomizationBase::RegisterSequencerCustomization(Builder);

	CameraActorClasses = { ACameraActor::StaticClass(), ACineCameraActor::StaticClass() };

	FSequencerCustomizationInfo Customization;

	TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension("BaseCommands", EExtensionHook::After, nullptr, FToolBarExtensionDelegate::CreateRaw(this, &FCameraAnimationSequenceCustomization::ExtendSequencerToolbar));
	Customization.ToolbarExtender = ToolbarExtender;

	Customization.OnAssetsDrop.BindLambda([](const TArray<UObject*>&, const FAssetDragDropOp&) -> ESequencerDropResult { return ESequencerDropResult::DropDenied; });
	Customization.OnClassesDrop.BindLambda([](const TArray<TWeakObjectPtr<UClass>>&, const FClassDragDropOp&) -> ESequencerDropResult { return ESequencerDropResult::DropDenied; });
	Customization.OnActorsDrop.BindLambda([](const TArray<TWeakObjectPtr<AActor>>&, const FActorDragDropOp&) -> ESequencerDropResult { return ESequencerDropResult::DropDenied; });

	Builder.AddCustomization(Customization);
}

void FCameraAnimationSequenceCustomization::UnregisterSequencerCustomization()
{
	FTemplateSequenceCustomizationBase::UnregisterSequencerCustomization();
}

void FCameraAnimationSequenceCustomization::ExtendSequencerToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginStyleOverride("SequencerToolBar");

	ToolbarBuilder.AddSeparator();

	ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FCameraAnimationSequenceCustomization::GetBoundCameraClassMenuContent),
			LOCTEXT("CameraTypePicker", "Camera Type"),
			LOCTEXT("CameraTypePickerTooltip", "Change the base camera actor type that this template sequence can bind to"),
			FSlateIcon(FTemplateSequenceEditorStyle::Get()->GetStyleSetName(), "TemplateSequenceEditor.Chain"));
	
	ToolbarBuilder.EndStyleOverride();
}

TSharedRef<SWidget> FCameraAnimationSequenceCustomization::GetBoundCameraClassMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (UClass* CameraActorClass : CameraActorClasses)
	{
		MenuBuilder.AddMenuEntry(
				FText::FromString(CameraActorClass->GetName()),
				LOCTEXT("CameraTypeChoice", "Choose this type of camera as the type this template sequence can bind to"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this, CameraActorClass]() { 
						OnBoundActorClassPicked(CameraActorClass); 
						}),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda([this, CameraActorClass]() {
						return IsBoundToActorClass(CameraActorClass) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; 
						})
					),
				NAME_None,
				EUserInterfaceActionType::RadioButton
				);
	}

	return MenuBuilder.MakeWidget();
}

bool FCameraAnimationSequenceCustomization::IsBoundToActorClass(UClass* InClass)
{
	UClass* BoundActorClass = GetBoundActorClass();
	return BoundActorClass == InClass;
}

#undef LOCTEXT_NAMESPACE
