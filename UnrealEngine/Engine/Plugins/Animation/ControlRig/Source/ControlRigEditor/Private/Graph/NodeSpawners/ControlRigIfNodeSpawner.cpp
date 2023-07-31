// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigIfNodeSpawner.h"
#include "ControlRigUnitNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMCore/RigVMUnknownType.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintUtils.h"
#include "RigVMModel/Nodes/RigVMIfNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigIfNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigIfNodeSpawner"

UControlRigIfNodeSpawner* UControlRigIfNodeSpawner::CreateGeneric(const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigIfNodeSpawner* NodeSpawner = NewObject<UControlRigIfNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Switch,If,Branch,Condition,Else,Flip"));
	MenuSignature.Icon = FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.RigUnit"));

	return NodeSpawner;
}

FBlueprintNodeSignature UControlRigIfNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigIfNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigIfNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bIsUserFacingNode = !bIsTemplateNode;

	// First create a backing member for our node
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);
	if(RigGraph == nullptr) return nullptr;
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph));
	check(RigBlueprint);

	FString CPPType = RigVMTypeUtils::GetWildCardCPPType();
	FName CPPTypeObjectPath = *RigVMTypeUtils::GetWildCardCPPTypeObject()->GetPathName();

	if(bIsUserFacingNode)
	{
		if (UControlRigGraphSchema* RigSchema = Cast<UControlRigGraphSchema>((UEdGraphSchema*)ParentGraph->GetSchema()))
		{
			if (const UEdGraphPin* LastPin = RigSchema->LastPinForCompatibleCheck)
			{
				if (URigVMPin* ModelPin = RigBlueprint->GetModel(ParentGraph)->FindPin(LastPin->GetName()))
				{
					RigVMTypeUtils::CPPTypeFromPin(ModelPin, CPPType, CPPTypeObjectPath, false);
				}
			}
		}
	}

	FName MemberName = NAME_None;

#if WITH_EDITOR
	if (GEditor && !bIsTemplateNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

	FName Name = *URigVMIfNode::IfName;

	if (!bIsTemplateNode)
	{
		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
	}

	if (URigVMIfNode* ModelNode = Controller->AddIfNode(CPPType, CPPTypeObjectPath, Location, Name.ToString(), bIsUserFacingNode, !bIsTemplateNode))
	{
		NewNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

		if (NewNode && bIsUserFacingNode)
		{
			Controller->ClearNodeSelection(true);
			Controller->SelectNode(ModelNode, true, true);
		}

		if (bIsUserFacingNode)
		{
			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->RemoveNode(ModelNode, false);
		}
	}
	else
	{
		if (bIsUserFacingNode)
		{
			Controller->CancelUndoBracket();
		}
	}


	return NewNode;
}

bool UControlRigIfNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Blueprint))
		{
			if (Filter.Context.Pins.Num() == 0)
			{
				return false;
			}

			FString PinPath = Filter.Context.Pins[0]->GetName();
			UEdGraph* EdGraph = Filter.Context.Pins[0]->GetOwningNode()->GetGraph();
			if (URigVMPin* ModelPin = RigBlueprint->GetModel(EdGraph)->FindPin(PinPath))
			{
				return ModelPin->IsExecuteContext();
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

