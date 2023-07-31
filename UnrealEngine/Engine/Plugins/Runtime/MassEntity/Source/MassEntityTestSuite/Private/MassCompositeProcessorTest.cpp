// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITestsCommon.h"

#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassExecutor.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

namespace FMassCompositeProcessorTest
{

struct FCompositeProcessorTest_Empty : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		UMassCompositeProcessor* CompositeProcessor = NewObject<UMassCompositeProcessor>();
		check(CompositeProcessor);
		FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
		// it should just run, no warnings
		UE::Mass::Executor::Run(*CompositeProcessor, ProcessingContext);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCompositeProcessorTest_Empty, "System.Mass.Processor.Composite.Empty");


struct FCompositeProcessorTest_MultipleSubProcessors : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		CA_ASSUME(EntityManager);

		UMassCompositeProcessor* CompositeProcessor = NewObject<UMassCompositeProcessor>();
		check(CompositeProcessor);

		int Result = 0;
		{
			TArray<UMassProcessor*> Processors;
			for (int i = 0; i < 3; ++i)
			{
				UMassTestProcessorBase* Processor = NewObject<UMassTestProcessorBase>();
				Processor->ExecutionFunction = [Processor, &Result, i](FMassEntityManager& InEntitySubsystem, FMassExecutionContext& Context) {
						check(Processor);
						Result += (int)FMath::Pow(10.f, float(i));
					};
				Processors.Add(Processor);
			}

			CompositeProcessor->SetChildProcessors(MoveTemp(Processors));
		}

		FMassProcessingContext ProcessingContext(EntityManager, /*DeltaSeconds=*/0.f);
		UE::Mass::Executor::Run(*CompositeProcessor, ProcessingContext);
		AITEST_EQUAL("All of the child processors should get run", Result, 111);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCompositeProcessorTest_MultipleSubProcessors, "System.Mass.Processor.Composite.MultipleSubProcessors");

} // FMassCompositeProcessorTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

