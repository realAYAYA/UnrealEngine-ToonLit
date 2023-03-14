// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntitySystemGraphs.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneSystemTaskDependencies.h"

#include "Templates/SubclassOf.h"
#include "Algo/IndexOf.h"
#include "Algo/Reverse.h"
#include "Algo/RemoveIf.h"
#include "Algo/BinarySearch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneEntitySystemGraphs)


FMovieSceneEntitySystemDirectedGraph::FDepthFirstSearch::FDepthFirstSearch(const FMovieSceneEntitySystemDirectedGraph* InGraph)
	: Visited(false, InGraph->Nodes.Num())
	, IsVisiting(false, InGraph->Nodes.Num())
{
	Graph = InGraph;
	PostNodes.Reserve(InGraph->Nodes.CountSetBits());
	check(!Graph->bHasDanglingEdges);
}

void FMovieSceneEntitySystemDirectedGraph::FDepthFirstSearch::Search(uint16 InNodeID)
{
	IsVisiting[InNodeID] = true;

	for (FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge Edge : Graph->GetEdgesFrom(InNodeID))
	{
		if (Visited[Edge.ToNode] == false)
		{
			if (!ensureMsgf(IsVisiting[Edge.ToNode] == false, TEXT("Cycle found in graph.")))
			{
				return;
			}
			Visited[Edge.ToNode] = true;
			Search(Edge.ToNode);
		}
	}

	PostNodes.Add(InNodeID);

	IsVisiting[InNodeID] = false;
}

FMovieSceneEntitySystemDirectedGraph::FBreadthFirstSearch::FBreadthFirstSearch(const FMovieSceneEntitySystemDirectedGraph* InGraph)
	: Visited(false, InGraph->Nodes.Num())
	, StackIndex(0)
{
	Graph = InGraph;
	Nodes.Reserve(InGraph->Nodes.CountSetBits());
	check(!InGraph->bHasDanglingEdges);
}

void FMovieSceneEntitySystemDirectedGraph::FBreadthFirstSearch::Search(uint16 InNodeID)
{
	if (Visited[InNodeID] == true)
	{
		return;
	}

	Nodes.Reset();
	StackIndex = 0;

	Visited[InNodeID] = true;
	Nodes.Add(InNodeID);

	while (StackIndex < Nodes.Num())
	{
		const int32 StackEnd = Nodes.Num();
		for ( ; StackIndex < StackEnd; ++StackIndex)
		{
			const uint16 NodeID = Nodes[StackIndex];

			// Visit all nodes this points to
			for (FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge Edge : Graph->GetEdgesFrom(NodeID))
			{
				if (Visited[Edge.ToNode] == false)
				{
					Visited[Edge.ToNode] = true;
					Nodes.Add(Edge.ToNode);
				}
			}
		}
	}
}

FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges::FDiscoverCyclicEdges(const FMovieSceneEntitySystemDirectedGraph* InGraph)
	: CyclicEdges(false, InGraph->SortedEdges.Num())
	, VisitedEdges(false, InGraph->SortedEdges.Num())
{
	Graph = InGraph;
	check(!Graph->bHasDanglingEdges);
}

void FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges::Search()
{
	for (uint16 EdgeIndex = 0; EdgeIndex < Graph->SortedEdges.Num(); ++EdgeIndex)
	{
		if (VisitedEdges[EdgeIndex] == false)
		{
			EdgeChain.Reset();
			EdgeChain.Add(EdgeIndex);

			SearchFrom(Graph->SortedEdges[EdgeIndex].FromNode);

			VisitedEdges[EdgeIndex] = true;
		}
	}
}

void FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges::SearchFrom(uint16 NodeID)
{
	TBitArray<> VisitedNodes(false, Graph->Nodes.Num());
	VisitedNodes[NodeID] = true;
	DiscoverCycles(NodeID, VisitedNodes);
}

void FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges::DiscoverCycles(uint16 NodeID, TBitArray<>& VisitedNodes)
{
	// Iterate all edges from this node
	for (int32 SubsequentEdge = Graph->FindEdgeStart(NodeID); SubsequentEdge < Graph->SortedEdges.Num() && Graph->SortedEdges[SubsequentEdge].FromNode == NodeID; ++SubsequentEdge)
	{
		if (VisitedEdges[SubsequentEdge] == true)
		{
			continue;
		}

		VisitedEdges[SubsequentEdge] = true;
		EdgeChain.Add(SubsequentEdge);

		const uint16 SubsequentNode = Graph->SortedEdges[SubsequentEdge].ToNode;
		if (VisitedNodes[SubsequentNode] == true)
		{
			TagCyclicChain(SubsequentNode);
		}
		else
		{
			VisitedNodes[SubsequentNode] = true;
			DiscoverCycles(SubsequentNode, VisitedNodes);
			VisitedNodes[SubsequentNode] = false;
		}

		EdgeChain.Pop();
	}
}

void FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges::TagCyclicChain(uint16 CyclicNodeID)
{
	// Found a cycle
	for (int32 EdgeChainIndex = EdgeChain.Num() - 1; EdgeChainIndex >= 0; --EdgeChainIndex)
	{
		const uint16 UpstreamEdgeIndex = EdgeChain[EdgeChainIndex];
		CyclicEdges.PadToNum(UpstreamEdgeIndex + 1, false);
		CyclicEdges[UpstreamEdgeIndex] = true;

		if (Graph->SortedEdges[UpstreamEdgeIndex].FromNode == CyclicNodeID)
		{
			return;
		}
	}
}

void FMovieSceneEntitySystemGraphNodes::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (FMovieSceneEntitySystemGraphNode& Node : const_cast<TSparseArray<FMovieSceneEntitySystemGraphNode>&>(Array))
	{
		Collector.AddReferencedObject(Node.System);
	}
}

void FMovieSceneEntitySystemDirectedGraph::AllocateNode(uint16 NodeID)
{
	CleanUpDanglingEdges();
	Nodes.PadToNum(NodeID + 1, false);
	Nodes[NodeID] = true;
}

bool FMovieSceneEntitySystemDirectedGraph::IsNodeAllocated(uint16 NodeID) const
{
	return Nodes.IsValidIndex(NodeID) && Nodes[NodeID] == true;
}

void FMovieSceneEntitySystemDirectedGraph::RemoveNode(uint16 NodeID)
{
	check(NodeID != TNumericLimits<uint16>::Max() && IsNodeAllocated(NodeID));

	// Remove the node from the graph
	Nodes[NodeID] = false;

	bHasDanglingEdges = true;
}

void FMovieSceneEntitySystemDirectedGraph::CleanUpDanglingEdges()
{
	if (!bHasDanglingEdges)
	{
		return;
	}

	bHasDanglingEdges = false;
	for (int32 Index = 0; Index < SortedEdges.Num(); )
	{
		FDirectionalEdge Edge = SortedEdges[Index];
		if (!IsNodeAllocated(Edge.ToNode) || !IsNodeAllocated(Edge.FromNode))
		{
			SortedEdges.RemoveAt(Index, 1, false);
		}
		else
		{
			++Index;
		}
	}
}

bool FMovieSceneEntitySystemDirectedGraph::IsCyclic() const
{
	TBitArray<> Visited(false, Nodes.Num());

	for (FDirectionalEdge Edge : SortedEdges)
	{
		if (Visited[Edge.ToNode] == true)
		{
			continue;
		}

		TBitArray<> Visiting(false, Nodes.Num());
		if (IsCyclicImpl(Edge.ToNode, Visiting))
		{
			return true;
		}

		Visited.CombineWithBitwiseOR(Visiting, EBitwiseOperatorFlags::MaxSize);
	}

	return false;
}

bool FMovieSceneEntitySystemDirectedGraph::IsCyclicImpl(uint16 NodeID, TBitArray<>& Visiting) const
{
	if (Visiting[NodeID] == true)
	{
		return true;
	}

	Visiting[NodeID] = true;

	for (FDirectionalEdge Edge : GetEdgesFrom(NodeID))
	{
		if (IsCyclicImpl(Edge.ToNode, Visiting))
		{
			return true;
		}
	}

	Visiting[NodeID] = false;
	return false;
}

