// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Graph.h"
#include "Graph/GraphEdge.h"
#include "Graph/GraphVertex.h"
#include "Graph/Algorithms/Connectivity/ConnectedComponents.h"
#include "GenericPlatform/GenericPlatformMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Graph)

FSerializableGraph UGraph::GetSerializableGraph() const
{
	FSerializableGraph Out;
	Out.Properties = Properties;

	for (const TPair<FGraphVertexHandle, TObjectPtr<UGraphVertex>>& Kvp : Vertices)
	{
		if (!Kvp.Key.IsComplete() || !Kvp.Value)
		{
			continue;
		}
		Out.Vertices.Add(Kvp.Key);
	}

	for (const TPair<FGraphEdgeHandle, TObjectPtr<UGraphEdge>>& Kvp : Edges)
	{
		if (!Kvp.Key.IsComplete() || !Kvp.Value)
		{
			continue;
		}

		Out.Edges.Add(Kvp.Key, Kvp.Value->GetSerializedData());
	}

	for (const TPair<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& Kvp : Islands)
	{
		if (!Kvp.Key.IsComplete() || !Kvp.Value)
		{
			continue;
		}

		Out.Islands.Add(Kvp.Key, Kvp.Value->GetSerializedData());
	}

	return Out;
}

void UGraph::LoadFromSerializedGraph(const FSerializableGraph& Input)
{
	InitializeFromProperties(Input.Properties);

	for (const FGraphVertexHandle& Handle : Input.Vertices)
	{
		if (!Handle.IsValid())
		{
			continue;
		}

		CreateVertex(Handle.GetUniqueIndex());
	}

	for (const TPair<FGraphEdgeHandle, FSerializedEdgeData>& Kvp : Input.Edges)
	{
		if (!Kvp.Key.IsValid())
		{
			continue;
		}

		CreateEdge(
			GetCompleteNodeHandle(Kvp.Value.Node1),
			GetCompleteNodeHandle(Kvp.Value.Node2),
			Kvp.Key.GetUniqueIndex(),
			false
		);
	}

	for (const TPair<FGraphIslandHandle, FSerializedIslandData>& Kvp : Input.Islands)
	{
		if (!Kvp.Key.IsValid())
		{
			continue;
		}

		TArray<FGraphVertexHandle> IslandVertices = Kvp.Value.Vertices;
		for (FGraphVertexHandle& VertexHandle : IslandVertices)
		{
			VertexHandle = GetCompleteNodeHandle(VertexHandle);
		}

		CreateIsland(IslandVertices, Kvp.Key.GetUniqueIndex());
	}
}

void UGraph::Empty()
{
	Vertices.Empty();
	Edges.Empty();
	Islands.Empty();
	NextAvailableVertexUniqueIndex = 0;
	NextAvailableEdgeUniqueIndex = 0;
	NextAvailableIslandUniqueIndex = 0;
}

void UGraph::InitializeFromProperties(const FGraphProperties& InProperties)
{
	Empty();
	Properties = InProperties;
}

FGraphVertexHandle UGraph::CreateVertex(int64 InUniqueIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::CreateVertex);
	TObjectPtr<UGraphVertex> Vertex = CreateTypedVertex();
	if (!Vertex)
	{
		return {};
	}
	Vertex->OnCreate();

	// No need to increment since AddNode will do that.
	Vertex->SetUniqueIndex((InUniqueIndex == INDEX_NONE) ? NextAvailableVertexUniqueIndex : InUniqueIndex);
	RegisterVertex(Vertex);
	OnVertexCreated.Broadcast(Vertex->Handle());
	return Vertex->Handle();
}

void UGraph::RegisterVertex(TObjectPtr<UGraphVertex> Vertex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RegisterVertex);
	if (!Vertex)
	{
		return;
	}

	const FGraphVertexHandle Handle = Vertex->Handle();
	Vertex->SetParentGraph(this);
	Vertices.Add(Handle, Vertex);
	NextAvailableVertexUniqueIndex = FMath::Max(NextAvailableVertexUniqueIndex, Handle.GetUniqueIndex() + 1);
}

