// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeFactory.h"

#include "PCGEditorGraphNodeBase.h"
#include "SPCGEditorGraphNode.h"

TSharedPtr<SGraphNode> FPCGEditorGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UPCGEditorGraphNodeBase* GraphNode = Cast<UPCGEditorGraphNodeBase>(InNode))
	{
		TSharedRef<SGraphNode> VisualNode =
			SNew(SPCGEditorGraphNode, GraphNode);

		VisualNode->SlatePrepass();

		return VisualNode;
	}

	return nullptr;
}
