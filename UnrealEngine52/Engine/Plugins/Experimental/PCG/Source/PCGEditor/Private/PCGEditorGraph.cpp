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

void UPCGEditorGraph::InitFromNodeGraph(UPCGGraph* InPCGGraph)
{
	check(InPCGGraph && !PCGGraph);
	PCGGraph = InPCGGraph;

	PCGGraph->OnGraphParametersChangedDelegate.AddUObject(this, &UPCGEditorGraph::OnGraphUserParametersChanged);

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
		
		if (Cast<UPCGRerouteSettings>(PCGNode->GetSettings()))
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
	// Build graph node to pcg node map
	TMap<UPCGNode*, UPCGEditorGraphNodeBase*> GraphNodeToPCGNodeMap;

	for (const TObjectPtr<UEdGraphNode>& EdGraphNode : Nodes)
	{
		if (UPCGEditorGraphNodeBase* SomeGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			GraphNodeToPCGNodeMap.Add(SomeGraphNode->GetPCGNode(), SomeGraphNode);
		}
	}

	// Forward the call
	CreateLinks(GraphNode, bCreateInbound, bCreateOutbound, GraphNodeToPCGNodeMap);
}

void UPCGEditorGraph::CreateLinks(UPCGEditorGraphNodeBase* GraphNode, bool bCreateInbound, bool bCreateOutbound, const TMap<UPCGNode*, UPCGEditorGraphNodeBase*>& GraphNodeToPCGNodeMap)
{
	check(GraphNode);
	const UPCGNode* PCGNode = GraphNode->GetPCGNode();
	check(PCGNode);

	if (bCreateInbound)
	{
		for (const UPCGPin* InputPin : PCGNode->GetInputPins())
		{
			UEdGraphPin* InPin = GraphNode->FindPin(InputPin->Properties.Label, EEdGraphPinDirection::EGPD_Input);

			if (!InPin)
			{
				if (!Cast<UPCGEditorGraphNodeInput>(GraphNode))
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid InputPin for %s"), *InputPin->Properties.Label.ToString());
				}

				continue;
			}

			for (const UPCGEdge* InboundEdge : InputPin->Edges)
			{
				if (!InboundEdge->IsValid())
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid inbound edge for %s"), *InputPin->Properties.Label.ToString());
					continue;
				}

				const UPCGNode* InboundNode = InboundEdge->InputPin->Node;
				if (UPCGEditorGraphNodeBase* const* ConnectedGraphNode = GraphNodeToPCGNodeMap.Find(InboundNode))
				{
					if (UEdGraphPin* OutPin = (*ConnectedGraphNode)->FindPin(InboundEdge->InputPin->Properties.Label, EEdGraphPinDirection::EGPD_Output))
					{
						OutPin->MakeLinkTo(InPin);
					}
					else
					{
						UE_LOG(LogPCGEditor, Error, TEXT("Could not create link to InputPin %s from Node %s"), *InputPin->Properties.Label.ToString(), *InboundNode->GetFName().ToString());
						continue;
					}
				}
			}
		}
	}

	if (bCreateOutbound)
	{
		for (const UPCGPin* OutputPin : PCGNode->GetOutputPins())
		{
			UEdGraphPin* OutPin = GraphNode->FindPin(OutputPin->Properties.Label, EEdGraphPinDirection::EGPD_Output);

			if (!OutPin)
			{
				if (!Cast<UPCGEditorGraphNodeOutput>(GraphNode))
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid OutputPin for %s"), *OutputPin->Properties.Label.ToString());
				}
				
				continue;
			}

			for (const UPCGEdge* OutboundEdge : OutputPin->Edges)
			{
				if (!OutboundEdge->IsValid())
				{
					UE_LOG(LogPCGEditor, Error, TEXT("Invalid outbound edge for %s"), *OutputPin->Properties.Label.ToString());
					continue;
				}

				const UPCGNode* OutboundNode = OutboundEdge->OutputPin->Node;
				if (UPCGEditorGraphNodeBase* const* ConnectedGraphNode = GraphNodeToPCGNodeMap.Find(OutboundNode))
				{
					if (UEdGraphPin* InPin = (*ConnectedGraphNode)->FindPin(OutboundEdge->OutputPin->Properties.Label, EEdGraphPinDirection::EGPD_Input))
					{
						OutPin->MakeLinkTo(InPin);
					}
					else
					{
						UE_LOG(LogPCGEditor, Error, TEXT("Could not create link from OutputPin %s to Node %s"), *OutputPin->Properties.Label.ToString(), *OutboundNode->GetFName().ToString());
						continue;
					}
				}
			}
		}
	}
}

void UPCGEditorGraph::OnGraphUserParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent ChangeType, FName ChangedPropertyName)
{
	if (ChangeType != EPCGGraphParameterEvent::Removed || InGraph != PCGGraph)
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