FGraphEdgeHandle UGraph::CreateEdge(FGraphVertexHandle Node1, FGraphVertexHandle Node2, int64 InUniqueIndex, bool bAddToIslands)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::CreateEdge);
	if (!Node1.IsComplete() || !Node2.IsComplete())
	{
		return {};
	}

	TObjectPtr<UGraphEdge> Edge = CreateTypedEdge();
	if (!Edge)
	{
		return {};
	}

	if (Node2 < Node1)
	{
		std::swap(Node1, Node2);
	}

	TObjectPtr<UGraphVertex> Node1Ptr = Node1.GetVertex();
	TObjectPtr<UGraphVertex> Node2Ptr = Node2.GetVertex();
	if (!Node1Ptr || !Node2Ptr)
	{
		return {};
	}

	// For edges, we also need to make sure the edge doesn't already exist.
	if (Node1Ptr->HasEdgeTo(Node2) || Node2Ptr->HasEdgeTo(Node1))
	{
		return {};
	}
	Edge->OnCreate();

	// No need to increment since AddEdge will do that.
	Edge->SetUniqueIndex((InUniqueIndex == INDEX_NONE) ? NextAvailableEdgeUniqueIndex : InUniqueIndex);
	Edge->SetNodes(Node1, Node2);

	RegisterEdge(Edge);

	// Also need to make sure the nodes are aware of the new edge.
	Node1Ptr->AddEdgeTo(Node2, Edge->Handle());
	Node2Ptr->AddEdgeTo(Node1, Edge->Handle());

	OnEdgeCreated.Broadcast(Edge->Handle());

	// If we want to keep track of islands, this is where we need to create/merge islands.
	if (Properties.bGenerateIslands && bAddToIslands)
	{
		MergeOrCreateIslands(Edge);
	}

	return Edge->Handle();
}

void UGraph::RegisterEdge(TObjectPtr<UGraphEdge> Edge)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RegisterEdge);
	if (!Edge)
	{
		return;
	}

	const FGraphEdgeHandle Handle = Edge->Handle();
	Edges.Add(Handle, Edge);
	Edge->SetParentGraph(this);

	VertexEdges.FindOrAdd(Edge->NodeA()).Add(Handle);
	VertexEdges.FindOrAdd(Edge->NodeB()).Add(Handle);

	NextAvailableEdgeUniqueIndex = FMath::Max(NextAvailableEdgeUniqueIndex, Handle.GetUniqueIndex() + 1);
}

FGraphIslandHandle UGraph::CreateIsland(TArray<FGraphVertexHandle> InputNodes, int64 InUniqueIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::CreateIsland);
	if (InputNodes.IsEmpty())
	{
		return {};
	}

	TObjectPtr<UGraphIsland> Island = CreateTypedIsland();
	if (!Island)
	{
		return {};
	}
	Island->OnCreate();

	Island->SetUniqueIndex((InUniqueIndex == INDEX_NONE) ? NextAvailableIslandUniqueIndex : InUniqueIndex);

	OnIslandCreated.Broadcast(Island->Handle());
	for (const FGraphVertexHandle& Node : InputNodes)
	{
		Island->AddVertex(Node);
	}

	RegisterIsland(Island);
	return Island->Handle();
}

void UGraph::RegisterIsland(TObjectPtr<UGraphIsland> Island)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RegisterIsland);
	if (!Island)
	{
		return;
	}
	const FGraphIslandHandle Handle = Island->Handle();
	Island->SetParentGraph(this);
	Islands.Add(Handle, Island);
	NextAvailableIslandUniqueIndex = FMath::Max(NextAvailableIslandUniqueIndex, Handle.GetUniqueIndex() + 1);
}

void UGraph::RemoveIsland(const FGraphIslandHandle& IslandHandle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RemoveIsland);
	if (!IslandHandle.IsValid())
	{
		return;
	}

	if (TObjectPtr<UGraphIsland> Island = IslandHandle.GetIsland())
	{
		Island->Destroy();
	}
	Islands.Remove(IslandHandle);
}

