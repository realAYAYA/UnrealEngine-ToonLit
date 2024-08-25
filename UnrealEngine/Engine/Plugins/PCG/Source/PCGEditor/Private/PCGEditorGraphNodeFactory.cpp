// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeFactory.h"

#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorGraphNodeReroute.h"
#include "PCGNode.h"
#include "SPCGEditorGraphNode.h"
#include "SPCGEditorGraphVarNode.h"
#include "Elements/PCGReroute.h"
#include "Elements/PCGUserParameterGet.h"

#include "SGraphNodeKnot.h"

class SPCGEditorGraphNodeKnot : public SGraphNodeKnot
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodeKnot) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InPCGGraphNode)
	{
		SGraphNodeKnot::Construct(SGraphNodeKnot::FArguments(), InPCGGraphNode);
		InPCGGraphNode->OnNodeChangedDelegate.BindSP(this, &SPCGEditorGraphNodeKnot::OnNodeChanged);
	}

private:
	void OnNodeChanged()
	{
		UpdateGraphNode();
	}
};


TSharedPtr<SGraphNode> FPCGEditorGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UPCGEditorGraphNodeBase* GraphNode = Cast<UPCGEditorGraphNodeBase>(InNode))
	{
		TSharedPtr<SGraphNode> VisualNode;
		
		const UPCGNode* PCGNode = GraphNode->GetPCGNode();
		if (PCGNode && Cast<UPCGNamedRerouteBaseSettings>(PCGNode->GetSettings()))
		{
			SAssignNew(VisualNode, SPCGEditorGraphVarNode, GraphNode);
		}
		else if (PCGNode && Cast<UPCGRerouteSettings>(PCGNode->GetSettings()))
		{
			SAssignNew(VisualNode, SPCGEditorGraphNodeKnot, GraphNode);
		}
		else if (PCGNode && Cast<UPCGUserParameterGetSettings>(PCGNode->GetSettings()))
		{
			SAssignNew(VisualNode, SPCGEditorGraphVarNode, GraphNode);
		}
		else
		{
			SAssignNew(VisualNode, SPCGEditorGraphNode, GraphNode);
		}
		
		VisualNode->SlatePrepass();

		return VisualNode.ToSharedRef();
	}

	return nullptr;
}
