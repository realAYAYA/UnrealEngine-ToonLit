// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphInvokeEntryNodeSpawner.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMBlueprint.h"
#include "RigVMModel/Nodes/RigVMInvokeEntryNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphInvokeEntryNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "RigVMEdGraphInvokeEntryNodeSpawner"

URigVMEdGraphInvokeEntryNodeSpawner* URigVMEdGraphInvokeEntryNodeSpawner::CreateForEntry(URigVMBlueprint* InBlueprint, const FName& InEntryName, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	URigVMEdGraphInvokeEntryNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphInvokeEntryNodeSpawner>(GetTransientPackage());
	NodeSpawner->Blueprint = InBlueprint;
	NodeSpawner->EntryName = InEntryName;
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Event,Entry,Invoke,Run,Launch,Start"));
	MenuSignature.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");

	return NodeSpawner;
}

void URigVMEdGraphInvokeEntryNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature URigVMEdGraphInvokeEntryNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(FString("InvokeEntryNode=" + EntryName.ToString()));
}

FBlueprintActionUiSpec URigVMEdGraphInvokeEntryNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* URigVMEdGraphInvokeEntryNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	URigVMEdGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);

	// First create a backing member for our node
	URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(ParentGraph);
	if(RigGraph == nullptr) return nullptr;
	URigVMBlueprint* RigBlueprint = Cast<URigVMBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph));
	check(RigBlueprint);

#if WITH_EDITOR
	if (GEditor && !bIsTemplateNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	FString NodeName;
	if(bIsTemplateNode)
	{
		// since we are removing the node at the end of this function
		// we need to create a unique here.
		static constexpr TCHAR InvokeEntryNodeNameFormat[] = TEXT("InvokeEntryNode_%s");
		NodeName = FString::Printf(InvokeEntryNodeNameFormat, *EntryName.ToString());

		TArray<FPinInfo> Pins;

		static UScriptStruct* ExecuteScriptStruct = FRigVMExecuteContext::StaticStruct();
		static const FName ExecuteStructName = *ExecuteScriptStruct->GetStructCPPName();
		Pins.Emplace(FRigVMStruct::ExecuteName, ERigVMPinDirection::IO, ExecuteStructName, ExecuteScriptStruct);

		return SpawnTemplateNode(ParentGraph, Pins, *NodeName);
	}

	URigVMController* Controller = RigBlueprint->GetController(ParentGraph);
	Controller->OpenUndoBracket(TEXT("Add Run Event"));

	if (URigVMNode* ModelNode = Controller->AddInvokeEntryNode(EntryName, Location, NodeName, true, true))
	{
		for (UEdGraphNode* Node : ParentGraph->Nodes)
		{
			if (URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(Node))
			{
				if (RigNode->GetModelNodeName() == ModelNode->GetFName())
				{
					NewNode = RigNode;
					break;
				}
			}
		}

		if (NewNode)
		{
			Controller->ClearNodeSelection(true);
			Controller->SelectNode(ModelNode, true, true);
		}
		Controller->CloseUndoBracket();
	}
	else
	{
		Controller->CancelUndoBracket();
	}


	return NewNode;
}

bool URigVMEdGraphInvokeEntryNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	if(URigVMEdGraphNodeSpawner::IsTemplateNodeFilteredOut(Filter))
	{
		return true;
	}

	if (Blueprint.IsValid())
	{
		if (!Filter.Context.Blueprints.Contains(Blueprint.Get()))
		{
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

