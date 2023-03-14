// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassEntityTypes.h"
#include "MassCommandBuffer.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

//----------------------------------------------------------------------//
// tests 
//----------------------------------------------------------------------//
namespace FMassObserverTest
{

auto EntityIndexSorted = [](const FMassEntityHandle& A, const FMassEntityHandle& B)
{
	return A.Index < B.Index;
};

struct FTagBaseOperation : FEntityTestBase
{
	TArray<FMassEntityHandle> AffectedEntities;
	UMassTestProcessorBase* TagObserver = nullptr;
	EMassObservedOperation OperationObserved = EMassObservedOperation::MAX;
	TArray<FMassEntityHandle> EntitiesInt;
	TArray<FMassEntityHandle> EntitiesIntsFloat;
	TArray<FMassEntityHandle> ExpectedEntities;
	// @return signifies if the test can continue
	virtual bool PerformOperation() = 0;

	virtual bool SetUp() override
	{
		if (FEntityTestBase::SetUp())
		{
			TagObserver = NewObject<UMassTestProcessorBase>();
			TagObserver->TestGetQuery().AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly);
			TagObserver->TestGetQuery().AddTagRequirement<FTestTag_A>(EMassFragmentPresence::All);
			TagObserver->ExecutionFunction = [this](FMassEntityManager& InEntitySubsystem, FMassExecutionContext& Context)
			{
				TagObserver->TestGetQuery().ForEachEntityChunk(InEntitySubsystem, Context, [this](FMassExecutionContext& Context)
					{
						AffectedEntities.Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
					});
			};

			return true;
		}
		return false;
	}

	virtual bool InstantTest() override
	{
		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FTestTag_A::StaticStruct(), OperationObserved, *TagObserver);

		EntityManager->BatchCreateEntities(IntsArchetype, 3, EntitiesInt);
		EntityManager->BatchCreateEntities(FloatsIntsArchetype, 3, EntitiesIntsFloat);

		if (PerformOperation())
		{
			EntityManager->FlushCommands();
			AITEST_EQUAL(TEXT("The tag observer is expected to be run for predicted number of entities"), AffectedEntities.Num(), ExpectedEntities.Num());
			
			ExpectedEntities.Sort(EntityIndexSorted);
			AffectedEntities.Sort(EntityIndexSorted);

			for (int i = 0; i < ExpectedEntities.Num(); ++i)
			{
				AITEST_EQUAL(TEXT("Expected and affected sets should be the same"), AffectedEntities[i], ExpectedEntities[i]);
			}

			/*AITEST_EQUAL(TEXT("The tag observer is expected to be run for a single entity"), AffectedEntities.Num(), 1);
			AITEST_EQUAL(TEXT("The tag observer is expected to be run for the entity that received the tag"), AffectedEntities[0], Entities[1]);*/
		}

		return true;
	}
};

struct FTag_SingleEntitySingleArchetypeAdd : FTagBaseOperation
{
	FTag_SingleEntitySingleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }
	virtual bool PerformOperation() override 
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddTag<FTestTag_A>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_SingleEntitySingleArchetypeAdd, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeAdd");

struct FTag_SingleEntitySingleArchetypeRemove : FTagBaseOperation
{
	FTag_SingleEntitySingleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };

		EntityManager->Defer().AddTag<FTestTag_A>(EntitiesInt[1]);
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().RemoveTag<FTestTag_A>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_SingleEntitySingleArchetypeRemove, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeRemove");

struct FTag_SingleEntitySingleArchetypeDestroy : FTagBaseOperation
{
	FTag_SingleEntitySingleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddTag<FTestTag_A>(EntitiesInt[1]); 
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().DestroyEntity(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_SingleEntitySingleArchetypeDestroy, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeDestroy");

struct FTag_MultipleArchetypeAdd : FTagBaseOperation
{
	FTag_MultipleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTestTag_A>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_MultipleArchetypeAdd, "System.Mass.Observer.Tag.MultipleArchetypesAdd");

struct FTag_MultipleArchetypeRemove : FTagBaseOperation
{
	FTag_MultipleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTestTag_A>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().RemoveTag<FTestTag_A>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_MultipleArchetypeRemove, "System.Mass.Observer.Tag.MultipleArchetypesRemove");

struct FTag_MultipleArchetypeDestroy : FTagBaseOperation
{
	FTag_MultipleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTestTag_A>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().DestroyEntity(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_MultipleArchetypeDestroy, "System.Mass.Observer.Tag.MultipleArchetypesDestroy");

struct FTag_MultipleArchetypeSwap : FTagBaseOperation
{
	FTag_MultipleArchetypeSwap() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesIntsFloat[1], EntitiesInt[0], EntitiesInt[2] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTestTag_A>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().SwapTags<FTestTag_A, FTestTag_B>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTag_MultipleArchetypeSwap, "System.Mass.Observer.Tag.MultipleArchetypesSwap");

} // FMassObserverTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
