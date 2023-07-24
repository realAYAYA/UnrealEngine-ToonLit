// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraph/AnimGraphNode_AnimNextInterfaceGraph.h"
#include "Kismet2/CompilerResultsLog.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
//#include "SVariableMappingWidget.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AnimationGraphSchema.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "ControlRigBlueprint.h"
#include "Misc/DefaultValueHelper.h"
#include "AnimNextInterfaceUncookedOnlyUtils.h"
#include "AnimNextInterfaceGraph_EditorData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_AnimNextInterfaceGraph)

#define LOCTEXT_NAMESPACE "AnimGraphNode_AnimNextInterfaceGraph"

UAnimGraphNode_AnimNextInterfaceGraph::UAnimGraphNode_AnimNextInterfaceGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_AnimNextInterfaceGraph::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// display control rig here
	return LOCTEXT("AnimGraphNode_AnimNextInterfaceGraph_Title", "AnimNextInterface Graph");
}

FText UAnimGraphNode_AnimNextInterfaceGraph::GetTooltipText() const
{
	// display control rig here
	return LOCTEXT("AnimGraphNode_AnimNextInterfaceGraph_Tooltip", "Evaluates a AnimNextInterface Graph");
}

void UAnimGraphNode_AnimNextInterfaceGraph::PreloadRequiredAssets()
{
	Super::PreloadRequiredAssets();

	if (Node.AnimNextInterfaceGraph)
	{
		PreloadObject(Node.AnimNextInterfaceGraph);
		PreloadObject(Node.AnimNextInterfaceGraph->EditorData);
	}
}

void UAnimGraphNode_AnimNextInterfaceGraph::CreateCustomPins(TArray<UEdGraphPin*>* OldPins)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	
	// we do this to refresh input variables
	//RebuildExposedProperties();

	// Grab the SKELETON class here as when we are reconstructed during during BP compilation
	// the full generated class is not yet present built.
	if (!GetTargetSkeletonClass())
	{
		// Nothing to search for properties
		return;
	}
}

void UAnimGraphNode_AnimNextInterfaceGraph::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (UClass* TargetClass = GetTargetClass())
	{
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_AnimNextInterfaceGraph::RebuildExposedProperties()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

}

bool UAnimGraphNode_AnimNextInterfaceGraph::IsInputProperty(const FName& PropertyName) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// this is true for both input variables and controls
	return InputVariables.Contains(PropertyName) || !OutputVariables.Contains(PropertyName);
}

bool UAnimGraphNode_AnimNextInterfaceGraph::IsPropertyExposeEnabled(FName PropertyName) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// if known exposable, and and if it hasn't been exposed yet
	if (CustomPinProperties.ContainsByPredicate([PropertyName](const FOptionalPinFromProperty& InOptionalPin){ return InOptionalPin.PropertyName == PropertyName; }))
	{
		return IsInputProperty(PropertyName);
	}

	return false;
}

ECheckBoxState UAnimGraphNode_AnimNextInterfaceGraph::IsPropertyExposed(FName PropertyName) const
{
	if (CustomPinProperties.ContainsByPredicate([PropertyName](const FOptionalPinFromProperty& InOptionalPin){ return InOptionalPin.bShowPin && InOptionalPin.PropertyName == PropertyName; }))
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void UAnimGraphNode_AnimNextInterfaceGraph::OnPropertyExposeCheckboxChanged(ECheckBoxState NewState, FName PropertyName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
}

void UAnimGraphNode_AnimNextInterfaceGraph::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::CustomizeDetails(DetailBuilder);

	// We dont allow multi-select here
	if (DetailBuilder.GetSelectedObjects().Num() > 1)
	{
		return;
	}

	// input/output exposure feature START
	RebuildExposedProperties();

	// ***************************************************************


	TSharedRef<IPropertyHandle> ClassHandle = DetailBuilder.GetProperty(TEXT("Node.AnimNextInterfaceGraph"), GetClass());
	if (ClassHandle->IsValidHandle())
	{
		ClassHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateUObject(this, &UAnimGraphNode_AnimNextInterfaceGraph::OnInstanceClassChanged, &DetailBuilder));
	}
}

void UAnimGraphNode_AnimNextInterfaceGraph::GetVariables(bool bInput, TMap<FName, FRigVMExternalVariable>& OutVariables) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	OutVariables.Reset();

	if (URigVMBlueprintGeneratedClass* TargetClass = Cast<URigVMBlueprintGeneratedClass>(GetTargetClass()))
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(TargetClass->ClassGeneratedBy))
		{
			//RigBlueprint->CleanupVariables();
			UControlRig* ControlRig = TargetClass->GetDefaultObject<UControlRig>();
			if (ControlRig)
			{
				const TArray<FRigVMExternalVariable>& PublicVariables = ControlRig->GetPublicVariables();
				for (const FRigVMExternalVariable& PublicVariable : PublicVariables)
				{
					if (!bInput || !PublicVariable.bIsReadOnly)
					{
						OutVariables.Add(PublicVariable.Name, PublicVariable);
					}
				}
			}
		}
	}
}

void UAnimGraphNode_AnimNextInterfaceGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool bRequiresNodeReconstruct = false;
	FProperty* ChangedProperty = PropertyChangedEvent.Property;

	if (ChangedProperty)
	{
		if (ChangedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNode_AnimNextInterfaceGraph, AnimNextInterfaceGraph))
		{
			bRequiresNodeReconstruct = true;
			RebuildExposedProperties();
		}
	}

	if (bRequiresNodeReconstruct)
	{
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_AnimNextInterfaceGraph::PostReconstructNode()
{
	Super::PostReconstructNode();
}

void UAnimGraphNode_AnimNextInterfaceGraph::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
}

UObject* UAnimGraphNode_AnimNextInterfaceGraph::GetJumpTargetForDoubleClick() const
{
	return Node.AnimNextInterfaceGraph;
}

#undef LOCTEXT_NAMESPACE
