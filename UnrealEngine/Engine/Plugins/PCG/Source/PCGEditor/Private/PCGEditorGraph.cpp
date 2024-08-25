// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraph.h"

#include "PCGEdge.h"
#include "PCGEditorGraphNode.h"
#include "PCGEditorGraphNodeInput.h"
#include "PCGEditorGraphNodeOutput.h"
#include "PCGEditorGraphNodeReroute.h"
#include "PCGEditorModule.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "Elements/PCGReroute.h"
#include "Elements/PCGUserParameterGet.h"

#include "EdGraph/EdGraphPin.h"

namespace PCGEditorGraphUtils
{
	void GetInspectablePin(const UPCGNode* InNode, const UPCGPin* InPin, const UPCGNode*& OutNode, const UPCGPin*& OutPin)
	{
		OutNode = InNode;
		OutPin = InPin;

		// Basically, this is needed so we can go up the graph when the selected node/pin combo is on a reroute node.
		while (OutPin && OutPin->IsOutputPin() &&
			OutNode && OutNode->GetSettings() && OutNode->GetSettings()->IsA<UPCGRerouteSettings>())
		{
			// Since it's a reroute node, we can look at the inbound edge (if any) on the reroute node and go up there
			check(OutNode->GetInputPin(PCGPinConstants::DefaultInputLabel));
			const TArray<TObjectPtr<UPCGEdge>>& Edges = OutNode->GetInputPin(PCGPinConstants::DefaultInputLabel)->Edges;
			// A reroute node can have at most one inbound edge, but we still need to make sure it exists
			if (Edges.Num() == 1)
			{
				OutPin = Edges[0]->InputPin;
				OutNode = OutPin->Node;
			}
			else
			{
				break;
			}
		}
	}
}

void UPCGEditorGraph::InitFromNodeGraph(UPCGGraph* InPCGGraph)
{
	check(InPCGGraph && !PCGGraph);
	PCGGraph = InPCGGraph;

	PCGGraph->OnGraphParametersChangedDelegate.AddUObject(this, &UPCGEditorGraph::OnGraphUserParametersChanged);

	ReconstructGraph();
}

void UPCGEditorGraph::ReconstructGraph()
{
	check(PCGGraph);

	// If there are already some nodes, remove all of them.
	if (!Nodes.IsEmpty())
	{
		Modify();

		TArray<TObjectPtr<class UEdGraphNode>> NodesCopy = Nodes;
		for (UEdGraphNode* Node : NodesCopy)
		{
			RemoveNode(Node);
		}
	}

	TMap<UPCGNode*, UPCGEditorGraphNodeBase*> NodeLookup;
	const bool bSelectNewNode = false;

	UPCGNode* InputNode = PCGGraph->GetInputNode();
	FGraphNodeCreator<UPCGEditorGraphNodeInput> InputNodeCreator(*this);
	UPCGEditorGraphNodeInput* InputGraphNode = InputNodeCreator.CreateNode(bSelectNewNode);
	InputGraphNode->Construct(InputNode);
	InputNodeCreator.Finalize();
	NodeLookup.Add(InputNode, InputGraphNode);

	UPCGNode* OutputNode = PCGGraph->GetOutputNode();
	FGraphNodeCreator<UPCGEditorGraphNodeOutput> OutputNodeCreator(*this);
	UPCGEditorGraphNodeOutput* OutputGraphNode = OutputNodeCreator.CreateNode(bSelectNewNode);
	OutputGraphNode->Construct(OutputNode);
	OutputNodeCreator.Finalize();
	NodeLookup.Add(OutputNode, OutputGraphNode);

	for (UPCGNode* PCGNode : PCGGraph->GetNodes())
	{
		if (!IsValid(PCGNode))
		{
			continue;
		}

		// TODO: replace this with a templated lambda because this is all very similar
		if (Cast<UPCGNamedRerouteDeclarationSettings>(PCGNode->GetSettings()))
		{
			FGraphNodeCreator<UPCGEditorGraphNodeNamedRerouteDeclaration> NodeCreator(*this);
			UPCGEditorGraphNodeNamedRerouteDeclaration* RerouteGraphNode = NodeCreator.CreateNode(bSelectNewNode);
			RerouteGraphNode->Construct(PCGNode);
			NodeCreator.Finalize();
			NodeLookup.Add(PCGNode, RerouteGraphNode);
		}
		else if (Cast<UPCGNamedRerouteUsageSettings>(PCGNode->GetSettings()))
		{
			FGraphNodeCreator<UPCGEditorGraphNodeNamedRerouteUsage> NodeCreator(*this);
			UPCGEditorGraphNodeNamedRerouteUsage* RerouteGraphNode = NodeCreator.CreateNode(bSelectNewNode);
			RerouteGraphNode->Construct(PCGNode);
			NodeCreator.Finalize();
			NodeLookup.Add(PCGNode, RerouteGraphNode);
		}
		else if (Cast<UPCGRerouteSettings>(PCGNode->GetSettings()))
		{
			FGraphNodeCreator<UPCGEditorGraphNodeReroute> NodeCreator(*this);
			UPCGEditorGraphNodeReroute* RerouteGraphNode = NodeCreator.CreateNode(bSelectNewNode);
			RerouteGraphNode->Construct(PCGNode);
			NodeCreator.Finalize();
			NodeLookup.Add(PCGNode, RerouteGraphNode);
		}
		else
		{
			FGraphNodeCreator<UPCGEditorGraphNode> NodeCreator(*this);
			UPCGEditorGraphNode* GraphNode = NodeCreator.CreateNode(bSelectNewNode);
			GraphNode->Construct(PCGNode);
			NodeCreator.Finalize();
			NodeLookup.Add(PCGNode, GraphNode);
		}
	}

	for (const auto& NodeLookupIt : NodeLookup)
	{
		UPCGNode* PCGNode = NodeLookupIt.Key;
		UPCGEditorGraphNodeBase* GraphNode = NodeLookupIt.Value;
		CreateLinks(GraphNode, /*bCreateInbound=*/false, /*bCreateOutbound=*/true, NodeLookup);
	}

	for (const UObject* ExtraNode : PCGGraph->GetExtraEditorNodes())
	{
		if (const UEdGraphNode* ExtraGraphNode = Cast<UEdGraphNode>(ExtraNode))
		{
			UEdGraphNode* NewNode = DuplicateObject(ExtraGraphNode, /*Outer=*/this);
			const bool bIsUserAction = false;
			AddNode(NewNode, bIsUserAction, bSelectNewNode);
		}
	}

	// Ensure graph structure visualization is nice and fresh upon opening.
	UpdateStructuralVisualization(nullptr, nullptr);
}

