// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "Containers/Deque.h"
#include "Containers/SparseArray.h"

namespace Chaos
{
	/** Graph node structure */
	template<typename NodeType>
	struct CHAOS_API TIslandGraphNode
	{
		/** List of edges connected to the node */
		TSparseArray<int32> NodeEdges;

		/**
		 * Node Island Index. Only used by valid nodes (Dynamic/Sleeping Particles).
		 * Static/kinematic particles could belong to several islands and this is always INDEX_NONE.
		 */
		int32 IslandIndex = INDEX_NONE;

		/** Check if a node is valid (checked for graph partitioning into islands).
		 * In the context of particles (nodes) and constraints (edges), bValidNode is true when
		 * a Particle is Dynamic or Sleeping, but not when Static or Kinematic. This is because we
		 * do not need to merge islands that are only connected through a Static/Kinematic Particle node.
		 */
		bool bValidNode = true;

		/** Check if a node is steady */
		bool bStationaryNode = true;

		/** Node counter to filter nodes already processed (@see SplitIsland) */
		int32 NodeCounter = 0;

		/** Node item that is stored per node. This is a ParticleHandle. */
		NodeType NodeItem;

		/** Node level index */
		int32 LevelIndex = INDEX_NONE;

		/** Set of used colors */
		TSet<int32> ColorIndices;
	};

	/** Graph edge structure */
	template<typename EdgeType>
	struct CHAOS_API TIslandGraphEdge
	{
		/** First node of the edge */
		int32 FirstNode = INDEX_NONE;

		/** Second node of the edge*/
		int32 SecondNode = INDEX_NONE;

		/** Current edge index in the list of edges of the first node */
		int32 FirstEdge = INDEX_NONE;

		/** Current edge index in the list of edges of the second node  */
		int32 SecondEdge = INDEX_NONE;

		/** Unique edge island index */
		int32 IslandIndex = INDEX_NONE;

		/** True if one or both nodes are valid (dynamic) */
		bool bValidEdge = true;

		/** Edge counter to filter edges already processed (@see SplitIsland) */
		int32 EdgeCounter = 0;

		/** Edge item that is stored per node */
		EdgeType EdgeItem;

		/** Item Container Id  */
		int32 ItemContainer = 0;

		/** Edge level index */
		int32 LevelIndex = INDEX_NONE;

		/** Edge Color index */
		int32 ColorIndex = INDEX_NONE;
	};

	/** Graph island structure */
	template<typename IslandType>
	struct CHAOS_API TIslandGraphIsland
	{
		/** Number of edges per islands*/
		int32 NumEdges = 0;

		/** Number of valid nodes per islands (should be less than the ones in solver islands since it is only including the valid ones)*/
		int32 NumNodes = 0;

		/** Island counter to filter islands already processed */
		int32 IslandCounter = 0;

		/** Boolean to check if an island is persistent or not. A persistent island is one where no particles or constraints were added or removed */
		bool bIsPersistent = true;

		/** Boolean to check if an island is sleeping or not */
		bool bIsSleeping = false;

		/** Boolean to check if an island was sleeping last tick or not */
		bool bWasSleeping = false;

		/** List of children islands  to be merged */
		// @todo(chaos): should we really have a set here? The implementation probably wastes a lot of memory
		TSet<int32> ChildrenIslands;

		/** Parent Island */
		int32 ParentIsland = INDEX_NONE;

		/** Island Item that is stored per island*/
		IslandType IslandItem;

		/** Max color per island */
		int32 MaxColors = INDEX_NONE;
	};

/**
 * Island Graph.
 *
 * This template implements a graph that will be stored on the island manager
 * The goal here is to minimize memory allocation while doing graph operations. 
 *
 * @param NodeType The node type of the item that will be stored in each node (e.g., ParticleHandle)
 * @param EdgeType The edge type of the item that will be stored in each edge (e.g., ConstraintHandle)
 */
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
class CHAOS_API FIslandGraph
{
public:
	// NOTE: IslandGraph internal structs are non-member structs for better natvis behaviour/debugging. Natvis 
	// has trouble with member classes of templated classes
	using FGraphNode = TIslandGraphNode<NodeType>;
	using FGraphEdge = TIslandGraphEdge<EdgeType>;
	using FGraphIsland = TIslandGraphIsland<IslandType>;
	using FGraphOwner = OwnerType;

	static constexpr const int32 MaxCount = 100000;

	/**
	 * @brief Set the graph owner. This will allow the owner to cache node and edge indices with the node/edge items
	*/
	void SetOwner(FGraphOwner* InOwner)
	{
		Owner = InOwner;
	}

