// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSNodeFactories.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowEdNode.h"
#include "EdGraphNode_Comment.h"
#include "Dataflow/DataflowCommentNode.h"
#include "Dataflow/DataflowSchema.h"

TSharedPtr<class SGraphNode> FDataflowSNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UDataflowEdNode* Node = Cast<UDataflowEdNode>(InNode))
	{
		return SNew(SDataflowEdNode, Node);
	}	
	else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(InNode))
	{
		if (CommentNode->GetSchema()->IsA(UDataflowSchema::StaticClass()))
		{
			return SNew(SDataflowEdNodeComment, CommentNode);
		}
	}
	return NULL;
}