void FMovieSceneEntitySystemDirectedGraph::MakeEdge(uint16 FromNode, uint16 ToNode)
{
	FDirectionalEdge NewEdge(FromNode, ToNode);

	const int32 InsertIndex = FindEdgeIndex(NewEdge);
	if (!SortedEdges.IsValidIndex(InsertIndex) || SortedEdges[InsertIndex] != NewEdge)
	{
		SortedEdges.Insert(NewEdge, InsertIndex);
	}
}

void FMovieSceneEntitySystemDirectedGraph::DestroyEdge(uint16 FromNode, uint16 ToNode)
{
	FDirectionalEdge Edge(FromNode, ToNode);

	const int32 RemoveIndex = Algo::BinarySearch(SortedEdges, Edge);
	if (RemoveIndex != INDEX_NONE)
	{
		SortedEdges.RemoveAt(RemoveIndex, 1, false);
	}
}

int32 FMovieSceneEntitySystemDirectedGraph::FindEdgeStart(uint16 FromNode) const
{
	return Algo::LowerBoundBy(SortedEdges, FromNode, &FDirectionalEdge::FromNode);
}

TArrayView<const FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge> FMovieSceneEntitySystemDirectedGraph::GetEdges() const
{
	check(!bHasDanglingEdges);
	return SortedEdges;
}

bool FMovieSceneEntitySystemDirectedGraph::HasEdgeFrom(uint16 InNode) const
{
	const int32 ExpectedIndex = FindEdgeStart(InNode);
	return SortedEdges.IsValidIndex(ExpectedIndex) && SortedEdges[ExpectedIndex].FromNode == InNode;
}

bool FMovieSceneEntitySystemDirectedGraph::HasEdgeTo(uint16 InNode) const
{
	check(InNode != TNumericLimits<uint16>::Max());
	return Algo::FindBy(SortedEdges, InNode, &FDirectionalEdge::ToNode) != nullptr;
}

TArrayView<const FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge> FMovieSceneEntitySystemDirectedGraph::GetEdgesFrom(uint16 InNodeID) const
{
	check(!bHasDanglingEdges);

	const int32 EdgeIndex = FindEdgeStart(InNodeID);

	int32 Num = 0;
	while (EdgeIndex + Num < SortedEdges.Num() && SortedEdges[EdgeIndex + Num].FromNode == InNodeID)
	{
		++Num;
	}

	if (Num > 0)
	{
		return MakeArrayView(SortedEdges.GetData() + EdgeIndex, Num);
	}
	return TArrayView<const FDirectionalEdge>();
}

TBitArray<> FMovieSceneEntitySystemDirectedGraph::FindEdgeUpstreamNodes() const
{
	check(!bHasDanglingEdges);

	TBitArray<> EdgeNodes(true, Nodes.Num());

	// Unmark nodes that have edges pointing towards them
	for (uint16 EdgeIndex = 0; EdgeIndex < SortedEdges.Num(); ++EdgeIndex)
	{
		const uint16 ToNode = SortedEdges[EdgeIndex].ToNode;
		EdgeNodes[ToNode] = false;
	}

	// Mask with nodes that are actually allocated
	return TBitArray<>::BitwiseAND(EdgeNodes, Nodes, EBitwiseOperatorFlags::MaxSize);
}

int32 FMovieSceneEntitySystemDirectedGraph::FindEdgeIndex(const FDirectionalEdge& Edge) const
{
	check(!bHasDanglingEdges);
	return Algo::LowerBound(SortedEdges, Edge);
}

bool FMovieSceneEntitySystemDirectedGraph::EdgeExists(const FDirectionalEdge& Edge) const
{
	check(!bHasDanglingEdges);

	const int32 EdgeIndex = FindEdgeIndex(Edge);
	return EdgeIndex < SortedEdges.Num() && SortedEdges[EdgeIndex] == Edge;
}


