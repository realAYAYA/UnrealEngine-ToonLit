// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"

#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/GJK.h"
#include "Chaos/Pair.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Sphere.h"
#include "Chaos/Utilities.h"

namespace ChaosTest
{
	using namespace Chaos;

	// The implicit velocity of a body is calculated correctly
	TEST(FSolverBodyTests, TestImplicitVelocity)
	{
		const FReal Dt = 1.0f / 30.0f;
		const FVec3 X0 = FVec3(0, 0, 0);
		const FVec3 V0 = FVec3(100, 10, 30);
		const FReal M0 = 100.0f;
		const FReal I0 = M0 * 1000.0f;

		FSolverBody SolverBody0;
		SolverBody0.SetX(X0);
		SolverBody0.SetP(X0);
		SolverBody0.SetInvM(1.0f / M0);
		SolverBody0.SetInvI(1.0f / I0);

		SolverBody0.ApplyPositionDelta(V0 * Dt);

		SolverBody0.SetImplicitVelocity(Dt);

		EXPECT_NEAR(SolverBody0.V().X, V0.X, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(SolverBody0.V().Y, V0.Y, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(SolverBody0.V().Z, V0.Z, KINDA_SMALL_NUMBER);
	}

	class FRigidSOAsTest : public ::testing::Test
	{
	protected:
		FRigidSOAsTest()
			: RigidSOAs(UniqueIndices)
		{
			PhysicsMaterial = MakeUnique<FChaosPhysicsMaterial>();
			PhysicsMaterial->Friction = FReal(0);
			PhysicsMaterial->Restitution = FReal(0);

			RigidSOAs.GetParticleHandles().AddArray(&Collided);
			RigidSOAs.GetParticleHandles().AddArray(&PhysicsMaterials);
			RigidSOAs.GetParticleHandles().AddArray(&PerParticlePhysicsMaterials);

		}

		~FRigidSOAsTest()
		{
		}

		FPBDRigidParticleHandle* CreateDynamicBox(FReal Radius)
		{
			return AppendDynamicParticleSphere(RigidSOAs, FVec3(Radius));
		}

		TArrayCollectionArray<bool> Collided;
		TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>> PhysicsMaterials;
		TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>> PerParticlePhysicsMaterials;
		TUniquePtr<FChaosPhysicsMaterial> PhysicsMaterial;
		FParticleUniqueIndicesMultithreaded UniqueIndices;
		FPBDRigidsSOAs RigidSOAs;
	};

	class FSolverBodyTest : public FRigidSOAsTest
	{
	protected:
		FSolverBodyTest()
		{
		}

		~FSolverBodyTest()
		{
		}

		FSolverBodyContainer SolverBodyContainer;
	};

	TEST_F(FSolverBodyTest, TestAddOrFind)
	{
		TArray<FPBDRigidParticleHandle*> Particles =
		{
			CreateDynamicBox(50.0f)
		};

		SolverBodyContainer.Reset(1);

		SolverBodyContainer.FindOrAdd(Particles[0]);
	}
}
