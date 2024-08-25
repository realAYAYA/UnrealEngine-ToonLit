// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/GraphColoring.h"
#include "Chaos/Array.h"
#include "ChaosLog.h"
#include "Chaos/Framework/Parallel.h"
#include "Containers/BitArray.h"
#include "Chaos/SoftsSolverParticlesRange.h"

template<typename DynamicParticlesType>
static bool VerifyGraph(TArray<TArray<int32>> ColorGraph, const TArray<Chaos::TVec2<int32>>& Graph, const DynamicParticlesType& InParticles)
{
	for (int32 i = 0; i < ColorGraph.Num(); ++i)
	{
		TMap<int32, int32> NodeToColorMap;
		for (const auto& Edge : ColorGraph[i])
		{
			int32 Node1 = Graph[Edge][0];
			int32 Node2 = Graph[Edge][1];
			if (NodeToColorMap.Contains(Node1))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node1);
				return false;
			}
			if (NodeToColorMap.Contains(Node2))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node2);
				return false;
			}
			if (InParticles.InvM(Node1) != 0)
			{
				NodeToColorMap.Add(Node1, i);
			}
			if (InParticles.InvM(Node2) != 0)
			{
				NodeToColorMap.Add(Node2, i);
			}
		}
	}
	return true;
}

template<typename DynamicParticlesType>
static bool VerifyGraph(TArray<TArray<int32>> ColorGraph, const TArray<Chaos::TVec3<int32>>& Graph, const DynamicParticlesType& InParticles)
{
	for (int32 i = 0; i < ColorGraph.Num(); ++i)
	{
		TMap<int32, int32> NodeToColorMap;
		for (const auto& Edge : ColorGraph[i])
		{
			int32 Node1 = Graph[Edge][0];
			int32 Node2 = Graph[Edge][1];
			int32 Node3 = Graph[Edge][2];
			if (NodeToColorMap.Contains(Node1))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node1);
				return false;
			}
			if (NodeToColorMap.Contains(Node2))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node2);
				return false;
			}
			if (NodeToColorMap.Contains(Node3))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node3);
				return false;
			}
			if (InParticles.InvM(Node1) != 0)
			{
				NodeToColorMap.Add(Node1, i);
			}
			if (InParticles.InvM(Node2) != 0)
			{
				NodeToColorMap.Add(Node2, i);
			}
			if (InParticles.InvM(Node3) != 0)
			{
				NodeToColorMap.Add(Node3, i);
			}
		}
	}
	return true;
}

template<typename DynamicParticlesType>
static bool VerifyGraph(TArray<TArray<int32>> ColorGraph, const TArray<Chaos::TVec4<int32>>& Graph, const DynamicParticlesType& InParticles)
{	
	for (int32 i = 0; i < ColorGraph.Num(); ++i)
	{
		TMap<int32, int32> NodeToColorMap;
		for (const auto& Edge : ColorGraph[i])
		{
			int32 Node1 = Graph[Edge][0];
			int32 Node2 = Graph[Edge][1];
			int32 Node3 = Graph[Edge][2];
			int32 Node4 = Graph[Edge][3];
			if (NodeToColorMap.Contains(Node1))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node1);
				return false;
			}
			if (NodeToColorMap.Contains(Node2))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node2);
				return false;
			}
			if (NodeToColorMap.Contains(Node3))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node3);
				return false;
			}
			if (NodeToColorMap.Contains(Node4))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node4);
				return false;
			}
			if (InParticles.InvM(Node1) != 0)
			{
				NodeToColorMap.Add(Node1, i);
			}
			if (InParticles.InvM(Node2) != 0)
			{
				NodeToColorMap.Add(Node2, i);
			}
			if (InParticles.InvM(Node3) != 0)
			{
				NodeToColorMap.Add(Node3, i);
			}
			if (InParticles.InvM(Node4) != 0)
			{
				NodeToColorMap.Add(Node4, i);
			}
		}
	}
	return true;
}


template<typename DynamicParticlesType>
static bool VerifyGraphAllDynamic(TArray<TArray<int32>> ColorGraph, const TArray<Chaos::TVec4<int32>>& Graph, const DynamicParticlesType& InParticles)
{
	for (int32 i = 0; i < ColorGraph.Num(); ++i)
	{
		TMap<int32, int32> NodeToColorMap;
		for (const auto& Edge : ColorGraph[i])
		{
			int32 Node1 = Graph[Edge][0];
			int32 Node2 = Graph[Edge][1];
			int32 Node3 = Graph[Edge][2];
			int32 Node4 = Graph[Edge][3];
			if (NodeToColorMap.Contains(Node1))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node1);
				return false;
			}
			if (NodeToColorMap.Contains(Node2))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node2);
				return false;
			}
			if (NodeToColorMap.Contains(Node3))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node3);
				return false;
			}
			if (NodeToColorMap.Contains(Node4))
			{
				UE_LOG(LogChaos, Error, TEXT("Color %d has duplicate Node %d"), i, Node4);
				return false;
			}

			NodeToColorMap.Add(Node1, i);
			NodeToColorMap.Add(Node2, i);
			NodeToColorMap.Add(Node3, i);
			NodeToColorMap.Add(Node4, i);
			
		}
	}
	return true;
}

//just verify if different element in each subcolor has intersecting nodes
template <typename T>
static bool VerifyGridBasedSubColoring(const TArray<TArray<int32>>& ElementsPerColor, const Chaos::TMPMGrid<T>& Grid, const TArray<TArray<int32>>& ConstraintsNodesSet, const TArray<TArray<TArray<int32>>>& ElementsPerSubColors)
{
	for (int32 i = 0; i < ElementsPerSubColors.Num(); i++)
	{
		for (int32 j = 0; j < ElementsPerSubColors[i].Num(); j++)
		{
			TSet<int32> CoveredGridNodes;
			//first gather all Grid nodes of an element:
			for (int32 k = 0; k < ElementsPerSubColors[i][j].Num(); k++)
			{
				int32 e = ElementsPerSubColors[i][j][k];
				for (int32 Node:ConstraintsNodesSet[e])
				{
					if (CoveredGridNodes.Contains(Node))
					{
						return false;
					}
					CoveredGridNodes.Emplace(Node);
				}
			}
		}
	}
	return true;

}

