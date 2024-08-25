// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "SGraphNodeComment.h"

#include "DataflowSCommentNode.generated.h"

//
// SDataflowEdNodeComment
//

class DATAFLOWEDITOR_API SDataflowEdNodeComment : public SGraphNodeComment
{
	typedef SGraphNodeComment Super;

public:
	SLATE_BEGIN_ARGS(SDataflowEdNodeComment) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode_Comment* InNode) { Super::Construct(SGraphNodeComment::FArguments(), InNode); }
};

//
// Action to add a node to the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode(const TSharedPtr<SGraphEditor>& InGraphEditor) : FEdGraphSchemaAction()
		, GraphEditor(InGraphEditor)
		{}

	FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode() {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const TSharedPtr<SGraphEditor>& GraphEditor);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	TSharedPtr<SGraphEditor> GraphEditor;
};

//
// Action to duplicate a comment node in the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode : public FEdGraphSchemaAction, public FGCObject
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode(const TSharedPtr<SGraphEditor>& InGraphEditor) : FEdGraphSchemaAction()
		, GraphEditor(InGraphEditor)
	{}

	FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode() {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const TSharedPtr<SGraphEditor>& GraphEditor);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDataflowConnectionDrawingPolicy"); }

	TSharedPtr<SGraphEditor> GraphEditor;

	TObjectPtr<UEdGraphNode_Comment> CommentNodeToDuplicate;
};

//
// Action to paste a comment node in the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode(const TSharedPtr<SGraphEditor>& InGraphEditor) : FEdGraphSchemaAction()
		, GraphEditor(InGraphEditor)
	{}

	FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode() {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const TSharedPtr<SGraphEditor>& GraphEditor);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	TSharedPtr<SGraphEditor> GraphEditor;
	FName NodeName;
	FVector2D Size;
	FLinearColor Color;
	int32 FontSize;
};
