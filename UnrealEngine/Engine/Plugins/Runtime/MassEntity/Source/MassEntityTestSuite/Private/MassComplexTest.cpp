// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "MassEntityManager.h"
#include "MassEntityTestTypes.h"
#include "MassExecutionContext.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

//-----------------------------------------------------------------------------
// This file contains tests spanning multiple functionalities.
//-----------------------------------------------------------------------------
namespace FMassComplexTest
{
//-----------------------------------------------------------------------------
// Testing the issue first reported here https://forums.unrealengine.com/t/masscommandaddfragmentinstances-adding-fragments-to-incorrect-entities/731341
// 
// The issue boiled down to FMassArchetypeData::BatchMoveEntitiesToAnotherArchetype (that's the function that does the 
// work behind FMassCommandAddFragmentInstances used in this test) was moving entities to the new archetype in a changed 
// order, while the call site (FMassEntityManager::BatchAddFragmentInstancesForEntities) the order has not changed and 
// subsequently assigned mismatched values to the freshly moved entities (via FMassArchetypeData::BatchSetFragmentValues)
// 
// The fix was as simple as splitting BatchMoveEntitiesToAnotherArchetype into two sections: 
// 1. adding all the new entities to the new archetype (that we do in original order) 
// 2. and only then removal from the previous archetype (that we need to do back-to-front to keep the chunk and entity indices relevant). 
// This way the order of entities moved to the new archetype remains the same as in the input data.
//-----------------------------------------------------------------------------
struct FComplex_AddingFragmentInstancesToDiscontinouousEntitiesCollection : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int EntitiesCount = 4;
		constexpr int MiddleIndex = EntitiesCount / 2;
		CA_ASSUME(EntityManager);

		// The setup of the test.
		// We create EntitiesCount entities containing only a FTestFragment_Int fragment and then set the value
		// to subsequent integer values
		TArray<FMassEntityHandle> EntitiesCreated;
		EntityManager->BatchCreateEntities(IntsArchetype, EntitiesCount, EntitiesCreated);

		FMassExecutionContext ExecContext(*EntityManager.Get());
		FMassEntityQuery Query({ FTestFragment_Int::StaticStruct() });
		Query.ForEachEntityChunk(*EntityManager, ExecContext, [](FMassExecutionContext& Context)
			{
				TArrayView<FTestFragment_Int> Ints = Context.GetMutableFragmentView<FTestFragment_Int>();
				for (int32 i = 0; i < Context.GetNumEntities(); ++i)
				{
					Ints[i].Value = i;
				}
			});

		for (int i = EntitiesCount - 1; i >= 0; --i)
		{
			// skipping one index somewhere in the middle to ensure discontinuity in entities being processed by the command
			if (i != MiddleIndex)
			{
				const FTestFragment_Int& IntFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Int>(EntitiesCreated[i]);
				EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(EntitiesCreated[i], FTestFragment_Float(static_cast<float>(IntFragment.Value)));
			}
		}
		EntityManager->FlushCommands();

		// by now we should have 1 remaining entity in the original IntsArchetype (no point in testing it, there are 
		// other tests doing that), and 3 entities in the Ints&Floats archetype, with value of Int fragment and Float 
		// fragment matching.
		for (int i = 0; i < EntitiesCount; ++i)
		{
			if (i != MiddleIndex)
			{
				const FTestFragment_Float& FloatFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Float>(EntitiesCreated[i]);
				const FTestFragment_Int& IntFragment = EntityManager->GetFragmentDataChecked<FTestFragment_Int>(EntitiesCreated[i]);
				
				AITEST_EQUAL("All int fragments are expected to contain the value indicating the order in which the entity has been created.", IntFragment.Value, i);
				AITEST_EQUAL("The int and float values are expected to remain in sync.", IntFragment.Value, (int)FloatFragment.Value);
			}
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FComplex_AddingFragmentInstancesToDiscontinouousEntitiesCollection, "System.Mass.Complex.AddingFragmentInstancesToDiscontinouousEntitiesCollection");

} // namespace FMassComplexTest

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE