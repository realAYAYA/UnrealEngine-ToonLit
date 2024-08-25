// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "UObject/GCObject.h"
#include "Dataflow/DataflowCore.h"

#include "DataflowSNode.generated.h"

class UDataflowEdNode;
class SCheckBox;

//
// SDataflowEdNode
//

class DATAFLOWEDITOR_API SDataflowEdNode : public SGraphNode , public FGCObject
{
	typedef SGraphNode Super;

public:
	typedef TFunction<void(UEdGraphNode* InNode, bool InEnabled)> FToggleRenderCallback;


	SLATE_BEGIN_ARGS(SDataflowEdNode)
		: _GraphNodeObj(nullptr)
	{}
	SLATE_ARGUMENT(UDataflowEdNode*, GraphNodeObj)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UDataflowEdNode* InNode);

	// SGraphNode interface
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const;
	virtual void UpdateErrorInfo() override;

	static void CopyDataflowNodeSettings(TSharedPtr<FDataflowNode> SourceDataflowNode, TSharedPtr<FDataflowNode> TargetDataflowNode);

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SDataflowEdNode");
	}
	//~ End FGCObject interface

private:
	TObjectPtr<UDataflowEdNode> DataflowGraphNode = nullptr;	

	FCheckBoxStyle CheckBoxStyle;
	TSharedPtr<SCheckBox> RenderCheckBoxWidget;

	//FCheckBoxStyle CacheStatusStyle;
	//TSharedPtr<SCheckBox> CacheStatus;
};

//
// Action to add a node to the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode() : FEdGraphSchemaAction() {}

	FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode(const FName& InType, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, 0, InKeywords), NodeTypeName(InType) {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const FName & NodeTypeName);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	FName NodeTypeName;
};

//
// Action to duplicate a set of nodes in the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode() : FEdGraphSchemaAction() {}

	FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode(const FName& InType, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, 0, InKeywords), NodeTypeName(InType) {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const FName& NodeTypeName);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FName NodeTypeName;
	TSharedPtr<FDataflowNode> DataflowNodeToDuplicate;
};

//
// Action to paste a set of nodes in the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode() : FEdGraphSchemaAction() {}

	FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode(const FName& InType, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, 0, InKeywords), NodeTypeName(InType) {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const FName& NodeTypeName);

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FName NodeTypeName;
	FName NodeName;
	FString NodeProperties;
};