template <typename T>
static bool VerifyWeakConstraintsColoring(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& ConstraintsPerColor)
{
	TArray<bool> ConstraintIsIncluded;
	ConstraintIsIncluded.Init(false, Indices.Num());
	for (int32 i = 0; i < ConstraintsPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ConstraintsPerColor[i].Num(); j++)
		{
			ConstraintIsIncluded[ConstraintsPerColor[i][j]] = true;
		}
	}

	for (int32 kk = 0; kk < Indices.Num(); kk++)
	{
		if (!ConstraintIsIncluded[kk])
		{
			return false;
		}
	}

	for (int32 i = 0; i < ConstraintsPerColor.Num(); i++)
	{
		TSet<int32> CoveredParticles;
		for (int32 j = 0; j < ConstraintsPerColor[i].Num(); j++)
		{
			for (int32 Node : Indices[ConstraintsPerColor[i][j]])
			{
				if (CoveredParticles.Contains(Node))
				{
					return false;
				}
				CoveredParticles.Emplace(Node);
			}

			if (SecondIndices.Num() > 0)
			{
				for (int32 Node : SecondIndices[ConstraintsPerColor[i][j]])
				{
					if (CoveredParticles.Contains(Node))
					{
						return false;
					}
					CoveredParticles.Emplace(Node);
				}

			}
		}
	}

	return true;

}

template <typename T>
static bool VerifyNodalColoring(const TArray<Chaos::TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TArray<TArray<int32>> ParticlesPerColor)
{
	TArray<bool> ParticleIsIncluded;
	ParticleIsIncluded.Init(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++) 
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++)
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0.)
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(-1, GraphParticlesEnd - GraphParticlesStart);
	for (int32 i = 0; i < IncidentElements.Num(); i++)
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]]] = i;
		}
	}

	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[Particle2Incident[ParticleIndex]].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[Particle2Incident[ParticleIndex]][k];
				for (int32 ie = 0; ie < 4; ie++)
				{
					LocalIncidentParticles.Emplace(Graph[ElementIndex][ie]);
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Emplace(LocalParticle);
				}
			}
		}
	}

	return true;

}

template <typename T>
static bool VerifyNodalColoring(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, const TArray<TArray<int32>> ParticlesPerColor)
{
 	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<bool> ParticleIsIncluded;
	ParticleIsIncluded.Init(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++)
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0. && IncidentElements[ParticleIndex].Num() > 0 )
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);
	for (int32 i = 0; i < IncidentElements.Num(); i++)
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]]] = i;
		}
	}

	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[Particle2Incident[ParticleIndex]].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[Particle2Incident[ParticleIndex]][k];
				for (int32 ie = 0; ie < Graph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(Graph[ElementIndex][ie]);
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Add(LocalParticle);
				}
			}
		}
	}

	return true;
}

template <typename T>
static bool VerifyExtraNodalColoring(const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements,const TArray<int32>& ParticleColors,const TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsIncluded(false, InParticles.Size());
	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			ParticleIsIncluded[ParticlesPerColor[i][j]] = true;
		}
	}

	for (int32 ParticleIndex = 0; ParticleIndex < (int32)InParticles.Size(); ParticleIndex++)
	{
		if (InParticles.InvM(ParticleIndex) != (T)0. && (IncidentElements[ParticleIndex].Num() > 0 || ExtraIncidentElements[ParticleIndex].Num() > 0))
		{
			if (!ParticleIsIncluded[ParticleIndex])
			{
				return false;
			}
		}
	}


	for (int32 i = 0; i < ParticlesPerColor.Num(); i++)
	{
		TSet<int32> IncidentParticles;
		IncidentParticles.Reserve(ParticlesPerColor[i].Num());
		for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
		{
			int32 ParticleIndex = ParticlesPerColor[i][j];
			TSet<int32> LocalIncidentParticles;
			for (int32 k = 0; k < IncidentElements[ParticleIndex].Num(); k++)
			{
				int32 ElementIndex = IncidentElements[ParticleIndex][k];
				for (int32 ie = 0; ie < Graph[ElementIndex].Num(); ie++)
				{
					LocalIncidentParticles.Add(Graph[ElementIndex][ie]);
				}
			}
			if (ParticleIndex < ExtraIncidentElements.Num())
			{
				for (int32 k = 0; k < ExtraIncidentElements[ParticleIndex].Num(); k++)
				{
					int32 ElementIndex = ExtraIncidentElements[ParticleIndex][k];
					for (int32 ie = 0; ie < ExtraGraph[ElementIndex].Num(); ie++)
					{
						LocalIncidentParticles.Add(ExtraGraph[ElementIndex][ie]);
					}
				}
			}
			if (IncidentParticles.Contains(ParticleIndex))
			{
				return false;
			}
			else
			{
				for (int32 LocalParticle : LocalIncidentParticles)
				{
					IncidentParticles.Add(LocalParticle);
				}
			}
		}
	}

	return true;
}

