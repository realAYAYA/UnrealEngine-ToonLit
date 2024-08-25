// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Graph/AnimNextGraph_EdGraphNode.h"
#include "Graph/SAnimNextGraphNode.h"

TSharedPtr<SGraphNode> FAnimNextGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UAnimNextGraph_EdGraphNode* AnimNextGraphNode = Cast<UAnimNextGraph_EdGraphNode>(Node))
	{
		TSharedPtr<SGraphNode> GraphNode =
			SNew(SAnimNextGraphNode)
			.GraphNodeObj(AnimNextGraphNode);

		GraphNode->SlatePrepass();
		AnimNextGraphNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}

	return nullptr;
}
