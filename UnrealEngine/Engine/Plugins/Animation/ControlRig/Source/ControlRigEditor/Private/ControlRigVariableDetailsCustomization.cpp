// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigVariableDetailsCustomization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "BlueprintEditorModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ControlRigBlueprint.h"
#include "RigVMModel/RigVMController.h"

#define LOCTEXT_NAMESPACE "ControlRigVariableDetailsCustomization"

TSharedPtr<IDetailCustomization> FControlRigVariableDetailsCustomization::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>((*Objects)[0]))
		{
			if (Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UControlRig::StaticClass()))
			{
				return MakeShareable(new FControlRigVariableDetailsCustomization(InBlueprintEditor, Blueprint));
			}
		}
	}

	return nullptr;
}

void FControlRigVariableDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 0)
	{
		UPropertyWrapper* PropertyWrapper = Cast<UPropertyWrapper>(ObjectsBeingCustomized[0].Get());
		TWeakFieldPtr<FProperty> PropertyBeingCustomized = PropertyWrapper ? PropertyWrapper->GetProperty() : nullptr;
		if (PropertyBeingCustomized.IsValid())
		{
			const FText AnimationInputText = LOCTEXT("AnimationInput", "Animation Input");
			const FText AnimationOutputText = LOCTEXT("AnimationOutput", "Animation Output");
			const FText AnimationInputTooltipText = LOCTEXT("AnimationInputTooltip", "Whether this variable acts as an input to this animation controller.\nSelecting this allow it to be exposed as an input pin on Evaluation nodes.");
			const FText AnimationOutputTooltipText = LOCTEXT("AnimationOutputTooltip", "Whether this variable acts as an output from this animation controller.\nSelecting this will add a pin to the Animation Output node.");
			
			DetailLayout.EditCategory("Variable")
				.AddCustomRow(AnimationInputText)
				.NameContent()
				[
					SNew(STextBlock)
					.Visibility(IsAnimationFlagEnabled(PropertyBeingCustomized) ? EVisibility::Visible : EVisibility::Hidden)
					.Font(DetailLayout.GetDetailFont())
					.Text(AnimationOutputText)
					.ToolTipText(AnimationOutputTooltipText)
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.Visibility(IsAnimationFlagEnabled(PropertyBeingCustomized) ? EVisibility::Visible : EVisibility::Hidden)
					.IsChecked_Raw(this, &FControlRigVariableDetailsCustomization::IsAnimationOutputChecked, PropertyBeingCustomized)
					.OnCheckStateChanged_Raw(this, &FControlRigVariableDetailsCustomization::HandleAnimationOutputCheckStateChanged, PropertyBeingCustomized)
					.ToolTipText(AnimationOutputTooltipText)
				];

			DetailLayout.EditCategory("Variable")
				.AddCustomRow(AnimationOutputText)
				.NameContent()
				[
					SNew(STextBlock)
					.Visibility(IsAnimationFlagEnabled(PropertyBeingCustomized) ? EVisibility::Visible : EVisibility::Hidden)
					.Font(DetailLayout.GetDetailFont())
					.Text(AnimationInputText)
					.ToolTipText(AnimationInputTooltipText)
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.Visibility(IsAnimationFlagEnabled(PropertyBeingCustomized) ? EVisibility::Visible : EVisibility::Hidden)
					.IsChecked_Raw(this, &FControlRigVariableDetailsCustomization::IsAnimationInputChecked, PropertyBeingCustomized)
					.OnCheckStateChanged_Raw(this, &FControlRigVariableDetailsCustomization::HandleAnimationInputCheckStateChanged, PropertyBeingCustomized)
					.ToolTipText(AnimationInputTooltipText)
				];
		}
	}
}

bool FControlRigVariableDetailsCustomization::IsAnimationFlagEnabled(TWeakFieldPtr<FProperty> PropertyBeingCustomized) const
{
	return false;
}

ECheckBoxState FControlRigVariableDetailsCustomization::IsAnimationOutputChecked(TWeakFieldPtr<FProperty> PropertyBeingCustomized) const
{
	return ECheckBoxState::Unchecked;
}

void FControlRigVariableDetailsCustomization::HandleAnimationOutputCheckStateChanged(ECheckBoxState CheckBoxState, TWeakFieldPtr<FProperty> PropertyBeingCustomized)
{
}

ECheckBoxState FControlRigVariableDetailsCustomization::IsAnimationInputChecked(TWeakFieldPtr<FProperty> PropertyBeingCustomized) const
{
	return ECheckBoxState::Unchecked;
}

void FControlRigVariableDetailsCustomization::HandleAnimationInputCheckStateChanged(ECheckBoxState CheckBoxState, TWeakFieldPtr<FProperty> PropertyBeingCustomized)
{
}

#undef LOCTEXT_NAMESPACE
