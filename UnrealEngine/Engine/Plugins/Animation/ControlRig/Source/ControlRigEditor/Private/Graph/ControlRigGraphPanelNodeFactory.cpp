// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/ControlRigGraphPanelNodeFactory.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraph.h"
#include "EdGraphNode_Comment.h"
#include "Graph/SControlRigGraphNode.h"
#include "Graph/SControlRigGraphNodeComment.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Graph/SControlRigGraphNodeKnot.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"

TSharedPtr<SGraphNode> FControlRigGraphPanelNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Node))
	{
		int32 InputPin = 0, OutputPin = 0;
		if (Node->ShouldDrawNodeAsControlPointOnly(InputPin, OutputPin))
		{
			TSharedRef<SGraphNode> GraphNode = SNew(SControlRigGraphNodeKnot, Node);
			return GraphNode;
		}

		TSharedRef<SGraphNode> GraphNode = 
			SNew(SControlRigGraphNode)
			.GraphNodeObj(ControlRigGraphNode);

		GraphNode->SlatePrepass();
		ControlRigGraphNode->SetDimensions(GraphNode->GetDesiredSize());
		return GraphNode;
	}
	
	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
	{
		if (CommentNode->GetSchema()->IsA(UControlRigGraphSchema::StaticClass()))
		{
			TSharedRef<SGraphNode> GraphNode =
				SNew(SControlRigGraphNodeComment, CommentNode);

			GraphNode->SlatePrepass();
			return GraphNode;
		}
	}

	return nullptr;
}