void FMovieSceneEntitySystemGraph::AddSystem(UMovieSceneEntitySystem* InSystem)
{
	checkf(ReentrancyGuard == 0, TEXT("Attempting to add a system to the graph recursively."));

	checkSlow(InSystem->GetGraphID() == TNumericLimits<uint16>::Max() && Nodes.Array.GetMaxIndex() < TNumericLimits<uint16>::Max() - 1);

	const int32 NewIndex = Nodes.Array.Add(FMovieSceneEntitySystemGraphNode(InSystem));
	check(NewIndex >= 0 && NewIndex < TNumericLimits<uint16>::Max());

	const uint16 NewNodeID = static_cast<uint16>(NewIndex);

	FlowGraph.AllocateNode(NewNodeID);
	ReferenceGraph.AllocateNode(NewNodeID);

	InSystem->SetGraphID(NewNodeID);

	++SerialNumber;
}

int32 FMovieSceneEntitySystemGraph::NumSubsequents(UMovieSceneEntitySystem* InSystem) const
{
	const uint16 GraphID = InSystem->GetGraphID();
	check(GraphID != TNumericLimits<uint16>::Max());

	return FlowGraph.GetEdgesFrom(GraphID).Num();
}

void FMovieSceneEntitySystemGraph::AddPrerequisite(UMovieSceneEntitySystem* Upstream, UMovieSceneEntitySystem* Downstream)
{
	const uint16 UpstreamID   = Upstream->GetGraphID();
	const uint16 DownstreamID = Downstream->GetGraphID();

	check(UpstreamID != TNumericLimits<uint16>::Max() && DownstreamID != TNumericLimits<uint16>::Max());

	FlowGraph.MakeEdge(UpstreamID, DownstreamID);

	++SerialNumber;
}

void FMovieSceneEntitySystemGraph::AddReference(UMovieSceneEntitySystem* FromReference, UMovieSceneEntitySystem* ToReference)
{
	const uint16 FromID = FromReference->GetGraphID();
	const uint16 ToID   = ToReference->GetGraphID();

	check(FromID != TNumericLimits<uint16>::Max() && ToID != TNumericLimits<uint16>::Max());

	ReferenceGraph.MakeEdge(FromID, ToID);
}

void FMovieSceneEntitySystemGraph::RemoveReference(UMovieSceneEntitySystem* FromReference, UMovieSceneEntitySystem* ToReference)
{
	const uint16 FromID = FromReference->GetGraphID();
	const uint16 ToID   = ToReference->GetGraphID();

	check(FromID != TNumericLimits<uint16>::Max() && ToID != TNumericLimits<uint16>::Max());

	ReferenceGraph.DestroyEdge(FromID, ToID);
}

void FMovieSceneEntitySystemGraph::RemoveSystem(UMovieSceneEntitySystem* InSystem)
{
	checkf(ReentrancyGuard == 0, TEXT("Attempting to remove a system from the graph recursively."));

	++ReentrancyGuard;

	const uint16 NodeID = InSystem->GetGraphID();
	check(NodeID != TNumericLimits<uint16>::Max());

	FlowGraph.RemoveNode(NodeID);
	ReferenceGraph.RemoveNode(NodeID);

	Nodes.Array.RemoveAt(NodeID);

	InSystem->SetGraphID(TNumericLimits<uint16>::Max());

	++SerialNumber;
	--ReentrancyGuard;

	FlowGraph.CleanUpDanglingEdges();
	ReferenceGraph.CleanUpDanglingEdges();
}