	/**
	 * Update current graph to merge connected islands 
	 */
	void UpdateGraph();

	/**
	 * Reserve a number of nodes in memory before adding them later
	 * @param NumNodes Number of nodes to be reserved in memory
	 */
	void ReserveNodes(const int32 NumNodes);

	/**
	 * Remove all nodes (and attached edges - so all of them) from the graph
	 */
	void ResetNodes();

	/**
	 * Given a node item, add a node to the graph nodes
	 * @param NodeItem Node Item to be stored in the graph node
	 * @param bValidNode Check if a node should be considered for graph partitioning (into islands)
	 * @param IslandIndex Potential island index we want the node to belong to
	 * @param bStationaryNode Boolean to check if a node is steady or not
	 * @return Node index that has just been added
	 */
	int32 AddNode(const NodeType& NodeItem, const bool bValidNode = true, const int32 IslandIndex = INDEX_NONE, const bool bStationaryNode = false);

	/**
	* Given a node item, update the graph node information (valid,discard...)
	* @param NodeIndex Sparse Index of the node to update
	* @param bValidNode Check if a node should be considered for graph partitioning (into islands)
	* @param bStationaryNode Boolean to check if a node is steady or not
	* @return Node index that has just been added
	*/
	void UpdateNode(const int32 NodeIndex, const bool bValidNode, const bool bStationaryNode);

	/**
	 * Remove a node from the graph nodes list
	 * @param NodeItem Item to be removed from the nodes list
	 */
	void RemoveNode(const NodeType& NodeItem);

	/**
	 * Get the level of the specified node item (distance from a kinematic node)
	 */
	int32 GetNodeItemLevel(const NodeType& NodeItem) const;

	/**
	 * Reserve a number of edges in memory before adding them later
	 * @param NumEdges Number of edges to be reserved in memory
	 */
	void ReserveEdges(const int32 NumEdges);

	/**
	 * Reset the graph edges without deallocating memory
	 */
	void ResetEdges();

	/**
	 * Given an edge item, add an edge to the graph edges
	 * @param EdgeItem Node Item to be stored in the graph edge
	 * @param ItemContainer Item container id that will be stored on the graph edge
	 * @param FirstNode First node index of the graph edge
	 * @param SecondNode Second node index of the graph edge
	 * @return Edge index that has just been added
	 */
	int32 AddEdge(const EdgeType& EdgeItem, const int32 ItemContainer, const int32 FirstNode,  const int32 SecondNode);

	/**
	 * Remove an edge from the graph edges list
	 * @param EdgeIndex Index of the edge to be removed from the edges list
	 */
	void RemoveEdge(const int32 EdgeIndex);

	/**
	 * Update edge state based on current node state
	 */
	void UpdateEdge(const int32 EdgeIndex);
	/**
	 * Remove all non-sleeping edges from the graph
	 */
	void RemoveAllAwakeEdges();

	/**
	 * Get the maximum number of edges that are inside a graph (SparseArray size)
	 */
	FORCEINLINE int32 MaxNumEdges() const { return GraphEdges.GetMaxIndex(); }

	/**
	 * Get the maximum number of nodes that are inside a graph (SparseArray size)
	 */
	FORCEINLINE int32 MaxNumNodes() const { return GraphNodes.GetMaxIndex(); }

	/**
	 * Get the maximum number of islands that are inside a graph (SparseArray size)
	 */
	FORCEINLINE int32 MaxNumIslands() const { return GraphIslands.GetMaxIndex(); }

	/**
	 * Link two islands to each other for future island traversal
	 * @param FirstIsland First island index to be linked
	 * @param SecondIsland Second island index to be linked
	 * @param bNonStationary Boolean to check if at least one of the 2 nodes are non stationary
	 */
	void ParentIslands(const int32 FirstIsland, const int32 SecondIsland,const bool bNonStationary);

	/**
	 * Given an edge index we try to attach islands. 
	 * @param EdgeIndex Index of the edge used to attach the islands
	 */
	void AttachIslands(const int32 EdgeIndex);

	/**
	 * Merge 2 islands
	 * @param ParentIndex Index of the parent island
	 * @param ChildIndex Index of the child island
	 */
	void MergeIslands(const int32 ParentIndex, const int32 ChildIndex);

	/**
	* Merge all the graph islands
	*/
	void MergeIslands();

	/**
	* Split one island given a root index 
	* @param NodeQueue node queue to use while splitting the island (avoid reallocation)
	* @param RootIndex root index of the island graph
	* @param IslandIndex index of the island we are splitting
	*/
	void SplitIsland(const int32 RootIndex, const int32 IslandIndex);

