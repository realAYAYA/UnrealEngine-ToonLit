// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "TestHarness.h"
#include "ChaosUserDataPT.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "Chaos/Box.h"

class FTestUserData : public Chaos::TUserDataManagerPT<FString> { };

TEST_CASE("ChaosUserDataPT", "[integration]")
{
	const float DeltaTime = 1.f;
	const FString TestString1 = TEXT("TestData1");
	const FString TestString2 = TEXT("TestData2");

	// Create a solver in the solvers module
	FChaosSolversModule* Module = FChaosSolversModule::GetModule();
	Chaos::FPBDRigidsSolver* Solver = Module->CreateSolver(nullptr, /*AsyncDt=*/DeltaTime, Chaos::EThreadingMode::TaskGraph);

	// Create a test userdata manager in the solver
	FTestUserData* TestUserData = Solver->CreateAndRegisterSimCallbackObject_External<FTestUserData>();

	// Make a box geometry
	auto BoxGeom = Chaos::FImplicitObjectPtr(new Chaos::TBox<Chaos::FReal, 3>(Chaos::FVec3(-1, -1, -1), Chaos::FVec3(1, 1, 1)));

	// Add some proxies to the solver
	Chaos::FSingleParticlePhysicsProxy* Proxy0 = Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle());
	Chaos::FSingleParticlePhysicsProxy* Proxy1 = Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle());
	Chaos::FSingleParticlePhysicsProxy* Proxy2 = Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle());
	Chaos::FRigidBodyHandle_External& HandleExternal0 = Proxy0->GetGameThreadAPI();
	Chaos::FRigidBodyHandle_External& HandleExternal1 = Proxy1->GetGameThreadAPI();
	Chaos::FRigidBodyHandle_External& HandleExternal2 = Proxy2->GetGameThreadAPI();
	HandleExternal0.SetGeometry(BoxGeom);
	HandleExternal1.SetGeometry(BoxGeom);
	HandleExternal2.SetGeometry(BoxGeom);
	Solver->RegisterObject(Proxy0);
	Solver->RegisterObject(Proxy1);
	Solver->RegisterObject(Proxy2);

	// Advance the solver twice to make sure the PT handles are created and in the evolution
	Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

	// Add data
	SECTION("Data propagates from GT to PT")
	{
		// Add userdata to the particle
		TestUserData->SetData_GT(HandleExternal0, TestString1);

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
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString1);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}

	// Change data
	SECTION("Data updates propagate from GT to PT")
	{
		// Add userdata to the particle
		TestUserData->SetData_GT(HandleExternal0, TestString1);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Set the userdata to something else
		TestUserData->SetData_GT(HandleExternal0, TestString2);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// The data should make it to the physics thread by this point
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString2);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}

	// Delete data
	SECTION("Data removals propagate from GT to PT")
	{
		// Add userdata to the particle and advance it to the physics thread
		TestUserData->SetData_GT(HandleExternal0, TestString1);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Delete the data
		TestUserData->RemoveData_GT(HandleExternal0);

		// Data should exist for one more update
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString1);
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
		TestUserData->SetData_GT(HandleExternal0, TestString1);
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

	SECTION("Clearing all data from a userdata manager")
	{
		// Add data to three particles, propagate it to the PT
		TestUserData->SetData_GT(HandleExternal0, TestString1);
		TestUserData->SetData_GT(HandleExternal1, TestString1);
		TestUserData->SetData_GT(HandleExternal2, TestString1);
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Clear all data from the userdata manager, but after that set data back on
		// particle 2 - the fact that it happened _after_ clearing should mean it is
		// still there for particle 2 after the clear reaches the PT.
		TestUserData->ClearData_GT();
		TestUserData->SetData_GT(HandleExternal2, TestString1);

		// Check to see that the data is still there at first
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(*TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == TestString1);
			REQUIRE(*TestUserData->GetData_PT(*Proxy1->GetPhysicsThreadAPI()) == TestString1);
			REQUIRE(*TestUserData->GetData_PT(*Proxy2->GetPhysicsThreadAPI()) == TestString1);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();

		// Check make sure that after a couple updates, the data is cleared
		Solver->EnqueueCommandImmediate([&]()
		{
			REQUIRE(TestUserData->GetData_PT(*Proxy0->GetPhysicsThreadAPI()) == nullptr);
			REQUIRE(TestUserData->GetData_PT(*Proxy1->GetPhysicsThreadAPI()) == nullptr);
			REQUIRE(*TestUserData->GetData_PT(*Proxy2->GetPhysicsThreadAPI()) == TestString1);
		});
		Solver->AdvanceAndDispatch_External(DeltaTime)->Wait();
	}
}
