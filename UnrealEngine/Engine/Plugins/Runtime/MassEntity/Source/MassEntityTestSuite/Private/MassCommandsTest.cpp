// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassEntityTypes.h"
#include "MassEntityView.h"

#include "Algo/Sort.h"
#include "Algo/RandomShuffle.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

//----------------------------------------------------------------------//
// tests 
//----------------------------------------------------------------------//

namespace FMassCommandsTest
{
#if WITH_MASSENTITY_DEBUG
struct FCommands_FragmentInstanceList : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		const int32 Count = 5;
		TArray<FMassEntityHandle> IntEntities;
		TArray<FMassEntityHandle> FloatEntities;
		EntityManager->BatchCreateEntities(IntsArchetype, Count, IntEntities);
		EntityManager->BatchCreateEntities(FloatsArchetype, Count, FloatEntities);

		for (int i = 0; i < Count; ++i)
		{
			EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(IntEntities[i], FTestFragment_Int(i), FTestFragment_Float(i));
			EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(FloatEntities[i], FTestFragment_Int(i), FTestFragment_Float(i));
		}

		EntityManager->FlushCommands();

		auto TestEntities = [this](const TArray<FMassEntityHandle>& Entities) -> bool {
			// all entities should have ended up in the same archetype, FloatsIntsArchetype
			for (int i = 0; i < Entities.Num(); ++i)
			{
				AITEST_EQUAL(TEXT("All entities should have ended up in the same archetype"), EntityManager->GetArchetypeForEntity(Entities[i]), FloatsIntsArchetype);

				FMassEntityView View(FloatsIntsArchetype, Entities[i]);
				AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Int>().Value, i);
				AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Float>().Value, float(i));
			}
			return true;
		};
		
		if (!TestEntities(IntEntities) || !TestEntities(FloatEntities))
		{
			return false;
		}
		//AITEST_EQUAL(TEXT("All entities should have ended up in the same archetype"), EntitySubsystem->GetArchetypeForEntity(FloatEntities[i]), FloatsIntsArchetype);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_FragmentInstanceList, "System.Mass.Commands.FragmentInstanceList");


struct FCommands_FragmentMemoryCleanup : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		const UScriptStruct* ArrayFragmentTypes[] = {
			FTestFragment_Array::StaticStruct(), 
			FTestFragment_Int::StaticStruct()
		};
		const FMassArchetypeHandle ArrayArchetype = EntityManager->CreateArchetype(MakeArrayView(ArrayFragmentTypes, 1));
		const FMassArchetypeHandle ArrayIntArchetype = EntityManager->CreateArchetype(MakeArrayView(ArrayFragmentTypes, 2));
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(ArrayArchetype);
		const int32 Count = EntitiesPerChunk * 2.5f;
		
		TArray<FMassEntityHandle> Entities;
		EntityManager->BatchCreateEntities(ArrayArchetype, Count, Entities);

		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(ArrayArchetype), Entities.Num());

		TArray<int32> EntitiesModified;
		for (int i = 0; i < Count; ++i)
		{
			if (FMath::FRand() < 0.2)
			{	
				FTestFragment_Array A;
				A.Value.Add(i);
				EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(Entities[i], A);
				EntityManager->Defer().AddFragment<FTestFragment_Int>(Entities[i]);
				EntitiesModified.Add(i);
			}
		}

		EntityManager->FlushCommands();

		for (int32 i : EntitiesModified)
		{
			FMassEntityView View(ArrayIntArchetype, Entities[i]);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Array>().Value.Num(), 1);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Array>().Value[0], i);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_FragmentMemoryCleanup, "System.Mass.Commands.MemoryManagement");

