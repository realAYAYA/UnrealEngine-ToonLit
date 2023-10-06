// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphPin.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphEdge.h"
#include "MovieRenderPipelineCoreModule.h"

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