// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphPin.h"

#include "Algo/Find.h"
#include "EdGraph/EdGraphSchema.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphEdge.h"
#include "Graph/Nodes/MovieGraphSubgraphNode.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Graph/MovieGraphConfig.h"

#if WITH_EDITOR
bool UMovieGraphPin::Modify(bool bAlwaysMarkDirty)
{
	SetFlags(RF_Transactional);
	return Super::Modify(bAlwaysMarkDirty);
}
#endif

bool UMovieGraphPin::AddEdgeTo(UMovieGraphPin* InOtherPin)
{
	if (!InOtherPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("AddEdgeTo: Invalid InOtherPin"));
		return false;
	}

	// Check to make sure the connection doesn't already exist
	for (UMovieGraphEdge* Edge : Edges)
	{
		if (Edge->GetOtherPin(this) == InOtherPin)
		{
			return false;
		}
	}

	// Don't allow connection between two output streams
	const bool bThisPinIsUpstream = IsOutputPin();
	const bool bOtherPinIsUpstream = InOtherPin->IsOutputPin();
	if (!ensure(bThisPinIsUpstream != bOtherPinIsUpstream))
	{
		return false;
	}

	Modify();
	InOtherPin->Modify();

	UMovieGraphEdge* NewEdge = NewObject<UMovieGraphEdge>(this);
	Edges.Add(NewEdge);
	InOtherPin->Edges.Add(NewEdge);

	NewEdge->InputPin = bThisPinIsUpstream ? this : InOtherPin;
	NewEdge->OutputPin = bThisPinIsUpstream ? InOtherPin : this;
	return true;
}

bool UMovieGraphPin::BreakEdgeTo(UMovieGraphPin* InOtherPin)
{
	if (!InOtherPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("BreakEdgeTo: Invalid InOtherPin"));
		return false;
	}

	for (UMovieGraphEdge* Edge : Edges)
	{
		if (Edge->GetOtherPin(this) == InOtherPin)
		{
			Modify();
			InOtherPin->Modify();
			ensure(InOtherPin->Edges.Remove(Edge));

			Edges.Remove(Edge);
			return true;
		}
	}

	return false;
}

bool UMovieGraphPin::BreakAllEdges()
{
	bool bChanged = false;
	if (!Edges.IsEmpty())
	{
		Modify();
	}

	for (UMovieGraphEdge* Edge : Edges)
	{
		if (UMovieGraphPin* OtherPin = Edge->GetOtherPin(this))
		{
			OtherPin->Modify();
			ensure(OtherPin->Edges.Remove(Edge));
			bChanged = true;
		}
	}

	Edges.Reset();
	return bChanged;
}

FPinConnectionResponse UMovieGraphPin::CanCreateConnection_PinConnectionResponse(const UMovieGraphPin* InOtherPin) const
{
	if (!InOtherPin)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "InvalidPinError", "InOtherPin is invalid!"));
	}
	
	// No Circular Connections
	if (Node == InOtherPin->Node)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "CircularPinError", "No Circular Connections!"));
	}

	const bool bBothPinsAreBranch = Properties.bIsBranch && InOtherPin->Properties.bIsBranch;

	const bool bBothPinsAreSameType = bBothPinsAreBranch ||				// Both are branches or
		(!Properties.bIsBranch && !InOtherPin->Properties.bIsBranch &&	// Neither is branch and
		Properties.Type == InOtherPin->Properties.Type &&				// They have the same property type and
		Properties.TypeObject == InOtherPin->Properties.TypeObject);	// They have the same type object (for enums, structs, objects, classes)

	// Pins need to be the same type
	if (!bBothPinsAreSameType)				
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "PinTypeMismatchError", "Pin types don't match!"));
	}

	if (!IsPinDirectionCompatibleWith(InOtherPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "PinDirectionMismatchError", "Directions are not compatible!"));
	}

	// Determine if the connection would violate branch restrictions enforced by the nodes involved in the connection.
	FText BranchRestrictionError;
	if (bBothPinsAreBranch && !IsConnectionToBranchAllowed(InOtherPin, BranchRestrictionError))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, BranchRestrictionError);
	}

	// We don't allow multiple things to be connected to an Input Pin
	const UMovieGraphPin* InputPin = IsInputPin() ? this : InOtherPin;
	if(InputPin->GetAllConnectedPins().Num() > 0)
	{
		const ECanCreateConnectionResponse ReplyBreakInputs = IsInputPin() ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakInputs, NSLOCTEXT("MoviePipeline", "PinInputReplaceExisting","Replace existing input connections"));
	}
	
	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, NSLOCTEXT("MoviePipeline", "PinConnect", "Connect nodes"));
}