void UPCGEditorGraph::BeginDestroy()
{
	Super::BeginDestroy();

	OnClose();
}

void UPCGEditorGraph::OnClose()
{
	if (PCGGraph)
	{
		PCGGraph->OnGraphParametersChangedDelegate.RemoveAll(this);
	}
}

void UPCGEditorGraph::CreateLinks(UPCGEditorGraphNodeBase* GraphNode, bool bCreateInbound, bool bCreateOutbound)
{
	check(GraphNode);

	// Build pcg node to pcg editor graph node map
	TMap<UPCGNode*, UPCGEditorGraphNodeBase*> PCGNodeToPCGEditorNodeMap;
	for (const TObjectPtr<UEdGraphNode>& EdGraphNode : Nodes)
	{
		if (UPCGEditorGraphNodeBase* SomeGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			PCGNodeToPCGEditorNodeMap.Add(SomeGraphNode->GetPCGNode(), SomeGraphNode);
		}
	}

	// Forward the call
	CreateLinks(GraphNode, bCreateInbound, bCreateOutbound, PCGNodeToPCGEditorNodeMap);
}

void UPCGEditorGraph::UpdateStructuralVisualization(UPCGComponent* PCGComponentBeingInspected, const FPCGStack* PCGStackBeingInspected)
{
	for (UEdGraphNode* EditorNode : Nodes)
	{
		UPCGEditorGraphNodeBase* PCGEditorNode = Cast<UPCGEditorGraphNodeBase>(EditorNode);
		if (PCGEditorNode && PCGEditorNode->UpdateStructuralVisualization(PCGComponentBeingInspected, PCGStackBeingInspected) != EPCGChangeType::None)
		{
			PCGEditorNode->ReconstructNode();
		}
	}
}

