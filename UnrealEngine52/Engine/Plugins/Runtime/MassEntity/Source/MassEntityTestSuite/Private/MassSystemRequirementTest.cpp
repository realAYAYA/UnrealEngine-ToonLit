// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "Engine/Engine.h"
#include "MassProcessorDependencySolver.h"
#include "MassEntityTestTypes.h"
#include "MassSubsystemAccess.h"
#include "MassExecutionContext.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

//////////////////////////////////////////////////////////////////////////
// UMassTestCustomSubsystem 

TWeakObjectPtr<UMassTestCustomSubsystem> UMassTestCustomSubsystem::Instance = nullptr;

UMassTestCustomSubsystem* UMassTestCustomSubsystem::Create()
{
	ensure(Instance.IsValid() == false);
	Instance = NewObject<UMassTestCustomSubsystem>();
	return Instance.Get();
}

UMassTestCustomSubsystem* UMassTestCustomSubsystem::Get()
{
	return Instance.Get();
}

/** FMassSubsystemAccess::FetchSubsystemInstance specialization for the UMassTestCustomSubsystem */
template<>
UMassTestCustomSubsystem* FMassSubsystemAccess::FetchSubsystemInstance<UMassTestCustomSubsystem>()
{
	return UMassTestCustomSubsystem::Get();
}

//////////////////////////////////////////////////////////////////////////
// Tests
namespace FMassSystemRequirementTest
{

struct FSystemRequirementTestBase : FEntityTestBase
{
	using Super = FEntityTestBase;
	FMassEntityQuery EntityQuery;

	FSystemRequirementTestBase()
	{
		bMakeWorldEntityManagersOwner = true;
	}

	virtual bool SetUp() override
	{		
		AddAutoDestroyObject(*UMassTestCustomSubsystem::Create());

		if (Super::SetUp())
		{
			EntityManager->CreateEntity(FloatsArchetype);
			EntityQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadOnly);
			return true;
		}

		return false;
	}
};	