int32 FMovieSceneEntitySystemGraph::RemoveIrrelevantSystems(UMovieSceneEntitySystemLinker* Linker)
{
	checkf(ReentrancyGuard == 0, TEXT("Attempting to remove a system from the graph recursively."));
	++ReentrancyGuard;

	check(PreviousSerialNumber == SerialNumber);

	int32 NumRemoved = 0;
	bool bHasUnreferecedIntermediateSystems = false;

	FMovieSceneEntitySystemDirectedGraph::FBreadthFirstSearch Search(&ReferenceGraph);

	// Search from all non-intermediate systems and mark systems that are still referenced
	for (const FMovieSceneEntitySystemGraphNode& Node : Nodes.Array)
	{
		const uint16 GraphID = Node.System->GetGraphID();
		if (Search.GetVisited()[GraphID] == false && Node.System->IsRelevant(Linker))
		{
			Search.Search(GraphID);
		}
	}

	const bool bAnyNotVisited = Search.GetVisited().Num() < Nodes.Array.GetMaxIndex() || Search.GetVisited().Find(false) != INDEX_NONE;
	if (bAnyNotVisited)
	{
		for (int32 Index = 0; Index < Nodes.Array.GetMaxIndex(); ++Index)
		{
			if (Nodes.Array.IsAllocated(Index) && Search.GetVisited()[Index] == false)
			{
				const uint16 NodeID = Index;

				FlowGraph.RemoveNode(NodeID);
				ReferenceGraph.RemoveNode(NodeID);

				UMovieSceneEntitySystem* System = Nodes.Array[NodeID].System;
				Nodes.Array.RemoveAt(NodeID);

				// Remove this system from the graph to ensure we are not re-entrant
				System->SetGraphID(TNumericLimits<uint16>::Max());
				System->Unlink();
				++NumRemoved;
			}
		}
	}

	if (NumRemoved > 0)
	{
		++SerialNumber;

		FlowGraph.CleanUpDanglingEdges();
		ReferenceGraph.CleanUpDanglingEdges();
	}

	--ReentrancyGuard;
	return NumRemoved;
}

void FMovieSceneEntitySystemGraph::UpdateCache()
{
	using namespace UE::MovieScene;

	if (PreviousSerialNumber == SerialNumber)
	{
		return;
	}

	FlowGraph.CleanUpDanglingEdges();
	ReferenceGraph.CleanUpDanglingEdges();

	checkf(!FlowGraph.IsCyclic(), TEXT("Cycle detected in system flow graph.\n")
		TEXT("----------------------------------------------------------------------------------\n")
		TEXT("%s\n")
		TEXT("----------------------------------------------------------------------------------\n"),
		*ToString());

	checkf(!ReferenceGraph.IsCyclic(), TEXT("Cycle detected in system reference graph.\n")
		TEXT("----------------------------------------------------------------------------------\n")
		TEXT("%s\n")
		TEXT("----------------------------------------------------------------------------------\n"),
		*ToString());

	SpawnPhase.Empty();
	InstantiationPhase.Empty();
	EvaluationPhase.Empty();
	FinalizationPhase.Empty();

	FMovieSceneEntitySystemDirectedGraph::FDepthFirstSearch DepthFirstSearch(&FlowGraph);

	TBitArray<> EdgeNodes = FlowGraph.FindEdgeUpstreamNodes();
	for (TConstSetBitIterator<> EdgeNodeIt(EdgeNodes); EdgeNodeIt; ++EdgeNodeIt)
	{
		const uint16 NodeID = static_cast<uint16>(EdgeNodeIt.GetIndex());
		check(Nodes.Array.IsAllocated(NodeID));

		DepthFirstSearch.Search(NodeID);
	}

	Algo::Reverse(DepthFirstSearch.PostNodes);

	for (uint16 NodeID : DepthFirstSearch.PostNodes)
	{
		ESystemPhase SystemPhase = Nodes.Array[NodeID].System->GetPhase();

		if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Spawn))
		{
			SpawnPhase.Emplace(NodeID);
		}
		if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Instantiation))
		{
			InstantiationPhase.Emplace(NodeID);
		}
		if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Evaluation))
		{
			EvaluationPhase.Emplace(NodeID);
		}
		if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Finalization))
		{
			FinalizationPhase.Emplace(NodeID);
		}
	}

	PreviousSerialNumber = SerialNumber;
}

void FMovieSceneEntitySystemGraph::DebugPrint() const
{
	const TCHAR FormatString[] =
		TEXT("----------------------------------------------------------------------------------\n")
		TEXT("%s\n")
		TEXT("----------------------------------------------------------------------------------\n");

	GLog->Log(TEXT("Printing debug graph for Entity System Graph (in standard graphviz syntax):"));
	GLog->Log(FString::Printf(FormatString, *ToString()));
}