const UPCGEditorGraphNodeBase* UPCGEditorGraph::GetEditorNodeFromPCGNode(const UPCGNode* InPCGNode) const
{
	if (ensure(InPCGNode))
	{
		for (const UEdGraphNode* EdGraphNode : Nodes)
		{
			if (const UPCGEditorGraphNodeBase* PCGEdGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
			{
				if (PCGEdGraphNode->GetPCGNode() == InPCGNode)
				{
					return PCGEdGraphNode;
				}
			}
		}
	}

	return nullptr;
}

void UPCGEditorGraph::CreateLinks(UPCGEditorGraphNodeBase* GraphNode, bool bCreateInbound, bool bCreateOutbound, const TMap<UPCGNode*, UPCGEditorGraphNodeBase*>& InPCGNodeToPCGEditorNodeMap)
{
	check(GraphNode);
	const UPCGNode* PCGNode = GraphNode->GetPCGNode();
	check(PCGNode);

	if (bCreateInbound)
	{
		for (UPCGPin* InputPin : PCGNode->GetInputPins())
		{
			if (!InputPin || InputPin->Properties.bInvisiblePin)
			{
				continue;
			}

			UEdGraphPin* InPin = GraphNode->FindPin(InputPin->Properties.Label, EEdGraphPinDirection::EGPD_Input);
			if (!InPin)
			{
				UE_LOG(LogPCGEditor, Error, TEXT("Invalid InputPin for %s"), *InputPin->Properties.Label.ToString());
				ensure(false);
				continue;
			}

			for (const UPCGEdge* InboundEdge : InputPin->Edges)
			{
				if (!InboundEdge || !InboundEdge->IsValid())
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid inbound edge for %s"), *InputPin->Properties.Label.ToString());
					ensure(false);
					continue;
				}

				const UPCGNode* InboundNode = InboundEdge->InputPin ? InboundEdge->InputPin->Node : nullptr;
				if (!ensure(InboundNode))
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid inbound node for %s"), *InputPin->Properties.Label.ToString());
					continue;
				}

				UPCGEditorGraphNodeBase* const* ConnectedGraphNode = InboundNode ? InPCGNodeToPCGEditorNodeMap.Find(InboundNode) : nullptr;
				UEdGraphPin* OutPin = ConnectedGraphNode ? (*ConnectedGraphNode)->FindPin(InboundEdge->InputPin->Properties.Label, EEdGraphPinDirection::EGPD_Output) : nullptr;
				if (OutPin)
				{
					OutPin->MakeLinkTo(InPin);
				}
				else
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Could not create link to InputPin %s from Node %s"), *InputPin->Properties.Label.ToString(), *InboundNode->GetFName().ToString());
					ensure(false);
				}
			}
		}
	}

	if (bCreateOutbound)
	{
		for (UPCGPin* OutputPin : PCGNode->GetOutputPins())
		{
			if (!OutputPin || OutputPin->Properties.bInvisiblePin)
			{
				continue;
			}

			UEdGraphPin* OutPin = GraphNode->FindPin(OutputPin->Properties.Label, EEdGraphPinDirection::EGPD_Output);
			if (!OutPin)
			{
				UE_LOG(LogPCGEditor, Error, TEXT("Invalid OutputPin for %s"), *OutputPin->Properties.Label.ToString());
				ensure(false);
				continue;
			}

			for (const UPCGEdge* OutboundEdge : OutputPin->Edges)
			{
				if (!OutboundEdge || !OutboundEdge->IsValid())
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid outbound edge for %s"), *OutputPin->Properties.Label.ToString());
					ensure(false);
					continue;
				}

				const UPCGNode* OutboundNode = OutboundEdge->OutputPin ? OutboundEdge->OutputPin->Node : nullptr;
				if (!ensure(OutboundNode))
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid outbound node for %s"), *OutputPin->Properties.Label.ToString());
					continue;
				}

				UPCGEditorGraphNodeBase* const* ConnectedGraphNode = OutboundNode ? InPCGNodeToPCGEditorNodeMap.Find(OutboundNode) : nullptr;
				UEdGraphPin* InPin = ConnectedGraphNode ? (*ConnectedGraphNode)->FindPin(OutboundEdge->OutputPin->Properties.Label, EEdGraphPinDirection::EGPD_Input) : nullptr;
				if (InPin)
				{
					OutPin->MakeLinkTo(InPin);
				}
				else
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Could not create link from OutputPin %s to Node %s"), *OutputPin->Properties.Label.ToString(), *OutboundNode->GetFName().ToString());
					ensure(false);
				}
			}
		}
	}
}

void UPCGEditorGraph::OnGraphUserParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent ChangeType, FName ChangedPropertyName)
{
	if ((ChangeType != EPCGGraphParameterEvent::RemovedUnused && ChangeType != EPCGGraphParameterEvent::RemovedUsed) || InGraph != PCGGraph)
	{
		return;
	}

	// If a parameter was removed, just look for getter nodes that do exists in the editor graph, but not in the PCG graph.
	TArray<UPCGEditorGraphNodeBase*> NodesToRemove;
	for (UEdGraphNode* EditorNode : Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEditorNode = Cast<UPCGEditorGraphNodeBase>(EditorNode))
		{
			if (UPCGNode* PCGNode = PCGEditorNode->GetPCGNode())
			{
				if (UPCGUserParameterGetSettings* Settings = Cast<UPCGUserParameterGetSettings>(PCGNode->GetSettings()))
				{
					if (!PCGGraph->Contains(PCGNode))
					{
						NodesToRemove.Add(PCGEditorNode);
					}
				}
			}
		}
	}

	if (NodesToRemove.IsEmpty())
	{
		return;
	}

	Modify();

	for (UPCGEditorGraphNodeBase* NodeToRemove : NodesToRemove)
	{
		NodeToRemove->DestroyNode();
	}
}
