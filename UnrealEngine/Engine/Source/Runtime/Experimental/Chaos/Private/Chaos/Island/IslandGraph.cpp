// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Island/IslandGraph.h"
#include "Chaos/Island/SolverIsland.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ConstraintHandle.h"

#include "ChaosStats.h"
#include "ChaosLog.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{

DECLARE_CYCLE_STAT(TEXT("MergeIslandGraph"), STAT_MergeIslandGraph, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("SplitIslandGraph"), STAT_SplitIslandGraph, STATGROUP_Chaos);

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ResetNodes()
{
	// If we remove all nodes, we must also remove edges
	ResetEdges();

	// Notify all nodes they were removed
	if (Owner != nullptr)
	{
		for (auto& ItemNode : GraphNodes)
		{
			Owner->GraphNodeRemoved(ItemNode.NodeItem);
		}
	}

	GraphNodes.Reset();
	ItemNodes.Reset();
	GraphIslands.Reset();
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ReserveNodes(const int32 NumNodes)
{
	GraphNodes.Reserve(NumNodes);
	ItemNodes.Reserve(NumNodes);
	GraphIslands.Reserve(NumNodes);
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ParentIslands(const int32 FirstIsland, const int32 SecondIsland, const bool bIsEdgeMoving)
{
	if (GraphIslands.IsValidIndex(FirstIsland) && GraphIslands.IsValidIndex(SecondIsland) && (FirstIsland != SecondIsland))
	{
		GraphIslands[SecondIsland].ChildrenIslands.Add(FirstIsland);
		GraphIslands[FirstIsland].ChildrenIslands.Add(SecondIsland);

		// NOTE: We used to set the islands to non-persistent if either nodes in the edge was moving, but we cannot do that here
		// because this is called as part of the graph update, and both particles may not yet be updated so their moving flag may not
		// be appropriately set. Instead we manage island persistance in the actual merge phase (see ReassignIslands)
	}
}
	
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::UpdateNode(const int32 NodeIndex, const bool bValidNode, const bool bStationaryNode)
{
	if (GraphNodes.IsValidIndex(NodeIndex))
	{
		FGraphNode& GraphNode = GraphNodes[NodeIndex];

		// Update node state (must come first because IsEdgeMoving relies in state on Stationary being up to date)
		const bool bWasValidNode = GraphNode.bValidNode;
		GraphNode.bValidNode = bValidNode;
		GraphNode.bStationaryNode = bStationaryNode;

		// In case the item is changing its state to be valid (Kinematic->Dynamic/Sleeping)
		// we merge all the connected islands (this will wake the islands)
		if (bValidNode && !bWasValidNode)
		{
			// @todo(chaos): we could just use the GraphNode.NodeIslands here if it were maintained internally
			int32 PrimaryIsland = INDEX_NONE;
			for (int32& EdgeIndex : GraphNode.NodeEdges)
			{
				FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];

				const bool bIsEdgeMoving = IsEdgeMoving(EdgeIndex);
				ParentIslands(PrimaryIsland, GraphEdge.IslandIndex, bIsEdgeMoving);
			
				// Primary island can be set to the island of any of our edges, so long as they aren't INDEX_NONE
				if (PrimaryIsland == INDEX_NONE)
				{
					PrimaryIsland = GraphEdge.IslandIndex;
				}
			}

			// Put the valid node into one of the islands - they will be merged anyway so doesn't matter which
			// If we did not have an island (no edges), one will be assigned later
			GraphNode.IslandIndex = PrimaryIsland;
		}

		// If we are changing to invalid (Dynamic/Sleeping->Kinematic) wake the island
		if (!bValidNode && bWasValidNode)
		{
			// Wake the node's island if the kinematic is moving
			if (GraphIslands.IsValidIndex(GraphNode.IslandIndex) && !bStationaryNode)
			{
				GraphIslands[GraphNode.IslandIndex].bIsPersistent = false;
			}

			// Invalid node island lists are built later (see PopulateIslands)
			GraphNode.IslandIndex = INDEX_NONE;
		}

		// If we change validity, we may have to change the validity of some of the edges
		if (bValidNode != bWasValidNode)
		{
			for (int32 EdgeIndex : GraphNode.NodeEdges)
			{
				UpdateEdge(EdgeIndex);
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
int32 FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::AddNode(const NodeType& NodeItem, const bool bValidNode, const int32 IslandIndex, const bool bStationaryNode)
{
	// Do not try to add a node that is already in the graph
	check(ItemNodes.Find(NodeItem) == nullptr);

	FGraphNode GraphNode;
	GraphNode.NodeItem = NodeItem;

	if (bValidNode)
	{
		GraphNode.IslandIndex = IslandIndex;
	}

	GraphNode.bValidNode = bValidNode;
	GraphNode.bStationaryNode = bStationaryNode;

	const int32 NodeIndex =  ItemNodes.Add(NodeItem, GraphNodes.Emplace(GraphNode));

	if (Owner != nullptr)
	{
		Owner->GraphNodeAdded(NodeItem, NodeIndex);
	}
	return NodeIndex;
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::RemoveNode(const NodeType& NodeItem)
{
	if (const int32* IndexPtr = ItemNodes.Find(NodeItem))
	{
		const int32 NodeIndex = *IndexPtr;
		if (GraphNodes.IsValidIndex(NodeIndex))
		{
			FGraphNode& GraphNode = GraphNodes[NodeIndex];

			// Remove all edges attached to the node, and flag any islands that contain the node 
			// as non-persistent so they will be woken on the next update
			if (GraphNode.IslandIndex != INDEX_NONE)
			{
				check(GraphIslands.IsValidIndex(GraphNode.IslandIndex));
				GraphIslands[GraphNode.IslandIndex].bIsPersistent = false;
			}
			for (int32 NodeEdgeIndex = GraphNode.NodeEdges.GetMaxIndex() - 1; NodeEdgeIndex >= 0; --NodeEdgeIndex)
			{
				if (GraphNode.NodeEdges.IsValidIndex(NodeEdgeIndex))
				{
					const int32 GraphEdgeIndex = GraphNode.NodeEdges[NodeEdgeIndex];
					const int32 GraphIslandIndex = GraphEdges[GraphEdgeIndex].IslandIndex;
					if (GraphIslandIndex != INDEX_NONE)
					{
						GraphIslands[GraphIslandIndex].bIsPersistent = false;
					}
					RemoveEdge(GraphEdgeIndex);
				}
			}

			ItemNodes.Remove(NodeItem);
			GraphNodes.RemoveAt(NodeIndex);

			if (Owner != nullptr)
			{
				Owner->GraphNodeRemoved(NodeItem);
			}
		}
		else
		{
			UE_LOG(LogChaos, Error, TEXT("Island Graph : Trying to remove a node at index %d in a list of size %d"), NodeIndex, GraphNodes.Num());
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
int32 FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::GetNodeItemLevel(const NodeType& NodeItem) const
{
	if (const int32* PNodeIndex = ItemNodes.Find(NodeItem))
	{
		const int32 NodeIndex = *PNodeIndex;
		if (GraphNodes.IsValidIndex(NodeIndex))
		{
			const FGraphNode& GraphNode = GraphNodes[NodeIndex];
			return GraphNode.LevelIndex;
		}
	}
	return 0;
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ResetEdges()
{
	if (Owner != nullptr)
	{
		for (auto& GraphEdge : GraphEdges)
		{
			Owner->GraphEdgeRemoved(GraphEdge.EdgeItem);
		}
	}

	for (auto& GraphNode : GraphNodes)
	{
		GraphNode.NodeEdges.Reset();
		GraphNode.IslandIndex = INDEX_NONE;
	}

	GraphEdges.Reset();
	ItemEdges.Reset();
	GraphIslands.Reset();
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ReserveEdges(const int32 NumEdges)
{
	GraphEdges.Reserve(NumEdges);
	ItemEdges.Reserve(NumEdges);
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::AttachIslands(const int32 EdgeIndex)
{
	if (GraphEdges.IsValidIndex(EdgeIndex))
	{
		FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];

		if (GraphNodes.IsValidIndex(GraphEdge.FirstNode) && GraphNodes.IsValidIndex(GraphEdge.SecondNode))
		{
			FGraphNode& FirstNode = GraphNodes[GraphEdge.FirstNode];
			FGraphNode& SecondNode = GraphNodes[GraphEdge.SecondNode];

			const bool bFirstValidIsland = GraphIslands.IsValidIndex(FirstNode.IslandIndex) && FirstNode.bValidNode;
			const bool bSecondValidIsland = GraphIslands.IsValidIndex(SecondNode.IslandIndex) && SecondNode.bValidNode;

			const bool bIsEdgeMoving = IsEdgeMoving(EdgeIndex);

			// We check if one of the 2 nodes have invalid island
			// if yes we set the invalid node island index 
			// and the edge one to be the valid one 
			// If none are valid we create a new island 
			if (!FirstNode.bValidNode && !SecondNode.bValidNode)
			{
				// If we have two invalid nodes, we just remove the edge from its island
				GraphEdge.IslandIndex = INDEX_NONE;
			}
			else if (bFirstValidIsland && !bSecondValidIsland)
			{
				GraphEdge.IslandIndex = FirstNode.IslandIndex;
				if (SecondNode.bValidNode)
				{
					SecondNode.IslandIndex = GraphEdge.IslandIndex;
				}
				if (SecondNode.bValidNode && bIsEdgeMoving)
				{
					GraphIslands[GraphEdge.IslandIndex].bIsPersistent = false;
				}
			}
			else if (!bFirstValidIsland && bSecondValidIsland)
			{
				GraphEdge.IslandIndex = SecondNode.IslandIndex;
				if (FirstNode.bValidNode)
				{
					FirstNode.IslandIndex = GraphEdge.IslandIndex;
				}
				if (FirstNode.bValidNode && bIsEdgeMoving)
				{
					GraphIslands[GraphEdge.IslandIndex].bIsPersistent = false;
				}
			}
			else if (!bFirstValidIsland && !bSecondValidIsland)
			{
				FGraphIsland GraphIsland = { 1, 2 };
				GraphEdge.IslandIndex = AddIsland(MoveTemp(GraphIsland));

				// We set both island indices to be the equal to the edge one
				if (FirstNode.bValidNode)
				{
					FirstNode.IslandIndex = GraphEdge.IslandIndex;
				}
				if (SecondNode.bValidNode)
				{
					SecondNode.IslandIndex = GraphEdge.IslandIndex;
				}
			}
			else
			{
				// If the 2 nodes are coming from 2 different islands, we need to merge these islands
				// In order to do that we build an island graph and we will 
				// merge recursively the children islands onto the parent one
				GraphEdge.IslandIndex = FMath::Min(FirstNode.IslandIndex, SecondNode.IslandIndex);
				ParentIslands(FirstNode.IslandIndex, SecondNode.IslandIndex, bIsEdgeMoving);
			}
		}
		else if (GraphNodes.IsValidIndex(GraphEdge.FirstNode) && !GraphNodes.IsValidIndex(GraphEdge.SecondNode) && GraphNodes[GraphEdge.FirstNode].bValidNode)
		{
			// If only the first node exists and is valid, check if its island index is valid and if yes use it for the edge.
			// otherwise we create a new island
			if(!GraphIslands.IsValidIndex(GraphNodes[GraphEdge.FirstNode].IslandIndex))
			{
				FGraphIsland GraphIsland = { 1, 1 };
				GraphNodes[GraphEdge.FirstNode].IslandIndex = AddIsland(MoveTemp(GraphIsland));
			}
			GraphEdge.IslandIndex =  GraphNodes[GraphEdge.FirstNode].IslandIndex;
		}
		else if (!GraphNodes.IsValidIndex(GraphEdge.FirstNode) && GraphNodes.IsValidIndex(GraphEdge.SecondNode) && GraphNodes[GraphEdge.SecondNode].bValidNode)
		{
			// If only the second node exists and is valid, check if its island index is valid and if yes use it for the edge.
			// otherwise we create a new island
			if(!GraphIslands.IsValidIndex(GraphNodes[GraphEdge.SecondNode].IslandIndex))
			{
				FGraphIsland GraphIsland = { 1, 1 };
				GraphNodes[GraphEdge.SecondNode].IslandIndex = AddIsland(MoveTemp(GraphIsland));
			}
			GraphEdge.IslandIndex =  GraphNodes[GraphEdge.SecondNode].IslandIndex;
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
int32 FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::AddEdge(const EdgeType& EdgeItem, const int32 ItermContainer, const int32 FirstNode, const int32 SecondNode)
{
	// Should not call AddEdge for edges that are already present
	check(ItemEdges.Find(EdgeItem) == nullptr);

	int32 EdgeIndex = INDEX_NONE;

	// We only add an edge if one of the 2 nodes are valid
	// @todo(chaos): we can relax this requirement now if necessary
	if (GraphNodes.IsValidIndex(FirstNode) || GraphNodes.IsValidIndex(SecondNode))
	{
		// Create a  new edge and enqueue the linked islands to be merged if necessary
		FGraphEdge GraphEdge;
		GraphEdge.EdgeItem = EdgeItem;
		GraphEdge.FirstNode = FirstNode;
		GraphEdge.SecondNode = SecondNode;
		GraphEdge.IslandIndex = INDEX_NONE;
		GraphEdge.ItemContainer = ItermContainer;
		GraphEdge.bValidEdge = true;

		EdgeIndex = GraphEdges.Emplace(GraphEdge);
		ItemEdges.Add(EdgeItem, EdgeIndex);

		GraphEdges[EdgeIndex].FirstEdge = GraphNodes.IsValidIndex(FirstNode) ? GraphNodes[FirstNode].NodeEdges.Add(EdgeIndex) : INDEX_NONE;
		GraphEdges[EdgeIndex].SecondEdge = GraphNodes.IsValidIndex(SecondNode) ? GraphNodes[SecondNode].NodeEdges.Add(EdgeIndex) : INDEX_NONE;

		AttachIslands(EdgeIndex);

		if (Owner != nullptr)
		{
			Owner->GraphEdgeAdded(EdgeItem, EdgeIndex);
		}
	}	
	else
	{
		UE_LOG(LogChaos, Error, TEXT("Island Graph : Trying to add an edge with invalid node indices %d  %d in a list of nodes of size %d"), FirstNode, SecondNode, GraphNodes.Num());
	}

	return EdgeIndex;
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::RemoveEdge(const int32 EdgeIndex)
{
	if (GraphEdges.IsValidIndex(EdgeIndex))
	{
		FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];
		if(GraphNodes.IsValidIndex(GraphEdge.FirstNode) && GraphNodes[GraphEdge.FirstNode].NodeEdges.IsValidIndex(GraphEdge.FirstEdge))
		{
			GraphNodes[GraphEdge.FirstNode].NodeEdges.RemoveAt(GraphEdge.FirstEdge);
		}
		if (GraphNodes.IsValidIndex(GraphEdge.SecondNode) && GraphNodes[GraphEdge.SecondNode].NodeEdges.IsValidIndex(GraphEdge.SecondEdge))
		{
			GraphNodes[GraphEdge.SecondNode].NodeEdges.RemoveAt(GraphEdge.SecondEdge);
		}
		// We then remove the edge from the item and graph edges
		ItemEdges.Remove(GraphEdge.EdgeItem);
		GraphEdges.RemoveAt(EdgeIndex);

		if (Owner != nullptr)
		{
			Owner->GraphEdgeRemoved(GraphEdge.EdgeItem);
		}
	}
	else
	{
		UE_LOG(LogChaos, Error, TEXT("Island Graph : Trying to remove an edge at index %d in a list of size %d"), EdgeIndex, GraphEdges.Num());
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::UpdateEdge(const int32 EdgeIndex)
{
	if (GraphEdges.IsValidIndex(EdgeIndex))
	{
		FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];
		const bool bFirstNodeValid = GraphNodes.IsValidIndex(GraphEdge.FirstNode) && GraphNodes[GraphEdge.FirstNode].bValidNode;
		const bool bSecondNodeValid = GraphNodes.IsValidIndex(GraphEdge.SecondNode) && GraphNodes[GraphEdge.SecondNode].bValidNode;
		GraphEdge.bValidEdge = bFirstNodeValid || bSecondNodeValid;
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::MergeIslands(const int32 ParentIndex, const int32 ChildIndex)
{
	int32 CurrentIndex;
	TQueue<int32> ChildQueue;
	ChildQueue.Enqueue(ChildIndex);
	
	while(!ChildQueue.IsEmpty())
	{
		ChildQueue.Dequeue(CurrentIndex);
	
		if (GraphIslands.IsValidIndex(CurrentIndex) && GraphIslands[CurrentIndex].IslandCounter != GraphCounter && ParentIndex != CurrentIndex )
		{
			GraphIslands[CurrentIndex].IslandCounter = GraphCounter;
			GraphIslands[CurrentIndex].ParentIsland = ParentIndex;

			// Recursively iterate over all the children ones
			for (auto& MergedIsland : GraphIslands[CurrentIndex].ChildrenIslands)
			{
				ChildQueue.Enqueue(MergedIsland);
			}
			GraphIslands[CurrentIndex].ChildrenIslands.Reset();
		}
	}
}
	
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::MergeIslands()
{
	GraphCounter = (GraphCounter + 1) % MaxCount;

	// Init the Parent index to be the island one
	for (int32 IslandIndex = 0, NumIslands = GraphIslands.GetMaxIndex(); IslandIndex < NumIslands; ++IslandIndex)
	{
		if (GraphIslands.IsValidIndex(IslandIndex))
		{
			GraphIslands[IslandIndex].ParentIsland = IslandIndex;
		}
	}

	// We loop over all the islands and if they have children
	// we recursively merge them onto the parent one
	for (int32 IslandIndex = 0, NumIslands = GraphIslands.GetMaxIndex(); IslandIndex < NumIslands; ++IslandIndex)
	{
		if (GraphIslands.IsValidIndex(IslandIndex))
		{
			FGraphIsland& GraphIsland = GraphIslands[IslandIndex];
			for (const int32& MergedIsland : GraphIsland.ChildrenIslands)
			{
				MergeIslands(IslandIndex, MergedIsland);
			}
			GraphIsland.ChildrenIslands.Reset();
		}
	}

	// Reassigns all the parent island indices to the nodes/edges
	ReassignIslands();

	// Once the merging process is done we remove all the children islands
	// since they have been merged onto the parent one
	for (int32 IslandIndex = (GraphIslands.GetMaxIndex() - 1); IslandIndex >= 0; --IslandIndex)
	{
		// Only the island counter of the children have been updated
		if (GraphIslands.IsValidIndex(IslandIndex) && GraphIslands[IslandIndex].NumNodes == 0)
		{
			GraphIslands.RemoveAt(IslandIndex);
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::InitSorting()
{
	// Reset nodes levels and colors
	for(auto& GraphNode : GraphNodes)
	{
		GraphNode.LevelIndex = INDEX_NONE;
		GraphNode.ColorIndices.Reset();
	}
	// Reset edges levels and colors
	for(auto& GraphEdge : GraphEdges)
	{
		GraphEdge.LevelIndex = INDEX_NONE;
		GraphEdge.ColorIndex = INDEX_NONE;
	}
	// Reset island max number of levels and colors
	for(auto& GraphIsland : GraphIslands)
	{
		GraphIsland.MaxColors = INDEX_NONE;
	}
}
	
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::UpdateLevels(const int32 NodeIndex)
{
	if(GraphNodes.IsValidIndex(NodeIndex))
	{
		FGraphNode& GraphNode = GraphNodes[NodeIndex];
		for (int32 EdgeIndex : GraphNode.NodeEdges)
		{
			FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
			// Valid edges must have a valid island
			ensure(!GraphEdge.bValidEdge || GraphIslands.IsValidIndex(GraphEdge.IslandIndex));
#endif

			// Skip edges that already have a level assigned.
			// Skip sleeping islands, unless they were just put to sleep (this is because we need to know
			// what particles and constraints are in islands that were just put to sleep so that we can
			// update their sleep state)
			if (GraphEdge.bValidEdge && (GraphEdge.LevelIndex == INDEX_NONE) && (!GraphIslands[GraphEdge.IslandIndex].bIsSleeping || !GraphIslands[GraphEdge.IslandIndex].bWasSleeping))
			{
				const int32 OtherIndex = (NodeIndex == GraphEdge.FirstNode) ?
							GraphEdge.SecondNode : GraphEdge.FirstNode;

				SetEdgeLevel(GraphEdge, GraphNode.LevelIndex);
					
				// If we have another node, append it to our queue on the next level
				if (GraphNodes.IsValidIndex(OtherIndex) && GraphNodes[OtherIndex].bValidNode && (GraphNodes[OtherIndex].LevelIndex == INDEX_NONE))
				{
					SetNodeLevel(GraphNodes[OtherIndex], GraphEdge.LevelIndex + 1);

					NodeQueue.PushLast(OtherIndex);
				}
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ComputeLevels()
{
	// Set all kinematic nodes to level 0 and enqueue them as roots for the recusrive level assignment.
	NodeQueue.Reset();
	for (int32 NodeIndex = 0, NumNodes = GraphNodes.GetMaxIndex(); NodeIndex < NumNodes; ++NodeIndex)
	{
		if(GraphNodes.IsValidIndex(NodeIndex))
		{
			FGraphNode& GraphNode = GraphNodes[NodeIndex];
			if(!GraphNode.bValidNode)
			{
				SetNodeLevel(GraphNode, 0);
				UpdateLevels(NodeIndex);
			}
		}
	}

	// Then iteratively loop over these root nodes and propagate the levels through connectivity
	int32 NodeIndex = INDEX_NONE;
	while(!NodeQueue.IsEmpty())
	{
		NodeIndex = NodeQueue.First();
		NodeQueue.PopFirst();

		UpdateLevels(NodeIndex);
	}

	// An island containing only dynamics will not have been assigned any levels. In this case we set all levels to 0.
	// NOTE: this is only required because we use the level assignment callback to put the particles and constraints
	// into per-island lists, otherwise it wouldn't really matter as long as levels are the same for all such particles.
	for(FGraphEdge& GraphEdge : GraphEdges)
	{
		if (GraphEdge.bValidEdge && (GraphEdge.LevelIndex == INDEX_NONE) && (!GraphIslands[GraphEdge.IslandIndex].bIsSleeping || !GraphIslands[GraphEdge.IslandIndex].bWasSleeping))
		{
			SetEdgeLevel(GraphEdge, 0);
		}
	}
	for (FGraphNode& GraphNode : GraphNodes)
	{
		if (GraphNode.bValidNode && (GraphNode.LevelIndex == INDEX_NONE) && (!GraphIslands[GraphNode.IslandIndex].bIsSleeping || !GraphIslands[GraphNode.IslandIndex].bWasSleeping))
		{
			SetNodeLevel(GraphNode, 0);
		}
	}
}
	
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
int32 FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::PickColor(const FGraphNode& GraphNode, const int32 OtherIndex)
{
	int32 ColorToUse = 0;
	if (GraphNodes.IsValidIndex(OtherIndex) && GraphNodes[OtherIndex].bValidNode)
	{
		FGraphNode& OtherNode = GraphNodes[OtherIndex];
		// Pick the first color not used by the 2 edge nodes
		while (OtherNode.ColorIndices.Contains(ColorToUse) || GraphNode.ColorIndices.Contains(ColorToUse))
		{
			ColorToUse++;
		}
		// The color will be added to the graph node in the UpdateColors function
		OtherNode.ColorIndices.Add(ColorToUse);
		if (OtherNode.NodeCounter != GraphCounter)
		{
			NodeQueue.PushLast(OtherIndex);
		}
	}
	else
	{
		// If only one node, only iterate over that node available color
		while (GraphNode.ColorIndices.Contains(ColorToUse))
		{
			ColorToUse++;
		}
	}
	return ColorToUse;
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::UpdateColors(const int32 NodeIndex, const int32 MinEdges)
{
	if (GraphNodes.IsValidIndex(NodeIndex))
	{
		FGraphNode& GraphNode = GraphNodes[NodeIndex];
		GraphNode.NodeCounter = GraphCounter;
		
		for (int32 EdgeIndex : GraphNode.NodeEdges)
		{
			FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
			// Valid edges must have a valid island
			ensure(!GraphEdge.bValidEdge || GraphIslands.IsValidIndex(GraphEdge.IslandIndex));
#endif

			// Do nothing if the edge is not coming from the same container or if the island is sleeping 
			if (GraphEdge.bValidEdge && (GraphEdge.ColorIndex == INDEX_NONE) &&
				!GraphIslands[GraphEdge.IslandIndex].bIsSleeping && (GraphIslands[GraphEdge.IslandIndex].NumEdges > MinEdges))
			{
				// Get the opposite node index for the given edge
				const int32 OtherIndex = (NodeIndex == GraphEdge.FirstNode) ?
					GraphEdge.SecondNode : GraphEdge.FirstNode;

				// Get the first available color to be used by the edge
				const int32 ColorToUse = PickColor(GraphNode, OtherIndex);
				
				GraphNode.ColorIndices.Add(ColorToUse);
				GraphEdge.ColorIndex = ColorToUse;
				
				GraphIslands[GraphEdge.IslandIndex].MaxColors = FGenericPlatformMath::Max(
					GraphIslands[GraphEdge.IslandIndex].MaxColors, ColorToUse);
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ComputeColors(const int32 MinEdges)
{
	GraphCounter = (GraphCounter + 1) % MaxCount;

	NodeQueue.Reset();
	int32 NodeIndex = INDEX_NONE;

	// We first loop over all the nodes that have not been processed and valid (dynamic/sleeping)
	for (int32 RootIndex = 0, NumNodes = GraphNodes.GetMaxIndex(); RootIndex < NumNodes; ++RootIndex)
	{
		if (GraphNodes.IsValidIndex(RootIndex))
		{
			FGraphNode& GraphNode = GraphNodes[RootIndex];
			if (GraphNode.NodeCounter != GraphCounter && GraphNode.bValidNode)
			{
				NodeQueue.PushLast(RootIndex);
				while (!NodeQueue.IsEmpty())
				{
					NodeIndex = NodeQueue.First();
					NodeQueue.PopFirst();

					UpdateColors(NodeIndex, MinEdges);
				}
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::SplitIsland(const int32 RootIndex, const int32 IslandIndex)
{
	NodeQueue.PushLast(RootIndex);
	int32 NodeIndex = RootIndex;
					
	while (!NodeQueue.IsEmpty())
	{
		NodeIndex = NodeQueue.First();
		NodeQueue.PopFirst();

		FGraphNode& GraphNode = GraphNodes[NodeIndex];
						
		// Graph counter is there to avoid processing multiple times the same node/edge
		if (GraphNode.NodeCounter != GraphCounter)
		{
			GraphNode.NodeCounter = GraphCounter;

			// We are always awake when SplitIslands is called so NodeIslands will be rebuilt
			if (GraphNode.bValidNode)
			{
				GraphNode.IslandIndex = IslandIndex;
			}
			else
			{
				GraphNode.IslandIndex = INDEX_NONE;
			}

			// Loop over the node edges to continue the island discovery
			for (int32& EdgeIndex : GraphNode.NodeEdges)
			{
				FGraphEdge& GraphEdge = GraphEdges[EdgeIndex];
				if (GraphEdge.EdgeCounter != GraphCounter)
				{
					GraphEdge.EdgeCounter = GraphCounter;
					GraphEdge.IslandIndex = IslandIndex;
				}
				const int32 OtherIndex = (NodeIndex == GraphEdge.FirstNode) ?
					GraphEdge.SecondNode : GraphEdge.FirstNode;

				// Only the valid nodes (Sleeping/Dynamic Particles) are allowed to continue the graph traversal (connect islands)
				if (GraphNodes.IsValidIndex(OtherIndex) && GraphNodes[OtherIndex].NodeCounter != GraphCounter && GraphNodes[OtherIndex].bValidNode)
				{
					NodeQueue.PushLast(OtherIndex);
				}
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::SplitIslands()
{
	SCOPE_CYCLE_COUNTER(STAT_SplitIslandGraph);

	GraphCounter = (GraphCounter + 1) % MaxCount;
	NodeQueue.Reset();
	for (int32 RootIndex = 0, NumNodes = GraphNodes.GetMaxIndex(); RootIndex < NumNodes; ++RootIndex)
	{
		// We pick all the nodes that are inside an island
		if(GraphNodes.IsValidIndex(RootIndex))
		{
			FGraphNode& RootNode = GraphNodes[RootIndex];
			if(RootNode.NodeCounter != GraphCounter)
			{
				if(RootNode.bValidNode)
				{
					int32 CurrentIsland = RootNode.IslandIndex;
					
					if (GraphIslands.IsValidIndex(CurrentIsland) && GraphIslands[CurrentIsland].bIsPersistent && !GraphIslands[CurrentIsland].bIsSleeping)
					{
						// We don't want to rebuild a new island if this one can't be splitted
						// It is why by default the first one is the main one
						if(GraphIslands[CurrentIsland].IslandCounter == GraphCounter)
						{
							FGraphIsland GraphIsland = { 0, 1, 0, false, false };
							CurrentIsland = AddIsland(MoveTemp(GraphIsland));
						}
						
						GraphIslands[CurrentIsland].IslandCounter = GraphCounter;

						SplitIsland(RootIndex, CurrentIsland);
					}
				}
				else
				{
					RootNode.IslandIndex = INDEX_NONE;
				}
			}
		}
	}
}
	
template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::ReassignIslands()
{
	// Update all the nodes indices
	for (FGraphIsland& GraphIsland : GraphIslands)
	{
		GraphIsland.NumNodes = 0;
		GraphIsland.NumEdges = 0;
	}
	
	// Update all the edges island indices
	for (FGraphEdge& GraphEdge : GraphEdges)
	{
		int32 EdgeIslandIndex = GraphEdge.IslandIndex;
		if (EdgeIslandIndex != INDEX_NONE)
		{
			check(GraphIslands.IsValidIndex(GraphEdge.IslandIndex))

			// ParentIndex will be same as EdgeIslandIndex if we are not merging
			const int32 ParentIndex = GraphIslands[GraphEdge.IslandIndex].ParentIsland;
			check(GraphIslands.IsValidIndex(ParentIndex));

			// If we are moving to another island, that island will be waking up if any of our nodes are moving
			if ((EdgeIslandIndex != ParentIndex) && IsEdgeMoving(GraphEdge))
			{
				GraphIslands[ParentIndex].bIsPersistent = false;
			}

			// Assign to edge to the (maybe new) island
			GraphIslands[ParentIndex].NumEdges++;
			GraphEdge.IslandIndex = ParentIndex;
		}
	}

	// Update all the nodes indices
	for (FGraphNode& GraphNode : GraphNodes)
	{
		if(GraphIslands.IsValidIndex(GraphNode.IslandIndex))
		{
			const int32 ParentIndex = GraphIslands[GraphNode.IslandIndex].ParentIsland;
			if(GraphIslands.IsValidIndex(ParentIndex))
			{
				if (GraphNode.bValidNode)
				{
					GraphNode.IslandIndex = ParentIndex;
				}
				else
				{
					GraphNode.IslandIndex = INDEX_NONE;
				}

				if(GraphNode.bValidNode)
				{
					GraphIslands[ParentIndex].NumNodes++;
				}
			}
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::UpdateGraph()
{
	SCOPE_CYCLE_COUNTER(STAT_MergeIslandGraph);

	// Reset the sleep state of the islands. If any node or edge ion the island has changed we will wake
	// the island. Also if anything was removed from the island since the last update (not persistant)
	// we set it to awake.
	for (auto& GraphIsland : GraphIslands)
	{
		GraphIsland.bWasSleeping = GraphIsland.bIsSleeping;
		GraphIsland.bIsSleeping = GraphIsland.bIsPersistent;
	}

	// Make sure valid edges are in an island (if they have valid nodes)
	for (FGraphEdge& GraphEdge : GraphEdges)
	{
		if (GraphEdge.bValidEdge && GraphEdge.IslandIndex == INDEX_NONE)
		{
			const int32 FirstIslandIndex
				= GraphNodes.IsValidIndex(GraphEdge.FirstNode)
				? GraphNodes[GraphEdge.FirstNode].IslandIndex
				: INDEX_NONE;

			const int32 SecondIslandIndex
				= GraphNodes.IsValidIndex(GraphEdge.SecondNode)
				? GraphNodes[GraphEdge.SecondNode].IslandIndex
				: INDEX_NONE;

			if (FirstIslandIndex != INDEX_NONE)
			{
				GraphEdge.IslandIndex = FirstIslandIndex;
			}
			else if (SecondIslandIndex != INDEX_NONE)
			{
				GraphEdge.IslandIndex = SecondIslandIndex;
			}
			else
			{
				// NOTE: Both particle and edge haven't been added to island.
				// They should get added next frame, but might be better to
				// work out a way to assign them an island immediately.
			}
		}
	}

	// Merge all the islands if necessary
	MergeIslands();

	// Add all the single particle islands and update the sleeping flag
	for (int32 NodeIndex = 0, NumNodes = GraphNodes.GetMaxIndex(); NodeIndex < NumNodes; ++NodeIndex)
	{
		if (GraphNodes.IsValidIndex(NodeIndex))
		{
			FGraphNode& GraphNode = GraphNodes[NodeIndex];

			// Add new islands for the all the particles that are not connected into the graph 
			if (GraphNode.bValidNode && !GraphIslands.IsValidIndex(GraphNode.IslandIndex))
			{
				FGraphIsland GraphIsland = FGraphIsland();
				GraphIsland.NumEdges = 0;
				GraphIsland.NumNodes = 1;
				GraphIsland.bIsSleeping = GraphNode.bStationaryNode;
				GraphNode.IslandIndex = AddIsland(MoveTemp(GraphIsland));
			}

			// Clear the sleeping flag on the island for moving nodes
			if (!GraphNode.bStationaryNode)
			{
				if (GraphNode.bValidNode)
				{
					// NOTE: All valid nodes should have an island index here (see above)
					check(GraphIslands.IsValidIndex(GraphNode.IslandIndex));

					// Valid node (Sleeping/Dynamic Particle) that is moving - island must be awake
					GraphIslands[GraphNode.IslandIndex].bIsSleeping = false;
				}
				else if (!GraphNode.bValidNode)
				{
					// Invalid node (Kinematic) that is moving - islands must be awake.
					// We have to iterate through all the edges since the node could belong to several islands
					// and the NodeIslands array is managed outside this class and may not be up to date.
					// @todo(chaos): move NodeIslands array management into FIslandGraph if possible
					for (auto& EdgeIndex : GraphNode.NodeEdges)
					{
						const int32 EdgeIslandIndex = GraphEdges[EdgeIndex].IslandIndex;
						if (EdgeIslandIndex != INDEX_NONE)
						{
							GraphIslands[EdgeIslandIndex].bIsSleeping = false;
						}
					}
				}
			}
		}
	}
	
	// Split the islands that are persistent and not sleeping if possible
	SplitIslands();

	// Remove edges from their island if:
	// - the island is awake and the edge is invalid (no valid nodes)
	// - the island was destroyed/merged but the edge wasn't moved to the new island (because it is invalid)
	// In other words: when we assigned islands we ignored constraints between two kinematic bodies, but
	// they may be in an island from the last tick (because they were dynamic then) so we reset their island index.
	// We only do this for awake islands. We leave kinematic-kinematic constraints in sleeping islands in case
	// the particle(s) get converted back to dynamic before the islands wakes.
	for (FGraphEdge& GraphEdge : GraphEdges)
	{
		const bool bValidIsland = GraphIslands.IsValidIndex(GraphEdge.IslandIndex);
		const bool bAwakeIsland = bValidIsland && !GraphIslands[GraphEdge.IslandIndex].bIsSleeping;
		const bool bValidEdge = GraphEdge.bValidEdge;
		if (!bValidIsland || (bAwakeIsland && !bValidEdge))
		{
			GraphEdge.IslandIndex = INDEX_NONE;
		}
	}
}

template<typename NodeType, typename EdgeType, typename IslandType, typename OwnerType>
void FIslandGraph<NodeType, EdgeType, IslandType, OwnerType>::RemoveAllAwakeEdges()
{
	// Remove of all the non sleeping edges
	for(int32 EdgeIndex = GraphEdges.GetMaxIndex(); EdgeIndex >= 0; --EdgeIndex)
	{
		if(GraphEdges.IsValidIndex(EdgeIndex) && GraphIslands.IsValidIndex(GraphEdges[EdgeIndex].IslandIndex))
		{
			if (!GraphIslands[GraphEdges[EdgeIndex].IslandIndex].bIsSleeping)
			{
				RemoveEdge(EdgeIndex);
			}
		}
	} 
}

template class FIslandGraph<FGeometryParticleHandle*, FConstraintHandleHolder, FPBDIsland*, FPBDIslandManager>;
template class FIslandGraph<int32, int32, int32, TNullIslandGraphOwner<int32, int32>>;
}
