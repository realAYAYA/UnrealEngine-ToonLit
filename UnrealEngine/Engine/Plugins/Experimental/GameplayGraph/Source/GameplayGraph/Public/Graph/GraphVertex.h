// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/GraphElement.h"

#include "GraphVertex.generated.h"

/** Event for when the node has been removed from the graph. */
DECLARE_MULTICAST_DELEGATE(FOnGraphVertexRemoved);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphVertexParentIslandSet, const FGraphIslandHandle&);

UCLASS()
class GAMEPLAYGRAPH_API UGraphVertex : public UGraphElement
{
	GENERATED_BODY()
public:
	UGraphVertex();

	FGraphVertexHandle Handle() const
	{
		return FGraphVertexHandle{ GetUniqueIndex(), const_cast<UGraphVertex*>(this)};
	}

	bool HasEdgeTo(const FGraphVertexHandle& Other) const;
	const FGraphIslandHandle& GetParentIsland() const { return ParentIsland; }

	template<typename TLambda>
	void ForEachAdjacentVertex(TLambda&& Lambda)
	{
		for (const TPair<FGraphVertexHandle, FGraphEdgeHandle>& Kvp : Edges)
		{
			Lambda(Kvp.Key);
		}
	}

	FOnGraphVertexRemoved OnVertexRemoved;
	FOnGraphVertexParentIslandSet OnParentIslandSet;

	friend class UGraph;
	friend class UGraphIsland;
protected:

	void AddEdgeTo(const FGraphVertexHandle& Node, const FGraphEdgeHandle& Edge);
	void RemoveEdge(const FGraphEdgeHandle& EdgeHandle);
	void SetParentIsland(const FGraphIslandHandle& Island);

	virtual void HandleOnVertexRemoved();
private:

	UPROPERTY()
	TMap<FGraphVertexHandle, FGraphEdgeHandle> Edges;

	UPROPERTY()
	FGraphIslandHandle ParentIsland;
};