// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"

#include "Chaos/Island/IslandManager.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDNullConstraints.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/Utilities.h"
#include "Modules/ModuleManager.h"

namespace ChaosTest
{
	using namespace Chaos;

	class FGraphEvolutionTest
	{
	public:
		FGraphEvolutionTest(const int32 NumParticles)
			: UniqueIndices()
			, Particles(UniqueIndices)
			, PhysicalMaterials()
			, Evolution(Particles, PhysicalMaterials)
			, Constraints()
			, TickCount(0)
		{
			IslandManager = &Evolution.GetConstraintGraph();

			// Bind the constraints to the evolution
			Evolution.AddConstraintContainer(Constraints);

			Evolution.GetGravityForces().SetAcceleration(FVec3(0));

			// Create some particles and constraints connecting them in a chain: 0-1-2-3-...-N
			ParticleHandles = Evolution.CreateDynamicParticles(NumParticles);

			for (FGeometryParticleHandle* ParticleHandle : ParticleHandles)
			{
				Evolution.EnableParticle(ParticleHandle);
			}
		}

		// Connect all the particles in a chain
		void MakeChain()
		{
			for (int32 ParticleIndex = 0; ParticleIndex < ParticleHandles.Num() - 1; ++ParticleIndex)
			{
				ConstraintHandles.Add(Constraints.AddConstraint({ ParticleHandles[ParticleIndex], ParticleHandles[ParticleIndex + 1] }));
			}
		}

		// Treat particle0 like a kinematic floor with all the other particles sat on it
		void MakeFloor()
		{
			Evolution.SetParticleObjectState(ParticleHandles[0], EObjectStateType::Kinematic);

			for (int32 ParticleIndex = 0; ParticleIndex < ParticleHandles.Num() - 1; ++ParticleIndex)
			{
				ConstraintHandles.Add(Constraints.AddConstraint({ ParticleHandles[0], ParticleHandles[ParticleIndex + 1] }));
			}
		}

		void Advance()
		{
			Evolution.AdvanceOneTimeStep(FReal(1.0 / 60.0));
			++TickCount;
		}

		void AdvanceUntilSleeping()
		{
			const int32 MaxIterations = 50;
			const int32 MaxTickCount = TickCount + MaxIterations;
			bool bIsSleeping = false;
			while (!bIsSleeping && (TickCount < MaxTickCount))
			{
				Advance();

				bIsSleeping = true;
				for (int32 IslandIndex = 0; IslandIndex < IslandManager->NumIslands(); ++IslandIndex)
				{
					bIsSleeping = bIsSleeping && IslandManager->GetIsland(IslandIndex)->IsSleeping();
				}
			}

			EXPECT_TRUE(bIsSleeping);
			EXPECT_LT(TickCount, MaxTickCount);
		}

		FParticleUniqueIndicesMultithreaded UniqueIndices;
		FPBDRigidsSOAs Particles;
		THandleArray<FChaosPhysicsMaterial> PhysicalMaterials;
		FPBDRigidsEvolutionGBF Evolution;

		FPBDNullConstraints Constraints;
		TArray<FPBDRigidParticleHandle*> ParticleHandles;
		TArray<FPBDNullConstraintHandle*> ConstraintHandles;
		FPBDIslandManager* IslandManager;

		int32 TickCount;
	};