	/**
	* Split all the graph islands if not sleeping
	*/
	void SplitIslands();

	/**
	* Reassign the updated island index from the merging phase to the nodes/edges
	*/
	void ReassignIslands();

	/**
	* Init all the islands/edges/nodes levels and colors for sorting
	*/
	void InitSorting();

	/**
	* Compute all the islands/edges/nodes levels
	*/
	void ComputeLevels();
	
	/**
	* Update all the islands/edges/nodes levels and push the connected nodes into the node queue
	* @param NodeIndex Node index that we are currently iterating over
	*/
	void UpdateLevels(const int32 NodeIndex);

	/**
	* Compute all the islands/edges/nodes colors
	* @param MinEdges Minimum number of edges to compute coloring
	*/
	void ComputeColors(const int32 MinEdges);

	/**
	* Update all the islands/edges/nodes colors and push the connected nodes into the node queue
	* @param NodeIndex Node index that we are currently iterating over
	* @param MinEdges Minimum number of edges to compute coloring
	*/
	void UpdateColors(const int32 NodeIndex, const int32 MinEdges);

	/**
	* Pick the first available color that is not used yet by the current graph node and the edge opposite one
	* @param GraphNode Graph node that we currently iterate over
	* @param OtherIndex Index of the edge opposite node
	* @return First available color 
	*/
	int32 PickColor(const FGraphNode& GraphNode, const int32 OtherIndex);

	/**
	* Check if at least one of the 2 edge nodes is moving
	* @return Boolean to check if the edge is moving or not
	*/
	bool IsEdgeMoving(const FGraphEdge& GraphEdge)
	{
		const bool bFirstNodeMoving = GraphNodes.IsValidIndex(GraphEdge.FirstNode) && !GraphNodes[GraphEdge.FirstNode].bStationaryNode;
		const bool bSecondNodeMoving = GraphNodes.IsValidIndex(GraphEdge.SecondNode) && !GraphNodes[GraphEdge.SecondNode].bStationaryNode;
		return bFirstNodeMoving || bSecondNodeMoving;
	}

	bool IsEdgeMoving(const int32 EdgeIndex)
	{
		if(GraphEdges.IsValidIndex(EdgeIndex))
		{
			return IsEdgeMoving(GraphEdges[EdgeIndex]);
		}
		return false;
	}

	int32 AddIsland(FGraphIsland&& Island)
	{
		const int32 IslandIndex = GraphIslands.Emplace(MoveTemp(Island));
		if (Owner != nullptr)
		{
			Owner->GraphIslandAdded(IslandIndex);
		}
		return IslandIndex;
	}

	void SetNodeLevel(FGraphNode& GraphNode, const int32 LevelIndex)
	{
		GraphNode.LevelIndex = LevelIndex;
		if (Owner != nullptr)
		{
			Owner->GraphNodeLevelAssigned(GraphNode);
		}
	}


	void SetEdgeLevel(FGraphEdge& GraphEdge, const int32 LevelIndex)
	{
		GraphEdge.LevelIndex = LevelIndex;
		if (Owner != nullptr)
		{
			Owner->GraphEdgeLevelAssigned(GraphEdge);
		}
	}

	FGraphOwner* Owner = nullptr;

	/** List of graph nodes */
	TSparseArray<FGraphNode> GraphNodes;

	/** List of graph edges */
	TSparseArray<FGraphEdge> GraphEdges;

	/** List of graph islands */
	TSparseArray<FGraphIsland> GraphIslands;

	/** Reverse list to find given an item the matching node */
	TMap<NodeType,int32> ItemNodes;

	/** Reverse list to find given an item the matching edge */
	TMap<EdgeType,int32> ItemEdges;

	/** Graph counter used for graph traversal to check if an edge/node/island has already been processed */
	int32 GraphCounter = 0;

	/** List of nodes to be processed in the graph functions */
	TDeque<int32> NodeQueue;
};

// For use with unit tests to pretend to be the owner of the graph
template<typename NodeType, typename EdgeType>
struct TNullIslandGraphOwner
{
	void GraphIslandAdded(const int32 IslandIndex) {}

	void GraphNodeAdded(const NodeType& NodeItem, const int32 NodeIndex) {}
	void GraphNodeRemoved(const NodeType& NodeItem) {}
	void GraphNodeLevelAssigned(TIslandGraphNode<NodeType>& Node) {}

	void GraphEdgeAdded(const EdgeType& EdgeItem, const int32 EdgeIndex) {}
	void GraphEdgeRemoved(const EdgeType& EdgeItem) {}
	void GraphEdgeLevelAssigned(TIslandGraphEdge<EdgeType>& Edge) {}
};


}