template<typename DynamicParticlesType>
TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVec2<int32>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
{
	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ColorGraph;
	TArray<FGraphNode> NodesSubArray;
	TArray<FGraphEdge> Edges;
	NodesSubArray.SetNum(GraphParticlesEnd - GraphParticlesStart);
	Edges.SetNum(Graph.Num());
	TArrayView<FGraphNode> Nodes(NodesSubArray.GetData() - GraphParticlesStart, GraphParticlesEnd); // Only nodes starting with GraphParticlesStart are valid to access

	int32 MaxColor = -1;

	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TVec2<int32>& Constraint = Graph[i];
		Edges[i].FirstNode = Constraint[0];
		Edges[i].SecondNode = Constraint[1];
		checkSlow(Constraint[0] >= GraphParticlesStart);
		checkSlow(Constraint[1] >= GraphParticlesStart);
		Nodes[Constraint[0]].Edges.Add(i);
		Nodes[Constraint[1]].Edges.Add(i);
	}

	TSet<int32> ProcessedNodes;
	TArray<int32> NodesToProcess;

	for (int32 ParticleNodeIndex = GraphParticlesStart; ParticleNodeIndex < GraphParticlesEnd; ++ParticleNodeIndex)
	{
		const bool bIsParticleDynamic = InParticles.InvM(ParticleNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
		if (ProcessedNodes.Contains(ParticleNodeIndex) || !bIsParticleDynamic)
		{
			continue;
		}

		NodesToProcess.Add(ParticleNodeIndex);

		while (NodesToProcess.Num())
		{
			const int32 NodeIndex = NodesToProcess.Last();
			FGraphNode& GraphNode = Nodes[NodeIndex];

			NodesToProcess.SetNum(NodesToProcess.Num() - 1, EAllowShrinking::No);
			ProcessedNodes.Add(NodeIndex);

			for (const int32 EdgeIndex : GraphNode.Edges)
			{
				FGraphEdge& GraphEdge = Edges[EdgeIndex];

				// If edge has been colored skip it
				if (GraphEdge.Color >= 0)
				{
					continue;
				}

				// Get index to the other node on the edge
				int32 OtherNodeIndex = INDEX_NONE;
				if (GraphEdge.FirstNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.SecondNode;
				}
				if (GraphEdge.SecondNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
				}

				// Find next color that is not used already at this node
				while (GraphNode.UsedColors.Contains(GraphNode.NextColor))
				{
					GraphNode.NextColor++;
				}
				int32 ColorToUse = GraphNode.NextColor;

				// Exclude colors used by the other node (but still allow this node to use them for other edges)
				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
				}

				// Assign color and set as used at this node
				MaxColor = FMath::Max(ColorToUse, MaxColor);
				GraphNode.UsedColors.Add(ColorToUse);
				GraphEdge.Color = ColorToUse;

				// Bump color to use next time, but only if we weren't forced to use a different color by the other node
				if ((ColorToUse == GraphNode.NextColor) && bIsParticleDynamic == true)
				{
					GraphNode.NextColor++;
				}

				if (ColorGraph.Num() <= MaxColor)
				{
					ColorGraph.SetNum(MaxColor + 1);
				}
				ColorGraph[GraphEdge.Color].Add(EdgeIndex);

				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex))
						{
							NodesToProcess.Add(OtherNodeIndex);
						}
					}
				}
			}
		}
	}

	checkSlow(VerifyGraph(ColorGraph, Graph, InParticles));
	return ColorGraph;
}

template<typename DynamicParticlesType>
TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<TVec3<int32>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ColorGraph;
	TArray<FGraphNode> NodesSubArray;
	TArray<FGraph3dEdge> Edges;
	NodesSubArray.SetNum(GraphParticlesEnd - GraphParticlesStart);
	Edges.SetNum(Graph.Num());
	TArrayView<FGraphNode> Nodes(NodesSubArray.GetData() - GraphParticlesStart, GraphParticlesEnd); // Only nodes starting with GraphParticlesStart are valid to access

	int32 MaxColor = -1;

	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TVec3<int32>& Constraint = Graph[i];
		Edges[i].FirstNode = Constraint[0];
		Edges[i].SecondNode = Constraint[1];
		Edges[i].ThirdNode = Constraint[2];
		checkSlow(Constraint[0] >= GraphParticlesStart);
		checkSlow(Constraint[1] >= GraphParticlesStart);
		checkSlow(Constraint[2] >= GraphParticlesStart);
		Nodes[Constraint[0]].Edges.Add(i);
		Nodes[Constraint[1]].Edges.Add(i);
		Nodes[Constraint[2]].Edges.Add(i);
	}

	TSet<int32> ProcessedNodes;
	TArray<int32> NodesToProcess;

	for (int32 ParticleNodeIndex = GraphParticlesStart; ParticleNodeIndex < GraphParticlesEnd; ++ParticleNodeIndex)
	{
		const bool bIsParticleDynamic = InParticles.InvM(ParticleNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
		if (ProcessedNodes.Contains(ParticleNodeIndex) || !bIsParticleDynamic)
		{
			continue;
		}

		NodesToProcess.Add(ParticleNodeIndex);

		while (NodesToProcess.Num())
		{
			const int32 NodeIndex = NodesToProcess.Last();
			FGraphNode& GraphNode = Nodes[NodeIndex];

			NodesToProcess.SetNum(NodesToProcess.Num() - 1, EAllowShrinking::No);
			ProcessedNodes.Add(NodeIndex);

			for (const int32 EdgeIndex : GraphNode.Edges)
			{
				FGraph3dEdge& GraphEdge = Edges[EdgeIndex];

				// If edge has been colored skip it
				if (GraphEdge.Color >= 0)
				{
					continue;
				}

				// Get index to the other node on the edge
				int32 OtherNodeIndex = INDEX_NONE;
				int32 OtherNodeIndex2 = INDEX_NONE;
				if (GraphEdge.FirstNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.SecondNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
				}
				if (GraphEdge.SecondNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
				}
				if (GraphEdge.ThirdNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.SecondNode;
				}

				// Find next color that is not used already at this node
				while (GraphNode.UsedColors.Contains(GraphNode.NextColor))
				{
					GraphNode.NextColor++;
				}
				int32 ColorToUse = GraphNode.NextColor;

				// Exclude colors used by the other node (but still allow this node to use them for other edges)
				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex2];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex2) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						if (OtherNodeIndex == INDEX_NONE)
						{
							while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
						else
						{
							FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
							while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
					}
				}

				// Assign color and set as used at this node
				MaxColor = FMath::Max(ColorToUse, MaxColor);
				GraphNode.UsedColors.Add(ColorToUse);
				GraphEdge.Color = ColorToUse;

				// Bump color to use next time, but only if we weren't forced to use a different color by the other node
				if ((ColorToUse == GraphNode.NextColor) && bIsParticleDynamic == true)
				{
					GraphNode.NextColor++;
				}

				if (ColorGraph.Num() <= MaxColor)
				{
					ColorGraph.SetNum(MaxColor + 1);
				}
				ColorGraph[GraphEdge.Color].Add(EdgeIndex);

				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex))
						{
							NodesToProcess.Add(OtherNodeIndex);
						}
					}
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex2];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex2) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex2))
						{
							NodesToProcess.Add(OtherNodeIndex2);
						}
					}
				}
			}
		}
	}

	checkSlow(VerifyGraph(ColorGraph, Graph, InParticles));
	return ColorGraph;
}