bool UMovieGraphPin::CanCreateConnection(const UMovieGraphPin* InOtherPin) const
{
	return CanCreateConnection_PinConnectionResponse(InOtherPin).Response != CONNECT_RESPONSE_DISALLOW;
}

bool UMovieGraphPin::IsConnected() const
{
	for (UMovieGraphEdge* Edge : Edges)
	{
		if (Edge->IsValid())
		{
			return true;
		}
	}

	return false;
}

bool UMovieGraphPin::IsInputPin() const
{
	check(Node);
	return Node->GetInputPin(Properties.Label) == this;
}

bool UMovieGraphPin::IsOutputPin() const
{
	check(Node);
	return Node->GetOutputPin(Properties.Label) == this;
}

int32 UMovieGraphPin::EdgeCount() const
{
	int32 EdgeCount = 0;
	for (UMovieGraphEdge* Edge : Edges)
	{
		if (Edge->IsValid())
		{
			EdgeCount++;
		}
	}

	return EdgeCount;
}

bool UMovieGraphPin::AllowsMultipleConnections() const
{
	// Always allow multiple connection on output pin
	return IsOutputPin() || Properties.bAllowMultipleConnections;
}

UMovieGraphPin* UMovieGraphPin::GetFirstConnectedPin() const
{
	if (Edges.IsEmpty())
	{
		return nullptr;
	}

	if (ensureMsgf(Edges[0], TEXT("Null edge found when trying to get connected pin!")))
	{
		return Edges[0]->GetOtherPin(this);
	}

	return nullptr;
}

TArray<UMovieGraphPin*> UMovieGraphPin::GetAllConnectedPins() const
{
	TArray<UMovieGraphPin*> ConnectedPins;

	for (UMovieGraphEdge* Edge : Edges)
	{
		if (ensureMsgf(Edge, TEXT("Null edge found when trying to get connected pin!")))
		{
			ConnectedPins.Add(Edge->GetOtherPin(this));
		}
	}

	return ConnectedPins;
}

TArray<UMovieGraphNode*> UMovieGraphPin::GetConnectedNodes() const
{
	TArray<TObjectPtr<UMovieGraphNode>> OutNodes;
	for (const TObjectPtr<UMovieGraphEdge>& Edge : Edges)
	{
		if (!Edge)
		{
			continue;
		}

		UMovieGraphPin* OtherPin = Edge->GetOtherPin(this);
		if (!OtherPin)
		{
			continue;
		}

		if (!OtherPin->Node)
		{
			continue;
		}

		OutNodes.Add(OtherPin->Node);
	}

	return OutNodes;
}

