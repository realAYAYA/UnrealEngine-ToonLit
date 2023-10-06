// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GraphEdge.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraphEdge)

UGraphEdge::UGraphEdge()
	: Super(EGraphElementType::Edge)
{
}

void UGraphEdge::SetNodes(const FGraphVertexHandle& InA, const FGraphVertexHandle& InB)
{
	A = InA;
	B = InB;
}

const FGraphVertexHandle& UGraphEdge::GetOtherNode(const FGraphVertexHandle& InNode) const
{
	if (InNode == A)
	{
		return B;
	}
	else
	{
		return A;
	}
}

bool UGraphEdge::ContainsNode(const FGraphVertexHandle& InNode) const
{
	return (InNode == A) || (InNode == B);
}

FSerializedEdgeData UGraphEdge::GetSerializedData() const
{
	FSerializedEdgeData Nodes;
	Nodes.Node1 = A;
	Nodes.Node2 = B;
	return Nodes;
}