template<typename DynamicParticlesType>
TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<TVec4<int32>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ColorGraph;
	TArray<FGraphNode> NodesSubArray;
	TArray<FGraphTetEdge> Edges;
	NodesSubArray.SetNum(GraphParticlesEnd - GraphParticlesStart);
	Edges.SetNum(Graph.Num());
	TArrayView<FGraphNode> Nodes(NodesSubArray.GetData() - GraphParticlesStart, GraphParticlesEnd); // Only nodes starting with GraphParticlesStart are valid to access

	int32 MaxColor = -1;

	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TVec4<int32>& Constraint = Graph[i];
		Edges[i].FirstNode = Constraint[0];
		Edges[i].SecondNode = Constraint[1];
		Edges[i].ThirdNode = Constraint[2];
		Edges[i].FourthNode = Constraint[3];
		checkSlow(Constraint[0] >= GraphParticlesStart);
		checkSlow(Constraint[1] >= GraphParticlesStart);
		checkSlow(Constraint[2] >= GraphParticlesStart);
		checkSlow(Constraint[3] >= GraphParticlesStart);
		Nodes[Constraint[0]].Edges.Add(i);
		Nodes[Constraint[1]].Edges.Add(i);
		Nodes[Constraint[2]].Edges.Add(i);
		Nodes[Constraint[3]].Edges.Add(i);
	}

	TSet<int32> ProcessedNodes;
	TArray<int32> NodesToProcess;

	for (int32 ParticleNodeIndex = GraphParticlesStart; ParticleNodeIndex < GraphParticlesEnd; ++ParticleNodeIndex)
	{
		const bool bIsParticleDynamic = InParticles.InvM(ParticleNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
		if (ProcessedNodes.Contains(ParticleNodeIndex) || !bIsParticleDynamic)
		{
			continue;
		}

		NodesToProcess.Add(ParticleNodeIndex);

		while (NodesToProcess.Num())
		{
			const int32 NodeIndex = NodesToProcess.Last();
			FGraphNode& GraphNode = Nodes[NodeIndex];

			NodesToProcess.SetNum(NodesToProcess.Num() - 1, EAllowShrinking::No);
			ProcessedNodes.Add(NodeIndex);

			for (const int32 EdgeIndex : GraphNode.Edges)
			{
				FGraphTetEdge& GraphEdge = Edges[EdgeIndex];

				// If edge has been colored skip it
				if (GraphEdge.Color >= 0)
				{
					continue;
				}

				// Get index to the other node on the edge
				int32 OtherNodeIndex = INDEX_NONE;
				int32 OtherNodeIndex2 = INDEX_NONE;
				int32 OtherNodeIndex3 = INDEX_NONE;
				if (GraphEdge.FirstNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.SecondNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.SecondNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.ThirdNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.SecondNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.FourthNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.SecondNode;
					OtherNodeIndex3 = GraphEdge.ThirdNode;
				}

				// Find next color that is not used already at this node
				while (GraphNode.UsedColors.Contains(GraphNode.NextColor))
				{
					GraphNode.NextColor++;
				}
				int32 ColorToUse = GraphNode.NextColor;

				// Exclude colors used by the other node (but still allow this node to use them for other edges)
				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex2];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex2) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						if (OtherNodeIndex == INDEX_NONE)
						{
							while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
						else
						{
							FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
							while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
					}
				}
				if (OtherNodeIndex3 != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex3];

					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex3) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						if (OtherNodeIndex == INDEX_NONE && OtherNodeIndex2 == INDEX_NONE)
						{
							while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
						else if (OtherNodeIndex == INDEX_NONE)
						{
							//OtherNodeIndex2 != INDEX_NONE
							FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex2];
							while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
						else if (OtherNodeIndex2 == INDEX_NONE)
						{
							FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
							while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
						else
						{
							FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
							FGraphNode& PrevPrevOtherNode = Nodes[OtherNodeIndex2];
							while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || PrevPrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
							{
								ColorToUse++;
							}
						}
					}
				}

				// Assign color and set as used at this node
				MaxColor = FMath::Max(ColorToUse, MaxColor);
				GraphNode.UsedColors.Add(ColorToUse);
				GraphEdge.Color = ColorToUse;

				// Bump color to use next time, but only if we weren't forced to use a different color by the other node
				if ((ColorToUse == GraphNode.NextColor) && bIsParticleDynamic == true)
				{
					GraphNode.NextColor++;
				}

				if (ColorGraph.Num() <= MaxColor)
				{
					ColorGraph.SetNum(MaxColor + 1);
				}
				ColorGraph[GraphEdge.Color].Add(EdgeIndex);

				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex))
						{
							NodesToProcess.Add(OtherNodeIndex);
						}
					}
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex2];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex2) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex2))
						{
							NodesToProcess.Add(OtherNodeIndex2);
						}
					}
				}
				if (OtherNodeIndex3 != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex3];
					const bool bIsOtherGraphNodeDynamic = InParticles.InvM(OtherNodeIndex3) != (decltype(InParticles.InvM(ParticleNodeIndex)))0.;
					if (bIsOtherGraphNodeDynamic)
					{
						// Mark other node as not allowing use of this color
						if (bIsParticleDynamic)
						{
							OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						}

						// Queue other node for processing
						if (!ProcessedNodes.Contains(OtherNodeIndex3))
						{
							NodesToProcess.Add(OtherNodeIndex3);
						}
					}
				}
			}
		}
	}

	checkSlow(VerifyGraph(ColorGraph, Graph, InParticles)); 
	return ColorGraph;
}

