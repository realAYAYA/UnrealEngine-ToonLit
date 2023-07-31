// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCommentNode.h"

#include "EdGraphNode_Comment.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Logging/LogMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Settings/EditorStyleSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SDataflowEdNodeComment"

//
// Add a menu option to create a graph node.
//
TSharedPtr<FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode> FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode::CreateAction(UEdGraph* ParentGraph, const TSharedPtr<SGraphEditor>& GraphEditor)
{
	return MakeShared<FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode>(GraphEditor);
}

//
//  Created comment node
//
UEdGraphNode* FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;
	FSlateRect Bounds;

	if (GraphEditor->GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	CommentTemplate->bCommentBubbleVisible_InDetailsPanel = false;
	CommentTemplate->bCommentBubbleVisible = false;
	CommentTemplate->bCommentBubblePinned = false;

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DataflowEditorNewNode", "Dataflow Editor: New Comment Node"));
	ParentGraph->Modify();

	CommentTemplate->SetFlags(RF_Transactional);

	// set outer to be the graph so it doesn't go away
	CommentTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
	ParentGraph->AddNode(CommentTemplate, true, bSelectNewNode);

	CommentTemplate->CreateNewGuid();
	CommentTemplate->PostPlacedNewNode();
	CommentTemplate->AllocateDefaultPins();
	CommentTemplate->AutowireNewNode(FromPin);

	CommentTemplate->NodePosX = SpawnLocation.X;
	CommentTemplate->NodePosY = SpawnLocation.Y;
	CommentTemplate->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

	ParentGraph->NotifyGraphChanged();

	return CommentTemplate;
}

//void FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode::AddReferencedObjects(FReferenceCollector& Collector)
//{
//	FEdGraphSchemaAction::AddReferencedObjects(Collector);
//	Collector.AddReferencedObject(NodeTemplate);
//}

#undef LOCTEXT_NAMESPACE
