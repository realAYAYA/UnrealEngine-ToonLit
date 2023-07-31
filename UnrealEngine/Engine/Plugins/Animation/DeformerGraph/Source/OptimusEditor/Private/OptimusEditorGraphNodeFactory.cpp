// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorGraphNodeFactory.h"

#include "OptimusEditorGraphNode.h"
#include "Widgets/SOptimusEditorGraphNode.h"

TSharedPtr<SGraphNode> FOptimusEditorGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UOptimusEditorGraphNode* GraphNode = Cast<UOptimusEditorGraphNode>(InNode))
	{
		TSharedRef<SGraphNode> VisualNode = 
			SNew(SOptimusEditorGraphNode).GraphNode(GraphNode);

		VisualNode->SlatePrepass();
		
		// FIXME: Copy visual node's dimensions back to graph node.
		return VisualNode;
	}

	return nullptr;
}

