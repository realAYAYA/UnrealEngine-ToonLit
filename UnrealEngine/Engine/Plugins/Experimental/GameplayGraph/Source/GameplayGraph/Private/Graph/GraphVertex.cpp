// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GraphVertex.h"
#include "Graph/GraphEdge.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraphVertex)

UGraphVertex::UGraphVertex()
	: Super(EGraphElementType::Node)
{
}

bool UGraphVertex::HasEdgeTo(const FGraphVertexHandle& Other) const
{
	return Edges.Contains(Other);
}

void UGraphVertex::AddEdgeTo(const FGraphVertexHandle& Node, const FGraphEdgeHandle& Edge)
{
	Edges.Add(Node, Edge);
}

void UGraphVertex::RemoveEdge(const FGraphEdgeHandle& EdgeHandle)
{
	if (!EdgeHandle.IsValid())
	{
		return;
	}

	TObjectPtr<UGraphEdge> Edge = EdgeHandle.GetEdge();
	if (!Edge || !Edge->ContainsNode(Handle()))
	{
		return;
	}

	Edges.Remove(Edge->GetOtherNode(Handle()));
}

void UGraphVertex::HandleOnVertexRemoved()
{
	OnVertexRemoved.Broadcast();
}

void UGraphVertex::SetParentIsland(const FGraphIslandHandle& Island)
{
	ParentIsland = Island;
	OnParentIslandSet.Broadcast(Island);
}