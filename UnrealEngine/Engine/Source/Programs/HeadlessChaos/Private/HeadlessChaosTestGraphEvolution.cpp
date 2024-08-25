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
			IslandManager = &Evolution.GetIslandManager();

			// Bind the constraints to the evolution
			Evolution.AddConstraintContainer(Constraints);

			Evolution.GetGravityForces().SetAcceleration(FVec3(0), 0);

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
				for (FPBDRigidParticleHandle* ParticleHandle : ParticleHandles)
				{
					if (ParticleHandle->IsDynamic() && !ParticleHandle->IsSleeping())
					{
						bIsSleeping = false;
					}
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
		Private::FPBDIslandManager* IslandManager;

		int32 TickCount;
	};

	// Veryify that the Null Constraint mockup is working as intended. We can create the container and constraints, and they are correctly bound to the evolution
	GTEST_TEST(GraphEvolutionTests, TestNullConstraint)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
	}

	// Start with a kinematic connected to a dynamic. Verify that removing the
	// constraint removes both particles.
	// This version explicitly removes the constraint from the graph.
	// 
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak - Bd
	// =>	{}
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicDynamic_Remove)
	{
		FGraphEvolutionTest Test(2);
		Test.MakeChain();

		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);

		Test.Advance();

		// Should have 1 island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		// Disable the constraint and remove it from the graph
		Test.ConstraintHandles[0]->SetEnabled(false);
		Test.IslandManager->RemoveConstraint(Test.ConstraintHandles[0]);

		Test.Advance();

		// Should have no islands and all particles should have been removed
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_FALSE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsInConstraintGraph());
	}

	// Start with a kinematic connected to a dynamic. Verify that removing the
	// constraint removes both particles.
	// This version has the constraint removed by making all particles kinematic
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak - Bd
	// =>	Ak - Bk
	// =>	{}
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicDynamic_Remove2)
	{
		FGraphEvolutionTest Test(2);
		Test.MakeChain();

		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);

		Test.Advance();

		// Should have 1 island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		// Make the other particle kinematic
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);

		Test.Advance();

		// Should have no islands and all particles should have been removed
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_FALSE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsInConstraintGraph());
	}

	// Start with a kinematic connected to a sleeping dynamic. Verify that removing the
	// kinematic removes both particles from the graph (because we do not keep islands 
	// unless there are constraints in them), but the dynamic particle is now awake.
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ak - Bs
	// =>	{} (B is dynamic/awake but the graph is empty)
	//
	// This tests a bug where we were not waking a particle if we removed all other particles 
	// from its island. We now defer island destruction to after sleep handling.
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicDynamic_RemoveKinematic)
	{
		FGraphEvolutionTest Test(2);
		Test.MakeChain();

		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);

		Test.AdvanceUntilSleeping();

		// Should have 1 island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		// B should be asleep
		EXPECT_TRUE(Test.ParticleHandles[1]->IsSleeping());

		// Remove A
		Test.Evolution.DisableParticle(Test.ParticleHandles[0]);

		// A and the constraint should have been removed from the graph
		EXPECT_FALSE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		// B will be removed from the graph because we do not track islands without
		// constraints, but not until the next tick. For now it will still be in
		// the graph and still asleep.
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsSleeping());

		// Tick physics. This will update the graph, waking B's island and therefore B.
		// B's island will then be destroyed because it has no constraints.
		Test.Advance();

		// Graph should be empty
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_FALSE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		
		// B should be awake
		EXPECT_FALSE(Test.ParticleHandles[1]->IsSleeping());
	}

	// Start with an island containing 4 particle connected in a chain, then make the second one kinematic.
	// Check that the island splits.
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd - Cd - Dd 
	// =>	Ad - Bk   Bk - Cd - Dd
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ToKinematic)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);

		// Convert particle B to kinematic to split the islands: A-B, B-C-D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);

		Test.Advance();

		// Should now have 2 islands
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);

		// A should be in its own island
		EXPECT_EQ(Test.IslandManager->GetParticleIsland(Test.ParticleHandles[0])->GetNumParticles(), 1);

		// C and D should be in same island
		EXPECT_EQ(Test.IslandManager->GetParticleIsland(Test.ParticleHandles[2]), Test.IslandManager->GetParticleIsland(Test.ParticleHandles[3]));

		// B should be in 2 islands
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 2);
	}

	// Start with an island containing 2 particle connected in a chain, then invalidate one of the particles.
	// Check that the particles and constraints are removed from the graph and then re-added at the next tick.
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd
	// =>	Ad   Bd
	// =>	Ad - Bd
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_Invalidate)
	{
		FGraphEvolutionTest Test(2);
		Test.MakeChain();

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		// Invalidate B
		Test.Evolution.InvalidateParticle(Test.ParticleHandles[1]);

		// Constraint was kicked from the graph, but particles remain until
		// explicitly removed or they have no constraint on next update
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsInConstraintGraph());

		Test.Advance();

		// Everything was added back to the graph
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
	}

	// An isolated stationary particle with no gravity should go to sleep
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ParticleSleep_Isolated)
	{
		FGraphEvolutionTest Test(1);
		Test.Evolution.GetGravityForces().SetAcceleration(FVec3(0), 0);

		// Make all the particles sleep
		Test.AdvanceUntilSleeping();

		// Particle should be asleep and it should have taken 21 ticks (ChaosSolverCollisionDefaultSleepCounterThreshold)
		EXPECT_TRUE(Test.ParticleHandles[0]->IsSleeping());
		EXPECT_EQ(Test.TickCount, 21);
	}

	// Wait for all particles to go to sleep naturally (i.e., as part of the tick and not by explicitly setting the state)
	// then check that the islands are preserved.
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ParticleSleep_Natural)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Make all the particles sleep
		Test.AdvanceUntilSleeping();

		// Island should be asleep
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
	}

	// Sleep all particles in the scene and ensure that the island manager puts the island to sleep
	// but retains all the constraints and particles in the island.
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_ParticleSleep_Manual)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Make all the particles sleep
		for (FPBDRigidParticleHandle* ParticleHandle : Test.ParticleHandles)
		{
			Test.Evolution.SetParticleObjectState(ParticleHandle, EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Island should be asleep
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
	}

	// Start with an island containing 4 particle connected in a chain, then make the middle two kinematic.
	// This makes the B-C constraint kinematic which means it does not belong in any island and is kicked 
	// out of the graph (the edge is deleted).
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		// Convert a particle B and C to kinematic to split the islands: A-B, C-D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[2], EObjectStateType::Kinematic);

		Test.Advance();

		// Constraint[1] was kicked from the graph
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[1]->IsInConstraintGraph());	// Not in graph
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		// Should now have 2 islands
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
	}


	// Same as TestConstraintGraph_KinematicKinematic but islands are sleeping when the change is made.
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	//		As - Bs - Cs - Ds
	// =>	As - Bk   Ck - Ds
	// =>	Ad - Bk   Ck - Ds
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicKinematic_Sleeping)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All constraints in graph in 1 island that is awake
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		// Wait for the sleep state
		Test.AdvanceUntilSleeping();

		// Island should be asleep but still contain all the particles and constraints
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		// Convert a particle B and C to kinematic to split the islands: A-B, C-D
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[2], EObjectStateType::Kinematic);

		Test.Advance();

		// The kinematic-kinematic constraint was kicked from graph
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		// Island will have split into two sleeping islands
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(Test.IslandManager->GetIsland(1)->IsSleeping());

		// Wake a dynamic particle
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// Should now have 2 islands and only one awake particle
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
		EXPECT_FALSE(Test.ParticleHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ParticleHandles[3]->IsSleeping());
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic which should merge all the islands into 1
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// All particles in same island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicToDynamic_WithSleep1)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeFloor();

		Test.AdvanceUntilSleeping();

		// Each particle in its own island (kinematic will be in all 3)
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic which should merge all the islands into 1
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Dynamic);

		Test.Advance();

		// All particles in one awake island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping which should merge all the islands into 1 but leave it asleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one asleep island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one awake island (D was awake so it would wake the island)
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[0]).Num(), 3);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[1]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[2]).Num(), 1);
		EXPECT_EQ(Test.IslandManager->FindParticleIslands(Test.ParticleHandles[3]).Num(), 1);

		// Convert A to dynamic sleeping
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);

		Test.Advance();

		// All particles in one awake island (B was awake so it would wake the island, even though A was set to sleeping)
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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

	// Start with an island containing 4 awake particles connected in a chain. Then sleep the island
	// by explicitly putting all particles to sleep
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd - Cd - Dd  =>	As - Bs - Cs - Ds
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepIsland)
	{
		FGraphEvolutionTest Test(4);
		Test.MakeChain();

		Test.Advance();

		// All particles and costraints are awake
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_FALSE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_FALSE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_FALSE(Test.ConstraintHandles[1]->IsSleeping());
		EXPECT_FALSE(Test.ConstraintHandles[2]->IsSleeping());
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);

		// Put all of the particles to sleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[2], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[3], EObjectStateType::Sleeping);

		Test.Advance();

		// Island and all particles and constraints should now be asleep
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		if (Test.IslandManager->GetNumIslands() == 1)
		{
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		}
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());

		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsInConstraintGraph());

		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsSleeping());
	}

	// @todo(chaos): Implement this. TestConstraintGraph_SleepIsland is intended to reproduce a bug 
	// exp[osed by collision constraints where collisions were being destroyed on particles that were 
	// explicitly put to sleep. However that bug was a result of how collision constraints are 
	// destroyed (i.e., when they are not updated this tick) and the NullConstraints don't have that same 
	// behaviour. We need a unit testing constraint that can reproduce that behaviour. 
	GTEST_TEST(GraphEvolutionTests, DISABLED_TestConstraintGraph_SleepIsland_Collisions)
	{
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
	// In this case, A and B start sleeping and get merged into a single 
	// still-sleeping island when we add the constraint.
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

		// Particles without any constraints are not in the graph
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());

		// Add a constraint A-B
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// A and B still asleep but now in an island with the constraint
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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

		// Chains the particles and make the first one kinematic
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Kinematic);
		Test.MakeChain();

		// Wait for sleep
		Test.AdvanceUntilSleeping();
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());

		// Add a constraint A - C
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[2] }));

		Test.Advance();

		// All still asleep, including the new constraint
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());
		EXPECT_TRUE(Test.ConstraintHandles[2]->IsSleeping());
	}

	// Add constraints beween objects on the tick where their island goes to sleep, and one either side just to be sure.
	// i.e., The SleepCounter does not get reset when we add a constraint between two particles.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad   Bd  =>    As - Bs
	//
	// @todo(chaos): this test would fail because we do not transfer isolated particle
	// sleep counts to the graph when we add a constraint to them. We could fix this
	// but probably not worth worrying about
	//
	GTEST_TEST(GraphEvolutionTests, DISABLED_TestConstraintGraph_SleepingSleepingConstraint_Timing_Isolated)
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

			// Should have no islands (because we have no constraints)
			EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping(), bExpectSleep);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping(), bExpectSleep);

			// Add a constraint A-B and tick
			Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));
			Test.Advance();

			// Should now have 1 island and it should be asleep
			EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

			// Constraint should also be asleep
			EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
		}
	}

	// Add constraints beween objects on the tick where their island goes to sleep, and one either side just to be sure.
	// i.e., The SleepCounter does not get reset when we add a constraint between two particles.
	//
	//		(d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd   Cd - Dd  =>    As - Bs - Cs - Ds
	//
	// NOTE: this one works where DISABLED_TestConstraintGraph_SleepingSleepingConstraint_Timing_Isolated
	// would fail because we retain SleepCounter when merging islands (but not when adding isloated
	// particles that have their own sleep counter)
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_SleepingSleepingConstraint_Timing)
	{
		// Count how many frames it takes the simulation to sleep
		int32 TicksToSleep = 0;
		{
			FGraphEvolutionTest SleepTest(4);
			SleepTest.ConstraintHandles.Add(SleepTest.Constraints.AddConstraint({ SleepTest.ParticleHandles[0], SleepTest.ParticleHandles[1] }));
			SleepTest.ConstraintHandles.Add(SleepTest.Constraints.AddConstraint({ SleepTest.ParticleHandles[2], SleepTest.ParticleHandles[3] }));

			SleepTest.AdvanceUntilSleeping();
			TicksToSleep = SleepTest.TickCount;
		}

		// Create a new simulation up to the sleep tick +/- a tick
		// Verify that adding a constraint on that tick leaves the scene as expected
		for (int32 SleepRelativeTickCount = -1; SleepRelativeTickCount < 2; ++SleepRelativeTickCount)
		{
			FGraphEvolutionTest Test(4);
			Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));
			Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[2], Test.ParticleHandles[3] }));

			for (int32 Frame = 0; Frame < TicksToSleep + SleepRelativeTickCount; ++Frame)
			{
				Test.Advance();
			}
			const bool bExpectSleep = (SleepRelativeTickCount >= 0);

			// Should have no islands (because we have no constraints)
			EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping(), bExpectSleep);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping(), bExpectSleep);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping(), bExpectSleep);
			EXPECT_EQ(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping(), bExpectSleep);

			// Add a constraint B-C and tick
			Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[1], Test.ParticleHandles[2] }));
			Test.Advance();

			// Should now have 1 island and it should be asleep
			EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
			EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[2])->Sleeping());
			EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[3])->Sleeping());

			// Constraints should also be asleep
			EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
			EXPECT_TRUE(Test.ConstraintHandles[1]->IsSleeping());
			EXPECT_TRUE(Test.ConstraintHandles[2]->IsSleeping());
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

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);

		// Explicitly put B to sleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		// Add a constraint A-B. B is asleep
		Test.ConstraintHandles.Add(Test.Constraints.AddConstraint({ Test.ParticleHandles[0], Test.ParticleHandles[1] }));

		Test.Advance();

		// Everything should be asleep
		// The bug was that the constraint was still flagged as awake, but in a sleeping island.
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Explicitly put both particles (and therefore their island) to sleep
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[0], EObjectStateType::Sleeping);
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Sleeping);

		Test.Advance();

		// Everything should be asleep
		// The bug was that the constraint was still flagged as awake, but in a sleeping island.
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[0])->Sleeping());
		EXPECT_TRUE(FConstGenericParticleHandle(Test.ParticleHandles[1])->Sleeping());
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsSleeping());
	}

	// Test isolated particles are not present in the graph
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_KinematicRemoveFromGraph)
	{
		// Create a scene with 3 dynamic particles
		FGraphEvolutionTest Test(3);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_EQ(Test.IslandManager->GetNumParticles(), 0);

		// Change a particle to kinematic
		Test.Evolution.SetParticleObjectState(Test.ParticleHandles[1], EObjectStateType::Kinematic);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_EQ(Test.IslandManager->GetNumParticles(), 0);

		Test.Advance();

		// State should not have changed with a second tick
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 0);
		EXPECT_EQ(Test.IslandManager->GetNumParticles(), 0);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_NE(Test.ConstraintHandles[0]->GetConstraintGraphEdge(), nullptr);
		EXPECT_NE(Test.ConstraintHandles[1]->GetConstraintGraphEdge(), nullptr);
		EXPECT_NE(Test.ConstraintHandles[2]->GetConstraintGraphEdge(), nullptr);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set to velocity mode and animate
		FKinematicGeometryParticleHandle* KinematicParticle = Test.ParticleHandles[0]->CastToKinematicParticle();
		ASSERT_NE(KinematicParticle, nullptr);
		KinematicParticle->KinematicTarget().SetVelocityMode();

		Test.Advance();

		// Expect one sleeping island as the velocity of the kinematic particle is still zero
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		KinematicParticle->SetV(FVec3(10.0f, 0.0f, 0.0f));

		Test.Advance();

		// Expect one awake island
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Put all particles back to sleep and now set angular velocity
		KinematicParticle->SetV(FVec3(0.0f, 0.0f, 0.0f));
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Check we've put the island back to sleep
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Now set angular velocity. Island should wake
		KinematicParticle->SetW(FVec3(0.0f, 1.0f, 0.0f));

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Put all particles back to sleep
		KinematicParticle->SetW(FVec3(0.0f, 0.0f, 0.0f));
		for (int32 ParticleIndex = 1; ParticleIndex < Test.ParticleHandles.Num(); ++ParticleIndex)
		{
			Test.Evolution.SetParticleObjectState(Test.ParticleHandles[ParticleIndex], EObjectStateType::Sleeping);
		}

		Test.Advance();

		// Check we've put the island back to sleep
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Now set to position mode. Initially the island should stay sleeping as the target
		// transform is the identity
		FKinematicTarget KinematicTarget;
		KinematicTarget.SetTargetMode(FRigidTransform3::Identity);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set a non-zero position target. Should cause the island to wake
		KinematicTarget.SetTargetMode(FVec3(10.0f, 0.0f, 0.0f), FRotation3::Identity);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
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
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.IslandManager->GetIsland(0)->IsSleeping());

		// Set a non-identity rotation target. Should cause the island to wake
		FRigidTransform3 TargetTransform(FVec3(0.0f, 0.0f, 0.0f), FQuat::MakeFromEuler(FVec3(1.0f, 0.0f, 2.0f)));
		KinematicTarget.SetTargetMode(TargetTransform);
		KinematicParticle->SetKinematicTarget(KinematicTarget);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_FALSE(Test.IslandManager->GetIsland(0)->IsSleeping());

	}

	// Test that island merging works if we remove the last constraint
	// in an island that was already queued for merge (PLAY-6440).
	// 
	// (d=dynamic, s=sleeping, k=kinematic)
	//		Ad - Bd   Cd - Dd
	// =>	Ad - Bd - Cd   Dd
	//
	GTEST_TEST(GraphEvolutionTests, TestConstraintGraph_IslandMerge_EnableDisable)
	{
		FGraphEvolutionTest Test(4);

		// Create constraints in a chain but disable the middle constraint so we have two islands {A-B} and {C-D}
		Test.MakeChain();
		Test.ConstraintHandles[1]->SetEnabled(false);

		Test.Advance();

		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 2);
		EXPECT_FALSE(Test.ConstraintHandles[1]->IsInConstraintGraph());

		// Enable the constraint B-C.
		// NOTE: In the implementation, the enable will add the constraint to one of the islands
		// and queue the two islands to be merged, but the actual merging happens in Advance().
		Test.ConstraintHandles[1]->SetEnabled(true);

		// Disable the constraint C-D.
		// NOTE: This will leave the second island without any constraints, but it is
		// not destroyed immediately because it is queued for merging and, even though
		// it has no constraints, it still contains particle C which needs to be
		// copied to the new merged island. Particle D would have been removed because
		// it does not have any constraints.
		// Issue (PLAY-6440) was caused by the island being destroyed because it was
		// empty, but it was still queued to be merged.
		Test.ConstraintHandles[2]->SetEnabled(false);

		Test.Advance();

		// We should now only have 1 island and D should not be in the graph
		EXPECT_EQ(Test.IslandManager->GetNumIslands(), 1);
		EXPECT_TRUE(Test.ConstraintHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ConstraintHandles[1]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ConstraintHandles[2]->IsInConstraintGraph());	// Not in graph
		EXPECT_TRUE(Test.ParticleHandles[0]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[1]->IsInConstraintGraph());
		EXPECT_TRUE(Test.ParticleHandles[2]->IsInConstraintGraph());
		EXPECT_FALSE(Test.ParticleHandles[3]->IsInConstraintGraph());	// Not in graph
	}


	// Test the sparse array repeatable index assignment. See TSparseArray::SortFreeList()
	GTEST_TEST(SparseArrayTests, TestSortFreeList)
	{
		TSparseArray<int32> Values;

		// The first time we add objects, they should be in consecutive indices starting from 0
		Values.Add(0);
		Values.Add(1);
		Values.Add(2);
		EXPECT_EQ(Values[0], 0);
		EXPECT_EQ(Values[1], 1);
		EXPECT_EQ(Values[2], 2);

		// Remove a couple items in the same order we added them
		Values.RemoveAt(1);
		Values.RemoveAt(2);

		// Add the items again, they will end up in reverse order
		// We don't rely on this behaviour but I'm testing here because if this changes
		// in the future then we may be able to remove our calls to SortFreeList in the graph.
		Values.Add(1);
		Values.Add(2);
		EXPECT_EQ(Values[0], 0);
		EXPECT_EQ(Values[1], 2);	// Swapped
		EXPECT_EQ(Values[2], 1);	// Swapped


		// Now do the same as above on a new array, but call SortFreeList before reusing it
		TSparseArray<int32> Values2;
		
		Values2.Add(0);
		Values2.Add(1);
		Values2.Add(2);
		Values2.RemoveAt(1);
		Values2.RemoveAt(2);

		// Rebuild the free list
		Values2.SortFreeList();

		// We should now get the same order as the first time we added items
		Values2.Add(1);
		Values2.Add(2);
		EXPECT_EQ(Values2[0], 0);
		EXPECT_EQ(Values2[1], 1);
		EXPECT_EQ(Values2[2], 2);
	}
}
