// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigRerouteNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintUtils.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigRerouteNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigRerouteNodeSpawner"

UControlRigRerouteNodeSpawner* UControlRigRerouteNodeSpawner::CreateGeneric(const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigRerouteNodeSpawner* NodeSpawner = NewObject<UControlRigRerouteNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Reroute,Elbow,Wire,Literal,Make Literal,Constant"));
	MenuSignature.Icon = FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.RigUnit"));

	return NodeSpawner;
}

void UControlRigRerouteNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature UControlRigRerouteNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigRerouteNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigRerouteNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bIsUserFacingNode = !bIsTemplateNode;

	// First create a backing member for our node
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);
	if(RigGraph == nullptr) return nullptr;
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph));
	check(RigBlueprint);

	FName MemberName = NAME_None;

#if WITH_EDITOR
	if (GEditor && bIsUserFacingNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

	if(bIsUserFacingNode)
	{
		Controller->OpenUndoBracket(TEXT("Added Reroute Node."));
	}

	FString PinPath;
	bool bIsInput = false;

	if(bIsUserFacingNode)
	{
		if (UControlRigGraphSchema* RigSchema = Cast<UControlRigGraphSchema>((UEdGraphSchema*)ParentGraph->GetSchema()))
		{
			bIsInput = RigSchema->bLastPinWasInput;

			if (const UEdGraphPin* LastPin = RigSchema->LastPinForCompatibleCheck)
			{
				if (URigVMPin* ModelPin = RigBlueprint->GetModel(ParentGraph)->FindPin(LastPin->GetName()))
				{
					PinPath = ModelPin->GetPinPath();
				}
			}
		}
	}

	URigVMNode* ModelNode = nullptr;
	if(!PinPath.IsEmpty())
	{
		ModelNode = Controller->AddRerouteNodeOnPin(PinPath, bIsInput, true, Location, FString(), bIsUserFacingNode, bIsUserFacingNode);
	}
	else
	{
		static const FString CPPType = RigVMTypeUtils::GetWildCardCPPType();
		static const FName CPPTypeObjectPath = *RigVMTypeUtils::GetWildCardCPPTypeObject()->GetPathName();
		ModelNode = Controller->AddFreeRerouteNode(true, CPPType, CPPTypeObjectPath, false, NAME_None, FString(), Location, FString(), bIsUserFacingNode);
	}

	if (ModelNode)
	{
		NewNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

		if (NewNode)
		{
			Controller->ClearNodeSelection(bIsUserFacingNode);
			Controller->SelectNode(ModelNode, true, bIsUserFacingNode);
		}

		if(bIsUserFacingNode)
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
		if(bIsUserFacingNode)
		{
			Controller->CancelUndoBracket();
		}
	}

	return NewNode;
}

#undef LOCTEXT_NAMESPACE