bool UMovieGraphPin::IsConnectionToBranchAllowed(const UMovieGraphPin* OtherPin, FText& OutError) const
{
	const UMovieGraphPin* InputPin = IsInputPin() ? this : OtherPin;
	const UMovieGraphPin* OutputPin = !IsInputPin() ? this : OtherPin;
	
	if (!InputPin || !OutputPin)
	{
		OutError = NSLOCTEXT("MovieGraph", "PinDirectionMismatchError", "Directions are not compatible!");
		return false;
	}
	
	const TObjectPtr<UMovieGraphNode> ToNode = InputPin->Node;
	const TObjectPtr<UMovieGraphNode> FromNode = OutputPin->Node;
	check(ToNode && FromNode);
	const UMovieGraphConfig* GraphConfig = ToNode->GetGraph();

	const bool ToNodeIsSubgraph = ToNode->IsA<UMovieGraphSubgraphNode>();
	const bool FromNodeIsSubgraph = FromNode->IsA<UMovieGraphSubgraphNode>();

	// Test High-Level Node Restrictions
	const EMovieGraphBranchRestriction FromNodeRestriction = FromNode->GetBranchRestriction();
	const EMovieGraphBranchRestriction ToNodeRestriction = ToNode->GetBranchRestriction();
	if (FromNodeRestriction != ToNodeRestriction &&						// If BranchRestrictions are not the same
		FromNodeRestriction != EMovieGraphBranchRestriction::Any &&		// And neither Node is an 'Any' Node
		ToNodeRestriction != EMovieGraphBranchRestriction::Any)			// Then do not allow connection
	{
		OutError = NSLOCTEXT("MovieGraph", "HighLevelPerNodeBranchRestrictionError", "Cannot connect a Globals-only Node to a RenderLayer-only Node!");
		return false;
	}

	// Get all upstream/downstream nodes that occur on the connection -- these are the nodes that need to be checked for branch restrictions.
	// FromNode/ToNode themselves also needs to be part of the validation checks.
	//
	// If the FromNode is a subgraph, there's no need to visit upstream nodes. The subgraph node will enforce branch restrictions, since it
	// effectively represents an Inputs node. The same logic applies to the ToNode behaving like an Outputs node.
	TArray<UMovieGraphNode*> NodesToCheck = {FromNode, ToNode};
	if (!FromNodeIsSubgraph)
	{
		GraphConfig->VisitUpstreamNodes(FromNode, UMovieGraphConfig::FVisitNodesCallback::CreateLambda(
			[&NodesToCheck](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
			{
				if (VisitedNode->IsA<UMovieGraphSubgraphNode>())
				{
					return false;	// Don't visit more upstream nodes
				}
			
				NodesToCheck.Add(VisitedNode);
				return true;
			}));
	}

	if (!ToNodeIsSubgraph)
	{
		GraphConfig->VisitDownstreamNodes(ToNode, UMovieGraphConfig::FVisitNodesCallback::CreateLambda(
			[&NodesToCheck](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
			{
				if (VisitedNode->IsA<UMovieGraphSubgraphNode>())
				{
					return false;	// Don't visit more downstream nodes
				}
				
				NodesToCheck.Add(VisitedNode);
				return true;
			}));
	}

	const FName InputName = InputPin->Properties.Label;
	const FName OutputName = OutputPin->Properties.Label;
	const bool bInputIsGlobals = (InputName == UMovieGraphNode::GlobalsPinName);
	const bool bOutputIsGlobals = (OutputName == UMovieGraphNode::GlobalsPinName);
	constexpr bool bStopAtSubgraph = true;

	// Determine which branch(es) are connected to this node up/downstream. If the To/From node is a subgraph, skip trying to traverse the graph past
	// the subgraph, because for the purposes of determining connection validity, the subgraph's input/output pin is enough.
	const TArray<FString> DownstreamBranchNames = ToNodeIsSubgraph
		? TArray{InputName.ToString()}
		: GraphConfig->GetDownstreamBranchNames(ToNode, InputPin, bStopAtSubgraph);
	
	const TArray<FString> UpstreamBranchNames = FromNodeIsSubgraph
		? TArray{OutputName.ToString()}
		: GraphConfig->GetUpstreamBranchNames(FromNode, OutputPin, bStopAtSubgraph);
	
	const bool bGlobalsIsDownstream = bInputIsGlobals || DownstreamBranchNames.Contains(UMovieGraphNode::GlobalsPinNameString);
	const bool bGlobalsIsUpstream = bOutputIsGlobals || UpstreamBranchNames.Contains(UMovieGraphNode::GlobalsPinNameString);
	const bool bDownstreamBranchExistsAndIsntOnlyGlobals =
		!DownstreamBranchNames.IsEmpty() && ((DownstreamBranchNames.Num() != 1) || (DownstreamBranchNames[0] != UMovieGraphNode::GlobalsPinNameString));
	const bool bUpstreamBranchExistsAndIsntOnlyGlobals =
		!UpstreamBranchNames.IsEmpty() && ((UpstreamBranchNames.Num() != 1) || (UpstreamBranchNames[0] != UMovieGraphNode::GlobalsPinNameString));

	// Subgraph nodes are a special case -- they can be connected to both Globals and render layer branches at the same time
	if (ToNodeIsSubgraph || FromNodeIsSubgraph)
	{
		// Only allow Globals -> Globals connections
		if ((ToNodeIsSubgraph && bInputIsGlobals && !bGlobalsIsUpstream) ||
			(FromNodeIsSubgraph && bOutputIsGlobals && !bGlobalsIsDownstream))
		{
			OutError = NSLOCTEXT("MovieGraph", "SubgraphGlobalsBranchMismatchError", "A subgraph Globals branch can only be connected to another Globals branch.");
			return false;
		}

		// Only allow non-Globals -> non-Globals connections
		if ((ToNodeIsSubgraph && !bInputIsGlobals && bGlobalsIsUpstream) ||
			(FromNodeIsSubgraph && !bOutputIsGlobals && bGlobalsIsDownstream))
		{
			OutError = NSLOCTEXT("MovieGraph", "SubgraphNonGlobalsBranchMismatchError", "A subgraph non-Globals branch can not be connected to the Globals branch.");
			return false;
		}
	}
	else
	{
		// Globals branches can only be connected to Globals branches
		if ((bGlobalsIsDownstream && bUpstreamBranchExistsAndIsntOnlyGlobals) || (bGlobalsIsUpstream && bDownstreamBranchExistsAndIsntOnlyGlobals))
		{
			OutError = NSLOCTEXT("MovieGraph", "GlobalsBranchMismatchError", "Globals branches can only be connected to other Globals branches.");
			return false;
		}
	}

	// Error out if any of the nodes that are part of the connection cannot be connected to the upstream/downstream branches.
	for (const UMovieGraphNode* NodeToCheck : NodesToCheck)
	{
		if (NodeToCheck->GetBranchRestriction() == EMovieGraphBranchRestriction::Globals)
		{
			// Globals-specific nodes have to be connected such that the only upstream/downstream branches are Globals.
			// If either the upstream/downstream branches are empty (ie, the node isn't connected to Inputs/Outputs yet)
			// then the connection is OK for now -- the branch restriction will be enforced when nodes are connected to
			// Inputs/Outputs.
			if (bDownstreamBranchExistsAndIsntOnlyGlobals || bUpstreamBranchExistsAndIsntOnlyGlobals)
			{
				OutError = FText::Format(
					NSLOCTEXT("MovieGraph", "GlobalsBranchRestrictionError", "The node '{0}' can only be connected to the Globals branch."),
						FText::FromString(NodeToCheck->GetName()));
				return false;
			}
		}

		// Check that render-layer-only nodes aren't connected to Globals.
		if (NodeToCheck->GetBranchRestriction() == EMovieGraphBranchRestriction::RenderLayer)
		{
			if (bGlobalsIsDownstream || bGlobalsIsUpstream)
			{
				OutError = FText::Format(
					NSLOCTEXT("MovieGraph", "RenderLayerBranchRestrictionError", "The node '{0}' can only be connected to a render layer branch."),
						FText::FromString(NodeToCheck->GetName()));
				return false;
			}
		}
	}

	return true;
}

bool UMovieGraphPin::IsPinDirectionCompatibleWith(const UMovieGraphPin* OtherPin) const
{
	return IsInputPin() != OtherPin->IsInputPin();
}
