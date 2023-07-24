// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Character/CharacterGroundConstraintContainer.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"

namespace ChaosTest
{
	using namespace Chaos;

	class CharacterGroundConstraintsProxyTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			FChaosSolversModule* Module = FChaosSolversModule::GetModule();
			Solver = Module->CreateSolver(/*Owner*/nullptr, /*AsyncDt=*/-1);
			Solver->SetThreadingMode_External(EThreadingModeTemp::SingleThread);
		}

		void TearDown() override
		{
			FChaosSolversModule* Module = FChaosSolversModule::GetModule();
			Module->DestroySolver(Solver);
		}

		FSingleParticlePhysicsProxy* CreateParticle()
		{
			auto Geom = TSharedPtr<FImplicitObject, ESPMode::ThreadSafe>(new TBox<FReal, 3>(FVec3(-100), FVec3(100)));
			FSingleParticlePhysicsProxy* Proxy = FSingleParticlePhysicsProxy::Create(FPBDRigidParticle::CreateParticle());
			Proxy->SetHandle(Solver->GetEvolution()->CreateDynamicParticles(1)[0]);
			Proxy->GetGameThreadAPI().SetGeometry(Geom);
			Proxy->GetGameThreadAPI().SetM(1.0);
			Proxy->GetGameThreadAPI().SetInvI(FVec3(1.0, 1.0, 1.0));
			return Proxy;
		}

		FPBDRigidsSolver* Solver;
	};

	// This tests initialization of a character ground constraint and pushing
	// of data from the game thread representation to the physics thread representation
	TEST_F(CharacterGroundConstraintsProxyTest, TestProxyDataSyncing)
	{
		// Create a game thread constraint and register it with the solver
		FCharacterGroundConstraint* Constraint_GT = new FCharacterGroundConstraint();
		FSingleParticlePhysicsProxy* CharacterProxy = CreateParticle();
		FSingleParticlePhysicsProxy* GroundProxy = CreateParticle();
		Constraint_GT->Init(CharacterProxy);
		Constraint_GT->SetGroundParticleProxy(GroundProxy);

		Solver->RegisterObject(CharacterProxy);
		Solver->RegisterObject(GroundProxy);
		Solver->RegisterObject(Constraint_GT);

		// Get the proxy from the game thread constraint
		// Should have been set when registering the constraint with the solver
		FCharacterGroundConstraintProxy* Proxy = Constraint_GT->GetProxy<FCharacterGroundConstraintProxy>();
		ASSERT_TRUE(Proxy != nullptr);

		EXPECT_FALSE(Proxy->IsInitialized());
		EXPECT_EQ(Proxy->GetGameThreadAPI(), Constraint_GT);
		EXPECT_EQ(Proxy->GetPhysicsThreadAPI(), nullptr);

		// Create a local dirty properties manager and properties to use to transfer data from
		// the game thread constraint to the physics thread constraint and initialize with the
		// game thread constraint data

		const int32 DataIdx = 0;
		FDirtyProxiesBucketInfo BucketInfo;
		BucketInfo.Num[(uint32)EPhysicsProxyType::CharacterGroundConstraintType] = 1;
		FDirtyPropertiesManager Manager;
		Manager.PrepareBuckets(BucketInfo);

		FDirtyChaosProperties RemoteData;

		Proxy->PushStateOnGameThread(Manager, 0, RemoteData);

		// Initialize the physics thread data on the proxy
		Proxy->InitializeOnPhysicsThread(Solver, Manager, DataIdx, RemoteData);

		FCharacterGroundConstraintHandle* Constraint_PT = Proxy->GetPhysicsThreadAPI();
		EXPECT_TRUE(Constraint_PT != nullptr);
		EXPECT_EQ(Constraint_PT->GetCharacterParticle(), CharacterProxy->GetHandle_LowLevel());
		EXPECT_EQ(Constraint_PT->GetGroundParticle(), GroundProxy->GetHandle_LowLevel());

		EXPECT_EQ(Solver->GetCharacterGroundConstraints().GetNumConstraints(), 1);

		// Change something on the game side and check that it is synced on the physics side
		RemoteData.Clear(Manager, DataIdx);

		Constraint_GT->SetGroundDistance(1234.5f);

		Proxy->PushStateOnGameThread(Manager, DataIdx, RemoteData);
		Proxy->PushStateOnPhysicsThread(Solver, Manager, DataIdx, RemoteData);

		EXPECT_FLOAT_EQ(Constraint_PT->GetData().GroundDistance, 1234.5f);

		// Remove the constraint and particles from the solver
		Solver->UnregisterObject(Constraint_GT);
		Solver->UnregisterObject(CharacterProxy);
		Solver->UnregisterObject(GroundProxy);

		EXPECT_EQ(Proxy->GetGameThreadAPI(), nullptr);

		// Physics thread constraint is deleted in a callback set on the marshalling manager
	}
}