template<typename DynamicParticlesType>
TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<TVec4<int32>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ColorGraph;
	TArray<FGraphNode> NodesSubArray;
	TArray<FGraphTetEdge> Edges;
	NodesSubArray.SetNum(GraphParticlesEnd - GraphParticlesStart);
	Edges.SetNum(Graph.Num());
	TArrayView<FGraphNode> Nodes(NodesSubArray.GetData() - GraphParticlesStart, GraphParticlesEnd); // Only nodes starting with GraphParticlesStart are valid to access

	int32 MaxColor = -1;

	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		const TVec4<int32>& Constraint = Graph[i];
		Edges[i].FirstNode = Constraint[0];
		Edges[i].SecondNode = Constraint[1];
		Edges[i].ThirdNode = Constraint[2];
		Edges[i].FourthNode = Constraint[3];
		checkSlow(Constraint[0] >= GraphParticlesStart);
		checkSlow(Constraint[1] >= GraphParticlesStart);
		checkSlow(Constraint[2] >= GraphParticlesStart);
		checkSlow(Constraint[3] >= GraphParticlesStart);
		Nodes[Constraint[0]].Edges.Add(i);
		Nodes[Constraint[1]].Edges.Add(i);
		Nodes[Constraint[2]].Edges.Add(i);
		Nodes[Constraint[3]].Edges.Add(i);
	}

	TSet<int32> ProcessedNodes;
	TArray<int32> NodesToProcess;

	for (int32 ParticleNodeIndex = GraphParticlesStart; ParticleNodeIndex < GraphParticlesEnd; ++ParticleNodeIndex)
	{
		//const bool bIsParticleDynamic = InParticles.InvM(ParticleNodeIndex) != (T)0.;
		if (ProcessedNodes.Contains(ParticleNodeIndex))
		{
			continue;
		}

		NodesToProcess.Add(ParticleNodeIndex);

		while (NodesToProcess.Num())
		{
			const int32 NodeIndex = NodesToProcess.Last();
			FGraphNode& GraphNode = Nodes[NodeIndex];

			NodesToProcess.SetNum(NodesToProcess.Num() - 1, EAllowShrinking::No);
			ProcessedNodes.Add(NodeIndex);

			for (const int32 EdgeIndex : GraphNode.Edges)
			{
				FGraphTetEdge& GraphEdge = Edges[EdgeIndex];

				// If edge has been colored skip it
				if (GraphEdge.Color >= 0)
				{
					continue;
				}

				// Get index to the other node on the edge
				int32 OtherNodeIndex = INDEX_NONE;
				int32 OtherNodeIndex2 = INDEX_NONE;
				int32 OtherNodeIndex3 = INDEX_NONE;
				if (GraphEdge.FirstNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.SecondNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.SecondNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.ThirdNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.ThirdNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.SecondNode;
					OtherNodeIndex3 = GraphEdge.FourthNode;
				}
				if (GraphEdge.FourthNode == NodeIndex)
				{
					OtherNodeIndex = GraphEdge.FirstNode;
					OtherNodeIndex2 = GraphEdge.SecondNode;
					OtherNodeIndex3 = GraphEdge.ThirdNode;
				}

				// Find next color that is not used already at this node
				while (GraphNode.UsedColors.Contains(GraphNode.NextColor))
				{
					GraphNode.NextColor++;
				}
				int32 ColorToUse = GraphNode.NextColor;

				// Exclude colors used by the other node (but still allow this node to use them for other edges)
				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex];

					while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
					{
						ColorToUse++;
					}
					
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex2];
					if (OtherNodeIndex == INDEX_NONE)
					{
						while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					else
					{
						FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
						while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					
				}
				if (OtherNodeIndex3 != INDEX_NONE)
				{
					FGraphNode& OtherNode = Nodes[OtherNodeIndex3];

					if (OtherNodeIndex == INDEX_NONE && OtherNodeIndex2 == INDEX_NONE)
					{
						while (OtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					else if (OtherNodeIndex == INDEX_NONE)
					{
						//OtherNodeIndex2 != INDEX_NONE
						FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex2];
						while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					else if (OtherNodeIndex2 == INDEX_NONE)
					{
						FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
						while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					else
					{
						FGraphNode& PrevOtherNode = Nodes[OtherNodeIndex];
						FGraphNode& PrevPrevOtherNode = Nodes[OtherNodeIndex2];
						while (OtherNode.UsedColors.Contains(ColorToUse) || PrevOtherNode.UsedColors.Contains(ColorToUse) || PrevPrevOtherNode.UsedColors.Contains(ColorToUse) || GraphNode.UsedColors.Contains(ColorToUse))
						{
							ColorToUse++;
						}
					}
					
				}

				// Assign color and set as used at this node
				MaxColor = FMath::Max(ColorToUse, MaxColor);
				GraphNode.UsedColors.Add(ColorToUse);
				GraphEdge.Color = ColorToUse;

				// Bump color to use next time, but only if we weren't forced to use a different color by the other node
				if (ColorToUse == GraphNode.NextColor) 
				{
					GraphNode.NextColor++;
				}

				if (ColorGraph.Num() <= MaxColor)
				{
					ColorGraph.SetNum(MaxColor + 1);
				}
				ColorGraph[GraphEdge.Color].Add(EdgeIndex);

				if (OtherNodeIndex != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex];

					// Mark other node as not allowing use of this color
					OtherGraphNode.UsedColors.Add(GraphEdge.Color);
					
					// Queue other node for processing
					if (!ProcessedNodes.Contains(OtherNodeIndex))
					{
						NodesToProcess.Add(OtherNodeIndex);
					}
					
				}
				if (OtherNodeIndex2 != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex2];

					// Mark other node as not allowing use of this color
					OtherGraphNode.UsedColors.Add(GraphEdge.Color);
					
					// Queue other node for processing
					if (!ProcessedNodes.Contains(OtherNodeIndex2))
					{
						NodesToProcess.Add(OtherNodeIndex2);
					}
					
				}
				if (OtherNodeIndex3 != INDEX_NONE)
				{
					FGraphNode& OtherGraphNode = Nodes[OtherNodeIndex3];
					// Mark other node as not allowing use of this color
					OtherGraphNode.UsedColors.Add(GraphEdge.Color);
						
					// Queue other node for processing
					if (!ProcessedNodes.Contains(OtherNodeIndex3))
					{
						NodesToProcess.Add(OtherNodeIndex3);
					}
					
				}
			}
		}
	}

	checkSlow(VerifyGraphAllDynamic(ColorGraph, Graph, InParticles));
	return ColorGraph;
}