FString FMovieSceneEntitySystemGraph::ToString() const
{
	using namespace UE::MovieScene;

	FString String;
	String += TEXT("\ndigraph FMovieSceneEntitySystemGraph {\n");
	String += TEXT("\tnode [shape=record,height=.1];\n");

	FString FlowStrings[] = 
	{
		TEXT("\tsubgraph cluster_flow_0 { label=\"Spawn\"; color=\"#0e868c\";\n"),
		TEXT("\tsubgraph cluster_flow_1 { label=\"Instantiation\"; color=\"#96c74c\";\n"),
		TEXT("\tsubgraph cluster_flow_2 { label=\"Evaluation\"; color=\"#6dc74c\";\n"),
		TEXT("\tsubgraph cluster_flow_3 { label=\"Finalization\"; color=\"#aa42f5\";\n"),
	};

	FString ReferenceGraphString = TEXT("\tsubgraph cluster_references { label=\"Explicit Reference Graph (connections imply ownership)\"; color=\"#bfc74c\";\n");

	for (int32 SystemIndex = 0; SystemIndex < this->Nodes.Array.GetMaxIndex(); ++SystemIndex)
	{
		if (Nodes.Array.IsAllocated(SystemIndex))
		{
			UMovieSceneEntitySystem* System = Nodes.Array[SystemIndex].System;

			ESystemPhase SystemPhase = System->GetPhase();

			if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Spawn))
			{
				FlowStrings[0] += FString::Printf(TEXT("\t\tflow_node%d_0[label=\"%s\"];\n"), SystemIndex, *System->GetName());
			}
			if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Instantiation))
			{
				FlowStrings[1] += FString::Printf(TEXT("\t\tflow_node%d_1[label=\"%s\"];\n"), SystemIndex, *System->GetName());
			}
			if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Evaluation))
			{
				FlowStrings[2] += FString::Printf(TEXT("\t\tflow_node%d_2[label=\"%s\"];\n"), SystemIndex, *System->GetName());
			}
			if (EnumHasAnyFlags(SystemPhase, ESystemPhase::Finalization))
			{
				FlowStrings[3] += FString::Printf(TEXT("\t\tflow_node%d_3[label=\"%s\"];\n"), SystemIndex, *System->GetName());
			}

			ReferenceGraphString += FString::Printf(TEXT("\t\treference_node%d[label=\"%s\"];\n"), SystemIndex, *System->GetName());
		}
	}

	for (FString& FlowString : FlowStrings)
	{
		String += FlowString;
		String += TEXT("\t}\n");
	}
	String += ReferenceGraphString;
	String += TEXT("\t}\n");

	{
		FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges CyclicEdges(&FlowGraph);
		CyclicEdges.Search();

		TArrayView<const FDirectionalEdge> FlowEdges = FlowGraph.GetEdges();
		for (int32 EdgeIndex = 0; EdgeIndex < FlowEdges.Num(); ++EdgeIndex)
		{
			FDirectionalEdge Edge = FlowEdges[EdgeIndex];
			const bool bIsCyclic = CyclicEdges.IsCyclic(EdgeIndex);

			ESystemPhase FromPhase = Nodes.Array[Edge.FromNode].System->GetPhase();
			ESystemPhase ToPhase   = Nodes.Array[Edge.ToNode].System->GetPhase();

			if (EnumHasAnyFlags(FromPhase, ESystemPhase::Spawn) && EnumHasAnyFlags(ToPhase, ESystemPhase::Spawn))
			{
				String += FString::Printf(TEXT("\tflow_node%d_0 -> flow_node%d_0 [color=\"%s\"];\n"), (int32)Edge.FromNode, (int32)Edge.ToNode, bIsCyclic ? TEXT("#FF0000") : TEXT("#39ad3b"));
			}
			if (EnumHasAnyFlags(FromPhase, ESystemPhase::Instantiation) && EnumHasAnyFlags(ToPhase, ESystemPhase::Instantiation))
			{
				String += FString::Printf(TEXT("\tflow_node%d_1 -> flow_node%d_1 [color=\"%s\"];\n"), (int32)Edge.FromNode, (int32)Edge.ToNode, bIsCyclic ? TEXT("#FF0000") : TEXT("#39ad3b"));
			}
			if (EnumHasAnyFlags(FromPhase, ESystemPhase::Evaluation) && EnumHasAnyFlags(ToPhase, ESystemPhase::Evaluation))
			{
				String += FString::Printf(TEXT("\tflow_node%d_2 -> flow_node%d_2 [color=\"%s\"];\n"), (int32)Edge.FromNode, (int32)Edge.ToNode, bIsCyclic ? TEXT("#FF0000") : TEXT("#39ad3b"));
			}
			if (EnumHasAnyFlags(FromPhase, ESystemPhase::Finalization) && EnumHasAnyFlags(ToPhase, ESystemPhase::Finalization))
			{
				String += FString::Printf(TEXT("\tflow_node%d_3 -> flow_node%d_3 [color=\"%s\"];\n"), (int32)Edge.FromNode, (int32)Edge.ToNode, bIsCyclic ? TEXT("#FF0000") : TEXT("#39ad3b"));
			}
		}
	}


	{
		FMovieSceneEntitySystemDirectedGraph::FDiscoverCyclicEdges CyclicEdges(&ReferenceGraph);
		CyclicEdges.Search();

		TArrayView<const FDirectionalEdge> ReferenceEdges = ReferenceGraph.GetEdges();
		for (int32 EdgeIndex = 0; EdgeIndex < ReferenceEdges.Num(); ++EdgeIndex)
		{
			FDirectionalEdge Edge = ReferenceEdges[EdgeIndex];
			const bool bIsCyclic = CyclicEdges.IsCyclic(EdgeIndex);

			String += FString::Printf(TEXT("\treference_node%d -> reference_node%d [color=\"%s\"];\n"), (int32)Edge.FromNode, (int32)Edge.ToNode, bIsCyclic ? TEXT("#FF0000") : TEXT("#3992ad"));
		}
	}

	String += TEXT("}");
	return String;
}