void UGraph::MergeOrCreateIslands(TObjectPtr<UGraphEdge> Edge)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::MergeOrCreateIslands);
	if (!Edge)
	{
		return;
	}

	FGraphVertexHandle AHandle = Edge->NodeA();
	FGraphVertexHandle BHandle = Edge->NodeB();

	TObjectPtr<UGraphVertex> NodeA = AHandle.GetVertex();
	TObjectPtr<UGraphVertex> NodeB = BHandle.GetVertex();
	if (!NodeA || !NodeB)
	{
		return;
	}

	FGraphIslandHandle IslandHandleA = NodeA->GetParentIsland();
	FGraphIslandHandle IslandHandleB = NodeB->GetParentIsland();

	TObjectPtr<UGraphIsland> IslandA = IslandHandleA.GetIsland();
	TObjectPtr<UGraphIsland> IslandB = IslandHandleB.GetIsland();

	if (IslandHandleA.IsValid() && IslandHandleB.IsValid() && IslandHandleA != IslandHandleB)
	{
		// Both are in separate, valid islands - merge islands (i.e. use island A and just get rid of island B).
		if (ensure(IslandA != nullptr))
		{
			IslandA->MergeWith(IslandB);
		}
		RemoveIsland(IslandHandleB);
	}
	else if (IslandHandleA.IsValid() && !IslandHandleB.IsValid())
	{
		// Only A is in an island, go into island A.
		if (ensure(IslandA != nullptr))
		{
			IslandA->AddVertex(BHandle);
		}
	}
	else if (IslandHandleB.IsValid() && !IslandHandleA.IsValid())
	{
		// Only B is in an island, go into island B.
		if (ensure(IslandB != nullptr))
		{
			IslandB->AddVertex(AHandle);
		}
	}
	else if (!IslandHandleA.IsValid() && !IslandHandleB.IsValid())
	{
		// Neither is in an island - need to create a new one.
		CreateIsland({ AHandle, BHandle });
	}
}

void UGraph::RemoveVertex(const FGraphVertexHandle& NodeHandle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RemoveVertex);
	if (!NodeHandle.IsValid())
	{
		return;
	}

	TObjectPtr<UGraphVertex> Node = NodeHandle.GetVertex();
	if (!Node)
	{
		return;
	}

	// We must remove every edge this node is a part of.
	for (const FGraphEdgeHandle& EdgeHandle : VertexEdges.FindOrAdd(NodeHandle))
	{
		// Don't immediately handle islands. We'll do it later.
		RemoveEdge(EdgeHandle, false);
	}

	if (Properties.bGenerateIslands)
	{
		// If the node is a part of an island - remove it from the island and evaluate the effect that has.
		if (const FGraphIslandHandle& IslandHandle = Node->GetParentIsland(); IslandHandle.IsValid())
		{
			if (TObjectPtr<UGraphIsland> Island = IslandHandle.GetIsland())
			{
				Island->RemoveVertex(NodeHandle);
				RemoveOrSplitIsland(Island);
			}
		}
	}

	Node->HandleOnVertexRemoved();
	Vertices.Remove(NodeHandle);
	VertexEdges.Remove(NodeHandle);
}

void UGraph::RemoveEdge(const FGraphEdgeHandle& EdgeHandle, bool bHandleIslands)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RemoveEdge);
	if (!EdgeHandle.IsValid())
	{
		return;
	}

	TObjectPtr<UGraphEdge> Edge = EdgeHandle.GetEdge();
	if (!Edge)
	{
		return;
	}

	FGraphIslandHandle IslandHandle;

	// Need to remove the edge reference from both nodes.
	if (const FGraphVertexHandle& NodeHandleA = Edge->NodeA(); NodeHandleA.IsValid())
	{
		if (TObjectPtr<UGraphVertex> Node = NodeHandleA.GetVertex(); Node)
		{
			Node->RemoveEdge(EdgeHandle);
			IslandHandle = Node->GetParentIsland();
		}
	}

	if (const FGraphVertexHandle& NodeHandleB = Edge->NodeB(); NodeHandleB.IsValid())
	{
		if (TObjectPtr<UGraphVertex> Node = NodeHandleB.GetVertex(); Node)
		{
			Node->RemoveEdge(EdgeHandle);

			// Technically shouldn't be necessary but in here just in case.
			if (!IslandHandle.IsValid())
			{
				IslandHandle = Node->GetParentIsland();
			}
		}
	}
	
	// Removing an edge should cause the island to check if it needs to split.
	if (Properties.bGenerateIslands && bHandleIslands)
	{
		// Note that we can assume that both nodes are in the same island.
		RemoveOrSplitIsland(IslandHandle.GetIsland());
	}
}

