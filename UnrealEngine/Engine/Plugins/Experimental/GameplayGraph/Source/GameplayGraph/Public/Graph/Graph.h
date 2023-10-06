// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Graph/GraphEdge.h"
#include "Graph/GraphHandle.h"
#include "Graph/GraphIsland.h"
#include "Graph/GraphVertex.h"
#include "Templates/SubclassOf.h"

#include "Graph.generated.h"

USTRUCT()
struct FGraphProperties
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bGenerateIslands = true;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphVertexCreated, const FGraphVertexHandle&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphEdgeCreated, const FGraphEdgeHandle&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphIslandCreated, const FGraphIslandHandle&);

/**
 * The minimum amount of data we need to serialize to be able to reconstruct the graph as it was.
 * Note that classes that inherit from UGraph and its elements will no doubt want to extend the graph
 * with actual information on each node/edge/island. In that case, they should extend FSerializableGraph
 * to contain the extra information per graph handle. Furthermore, they'll need to extend UGraph to have
 * its own typed serialization save/load functions that call the base functions in UGraph first.
 */
USTRUCT()
struct FSerializableGraph
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGraphProperties Properties;

	UPROPERTY(SaveGame)
	TArray<FGraphVertexHandle> Vertices;

	UPROPERTY(SaveGame)
	TMap<FGraphEdgeHandle, FSerializedEdgeData> Edges;

	UPROPERTY(SaveGame)
	TMap<FGraphIslandHandle, FSerializedIslandData> Islands;
};

/**
 * A UGraph is a collection of nodes and edges. This graph representation
 * is meant to be easily integrable into gameplay systems in the Unreal Engine.
 * 
 * Conceptually, you can imagine that a graph is meant to easily represent relationships
 * so we can answer queries such as:
 *	- Are these two nodes connected to each other?
 *  - How far away are these two nodes?
 *  - Who is the closest node that has XYZ?
 *  - etc.
 * 
 * UGraph provides an interface to be able to run such queries. However, ultimately what
 * makes the graph useful is not only the relationships represented by the edges, but also
 * the data that is stored on each node and each edge. Depending on what the user wants to
 * represent, the user will have to subclass UGraphVertex and UGraphEdge to hold that data.
 * 
 * As the user adds nodes and edges into the graph, they will also be implicitly creating "islands"
 * (i.e. a connected component in the graph). Each graph may have multiple islands. Users
 * of the graph can disable the island detection/creation if needed.
 * 
 * Note that this is an UNDIRECTED GRAPH.
 */
UCLASS()
class GAMEPLAYGRAPH_API UGraph: public UObject
{
	GENERATED_BODY()
public:
	UGraph() = default;

	FSerializableGraph GetSerializableGraph() const;
	void LoadFromSerializedGraph(const FSerializableGraph& Input);

	void Empty();
	void InitializeFromProperties(const FGraphProperties& Properties);

	/** Given a node handle, find the handle in the current graph with a proper element set. */
	FGraphVertexHandle GetCompleteNodeHandle(const FGraphVertexHandle& InHandle);

	/** Create a node with the specified subclass, adds it to the graph, and returns a handle to it. */
	FGraphVertexHandle CreateVertex(int64 InUniqueIndex = INDEX_NONE);

	/** Creates an edge between the two nodes. */
	FGraphEdgeHandle CreateEdge(FGraphVertexHandle Node1, FGraphVertexHandle Node2, int64 InUniqueIndex = INDEX_NONE, bool bAddToIslands = true);

	/** Removes a node from the graph along with any edges that contain it. */
	void RemoveVertex(const FGraphVertexHandle& NodeHandle);

	/** Removes an edge from the graph. */
	void RemoveEdge(const FGraphEdgeHandle& EdgeHandle, bool bHandleIslands);

	/** This should be called immediately after a node and any relevant edges have been added to the graph. */
	virtual void FinalizeVertex(const FGraphVertexHandle& InHandle);

	/** Remove an island from the graph. */
	void RemoveIsland(const FGraphIslandHandle& IslandHandle);

	int32 NumVertices() const { return Vertices.Num(); }
	int32 NumEdges() const { return Edges.Num(); }
	int32 NumIslands() const { return Islands.Num(); }

	FOnGraphVertexCreated OnVertexCreated;
	FOnGraphEdgeCreated OnEdgeCreated;
	FOnGraphIslandCreated OnIslandCreated;

protected:
	const TMap<FGraphVertexHandle, TObjectPtr<UGraphVertex>>& GetVertices() const { return Vertices; }
	const TMap<FGraphEdgeHandle, TObjectPtr<UGraphEdge>>& GetEdges() const { return Edges; }
	const TMap<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& GetIslands() const { return Islands; }
	const FGraphProperties& GetProperties() const { return Properties; }

private:
	UPROPERTY()
	TMap<FGraphVertexHandle, TObjectPtr<UGraphVertex>> Vertices;

	/** Keeps track of every edge a node is a part of. The node itself will only track edges it's the "source" node of in directed graphs. */
	TMap<FGraphVertexHandle, TSet<FGraphEdgeHandle>> VertexEdges;

	UPROPERTY()
	TMap<FGraphEdgeHandle, TObjectPtr<UGraphEdge>> Edges;

	UPROPERTY()
	TMap<FGraphIslandHandle, TObjectPtr<UGraphIsland>> Islands;
	FGraphProperties Properties;

	/**
	 *  The unique index to assign to the next node that gets created.
	 *  If loading from persistence, as nodes get added into the graph, this
	 *  value will automatically keep increasing so we never try to reuse
	 *  the same unique index.
	 */
	int64 NextAvailableVertexUniqueIndex = 0;
	int64 NextAvailableEdgeUniqueIndex = 0;
	int64 NextAvailableIslandUniqueIndex = 0;

	/** Creates an island out of a given set of nodes. */
	FGraphIslandHandle CreateIsland(TArray<FGraphVertexHandle> Nodes, int64 InUniqueIndex = INDEX_NONE);

	/** Adds a node to the graph's node collection and modifies the NextAvailableNodeUniqueIndex as necessary to maintain its validity. */
	void RegisterVertex(TObjectPtr<UGraphVertex> Node);

	/** Add an edge to the graph and modifies the NextAvailableEdgeUniqueIndex as necessary to maintain its validity. */
	void RegisterEdge(TObjectPtr<UGraphEdge> Edge);

	/** Add an island to the graph and modifies the NextAvailableIslandUniqueIndex as necessary to maintain its validity. */
	void RegisterIsland(TObjectPtr<UGraphIsland> Edge);

	/** When we add an edge into a graph, we will want to merge islands if the edge connects two nodes that are in different islands. */
	void MergeOrCreateIslands(TObjectPtr<UGraphEdge> Edge);

	/** After a change, this function will remove the island if it's empty or will attempt to split it into two smaller islands. */
	void RemoveOrSplitIsland(TObjectPtr<UGraphIsland> Island);

	/** Factory functions for creating an appropriately typed node/edge/island. */
	virtual TObjectPtr<UGraphVertex> CreateTypedVertex() const;
	virtual TObjectPtr<UGraphEdge> CreateTypedEdge() const;
	virtual TObjectPtr<UGraphIsland> CreateTypedIsland() const;
};