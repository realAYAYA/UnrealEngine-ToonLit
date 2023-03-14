// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphSchemaActions.h"

#include "Elements/PCGExecuteBlueprint.h"
#include "PCGEditorCommon.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNode.h"
#include "PCGEditorModule.h"
#include "PCGGraph.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditor.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphSchemaActions"

UEdGraphNode* FPCGEditorGraphSchemaAction_NewNativeElement::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(ParentGraph);
	if (!EditorGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid EditorGraph"));
		return nullptr;
	}

	UPCGGraph* PCGGraph = EditorGraph->GetPCGGraph();
	if (!PCGGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid PCGGraph"));
		return nullptr;
	}

	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorNewNativeElement", "PCG Editor: New Native Element"), nullptr);
	EditorGraph->Modify();

	UPCGSettings* DefaultNodeSettings = nullptr;
	UPCGNode* NewPCGNode = PCGGraph->AddNodeOfType(SettingsClass, DefaultNodeSettings);

	FGraphNodeCreator<UPCGEditorGraphNode> NodeCreator(*EditorGraph);
	UPCGEditorGraphNode* NewNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
	NewNode->Construct(NewPCGNode);
	NewNode->NodePosX = Location.X;
	NewNode->NodePosY = Location.Y;
	NodeCreator.Finalize();

	NewPCGNode->PositionX = Location.X;
	NewPCGNode->PositionY = Location.Y;

	if (FromPin)
	{
		NewNode->AutowireNewNode(FromPin);
	}

	return NewNode;
}

UEdGraphNode* FPCGEditorGraphSchemaAction_NewBlueprintElement::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(ParentGraph);
	if (!EditorGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid EditorGraph"));
		return nullptr;
	}

	UPCGGraph* PCGGraph = EditorGraph->GetPCGGraph();
	if (!PCGGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid PCGGraph"));
		return nullptr;
	}

	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorNewBlueprintELement", "PCG Editor: New Blueprint Element"), nullptr);
	EditorGraph->Modify();

	UPCGSettings* DefaultNodeSettings = nullptr;
	UPCGNode* NewPCGNode = PCGGraph->AddNodeOfType(UPCGBlueprintSettings::StaticClass(), DefaultNodeSettings);
	UPCGBlueprintSettings* DefaultBlueprintSettings = CastChecked<UPCGBlueprintSettings>(DefaultNodeSettings);

	UPCGBlueprintElement* ElementInstance = nullptr;
	TSubclassOf<UPCGBlueprintElement> BlueprintClass = BlueprintClassPath.TryLoadClass<UPCGBlueprintElement>();
	DefaultBlueprintSettings->SetElementType(BlueprintClass, ElementInstance);

	NewPCGNode->UpdateAfterSettingsChangeDuringCreation();

	FGraphNodeCreator<UPCGEditorGraphNode> NodeCreator(*EditorGraph);
	UPCGEditorGraphNode* NewNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
	NewNode->Construct(NewPCGNode);
	NewNode->NodePosX = Location.X;
	NewNode->NodePosY = Location.Y;
	NodeCreator.Finalize();

	NewPCGNode->PositionX = Location.X;
	NewPCGNode->PositionY = Location.Y;

	if (FromPin)
	{
		NewNode->AutowireNewNode(FromPin);
	}

	return NewNode;
}

UEdGraphNode* FPCGEditorGraphSchemaAction_NewSubgraphElement::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(ParentGraph);
	if (!EditorGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid EditorGraph"));
		return nullptr;
	}

	UPCGGraph* PCGGraph = EditorGraph->GetPCGGraph();
	if (!PCGGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid PCGGraph"));
		return nullptr;
	}

	UPCGGraph* Subgraph = CastChecked<UPCGGraph>(SubgraphObjectPath.TryLoad());
	if (Subgraph == PCGGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid Subgraph"));
		return nullptr;
	}

	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorNewSubgraphElement", "PCG Editor: New Subgraph Element"), nullptr);
	EditorGraph->Modify();

	UPCGSettings* DefaultNodeSettings = nullptr;
	UPCGNode* NewPCGNode = PCGGraph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), DefaultNodeSettings);
	UPCGSubgraphSettings* DefaultSubgraphSettings = CastChecked<UPCGSubgraphSettings>(DefaultNodeSettings);
	DefaultSubgraphSettings->Subgraph = Subgraph;

	NewPCGNode->UpdateAfterSettingsChangeDuringCreation();

	FGraphNodeCreator<UPCGEditorGraphNode> NodeCreator(*EditorGraph);
	UPCGEditorGraphNode* NewNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
	NewNode->Construct(NewPCGNode);
	NewNode->NodePosX = Location.X;
	NewNode->NodePosY = Location.Y;
	NodeCreator.Finalize();

	NewPCGNode->PositionX = Location.X;
	NewPCGNode->PositionY = Location.Y;

	if (FromPin)
	{
		NewNode->AutowireNewNode(FromPin);
	}

	return NewNode;
}

UEdGraphNode* FPCGEditorGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /*= true*/)
{
	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(ParentGraph);
	if (!EditorGraph)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Invalid EditorGraph"));
		return nullptr;
	}

	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	TSharedPtr<SGraphEditor> GraphEditorPtr = SGraphEditor::FindGraphEditorForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;
	FSlateRect Bounds;
	if (GraphEditorPtr && GraphEditorPtr->GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	UEdGraphNode_Comment* NewNode = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);

	return NewNode;
}

#undef LOCTEXT_NAMESPACE
