// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITestsCommon.h"

#include "MassProcessorDependencySolver.h"
#include "MassEntityTestTypes.h"

// D:\p4\Starship_review\Engine\Source\Runtime\NavigationSystem\Public\NavigationSystem.h
//#include "MassNavigationSubsystem.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

//template<>
//struct TMassExternalSystemGetter<FMassEntityManager>
//{
//	
//};

namespace FMassSystemRequirementTest
{

struct FSystemRequirementTestBase : FEntityTestBase
{
	using Super = FEntityTestBase;
	virtual bool SetUp() override
	{		
		return Super::SetUp();
	}
};	

struct FMutableRequirement : FSystemRequirementTestBase
{
	virtual bool InstantTest() override
	{
		EntityManager->CreateEntity(FloatsArchetype);

		FMassEntityQuery EntityQuery;
		EntityQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddSubsystemRequirement<UMassTestWorldSubsystem>(EMassFragmentAccess::ReadWrite);

		check(World);
		UMassTestWorldSubsystem* TestSystemActual = World->GetSubsystem<UMassTestWorldSubsystem>();
		UMassTestWorldSubsystem* TestSystem = nullptr;
		const UMassTestWorldSubsystem* TestSystem2 = nullptr;
		FMassExecutionContext ExecutionContext;
		EntityQuery.ForEachEntityChunk(*EntityManager, ExecutionContext, [this, &TestSystem, &TestSystem2](FMassExecutionContext& Context)
		{
			TestSystem = Context.GetMutableSubsystem<UMassTestWorldSubsystem>(World);
			TestSystem2 = Context.GetSubsystem<UMassTestWorldSubsystem>(World);
		});

		AITEST_NOT_NULL(TEXT(""), TestSystem);
		AITEST_EQUAL(TEXT(""), TestSystem, TestSystemActual);
		AITEST_EQUAL(TEXT(""), TestSystem, TestSystem2);		

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMutableRequirement, "System.Mass.Query.System.TrivialMutable");

struct FConstRequirement : FSystemRequirementTestBase
{
	virtual bool InstantTest() override
	{
		EntityManager->CreateEntity(FloatsArchetype);

		FMassEntityQuery EntityQuery;
		EntityQuery.AddRequirement<FTestFragment_Float>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddSubsystemRequirement<UMassTestWorldSubsystem>(EMassFragmentAccess::ReadOnly);

		check(World);
		UMassTestWorldSubsystem* TestMutableSystem = nullptr;
		const UMassTestWorldSubsystem* TestConstSystem = nullptr;
		FMassExecutionContext ExecutionContext;
		EntityQuery.ForEachEntityChunk(*EntityManager, ExecutionContext, [this, &TestMutableSystem, &TestConstSystem](FMassExecutionContext& Context)
		{
			// commented out since there's no way to seamlessly handle failed ensures, and the line below should cause that.
			TestConstSystem = Context.GetSubsystem<UMassTestWorldSubsystem>(World);
		});

		AITEST_NOT_NULL(TEXT("It should be possible to only fetch const system instance"), TestConstSystem);
		AITEST_NULL(TEXT("Mutable system access should not be possible"), TestMutableSystem);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FConstRequirement, "System.Mass.Query.System.TrivialConst");

} // FMassSystemRequirementTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
