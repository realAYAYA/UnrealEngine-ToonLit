// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusCoreNotify.h"

#include "EdGraph/EdGraph.h"
#include "Containers/Set.h"

#include "OptimusEditorGraph.generated.h"

struct FSlateBrush;
class UOptimusNode;
class UOptimusDeformer;
class UOptimusEditorGraphNode;

UCLASS()
class UOptimusEditorGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UOptimusEditorGraph();

	void InitFromNodeGraph(UOptimusNodeGraph* InNodeGraph);

	void Reset();

	UOptimusNodeGraph* GetModelGraph() const { return NodeGraph; }

	UOptimusEditorGraphNode* FindGraphNodeFromModelNode(const UOptimusNode* Node);

	const TSet<UOptimusEditorGraphNode*> &GetSelectedNodes() const { return SelectedNodes; }

	// Do a visual refresh of the node.
	void RefreshVisualNode(UOptimusEditorGraphNode *InGraphNode);

	///
	static const FSlateBrush* GetGraphTypeIcon(const UOptimusNodeGraph* InModelGraph);

protected:
	friend class FOptimusEditor;

	void SetSelectedNodes(const TSet<UOptimusEditorGraphNode*>& InSelectedNodes)
	{
		SelectedNodes = InSelectedNodes;
	}

private:
	void HandleThisGraphModified(
		const FEdGraphEditAction &InEditAction
	);

	void HandleNodeGraphModified(
		EOptimusGraphNotifyType InNotifyType, 
		UOptimusNodeGraph *InNodeGraph, 
		UObject *InSubject
		);

	UOptimusEditorGraphNode* AddGraphNodeFromModelNode(UOptimusNode* InModelNode);

	TObjectPtr<UOptimusNodeGraph> NodeGraph;

	TSet<UOptimusEditorGraphNode*> SelectedNodes;
};