template<typename T>
void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<T>& Grid, const int32 GridSize, TArray<TArray<int32>>*& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors) 
{
	bool InitialGuess = true;
	if (!PreviousColoring) {
		InitialGuess = false;
		PreviousColoring = new TArray<TArray<int32>>();
		PreviousColoring->SetNum(ElementsPerColor.Num());
	}
	ElementsPerSubColors.SetNum(ElementsPerColor.Num());

	//for (int32 i = 0; i < ElementsPerColor.Num(); i++) {
	PhysicsParallelFor(ElementsPerColor.Num(), [&](const int32 i)
		{
			if (!InitialGuess) {
				PreviousColoring->operator[](i).Init(-1, ElementsPerColor[i].Num());
			}
			int32 NumNodes = GridSize;
			TArray<int32> ElementSubColors;
			ElementSubColors.Init(-1, ElementsPerColor[i].Num());
			TArray<TSet<int32>*> UsedColors;
			UsedColors.Init(nullptr, NumNodes);
			for (int32 j = 0; j < ElementsPerColor[i].Num(); j++) {
				int32 ColorToUse = 0;
				int32 e = ElementsPerColor[i][j];
				// check initial guess:
				if (InitialGuess) {
					ColorToUse = PreviousColoring->operator[](i)[j];
					bool ColorFound = false;
					for (auto node : ConstraintsNodesSet[e]) {
						if (!UsedColors[node]) {
							UsedColors[node] = new TSet<int32>();
						}
						if (UsedColors[node]->Contains(ColorToUse)) {
							ColorFound = true;
							break;
						}
					}
					if (ColorFound) {
						ColorToUse = 0;
					}
					else {
						for (auto node : ConstraintsNodesSet[e]) {
							UsedColors[node]->Emplace(ColorToUse);
						}
						ElementSubColors[j] = ColorToUse;
					}
				}
				if (ElementSubColors[j] == -1) {
					while (true) {
						bool ColorFound = false;
						for (auto node : ConstraintsNodesSet[e]) {
							if (!UsedColors[node]) {
								UsedColors[node] = new TSet<int32>();// new std::unordered_set<int>;
							}
							if (UsedColors[node]->Contains(ColorToUse)) {
								ColorFound = true;
								break;
							}
						}
						if (!ColorFound) {
							break;
						}
						ColorToUse++;
					}
					ElementSubColors[j] = ColorToUse;
					for (auto node : ConstraintsNodesSet[e]) {
						UsedColors[node]->Emplace(ColorToUse);
					}
				}

				// assign colors to previous guess for next timestep:
				PreviousColoring->operator[](i)[j] = ColorToUse;
			}

			for (int ii = 0; ii < UsedColors.Num(); ii++) {
				delete (UsedColors[ii]);
			}

			int32 NumColors = FMath::Max<int32>(ElementSubColors);

			//int32 num_colors = *std::max_element(ElementSubColors.begin(), ElementSubColors.end());
			ElementsPerSubColors[i].SetNum(0);
			ElementsPerSubColors[i].SetNum(NumColors + 1);

			for (int32 j = 0; j < ElementsPerColor[i].Num(); j++) {
				ElementsPerSubColors[i][ElementSubColors[j]].Emplace(ElementsPerColor[i][j]);
			}
		}, ElementsPerColor.Num() < 20);
	
	checkSlow(VerifyGridBasedSubColoring<T>(ElementsPerColor, Grid, ConstraintsNodesSet, ElementsPerSubColors));
}


