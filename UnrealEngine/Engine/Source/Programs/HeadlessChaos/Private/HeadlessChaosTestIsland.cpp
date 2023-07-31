// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "Chaos/Island/IslandGraph.h"
#include "Chaos/Utilities.h"
#include "ChaosLog.h"
#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Modules/ModuleManager.h"

namespace ChaosTest 
{
	/** The goal here is to test if all the islands are well built based on the constraint connectivity */
	TEST(IslandTests,TestMergeIslands)
	{
		static constexpr int32 NumParticles = 21;
		using IslandGraphType = Chaos::FIslandGraph<int32, int32, int32, TNullIslandGraphOwner<int32, int32>>;

		IslandGraphType IslandGraph;

		// Reserve and add nodes to the graph
		IslandGraph.ReserveNodes(NumParticles);
		for (int32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
		{
			IslandGraph.AddNode(ParticleIndex);
		}

		TArray<TVec2<int32>> ConstrainedParticles0 =
		{
			//
			{0, 1},
			{0, 2},
			{0, 3},
			{3, 4},
			{3, 5},
			{6, 4},
			//
			{8, 7},
			{8, 9},
			//
			{13, 18},
			//
			{20, 17},
		};

		TArray<TVec2<int32>> ConstrainedParticles1 =
		{
			//
			{0, 1},
			{2, 1},
			//
			{9, 10},
			{11, 10},
			{11, 13},
			//
			{14, 15},
			{16, 14},
			{17, 14},
		};
		int32 ConstraintIndex = 0;

		// Reserve and add edges to the graph
		IslandGraph.ReserveEdges(ConstrainedParticles0.Num() + ConstrainedParticles1.Num());
		for (const auto& ConstrainedParticles : ConstrainedParticles0)
		{
			IslandGraph.AddEdge(ConstraintIndex++, 0, ConstrainedParticles[0], ConstrainedParticles[1]);
		}
		for (const auto& ConstrainedParticles : ConstrainedParticles1)
		{
			IslandGraph.AddEdge(ConstraintIndex++, 0, ConstrainedParticles[0], ConstrainedParticles[1]);
		}

		TArray<TSet<int32>> IslandNodes = 
		{
			{0, 1, 2, 3, 4, 5, 6},
			{7, 8, 9, 10, 11, 13, 18},
			{12},
			{14, 15, 16, 17, 20},
			{19},
		};

		// Update the graph islands based on the constrainmts connectivity
		IslandGraph.UpdateGraph();
		EXPECT_EQ(IslandGraph.GraphIslands.Num(), IslandNodes.Num());

		for (int32 IslandIndex = 0; IslandIndex < IslandGraph.GraphIslands.Num(); ++IslandIndex)
		{
			// // Check if the number of nodes per islands is right
			// TArray<int32> LocalNodes;
			// IslandGraph.ForEachNodes(IslandIndex, [IslandIndex,&LocalNodes](const IslandGraphType::FGraphNode& GraphNode) {
			// 	UE_LOG(LogHeadlessChaos, Log, TEXT("Island : %d | Node : %d"), IslandIndex, GraphNode.NodeItem); LocalNodes.Add(GraphNode.NodeItem);});
			// EXPECT_EQ(LocalNodes.Num(), IslandNodes[IslandIndex].Num());
			//
			// // Check if the nodes indices are correcty
			// LocalNodes.Sort();
			// int32 NodeIndex = 0;
			// for (const auto& GraphNode : IslandNodes[IslandIndex])
			// {
			// 	EXPECT_EQ(GraphNode, LocalNodes[NodeIndex]);
			// 	++NodeIndex;
			// }
		}
	}
}

