// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "TestHarness.h"
#include "ChaosUserDataPT.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "Chaos/Box.h"
#include "TestCommon/Initialization.h"

class FTestUserData : public Chaos::TUserDataManagerPT<FString> { };

TEST_CASE("ChaosUserDataPT", "[integration]")
{
	const float DeltaTime = 1.f;
	const FString TestString = TEXT("TestData");

	// Create a solver in the solvers module
	FChaosSolversModule* Module = FChaosSolversModule::GetModule();
	Chaos::FPBDRigidsSolver* Solver = Module->CreateSolver(nullptr, /*AsyncDt=*/DeltaTime, Chaos::EThreadingMode::TaskGraph);

	// Create a test userdata manager in the solver
	FTestUserData* TestUserData = Solver->CreateAndRegisterSimCallbackObject_External<FTestUserData>();

	// Make a box geometry
	auto BoxGeom = TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe>(new Chaos::TBox<Chaos::FReal, 3>(Chaos::FVec3(-1, -1, -1), Chaos::FVec3(1, 1, 1)));

	// Add a proxy to the solver, advance twice to get a valid internal handle
	Chaos::FSingleParticlePhysicsProxy* Proxy0 = Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle());
	Chaos::FRigidBodyHandle_External& HandleExternal0 = Proxy0->GetGameThreadAPI();
	HandleExternal0.SetGeometry(BoxGeom);
	Solver->RegisterObject(Proxy0);
	Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

	// Add data
	SECTION("Data propagates from GT to PT")
	{
		// Add userdata to the particle
		TestUserData->SetData_GT(HandleExternal0, TestString);

		// The first callback should show no data because the ensure will occur before
		// the sim callback has occurred.
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == nullptr);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// The data should make it to the physics thread by this point
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}

	// Delete data
	SECTION("Data removals propagate from GT to PT")
	{
		// Add userdata to the particle and advance it to the physics thread
		TestUserData->SetData_GT(HandleExternal0, TestString);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Delete the data
		TestUserData->RemoveData_GT(HandleExternal0);

		// Data should exist for one more update
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Data should be deleted at this point
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == nullptr);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}

	// Delete data from particle that doesn't have it
	SECTION("Removing data from a particle that never had data set on it does nothing")
	{
		// Delete data that isn't there
		TestUserData->RemoveData_GT(HandleExternal0);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == nullptr);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}

	// Make sure a particle with a recycled index can't get a deleted particle's userdata
	SECTION("Deleting a particle that has userdata associated with it should remove the userdata")
	{
		// Add data to a particle, make sure it gets to PT, then delete the particle.
		const Chaos::FUniqueIdx UniqueIdx0 = HandleExternal0.UniqueIdx();
		TestUserData->SetData_GT(HandleExternal0, TestString);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
		Solver->UnregisterObject(Proxy0);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		struct FMockHandle
		{
			FMockHandle(Chaos::FUniqueIdx InUniqueIdx) : MUniqueIdx(InUniqueIdx) { }
			Chaos::FUniqueIdx UniqueIdx() const { return MUniqueIdx; }
			Chaos::FUniqueIdx MUniqueIdx;
		};

		// Access userdata with the invalid particle handle - it should retrieve nothing
		Solver->EnqueueCommandImmediate([&]()
		{
			const FMockHandle MockHandle0 = FMockHandle(UniqueIdx0);
			REQUIRE(TestUserData->GetData_PT(MockHandle0) == nullptr);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}
}