void UGraph::RemoveOrSplitIsland(TObjectPtr<UGraphIsland> Island)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::RemoveOrSplitIsland);
	if (!Island)
	{
		return;
	}

	if (Island->IsEmpty())
	{
		RemoveIsland(Island->Handle());
	}
	else
	{
		TSet<FGraphVertexHandle> IslandNodes = Island->GetVertices();
		TArray<TSet<FGraphVertexHandle>> ConnectedComponents = Graph::Algorithms::FindConnectedComponents(Island->GetVertices());

		// Sort the connected components from largest to smallest since we'll make the assumption that ConnectedComponents[0] will
		// correspond to the already existing island. This way we do the least amount of transferal of nodes from the initial island
		// to the new islands we're going to create.
		ConnectedComponents.Sort(
			[](const TSet<FGraphVertexHandle>& A, const TSet<FGraphVertexHandle>& B)
			{
				return A.Num() > B.Num();
			}
		);

		TArray<TObjectPtr<UGraphIsland>> AffectedIslands = { Island };
		const bool bIsIdentical = ConnectedComponents.Num() == 1 && ConnectedComponents[0].Num() == IslandNodes.Num();
		if (!bIsIdentical)
		{
			// First remove any of the nodes that need to be removed from the existing island.
			for (const FGraphVertexHandle& NodeHandle : IslandNodes)
			{
				if (!ConnectedComponents[0].Contains(NodeHandle))
				{
					Island->RemoveVertex(NodeHandle);
				}
			}

			// Next, create islands for every additional connected component.
			for (int32 Index = 1; Index < ConnectedComponents.Num(); ++Index)
			{
				FGraphIslandHandle NewIsland = CreateIsland(ConnectedComponents[Index].Array());
				if (NewIsland.IsComplete())
				{
					AffectedIslands.Add(NewIsland.GetIsland());
				}
			}
		}

		// All the islands that got changed need to have OnConnectivityChanged called.
		// This let's all the listeners of the islands make a decision on what to do with
		// the island that had a destructive change to its connected components as well as
		// the newly created islands.
		for (TObjectPtr<UGraphIsland>& AffectedIsland : AffectedIslands)
		{
			AffectedIsland->HandleOnConnectivityChanged();
		}
	}
}

FGraphVertexHandle UGraph::GetCompleteNodeHandle(const FGraphVertexHandle& InHandle)
{
	if (!InHandle.IsValid() || InHandle.HasElement())
	{
		return InHandle;
	}

	TObjectPtr<UGraphVertex>* Data = Vertices.Find(InHandle);
	if (!Data || !*Data)
	{
		return {};
	}

	return (*Data)->Handle();
}

TObjectPtr<UGraphVertex> UGraph::CreateTypedVertex() const
{
	return NewObject<UGraphVertex>(const_cast<UGraph*>(this), UGraphVertex::StaticClass());
}

TObjectPtr<UGraphEdge> UGraph::CreateTypedEdge() const
{
	return NewObject<UGraphEdge>(const_cast<UGraph*>(this), UGraphEdge::StaticClass());
}

TObjectPtr<UGraphIsland> UGraph::CreateTypedIsland() const
{
	return NewObject<UGraphIsland>(const_cast<UGraph*>(this), UGraphIsland::StaticClass());
}

void UGraph::FinalizeVertex(const FGraphVertexHandle& InHandle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraph::FinalizeVertex);
	if (!InHandle.IsComplete())
	{
		return;
	}

	TObjectPtr<UGraphVertex> Node = InHandle.GetVertex();
	if (!Node)
	{
		return;
	}

	FGraphIslandHandle IslandHandle = Node->GetParentIsland();
	if (!IslandHandle.IsComplete())
	{
		// Every vertex must be in an island.
		IslandHandle = CreateIsland({ InHandle });
	}

	if (TObjectPtr<UGraphIsland> Island = IslandHandle.GetIsland())
	{
		Island->HandleOnConnectivityChanged();
	}
}