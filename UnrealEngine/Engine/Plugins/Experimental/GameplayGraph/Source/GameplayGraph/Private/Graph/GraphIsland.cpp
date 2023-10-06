// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GraphIsland.h"
#include "Graph/GraphVertex.h"
#include "Graph/Graph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraphIsland)

UGraphIsland::UGraphIsland()
	: Super(EGraphElementType::Island)
{
	bPendingDestroy = false;
}

void UGraphIsland::Destroy()
{
	bPendingDestroy = true;
	TSet<FGraphVertexHandle> VertexCopy = Vertices;
	Vertices.Empty();

	// Need to work off a copy of the vertex handles since calling
	// UGraph::RemoveVertex will attempt to remove the node from its
	// parent island as well and that'll run into an error of modifying
	// the Vertices TSet during iteration.
	for (const FGraphVertexHandle& Node : VertexCopy)
	{
		if (TObjectPtr<UGraph> Graph = GetGraph())
		{
			Graph->RemoveVertex(Node);
		}
	}

	Vertices.Empty();
	HandleOnDestroyed();
}

void UGraphIsland::MergeWith(TObjectPtr<UGraphIsland> OtherIsland)
{
	if (!OtherIsland)
	{
		return;
	}

	for (const FGraphVertexHandle& Node : OtherIsland->Vertices)
	{
		OtherIsland->HandleOnVertexRemoved(Node);
		AddVertex(Node);
	}

	OtherIsland->Vertices.Empty();
}

void UGraphIsland::AddVertex(const FGraphVertexHandle& Node)
{
	if (!Node.IsValid())
	{
		return;
	}

	TObjectPtr<UGraphVertex> NodePtr = Node.GetVertex();
	if (!NodePtr)
	{
		return;
	}
	NodePtr->SetParentIsland(Handle());
	Vertices.Add(Node);
	HandleOnVertexAdded(Node);
}

void UGraphIsland::RemoveVertex(const FGraphVertexHandle& Node)
{
	if (!Node.IsValid())
	{
		return;
	}

	TObjectPtr<UGraphVertex> NodePtr = Node.GetVertex();
	if (!NodePtr)
	{
		return;
	}
	NodePtr->SetParentIsland({});
	Vertices.Remove(Node);
	HandleOnVertexRemoved(Node);
}

FSerializedIslandData UGraphIsland::GetSerializedData() const
{
	FSerializedIslandData Out;
	for (const FGraphVertexHandle& Handle : Vertices)
	{
		Out.Vertices.Add(Handle);
	}
	return Out;
}

void UGraphIsland::HandleOnVertexAdded(const FGraphVertexHandle& Handle)
{
	OnVertexAdded.Broadcast(this->Handle(), Handle);
}

void UGraphIsland::HandleOnVertexRemoved(const FGraphVertexHandle& Handle)
{
	OnVertexRemoved.Broadcast(this->Handle(), Handle);
}

void UGraphIsland::HandleOnDestroyed()
{
	OnDestroyed.Broadcast(Handle());
}

void UGraphIsland::HandleOnConnectivityChanged()
{
	OnConnectivityChanged.Broadcast(Handle());
}