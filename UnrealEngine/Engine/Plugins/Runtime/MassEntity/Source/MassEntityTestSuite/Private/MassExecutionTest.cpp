// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassExecutor.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

//----------------------------------------------------------------------//
// tests 
//----------------------------------------------------------------------//
namespace FMassExecutionTest
{

struct FExecution_Setup : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		AITEST_NOT_NULL("World needs to exist for the test to be performed", World);
		AITEST_NOT_NULL("EntitySubsystem needs to be created for the test to be performed", EntityManager.Get());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_Setup, "System.Mass.Execution.Setup");


struct FExecution_EmptyArray : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager); // if EntitySubsystem null InstantTest won't be called at all
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
		// no test performed, let's just see if it results in errors/warnings
		UE::Mass::Executor::RunProcessorsView(TArrayView<UMassProcessor*>(), ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_EmptyArray, "System.Mass.Execution.EmptyArray");


struct FExecution_EmptyPipeline : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager); // if EntitySubsystem null InstantTest won't be called at all
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
		FMassRuntimePipeline Pipeline;
		// no test performed, let's just see if it results in errors/warnings
		UE::Mass::Executor::Run(Pipeline, ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_EmptyPipeline, "System.Mass.Execution.EmptyPipeline");


struct FExecution_InvalidProcessingContext : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext;
		// test assumption
		AITEST_NULL("FMassProcessingContext\'s default constructor is expected to set FMassProcessingContext.EntitySubsystem to null", ProcessingContext.EntityManager.Get());
		
		GetTestRunner().AddExpectedError(TEXT("ProcessingContext.EntityManager is null"), EAutomationExpectedErrorFlags::Contains, 1);		
		// note that using RunProcessorsView is to bypass reasonable tests UE::Mass::Executor::Run(Pipeline,...) does that are 
		// reported via ensures which are not handled by the automation framework
		UE::Mass::Executor::RunProcessorsView(TArrayView<UMassProcessor*>(), ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_InvalidProcessingContext, "System.Mass.Execution.InvalidProcessingContext");


#if WITH_MASSENTITY_DEBUG
struct FExecution_SingleNullProcessor : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(EntityManager, DeltaSeconds);
		TArray<UMassProcessor*> Processors;
		Processors.Add(nullptr);
		

		GetTestRunner().AddExpectedError(TEXT("Processors contains nullptr"), EAutomationExpectedErrorFlags::Contains, 1);
		// note that using RunProcessorsView is to bypass reasonable tests UE::Mass::Executor::Run(Pipeline,...) does that are 
		// reported via ensures which are not handled by the automation framework
		UE::Mass::Executor::RunProcessorsView(Processors, ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_SingleNullProcessor, "System.Mass.Execution.SingleNullProcessor");


struct FExecution_SingleValidProcessor : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(EntityManager, DeltaSeconds);
		UMassTestProcessorBase* Processor = NewObject<UMassTestProcessorBase>();
		check(Processor);

		// nothing should break. The actual result of processing is getting tested in MassProcessorTests.cpp
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_SingleValidProcessor, "System.Mass.Execution.SingleValidProcessor");


struct FExecution_MultipleNullProcessors : FExecutionTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(EntityManager, DeltaSeconds);
		TArray<UMassProcessor*> Processors;
		Processors.Add(nullptr);
		Processors.Add(nullptr);
		Processors.Add(nullptr);

		GetTestRunner().AddExpectedError(TEXT("Processors contains nullptr"), EAutomationExpectedErrorFlags::Contains, 1);
		// note that using RunProcessorsView is to bypass reasonable tests UE::Mass::Executor::Run(Pipeline,...) does that are 
		// reported via ensures which are not handled by the automation framework
		UE::Mass::Executor::RunProcessorsView(Processors, ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_MultipleNullProcessors, "System.Mass.Execution.MultipleNullProcessors");
#endif // WITH_MASSENTITY_DEBUG


struct FExecution_Sparse : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);
		const float DeltaSeconds = 0.f;
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
		UMassTestProcessorBase* Processor = NewObject<UMassTestProcessorBase>();
		check(Processor);

		FMassRuntimePipeline Pipeline;
		{
			TArray<UMassProcessor*> Processors;
			Processors.Add(Processor);
			Pipeline.SetProcessors(MoveTemp(Processors));
		}

		FMassArchetypeEntityCollection EntityCollection(FloatsArchetype);
		// nothing should break. The actual result of processing is getting tested in MassProcessorTests.cpp
		
		UE::Mass::Executor::RunSparse(Pipeline, ProcessingContext, EntityCollection);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FExecution_Sparse, "System.Mass.Execution.Sparse");
} // FMassExecutionTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