TArray<UMovieSceneEntitySystem*> FMovieSceneEntitySystemGraph::GetSystems() const
{
	TArray<UMovieSceneEntitySystem*> Systems;
	for (const FMovieSceneEntitySystemGraphNode& Node : Nodes.Array)
	{
		Systems.Add(Node.System);
	}
	return Systems;
}

UMovieSceneEntitySystem* FMovieSceneEntitySystemGraph::FindSystemOfType(TSubclassOf<UMovieSceneEntitySystem> InClassType) const
{
	UClass* ClassType = InClassType.Get();
	for (const FMovieSceneEntitySystemGraphNode& Node : Nodes.Array)
	{
		if (Node.System->GetClass() == ClassType)
		{
			return Node.System;
		}
	}
	return nullptr;
}

void FMovieSceneEntitySystemGraph::ExecutePhase(UE::MovieScene::ESystemPhase Phase, UMovieSceneEntitySystemLinker* Linker, FGraphEventArray& OutTasks)
{
	UpdateCache();

	switch (Phase)
	{
	case UE::MovieScene::ESystemPhase::Spawn:         ExecutePhase(SpawnPhase,         Linker, OutTasks); break;
	case UE::MovieScene::ESystemPhase::Instantiation: ExecutePhase(InstantiationPhase, Linker, OutTasks); break;
	case UE::MovieScene::ESystemPhase::Evaluation:    ExecutePhase(EvaluationPhase,    Linker, OutTasks); break;
	case UE::MovieScene::ESystemPhase::Finalization:  ExecutePhase(FinalizationPhase,  Linker, OutTasks); break;
	default: ensureMsgf(false, TEXT("Invalid phase specified for execution.")); break;
	}
}

void FMovieSceneEntitySystemGraph::IteratePhase(UE::MovieScene::ESystemPhase Phase, TFunctionRef<void(UMovieSceneEntitySystem*)> InIter)
{
	UpdateCache();

	TArrayView<const uint16> Array;
	switch (Phase)
	{
	case UE::MovieScene::ESystemPhase::Spawn:         Array = SpawnPhase;         break;
	case UE::MovieScene::ESystemPhase::Instantiation: Array = InstantiationPhase; break;
	case UE::MovieScene::ESystemPhase::Evaluation:    Array = EvaluationPhase;    break;
	case UE::MovieScene::ESystemPhase::Finalization:  Array = FinalizationPhase;  break;
	default: ensureMsgf(false, TEXT("Invalid phase specified for iteration."));   return;
	}

	for (uint16 NodeID : Array)
	{
		InIter(Nodes.Array[NodeID].System);
	}
}