	// Veryify that the Null Constraint mockup is working as intended. We can create the container and constraints, and they are correctly bound to the evolution
	GTEST_TEST(GraphEvolutionTests, TestNullConstraint)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
	}

	// Start with an island containing 4 particle connected in a chain, then make the second one kinematic.
	// Check that the island splits.
	//		A-B-C-D 
	// =>	A-B, B-C-D
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ToKinematic)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);

		// Convert particle B to kinematic to split the islands: A-B, B-C-D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);

		Test.Advance();

		// Should now have 2 islands
		EXPECT_EQ(Test.IslandManager->NumIslands(), 2);
	}

	// Wait for all particles to go to sleep naturally (i.e., as part of the tick and not by explicitly setting the state)
	// then check that the islands are preserved.
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ParticleSleep_Natural)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Make all the particles sleep
		Test.AdvanceUntilSleeping();

		// Island should be asleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
	}

	// Sleep all particles in the scene and ensure that the island manager puts the island to sleep
	// but retains all the constraints and particles in the island.
	// 
	// @todo(chaos): this test fails. Manually setting all particles in an island to the sleep state
	// does put the island to sleep, but only after all the constraints have been removed because
	// InitializeGraph is called before the sleep state is updated.
	//
	GTEST_TEST(GraphEvolutionTests, DISABLED_TestConstraintGraph_ParticleSleep_Manual)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Make all the particles sleep
		for (FPBDRigidParticleHandle* ParticleHandle : Test.ParticleHandles)
		{
			Test.Evolution.SetParticleObjectState(ParticleHandle, EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Island should be asleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
	}

	// Start with an island containing 4 particle connected in a chain, then make the middle two kinematic.
	// This makes the B-C constraint kinematic ("invalid" in the Constraint Graph terminolgy) which means
	// it does not belong in any island.
	// Check that the island manager handles kinematic-kinematic constraints
	//		A-B-C-D 
	// =>	A-B  C-D
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicKinematic)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);

		// Convert a particle B and C to kinematic to split the islands: A-B, C-D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[2], EObjectStateType::Kinematic);

		Test.Advance();

		// Constraint[1] is not in an island, but is still in the graph (we have not explicitly removed it)
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);				// Still in graph
		EXPECT_EQ(Test.IslandManager->GetConstraintIsland(Test.ConstraintHandles[1]), INDEX_NONE);	// Not in an island
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);

		// Should now have 2 islands
		EXPECT_EQ(Test.IslandManager->NumIslands(), 2);
	}


	// Same as TestConstraintGraph_KinematicKinematic but islands are sleeping when the change is made.
	// Sleeping islands are not cleared of constraints, but they should be awoken by the dynamic->kinematic conversion
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicKinematic_Sleeping)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Wait for the sleep state
		Test.AdvanceUntilSleeping();

		// Island should be asleep but still contain all the particles and constraints
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);

		// Convert a particle B and C to kinematic to split the islands: A-B, C-D
		// NOTE: the island won't split until the island is woken
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[2], EObjectStateType::Kinematic);

		Test.Advance();

		// Island is asleep and won't have split yet
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);

		// The kinematic-kinematic constraint will still be in the graph, and still in its island (it will not be removed until the island wakes)
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.IslandManager->GetConstraintIsland(Test.ConstraintHandles[1]), INDEX_NONE);

		// Wake a dynamic particle the island and we should get the split at the next update
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// Should now have 2 islands (with 2 particles and 1 constraint each though we can't check that atm)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 2);

		// The kinematic-kinematic constraint will still be in the graph, but not in any island
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_EQ(Test.IslandManager->GetConstraintIsland(Test.ConstraintHandles[1]), INDEX_NONE);
	}

	// 3 objects sat on the floor awake. Make the floor dynamic.
	// This tests what happens when a kinematic in multiple islands gets converted to a dynamic.
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	// Bd   Cd   Dd          Bd   Cd   Dd
	//  \   |   /	   =>     \   |   /
	//      Ak		              Ad
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.Advance();

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic which should merge all the islands into 1
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// All particles in same island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);
	}

	// 3 objects sat on the floor asleep. Make the floor dynamic.
	// This tests what happens when a kinematic in multiple sleeping islands gets converted to a dynamic.
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	// Bs   Cs   Ds          Bd   Cd   Dd
	//  \   |   /	   =>     \   |   /
	//      Ak		              Ad
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic_WithSleep)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.AdvanceUntilSleeping();

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic which should merge all the islands into 1
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// All particles in one awake island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// All particles awake
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
	}

	// 3 Objects sat on the floor asleep. Make the floor dynamic and asleep.
	// This tests that adding a sleeping body to an island does not wake it.
	// This is required for streaming to work which adds bodies over multiple frames.
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	// Bs   Cs   Ds          Bs   Cs   Ds
	//  \   |   /      =>     \   |   /
	//      Ak                    As
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic_WithSleep2)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.AdvanceUntilSleeping();

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping which should merge all the islands into 1 but leave it asleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one asleep island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// All particles asleep
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());

	}

	// 3 Objects sat on the floor, 2 asleep and 1 awake. Make the floor dynamic and asleep.
	// In this case we should get 1 awake island and all particles should wake.
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	// Bs   Cs   Dd          Bd   Cd   Dd
	//  \   |   /      =>     \   |   /
	//      Ak	                  Ad
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic_WithSleep3)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.AdvanceUntilSleeping();

		// Wake D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[3], EObjectStateType::Dynamic);

		Test.Advance();

		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one awake island (D was awake so it would wake the island)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// All particles awake
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
	}

	// 3 Objects sat on the floor, 2 asleep and 1 awake. Make the floor dynamic and asleep.
	// Same as TestConstraintGraph_KinematicToDynamic_WithSleep3 except we wake a different particle to 
	// be sure we weren't just lucky above (when we make a kinematic into a dynamic we add it to one
	// of the islands it is in. This is testing that this is ok).
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	// Bd   Cs   Ds          Bd   Cd   Dd
	//  \   |   /      =>     \   |   /
	//      Ak	                  Ad
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic_WithSleep4)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.AdvanceUntilSleeping();

		// Wake B
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Dynamic);

		Test.Advance();

		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one awake island (B was awake so it would wake the island, even though A was set to sleeping)
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// All particles awake
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
	}

	// Start with an island containing 4 sleeping particles connected in a chain. Then wake the island
	// using the WakeIsland method.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		As - Bs - Cs - Ds  =>	Ad - Bd - Cd - Dd
	//
	// @todo(chaos): Explicit waking of islands is not currently supported
	GTEST_TEST(GraphEvolutionTests, DISABLED_TestConstraintGraph_WakeIsland)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.AdvanceUntilSleeping();

		// All particles asleep
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);

		// Flag the island for wake up
		if (Test.IslandManager->NumIslands() == 1)
		{
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
			EXPECT_TRUE(false);	// Should be calling WakeIsland here
			//Test.IslandManager->WakeIsland(Test.Particles, 0);
		}

		Test.Advance();

		// Island and all particles should now be awake
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		if (Test.IslandManager->NumIslands() == 1)
		{
			EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
		}
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
	}

	// Start with an island containing 4 awake particles connected in a chain. Then sleep the island
	// using the SleepIsland method.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd - Cd - Dd  =>	As - Bs - Cs - Ds
	//
	// @todo(chaos): this test fails. Explicit call to SleepIsland do not work atm
	GTEST_TEST(GraphEvolutionTests, DISABLED_TestConstraintGraph_SleepIsland)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All particles awake
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);

		// Flag the island for sleep
		if (Test.IslandManager->NumIslands() == 1)
		{
			EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
			Test.IslandManager->SleepIsland(Test.Particles, 0);
		}

		Test.Advance();

		// Island and all particles should now be asleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		if (Test.IslandManager->NumIslands() == 1)
		{
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		}
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
	}

	// Add a constraint between a sleeping and a kinematic body and tick.
	// Nothing should change.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak - Bs    =>    Ak - Bs
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingKinematicConstraint)
	{
		FGraphEvolutionTest Test(2);

		// Make A kinematic, B sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		// Add a constraint A-B
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// Everything asleep
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// Add a constraint between a sleeping and a kinematic body, one tick after the bodies were added.
	// 
	// This differs from TestConstraintGraph_SleepingKinematicConstraint in that we tick the scene one
	// time before adding the constraint, which means the particles are already in separate islands.
	// Nothing should wake and the constraint should be flagged as sleeping.
	// 
	// This behaviour is required for streaming to work since scene creation may
	// be amortized over multiple frames and constraints may be made betweens
	// sleeping particles in a later tick.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak  Bs    =>    Ak - Bs
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingKinematicConstraint2)
	{
		FGraphEvolutionTest Test(2);

		// Make A kinematic, B sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		Test.Advance();
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());

		// Add a constraint A-B
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// B still asleep
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// Add a constraint between two sleeping particles.
	// Nothing should wake and the constraint should be flagged as sleeping.
	// This behaviour is required for streaming to work since scene creation may
	// be amortized over multiple frames and constraints may be made betweens
	// sleeping particles in a later tick.
	// In this case, A and B start in different sleeping island and get merged into
	// a single still-sleeping island.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		As  Bs    =>    As - Bs
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint)
	{
		FGraphEvolutionTest Test(2);

		// Make A and B sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		Test.Advance();
		EXPECT_EQ(Test.IslandManager->NumIslands(), 2);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());

		// Add a constraint A-B
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// A and B still asleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// Similar to TestConstraintGraph_SleepingKinematicConstraint, but we are adding a constraint 
	// between sleeping and kinematic particles that are already in an existing sleeping island
	// with multiple sleeping constraints.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak - Bs - Cs  =>    Ak - Bs - Cs
	//		                     ^--------^
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingKinematicConstraint_SameIsland)
	{
		FGraphEvolutionTest Test(3);

		// Chains the particles and make the first one kinematix
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		Test.MakeChain();

		// Wait for sleep
		Test.AdvanceUntilSleeping();
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());

		// Add a constraint A - C
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[2] }));

		Test.Advance();

		// All still asleep, including the new constraint
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsSleeping());
	}

	// Similar to TestConstraintGraph_SleepingKinematicConstraint, but we are adding a constraint 
	// between two sleeping particles in different island, but where each island already contains
	// sleeping constraints.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		As - Bs   Cs - Ds  =>    As - Bs - Cs - Ds
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint_MergeIslands)
	{
		FGraphEvolutionTest Test(4);

		// Add constraints A-B and C-D
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[2], Test.ParticleHandles[3] }));

		// Wait for sleep
		Test.AdvanceUntilSleeping();
		EXPECT_EQ(Test.IslandManager->NumIslands(), 2);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());

		// Add a constraint B - C
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[1], Test.ParticleHandles[2] }));

		Test.Advance();

		// All still asleep, including the new constraint
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsSleeping());
	}

	// Add constraints beween objects on the tick where their island goes to sleep, and one either side just to be sure.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad   Bd  =>    As - Bs
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint_Timing)
	{
		// Count how many frames it takes the simulation to sleep
		FGraphEvolutionTest SleepTest(2);
		SleepTest.AdvanceUntilSleeping();
		const int32 TicksToSleep = SleepTest.TickCount;

		// Create a new simulation up to the sleep tick +/- a tick
		// Verify that adding a constraint on that tick leaves the scene as expected
		for (int32 SleepRelativeTickCount = -1; SleepRelativeTickCount < 2; ++SleepRelativeTickCount)
		{
			FGraphEvolutionTest Test(2);
			for (int32 Frame = 0; Frame < TicksToSleep + SleepRelativeTickCount; ++Frame)
			{
				Test.Advance();
			}
			const bool bExpectSleep = (SleepRelativeTickCount >= 0);

			// Should have 2 islands in a state that depends on whether we hit the sleep tick yet
			EXPECT_EQ(Test.IslandManager->NumIslands(), 2);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping(), bExpectSleep);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping(), bExpectSleep);
			EXPECT_EQ(Test.IslandManager->GetIsland(0)->IsSleeping(), bExpectSleep);
			EXPECT_EQ(Test.IslandManager->GetIsland(1)->IsSleeping(), bExpectSleep);

			// Add a constraint A-B and tick
			Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));
			Test.Advance();

			// Should now have 1 island and it should be asleep
			EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

			// Constraint should also be asleep
			EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		}
	}

	// Test an edge case bug that is probably easy to accidentally reintroduce. This would leave
	// a dangling pointer in the constraint graph due to a collision constraint being deleted while
	// in a sleeping island.
	// 
	// The fix was to ensure that we build the Island particle and constraint lists for islands that
	// have just been put to sleep (we still don't bother for thoise that were already asleep) so
	// that we can visit all the particles and constraints to set the sleep state.
	// 
	// 1: A dynamic particle is in its own awake island
	// - Tick
	// 2a: The particle is manually put to sleep
	// 2b: A constraint is added between the particle and a kinematic
	// - Tick
	// During the graph update on this tick, the particle's island is put to sleep in UpdateGraph
	// because all particles in it are asleep. However, the constraint was added this tick as well,
	// but when it was added the island was awake, so the constraint starts in the awake state.
	// 
	// Verify that the constraint does actually get put to sleep at some point in the graph update.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak   Bd  =>    As - Bs
	//
	// NOTE: the transition to sleep is by a user call, not the automatic sleep-when-not-moving system
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint_Timing2)
	{
		FGraphEvolutionTest Test(2);

		// Make A kinematic, B dynamic
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Dynamic);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Explicitly put B to sleep (its island will still be considered awake until the tick)
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		// Add a constraint A-B. B is asleep, but its island is awake
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// Everything should be asleep
		// The bug was that the constraint was still flagged as awake, but in a sleeping island.
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// This is a very similar test to TestConstraintGraph_SleepingSleepingConstraint_Timing2
	// in that is exposes the same bug where an island that is implicitly put to sleep because
	// all its particles were explicitly put to sleep did not put its constraints to sleep.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad -  Bd  =>    As - Bs
	// 
	// NOTE: the transition to sleep is by a user call, not the automatic sleep-when-not-moving system
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint_Timing3)
	{
		FGraphEvolutionTest Test(2);

		// Make A, B dynamic
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Dynamic);

		// Add a constraint A-B
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Explicitly put both particles (and therefore their island) to sleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		Test.Advance();

		// Everything should be asleep
		// The bug was that the constraint was still flagged as awake, but in a sleeping island.
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// Test the conditions for a kinematic particle waking an island
	// If a kinematic is being animated by velocity or by setting a target
	// position the island should wake but only if the target velocity is
	// non-zero or the target transform is different from the identity
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicWakeIslandConditions)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		// Set the root of the chain to be kinematic and the rest to be sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Expect one sleeping island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphIndex(), INDEX_NONE);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set to velocity mode and animate
		FKinematicGeometryParticleHandle* KinematicParticle = Test.ParticleHandles[0]->CastToKinematicParticle();
		ASSERT_NE(KinematicParticle, nullptr);
		KinematicParticle->KinematicTarget().SetVelocityMode();

		Test.Advance();

		// Expect one sleeping island as the velocity of the kinematic particle is still zero
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		KinematicParticle->SetV(FVec3(10.0f, 0.0f, 0.0f));

		Test.Advance();

		// Expect one awake island
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Put all particles back to sleep and now set angular velocity
		KinematicParticle->SetV(FVec3(0.0f, 0.0f, 0.0f));
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Check we've put the island back to sleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Now set angular velocity. Island should wake
		KinematicParticle->SetW(FVec3(0.0f, 1.0f, 0.0f));

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Put all particles back to sleep
		KinematicParticle->SetW(FVec3(0.0f, 0.0f, 0.0f));
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Check we've put the island back to sleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Now set to position mode. Initially the island should stay sleeping as the target
		// transform is the identity
		FKinematicTarget KinematicTarget;
		KinematicTarget.SetTargetMode(FRigidTransform3::Identity);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set a non-zero position target. Should cause the island to wake
		KinematicTarget.SetTargetMode(FVec3(10.0f, 0.0f, 0.0f), FRotation3::Identity);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Put all particles back to sleep
		KinematicTarget.SetTargetMode(FRigidTransform3::Identity);
		KinematicParticle->SetKinematicTarget(KinematicTarget);
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Check we've put the island back to sleep
		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set a non-identity rotation target. Should cause the island to wake
		FRigidTransform3 TargetTransform(FVec3(0.0f, 0.0f, 0.0f), FQuat::MakeFromEuler(FVec3(1.0f, 0.0f, 2.0f)));
		KinematicTarget.SetTargetMode(TargetTransform);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->NumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

	}
}