// @todo add "add-then remove some to make holes in chunks-then add again" test
struct FCommands_BuildEntitiesWithFragments : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsIntsArchetype);
		const int32 Count = EntitiesPerChunk * 2.5f;

		TArray<FMassEntityHandle> Entities;
		for (int i = 0; i < Count; ++i)
		{
			Entities.Add(EntityManager->ReserveEntity());
			EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(Entities.Last(), FTestFragment_Int(i), FTestFragment_Float(i));
		}

		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), 0);
		EntityManager->FlushCommands();
		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), Entities.Num());

		for (int i = 0; i < Entities.Num(); ++i)
		{
			FMassEntityView View(FloatsIntsArchetype, Entities[i]);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Int>().Value, i);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Float>().Value, (float)i);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_BuildEntitiesWithFragments, "System.Mass.Commands.BuildEntitiesWithFragments");

struct FCommands_BuildEntitiesInHoles : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsIntsArchetype);
		const int32 Count = int(EntitiesPerChunk * 1.25f) * 2; // making sure it's even

		TArray<FMassEntityHandle> Entities;
		EntityManager->BatchCreateEntities(FloatsIntsArchetype, Count, Entities);
		FMath::SRandInit(0);
		Algo::RandomShuffle(Entities);
		EntityManager->BatchDestroyEntities(MakeArrayView(Entities.GetData(), Entities.Num()/2));

		Entities.Reset();
		for (int i = 0; i < EntitiesPerChunk; ++i)
		{
			Entities.Add(EntityManager->ReserveEntity());
			EntityManager->Defer().PushCommand<FMassCommandAddFragmentInstances>(Entities.Last(), FTestFragment_Int(i), FTestFragment_Float(i));
		}

		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), Count / 2);
		EntityManager->FlushCommands();
		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), Count / 2 + Entities.Num());

		for (int i = 0; i < Entities.Num(); ++i)
		{
			FMassEntityView View(FloatsIntsArchetype, Entities[i]);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Int>().Value, i);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Float>().Value, (float)i);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_BuildEntitiesInHoles, "System.Mass.Commands.BuildEntitiesInHoles");

struct FCommands_BuildEntitiesWithFragmentInstances : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsIntsArchetype);
		const int32 Count = EntitiesPerChunk * 2.5f;

		TArray<FMassEntityHandle> Entities;
		for (int i = 0; i < Count; ++i)
		{
			Entities.Add(EntityManager->ReserveEntity());
			EntityManager->Defer().PushCommand<FMassCommandBuildEntity>(Entities.Last(), FTestFragment_Int(i), FTestFragment_Float(i));
		}

		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), 0);
		EntityManager->FlushCommands();
		AITEST_EQUAL(TEXT("All entities created should be in ArrayArchetype"), EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), Entities.Num());

		for (int i = 0; i < Entities.Num(); ++i)
		{
			FMassEntityView View(FloatsIntsArchetype, Entities[i]);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Int>().Value, i);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Float>().Value, (float)i);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_BuildEntitiesWithFragmentInstances, "System.Mass.Commands.BuildEntitiesWithFragmentInstances");

struct FCommands_DeferredFunction : FEntityTestBase
{
	virtual bool InstantTest() override
	{
		constexpr int32 Count = 5;
		const int32 Offset = 1000;

		TArray<FMassEntityHandle> Entities;
		EntityManager->BatchCreateEntities(IntsArchetype, Count, Entities);

		int i = 0;
		for (FMassEntityHandle Entity : Entities)
		{
			FMassEntityView View(IntsArchetype, Entity);
			View.GetFragmentData<FTestFragment_Int>().Value = Offset + i++;

			EntityManager->Defer().PushCommand<FMassDeferredSetCommand>([Entity, Archetype = IntsArchetype, Offset](FMassEntityManager& System)
				{
					FMassEntityView View(Archetype, Entity);
					View.GetFragmentData<FTestFragment_Int>().Value -= Offset;
				});
		}

		EntityManager->FlushCommands();

		for (i = 0; i < Entities.Num(); ++i)
		{
			FMassEntityView View(IntsArchetype, Entities[i]);
			AITEST_EQUAL(TEXT("Should have predicted values"), View.GetFragmentData<FTestFragment_Int>().Value, i);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCommands_DeferredFunction, "System.Mass.Commands.DeferredFunction");

#endif // WITH_MASSENTITY_DEBUG
} // FMassCommandsTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