//////////////////////////////////////////////////////////////////////////
// The main point of this specific test is to verify that we're getting the same answers from both
// FMassEntityQuery and FMassSubsystemAccess.
struct FSubsystemAccessAPI : FSystemRequirementTestBase
{
	virtual bool InstantTest() override
	{
		UWorld* World = FAITestHelpers::GetWorld();
		check(World);
		FMassSubsystemAccess SubsystemAccessBits(World);
		FMassSubsystemAccess SubsystemAccessClasses(World);

		EntityQuery.AddSubsystemRequirement<UMassTestWorldSubsystem>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddSubsystemRequirement<UMassTestEngineSubsystem>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddSubsystemRequirement<UMassTestCustomSubsystem>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddSubsystemRequirement<UMassTestLocalPlayerSubsystem>(EMassFragmentAccess::ReadWrite);
		EntityQuery.AddSubsystemRequirement<UMassTestGameInstanceSubsystem>(EMassFragmentAccess::ReadWrite);

		SubsystemAccessBits.SetSubsystemRequirementBits(EntityQuery.GetRequiredConstSubsystems(), EntityQuery.GetRequiredMutableSubsystems());
		SubsystemAccessClasses.SetSubsystemRequirements(EntityQuery);

		UMassTestWorldSubsystem* TestWorldSubsystemActual = World->GetSubsystem<UMassTestWorldSubsystem>();
		UMassTestEngineSubsystem* TestEngineSubsystemActual = GEngine->GetEngineSubsystem<UMassTestEngineSubsystem>();
		UMassTestCustomSubsystem* TestCustomSubsystemActual = UMassTestCustomSubsystem::Get();
		ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
		UMassTestLocalPlayerSubsystem* TestLocalPlayerSubsystemActual = LocalPlayer ? LocalPlayer->GetSubsystem<UMassTestLocalPlayerSubsystem>() : nullptr;
		UGameInstance* GameInstance = World->GetGameInstance();
		UMassTestGameInstanceSubsystem* TestGameInstanceSubsystemActual = GameInstance ? GameInstance->GetSubsystem<UMassTestGameInstanceSubsystem>() : nullptr;

		UMassTestWorldSubsystem* AccessWorldSubsystem = nullptr;
		UMassTestEngineSubsystem* AccessEngineSubsystem = nullptr;
		UMassTestCustomSubsystem* AccessCustomSubsystem = nullptr;
		UMassTestLocalPlayerSubsystem* AccessLocalPlayerSubsystem = nullptr;
		UMassTestGameInstanceSubsystem* AccessGameInstanceSubsystem = nullptr;

		const UMassTestWorldSubsystem* ConstAccessWorldSubsystem = nullptr;
		const UMassTestEngineSubsystem* ConstAccessEngineSubsystem = nullptr;
		const UMassTestCustomSubsystem* ConstAccessCustomSubsystem = nullptr;
		const UMassTestLocalPlayerSubsystem* ConstAccessLocalPlayerSubsystem = nullptr;
		const UMassTestGameInstanceSubsystem* ConstAccessGameInstanceSubsystem = nullptr;

		FMassExecutionContext ExecutionContext(*EntityManager);
		EntityQuery.ForEachEntityChunk(*EntityManager, ExecutionContext, [&](FMassExecutionContext& Context)
			{
				AccessWorldSubsystem = Context.GetMutableSubsystem<UMassTestWorldSubsystem>();
				AccessEngineSubsystem = Context.GetMutableSubsystem<UMassTestEngineSubsystem>();
				AccessCustomSubsystem = Context.GetMutableSubsystem<UMassTestCustomSubsystem>();
				AccessLocalPlayerSubsystem = Context.GetMutableSubsystem<UMassTestLocalPlayerSubsystem>();
				AccessGameInstanceSubsystem = Context.GetMutableSubsystem<UMassTestGameInstanceSubsystem>();

				ConstAccessWorldSubsystem = Context.GetSubsystem<UMassTestWorldSubsystem>();
				ConstAccessEngineSubsystem = Context.GetSubsystem<UMassTestEngineSubsystem>();
				ConstAccessCustomSubsystem = Context.GetSubsystem<UMassTestCustomSubsystem>();
				ConstAccessLocalPlayerSubsystem = Context.GetSubsystem<UMassTestLocalPlayerSubsystem>();
				ConstAccessGameInstanceSubsystem = Context.GetSubsystem<UMassTestGameInstanceSubsystem>();
			});

		AITEST_NOT_NULL(TEXT("WorldSubsystem: Subsystem Actual is expected to be not NULL"), TestWorldSubsystemActual);
		AITEST_EQUAL(TEXT("WorldSubsystem: Mutable subsystem fetched is expected to be the same as the Actual"), TestWorldSubsystemActual, AccessWorldSubsystem);
		AITEST_EQUAL(TEXT("WorldSubsystem: Const subsystem fetched is expected to be the same as the Actual"), TestWorldSubsystemActual, ConstAccessWorldSubsystem);
		AITEST_EQUAL(TEXT("WorldSubsystem: subsystem fetched via bits-requirements is expected to be the same as the Actual")
			, TestWorldSubsystemActual, SubsystemAccessBits.GetSubsystem<UMassTestWorldSubsystem>());
		AITEST_EQUAL(TEXT("WorldSubsystem: subsystem fetched via class-requirements is expected to be the same as the Actual")
			, TestWorldSubsystemActual, SubsystemAccessClasses.GetSubsystem<UMassTestWorldSubsystem>());

		AITEST_NOT_NULL(TEXT("EngineSubsystem: Subsystem Actual is expected to be not NULL"), TestEngineSubsystemActual);
		AITEST_EQUAL(TEXT("EngineSubsystem: Mutable subsystem fetched is expected to be the same as the Actual"), TestEngineSubsystemActual, AccessEngineSubsystem);
		AITEST_EQUAL(TEXT("EngineSubsystem: Const subsystem fetched is expected to be the same as the Actual"), TestEngineSubsystemActual, ConstAccessEngineSubsystem);
		AITEST_EQUAL(TEXT("EngineSubsystem: subsystem fetched via bits-requirements is expected to be the same as the Actual")
			, TestEngineSubsystemActual, SubsystemAccessBits.GetSubsystem<UMassTestEngineSubsystem>());
		AITEST_EQUAL(TEXT("EngineSubsystem: subsystem fetched via class-requirements is expected to be the same as the Actual")
			, TestEngineSubsystemActual, SubsystemAccessClasses.GetSubsystem<UMassTestEngineSubsystem>());

		AITEST_NOT_NULL(TEXT("CustomSubsystem: Subsystem Actual is expected to be not NULL"), TestCustomSubsystemActual);
		AITEST_EQUAL(TEXT("CustomSubsystem: Mutable subsystem fetched is expected to be the same as the Actual"), TestCustomSubsystemActual, AccessCustomSubsystem);
		AITEST_EQUAL(TEXT("CustomSubsystem: Const subsystem fetched is expected to be the same as the Actual"), TestCustomSubsystemActual, ConstAccessCustomSubsystem);
		AITEST_EQUAL(TEXT("CustomSubsystem: subsystem fetched via bits-requirements is expected to be the same as the Actual")
			, TestCustomSubsystemActual, SubsystemAccessBits.GetSubsystem<UMassTestCustomSubsystem>());
		AITEST_EQUAL(TEXT("CustomSubsystem: subsystem fetched via class-requirements is expected to be the same as the Actual")
			, TestCustomSubsystemActual, SubsystemAccessClasses.GetSubsystem<UMassTestCustomSubsystem>());

		AITEST_EQUAL(TEXT("LocalPlayerSubsystem: Mutable subsystem fetched is expected to be the same as the Actual"), TestLocalPlayerSubsystemActual, AccessLocalPlayerSubsystem);
		AITEST_EQUAL(TEXT("LocalPlayerSubsystem: Const subsystem fetched is expected to be the same as the Actual"), TestLocalPlayerSubsystemActual, ConstAccessLocalPlayerSubsystem);
		AITEST_EQUAL(TEXT("LocalPlayerSubsystem: subsystem fetched via bits-requirements is expected to be the same as the Actual")
			, TestLocalPlayerSubsystemActual, SubsystemAccessBits.GetSubsystem<UMassTestLocalPlayerSubsystem>());
		AITEST_EQUAL(TEXT("LocalPlayerSubsystem: subsystem fetched via class-requirements is expected to be the same as the Actual")
			, TestLocalPlayerSubsystemActual, SubsystemAccessClasses.GetSubsystem<UMassTestLocalPlayerSubsystem>());

		AITEST_EQUAL(TEXT("GameInstanceSubsystem: Mutable subsystem fetched is expected to be the same as the Actual"), TestGameInstanceSubsystemActual, AccessGameInstanceSubsystem);
		AITEST_EQUAL(TEXT("GameInstanceSubsystem: Const subsystem fetched is expected to be the same as the Actual"), TestGameInstanceSubsystemActual, ConstAccessGameInstanceSubsystem);
		AITEST_EQUAL(TEXT("GameInstanceSubsystem: subsystem fetched via bits-requirements is expected to be the same as the Actual")
			, TestGameInstanceSubsystemActual, SubsystemAccessBits.GetSubsystem<UMassTestGameInstanceSubsystem>());
		AITEST_EQUAL(TEXT("GameInstanceSubsystem: subsystem fetched via class-requirements is expected to be the same as the Actual")
			, TestGameInstanceSubsystemActual, SubsystemAccessClasses.GetSubsystem<UMassTestGameInstanceSubsystem>());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSubsystemAccessAPI, "System.Mass.Query.System.AccessAPI");

} // FMassSystemRequirementTest

#undef LOCTEXT_NAMESPACE

UE_ENABLE_OPTIMIZATION_SHIP