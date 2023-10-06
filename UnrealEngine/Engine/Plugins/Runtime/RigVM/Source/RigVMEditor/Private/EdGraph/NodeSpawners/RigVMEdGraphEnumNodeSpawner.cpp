// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphEnumNodeSpawner.h"
#include "EdGraph/NodeSpawners/RigVMEdGraphUnitNodeSpawner.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMBlueprint.h"

#include "RigVMModel/Nodes/RigVMEnumNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphEnumNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "RigVMEdGraphEnumNodeSpawner"

URigVMEdGraphEnumNodeSpawner* URigVMEdGraphEnumNodeSpawner::CreateForEnum(UEnum* InEnum, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	URigVMEdGraphEnumNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphEnumNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();
	NodeSpawner->Enum = InEnum;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Enum"));
	MenuSignature.Icon = FSlateIcon(TEXT("RigVMEditorStyle"), TEXT("RigVM.Unit"));

	return NodeSpawner;
}

FBlueprintNodeSignature URigVMEdGraphEnumNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(FString("RigUnit=" + Enum->GetFName().ToString()));
}

FBlueprintActionUiSpec URigVMEdGraphEnumNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* URigVMEdGraphEnumNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	URigVMEdGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bIsUserFacingNode = !bIsTemplateNode;

	if (bIsTemplateNode)
	{
		NewNode = NewObject<URigVMEdGraphNode>(ParentGraph, TEXT("EnumNode"));
		ParentGraph->AddNode(NewNode, false);

		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();

		UEdGraphPin* OutputValuePin = UEdGraphPin::CreatePin(NewNode);
		NewNode->Pins.Add(OutputValuePin);

		OutputValuePin->PinType.PinCategory = TEXT("int32");
		OutputValuePin->Direction = EGPD_Output;
		NewNode->SetFlags(RF_Transactional);

		return NewNode;
	}

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

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

	FName Name = *URigVMEnumNode::EnumName;

	if (!bIsTemplateNode)
	{
		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
	}

	if (URigVMEnumNode* ModelNode = Controller->AddEnumNode(*Enum->GetPathName(), Location, Name.ToString(), bIsUserFacingNode, !bIsTemplateNode))
	{
		NewNode = Cast<URigVMEdGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

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

#undef LOCTEXT_NAMESPACE