template<typename T>
void Chaos::ComputeWeakConstraintsColoring(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<T, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor)
{
	TArray<TSet<int32>*> UsedColors;
	UsedColors.Init(nullptr, InParticles.Size());

	ensure(Indices.Num() == SecondIndices.Num() || SecondIndices.Num() == 0);

	TArray<int32> ConstraintColors;
	ConstraintColors.Init(-1, Indices.Num());

	if (SecondIndices.Num() == 0)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < Indices.Num(); ConstraintIndex++)
		{
			for (int32 i = 0; i < Indices[ConstraintIndex].Num(); i++)
			{
				if (!UsedColors[Indices[ConstraintIndex][i]]) {
					UsedColors[Indices[ConstraintIndex][i]] = new TSet<int32>();
				}
				
			}
			int32 ColorToUse = 0;
			while (true)
			{
				bool ColorFound = false;
				for (auto node : Indices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) {
					break;
				}
				ColorToUse++;
			}
			ConstraintColors[ConstraintIndex] = ColorToUse;
		}
	}
	else 
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < Indices.Num(); ConstraintIndex++)
		{
			for (int32 i = 0; i < Indices[ConstraintIndex].Num(); i++)
			{
				if (!UsedColors[Indices[ConstraintIndex][i]]) {
					UsedColors[Indices[ConstraintIndex][i]] = new TSet<int32>();
				}
			}
			for (int32 j = 0; j < SecondIndices[ConstraintIndex].Num(); j++)
			{
				if (!UsedColors[SecondIndices[ConstraintIndex][j]]) {
					UsedColors[SecondIndices[ConstraintIndex][j]] = new TSet<int32>();
				}
			}
			int32 ColorToUse = 0;
			while (true)
			{
				bool ColorFound = false;
				for (auto node : Indices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				for (auto node : SecondIndices[ConstraintIndex]) {
					if (!UsedColors[node]) {
						UsedColors[node] = new TSet<int32>();
					}
					if (UsedColors[node]->Contains(ColorToUse)) {
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) {
					break;
				}
				ColorToUse++;
			}
			ConstraintColors[ConstraintIndex] = ColorToUse;
			for (auto node : Indices[ConstraintIndex]) {
				UsedColors[node]->Emplace(ColorToUse);
			}
			for (auto node : SecondIndices[ConstraintIndex]) {
				UsedColors[node]->Emplace(ColorToUse);
			}
		}
	}

	for (int ii = 0; ii < UsedColors.Num(); ii++) {
		delete (UsedColors[ii]);
	}

	int32 NumColors = FMath::Max<int32>(ConstraintColors);

	//int32 num_colors = *std::max_element(ElementSubColors.begin(), ElementSubColors.end());
	ConstraintsPerColor.Empty();
	ConstraintsPerColor.SetNum(NumColors + 1);

	for (int32 j = 0; j < Indices.Num(); j++) 
	{
		ConstraintsPerColor[ConstraintColors[j]].Emplace(j);
	}

	checkSlow(VerifyWeakConstraintsColoring<T>(Indices, SecondIndices, InParticles, ConstraintsPerColor));
}


template<typename T>
TArray<TArray<int32>> Chaos::ComputeNodalColoring(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ParticlesPerColor;

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(-1, GraphParticlesEnd - GraphParticlesStart);
	//Assuming that offset of Graph is GraphParticlesStart
	for (int32 i = 0; i < IncidentElements.Num(); i++) 
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]] - GraphParticlesStart] = i;
		}
	}

	TArray<TSet<int32>*> ElementColorsSet;
	ElementColorsSet.Init(nullptr, Graph.Num());
	TArray<int32> ParticleColors;
	ParticleColors.Init(-1, GraphParticlesEnd - GraphParticlesStart);

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++) 
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0. && Particle2Incident[ParticleIndex] != -1) 
		{
			for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
			{
				int32 LocalIndex = IncidentElementsLocalIndex[Particle2Incident[ParticleIndex]][j];
				int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
				if (!ElementColorsSet[e]) 
				{
					ElementColorsSet[e] = new TSet<int32>();
				}
			}
			int32 color_to_use = 0;
			while (true) 
			{
				bool ColorFound = false;
				for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
				{
					int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
					if (ElementColorsSet[e]->Contains(color_to_use)) 
					{
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) 
				{
					ParticleColors[p] = color_to_use;
					for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
					{
						int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
						ElementColorsSet[e]->Emplace(color_to_use);
						if (ElementColorsSet[e]->Num() == 4) 
						{
							delete ElementColorsSet[e];
							ElementColorsSet[e] = nullptr;
						}
					}
					break;
				}
				color_to_use++;
			}
		}
	}

	for (int32 ii = 0; ii < ElementColorsSet.Num(); ii++) 
	{
		if (ElementColorsSet[ii]) 
		{
			delete (ElementColorsSet[ii]);
		}
	}

	int32 SizeColors = FMath::Max<int32>(ParticleColors);

	ParticlesPerColor.SetNum(0);
	ParticlesPerColor.Init(TArray<int32>(), SizeColors + 1);

	for (int32 j = 0; j < ParticleColors.Num(); j++) 
	{
		if (ParticleColors[j] != -1) 
		{
			ParticlesPerColor[ParticleColors[j]].Emplace(j + GraphParticlesStart);
		}
	}

	checkSlow(VerifyNodalColoring<T>(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd, IncidentElements, IncidentElementsLocalIndex, ParticlesPerColor));

	return ParticlesPerColor;
}


template<typename T>
TArray<TArray<int32>> Chaos::ComputeNodalColoring(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, TArray<int32>* ParticleColorsOut)
{
	using namespace Chaos;

	checkSlow(GraphParticlesStart <= GraphParticlesEnd);
	checkSlow(GraphParticlesEnd <= (int32)InParticles.Size());
	TArray<TArray<int32>> ParticlesPerColor;

	TArray<int32> Particle2Incident;
	Particle2Incident.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);
	//Assuming that offset of Graph is GraphParticlesStart
	for (int32 i = 0; i < IncidentElements.Num(); i++) 
	{
		if (IncidentElements[i].Num() > 0)
		{
			Particle2Incident[Graph[IncidentElements[i][0]][IncidentElementsLocalIndex[i][0]] - GraphParticlesStart] = i;
		}
	}

	TArray<TSet<int32>*> ElementColorsSet;
	ElementColorsSet.Init(nullptr, Graph.Num());
	TArray<int32> ParticleColors;
	ParticleColors.Init(INDEX_NONE, GraphParticlesEnd - GraphParticlesStart);

	for (int32 p = 0; p < GraphParticlesEnd - GraphParticlesStart; p++) 
	{
		int32 ParticleIndex = p + GraphParticlesStart;
		if (InParticles.InvM(ParticleIndex) != (T)0. && Particle2Incident[ParticleIndex] != INDEX_NONE) 
		{
			for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
			{
				int32 LocalIndex = IncidentElementsLocalIndex[Particle2Incident[ParticleIndex]][j];
				int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
				if (!ElementColorsSet[e]) 
				{
					ElementColorsSet[e] = new TSet<int32>();
				}
			}
			int32 color_to_use = 0;
			while (true) 
			{
				bool ColorFound = false;
				for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
				{
					int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
					if (ElementColorsSet[e]->Contains(color_to_use)) 
					{
						ColorFound = true;
						break;
					}
				}
				if (!ColorFound) 
				{
					ParticleColors[p] = color_to_use;
					for (int32 j = 0; j < IncidentElements[Particle2Incident[ParticleIndex]].Num(); j++) 
					{
						int32 e = IncidentElements[Particle2Incident[ParticleIndex]][j];
						ElementColorsSet[e]->Emplace(color_to_use);
						if (ElementColorsSet[e]->Num() == Graph[e].Num())
						{
							delete ElementColorsSet[e];
							ElementColorsSet[e] = nullptr;
						}
					}
					break;
				}
				color_to_use++;
			}
		}
	}

	for (int32 ii = 0; ii < ElementColorsSet.Num(); ii++) 
	{
		if (ElementColorsSet[ii]) 
		{
			delete (ElementColorsSet[ii]);
		}
	}

	int32 SizeColors = FMath::Max<int32>(ParticleColors);

	ParticlesPerColor.SetNum(0);
	ParticlesPerColor.Init(TArray<int32>(), SizeColors + 1);

	for (int32 j = 0; j < ParticleColors.Num(); j++) 
	{
		if (ParticleColors[j] != INDEX_NONE) 
		{
			ParticlesPerColor[ParticleColors[j]].Emplace(j + GraphParticlesStart);
		}
	}

	if (ParticleColorsOut)
	{
		*ParticleColorsOut = ParticleColors;
	}

	checkSlow(VerifyNodalColoring<T>(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd, IncidentElements, IncidentElementsLocalIndex, ParticlesPerColor));

	return ParticlesPerColor;
}


template<typename T>
void Chaos::ComputeExtraNodalColoring(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor)
{
	TBitArray ParticleIsAffected(false, IncidentElements.Num());
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ExtraIncidentElements[i].Num() > 0)
		{
			ParticleIsAffected[i] = true;
		}
	}

	TSet<int32> UsedColors;
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i])
		{
			int32 OriginalColor = ParticleColors[i];
			ParticleColors[i] = INDEX_NONE;
			if (OriginalColor != INDEX_NONE)
			{
				UsedColors.Reset();
				UsedColors.Reserve(ExtraIncidentElements[i].Num() + IncidentElements[i].Num());
				for (int32 j = 0; j < IncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = IncidentElements[i][j];
					for (int32 ie = 0; ie < Graph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[Graph[ConstraintIndex][ie]]);
					}
				}
				for (int32 j = 0; j < ExtraIncidentElements[i].Num(); j++)
				{
					int32 ConstraintIndex = ExtraIncidentElements[i][j];
					for (int32 ie = 0; ie < ExtraGraph[ConstraintIndex].Num(); ie++)
					{
						UsedColors.Add(ParticleColors[ExtraGraph[ConstraintIndex][ie]]);
					}
				}
				if (UsedColors.Contains(OriginalColor))
				{
					OriginalColor = 0;
					while (UsedColors.Contains(OriginalColor))
					{
						OriginalColor++;
					}
					ParticleColors[i] = OriginalColor;
				}
				else
				{
					ParticleColors[i] = OriginalColor;
					ParticleIsAffected[i] = false;
				}
			}
		}
	}


	PhysicsParallelFor(ParticlesPerColor.Num(), [&](const int32 i)
		{
			int32 CurrentIndex = 0;
			for (int32 j = 0; j < ParticlesPerColor[i].Num(); j++)
			{
				if (!ParticleIsAffected[ParticlesPerColor[i][j]])
				{
					if (CurrentIndex != j)
					{
						ParticlesPerColor[i][CurrentIndex] = ParticlesPerColor[i][j];
					}
					CurrentIndex += 1;
				}
			}
			ParticlesPerColor[i].SetNum(CurrentIndex);
		}, ParticlesPerColor[0].Num() < 1000);

	ParticlesPerColor.SetNum(ParticleColors.Max() + 1);
	for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
	{
		if (ParticleIsAffected[i] && ParticleColors[i] != INDEX_NONE)
		{
			ParticlesPerColor[ParticleColors[i]].Emplace(i);
		}
	}

	checkSlow(VerifyExtraNodalColoring<T>(InParticles, Graph, ExtraGraph, IncidentElements, ExtraIncidentElements, ParticleColors, ParticlesPerColor));
}




template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 2>>&, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 2>>&, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 2>>&, const Chaos::Softs::FSolverParticles&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 2>>&, const Chaos::Softs::FSolverParticlesRange&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 3>>&, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 3>>&, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 3>>&, const Chaos::Softs::FSolverParticles&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 3>>&, const Chaos::Softs::FSolverParticlesRange&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::Softs::FSolverParticles&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::Softs::FSolverParticlesRange&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::Softs::FSolverParticles&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API TArray<TArray<int32>> Chaos::FGraphColoring::ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<Chaos::TVector<int32, 4>>&, const Chaos::Softs::FSolverParticlesRange&, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
template CHAOS_API void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<Chaos::FRealSingle>& Grid, const int32 GridSize, TArray<TArray<int32>>*& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);
template CHAOS_API void Chaos::ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<Chaos::FRealDouble>& Grid, const int32 GridSize, TArray<TArray<int32>>*& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);
template CHAOS_API void Chaos::ComputeWeakConstraintsColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor);
template CHAOS_API void Chaos::ComputeWeakConstraintsColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealSingle>(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealDouble>(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, TArray<int32>* ParticleColorsOut);
template CHAOS_API TArray<TArray<int32>> Chaos::ComputeNodalColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, TArray<int32>* ParticleColorsOut);
template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealSingle>(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealSingle, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);
template CHAOS_API void Chaos::ComputeExtraNodalColoring<Chaos::FRealDouble>(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<Chaos::FRealDouble, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);