template<typename ArrayType>
void FMovieSceneEntitySystemGraph::ExecutePhase(const ArrayType& SortedEntries, UMovieSceneEntitySystemLinker* Linker, FGraphEventArray& OutTasks)
{
	using namespace UE::MovieScene;

	FSystemSubsequentTasks DownstreamTasks(this, &OutTasks);

	FSystemTaskPrerequisites NoPrerequisites;

	const bool bStructureCanChange = !Linker->EntityManager.IsLockedDown();

	for (int32 CurrentIndex = 0; CurrentIndex < SortedEntries.Num(); ++CurrentIndex)
	{
		const uint16 NodeID = SortedEntries[CurrentIndex];

		UMovieSceneEntitySystem* System = Nodes.Array[NodeID].System;
		checkSlow(System);

		// Initilaize downstream task structure for this system
		DownstreamTasks.ResetNode(NodeID);

		TSharedPtr<FSystemTaskPrerequisites> Prerequisites = Nodes.Array[NodeID].Prerequisites;
		System->Run(Prerequisites ? *Prerequisites : NoPrerequisites, DownstreamTasks);

		if (bStructureCanChange)
		{
			Linker->AutoLinkRelevantSystems();
		}

		// If we linked any new systems, we may have to move our current offset
		if (SerialNumber != PreviousSerialNumber)
		{
#if DO_CHECK
			// Cache the systems we've already run
			TArray<uint16> HeadList(SortedEntries.GetData(), CurrentIndex+1);
#endif

			// This may actually change the SortedEntries array
			UpdateCache();

			CurrentIndex = Algo::IndexOf(SortedEntries, NodeID);
			checkf(CurrentIndex != INDEX_NONE, TEXT("System has removed itself while being Run. This is not supported."));

#if DO_CHECK
			for (int32 NewIndex = 0; NewIndex < CurrentIndex; ++NewIndex)
			{
				if (!HeadList.Contains(SortedEntries[NewIndex]))
				{
					const uint16 NewNodeIndex     = SortedEntries[NewIndex];
					const uint16 CurrentNodeIndex = SortedEntries[CurrentIndex];
					ensureAlwaysMsgf(false, 
						TEXT("System %s has been inserted upstream of %s in the same execution phase that is currently in-flight, and will not be run this frame. "
							 "This can be either because this system has been newly linked, or because it has been re-ordered due to other newly linked systems."),
						*this->Nodes.Array[NewNodeIndex].System->GetName(),
						*this->Nodes.Array[CurrentNodeIndex].System->GetName()
					);
				}
			}
#endif
		}

		// Don't need prerequisites now
		if (Prerequisites)
		{
			Prerequisites->Empty();
		}

		// Propagate subsequent tasks
		if (DownstreamTasks.Subsequents && DownstreamTasks.Subsequents->Num() != 0)
		{
			SCOPE_CYCLE_COUNTER(MovieSceneEval_SystemDependencyCost)

			for (FDirectionalEdge Edge : FlowGraph.GetEdgesFrom(NodeID))
			{
				FMovieSceneEntitySystemGraphNode& ToNode = Nodes.Array[Edge.ToNode];
				if (!ToNode.Prerequisites)
				{
					ToNode.Prerequisites = MakeShared<FSystemTaskPrerequisites>();
				}
				ToNode.Prerequisites->Consume(*DownstreamTasks.Subsequents);
			}
		}
	}
}

void FMovieSceneEntitySystemGraph::Shutdown()
{
	checkf(ReentrancyGuard == 0, TEXT("Attempting to shutdown the system graph while it is in use."));

	++ReentrancyGuard;

	for (int32 Index = 0; Index < Nodes.Array.GetMaxIndex(); ++Index)
	{
		if (Nodes.Array.IsAllocated(Index))
		{
			Nodes.Array[Index].System->Abandon();
		}
	}

	--ReentrancyGuard;

	*this = FMovieSceneEntitySystemGraph();
}

uint16 FMovieSceneEntitySystemGraph::GetGraphID(const UMovieSceneEntitySystem* InSystem)
{
	return InSystem->GetGraphID();
}
