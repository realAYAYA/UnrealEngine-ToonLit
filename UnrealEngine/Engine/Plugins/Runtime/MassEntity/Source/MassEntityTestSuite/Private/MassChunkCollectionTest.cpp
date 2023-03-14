// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityManager.h"
#include "MassEntityTestTypes.h"
#include "Algo/RandomShuffle.h"

#define LOCTEXT_NAMESPACE "MassTest"

PRAGMA_DISABLE_OPTIMIZATION

//----------------------------------------------------------------------//
// tests 
//----------------------------------------------------------------------//
namespace FMassEntityCollectionTest
{

struct FEntityCollectionTestBase : FEntityTestBase
{
	TArray<FMassEntityHandle> Entities;

	virtual bool SetUp() override
	{
		FEntityTestBase::SetUp();

		const int32 Count = 100;
		EntityManager->BatchCreateEntities(FloatsArchetype, Count, Entities);
		return true;
	}

	virtual void TearDown() override
	{
		Entities.Reset();
		FEntityTestBase::TearDown();
	}
};

struct FEntityCollection_CreateBasic : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> EntitiesSubSet;
		
		// Should end up as last chunk
		EntitiesSubSet.Add(Entities[99]);
		EntitiesSubSet.Add(Entities[97]);
		EntitiesSubSet.Add(Entities[98]);

		// Should end up as third chunk
		EntitiesSubSet.Add(Entities[20]);
		EntitiesSubSet.Add(Entities[22]);
		EntitiesSubSet.Add(Entities[21]);

		// Should end up as second chunk
		EntitiesSubSet.Add(Entities[18]);

		// Should end up as first chunk
		EntitiesSubSet.Add(Entities[10]);
		EntitiesSubSet.Add(Entities[13]);
		EntitiesSubSet.Add(Entities[11]);
		EntitiesSubSet.Add(Entities[12]);

		FMassArchetypeEntityCollection EntityCollection(FloatsArchetype, EntitiesSubSet, FMassArchetypeEntityCollection::NoDuplicates);
		FMassArchetypeEntityCollection::FConstEntityRangeArrayView Ranges = EntityCollection.GetRanges();
		AITEST_EQUAL("The predicted sub-chunk count should match", Ranges.Num(), 4);
		AITEST_EQUAL("The [10-13] chunk should be first and start at 10", Ranges[0].SubchunkStart, 10);
		AITEST_EQUAL("The [10-13] chunk should be first and have a length of 4", Ranges[0].Length, 4);
		AITEST_EQUAL("The [18] chunk should be second and start at 18", Ranges[1].SubchunkStart, 18); 
		AITEST_EQUAL("The [18] chunk should be second and have a length of 1", Ranges[1].Length, 1);
		AITEST_EQUAL("The [20-22] chunk should be third and start at 20", Ranges[2].SubchunkStart, 20);
		AITEST_EQUAL("The [20-22] chunk should be third and have a length of 3", Ranges[2].Length, 3);
		AITEST_EQUAL("The [97-99] chunk should be third and start at 97", Ranges[3].SubchunkStart, 97);
		AITEST_EQUAL("The [97-99] chunk should be third and have a length of 3", Ranges[3].Length, 3);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateBasic, "System.Mass.EntityCollection.Create.Basic");

struct FEntityCollection_CreateOrderInvariant : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> EntitiesSubSet(&Entities[10], 30);
		EntitiesSubSet.RemoveAt(10, 1, false);

		FMassArchetypeEntityCollection CollectionFromOrdered(FloatsArchetype, EntitiesSubSet, FMassArchetypeEntityCollection::NoDuplicates);

		FRandomStream Rand(0);
		ShuffleDataWithRandomStream(Rand, EntitiesSubSet);

		FMassArchetypeEntityCollection CollectionFromRandom(FloatsArchetype, EntitiesSubSet, FMassArchetypeEntityCollection::NoDuplicates);

		AITEST_TRUE("The resulting chunk collection should be the same regardless of the order of input entities", CollectionFromOrdered.IsSame(CollectionFromRandom));
		
		// just to roughly make sure the result is what we expect
		FMassArchetypeEntityCollection::FConstEntityRangeArrayView Ranges = CollectionFromOrdered.GetRanges();
		AITEST_EQUAL("The result should contain two chunks", Ranges.Num(), 2);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateOrderInvariant, "System.Mass.EntityCollection.Create.OrderInvariant");

struct FEntityCollection_CreateCrossChunk : FEntityTestBase
{
	virtual bool InstantTest() override
	{
#if WITH_MASSENTITY_DEBUG
		TArray<FMassEntityHandle> Entities;
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsArchetype);

		const int32 SpillOver = 10;
		const int32 Count = EntitiesPerChunk + SpillOver;
		EntityManager->BatchCreateEntities(FloatsArchetype, Count, Entities);
		
		TArray<FMassEntityHandle> EntitiesSubCollection;
		EntitiesSubCollection.Add(Entities[EntitiesPerChunk]);
		for (int i = 1; i < SpillOver; ++i)
		{
			EntitiesSubCollection.Add(Entities[EntitiesPerChunk + i]);
			EntitiesSubCollection.Add(Entities[EntitiesPerChunk - i]);
		}
		
		FMassArchetypeEntityCollection EntityCollection(FloatsArchetype, EntitiesSubCollection, FMassArchetypeEntityCollection::NoDuplicates);
		FMassArchetypeEntityCollection::FConstEntityRangeArrayView Ranges = EntityCollection.GetRanges();
		AITEST_EQUAL("The given continuous range should get split in two", Ranges.Num(), 2);
		AITEST_EQUAL("The part in first archetype\'s chunk should contain 9 elements", Ranges[0].Length, 9);
		AITEST_EQUAL("The part in second archetype\'s chunk should contain 10 elements", Ranges[1].Length, 10);
#endif // WITH_MASSENTITY_DEBUG
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateCrossChunk, "System.Mass.EntityCollection.Create.CrossChunk");

struct FEntityCollection_CreateWithDuplicates : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> EntitiesWithDuplicates;
		EntitiesWithDuplicates.Add(Entities[0]);
		EntitiesWithDuplicates.Add(Entities[0]);
		EntitiesWithDuplicates.Add(Entities[0]);
		EntitiesWithDuplicates.Add(Entities[1]);
		EntitiesWithDuplicates.Add(Entities[2]);
		EntitiesWithDuplicates.Add(Entities[2]);

		FMassArchetypeEntityCollection EntityCollection(FloatsArchetype, EntitiesWithDuplicates, FMassArchetypeEntityCollection::FoldDuplicates);
		FMassArchetypeEntityCollection::FConstEntityRangeArrayView Ranges = EntityCollection.GetRanges();
		AITEST_EQUAL("The result should have a single subchunk", Ranges.Num(), 1);
		AITEST_EQUAL("The resulting subchunk should be of length 3", Ranges[0].Length, 3);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateWithDuplicates, "System.Mass.EntityCollection.Create.Duplicates");

#if WITH_MASSENTITY_DEBUG
struct FEntityCollection_WithPayload : FEntityCollectionTestBase
{
	virtual bool SetUp() override
	{
		// skipping FEntityCollectionTestBase::SetUp on purpose to manually create entities
		FEntityTestBase::SetUp();

		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsArchetype);
		EntityManager->BatchCreateEntities(FloatsArchetype, EntitiesPerChunk * 2, Entities);
		return true;
	}

	virtual bool InstantTest() override
	{
		// Entities contains 100 FloatArchetype entities by now, created in SetUp
		const int32 TotalCount = Entities.Num();
		const int32 SubSetCount = int(0.6 * Entities.Num()); // using >0.5 to ensure some entities picked being in sequence and/or in different chunks
		TArray<FMassEntityHandle> EntitiesSubSet;
		TArray<FTestFragment_Int> Payload;

		TArray<uint8> Indices;
		Indices.AddUninitialized(TotalCount);
		for (int i = 0; i < Indices.Num(); ++i)
		{
			Indices[i] = uint8(i);
		}

		FMath::SRandInit(TotalCount);
		Algo::RandomShuffle(Indices);
		Indices.SetNum(SubSetCount);

		for (uint8 i : Indices)
		{
			EntitiesSubSet.Add(Entities[i]);
			Payload.Add(FTestFragment_Int(int32(i)));
		}

		// transform typed payload array into generic one for sorting purposes
		FStructArrayView PaloadView(Payload);
		TArray<FMassArchetypeEntityCollectionWithPayload> Result;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(*EntityManager, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(MakeArrayView(&PaloadView, 1)), Result);

		// at this point Payload should be sorted ascending 
		// and the values in Payload should match entities at given locations
		for (int i = 1; i < Payload.Num(); ++i)
		{
			AITEST_TRUE("Items in Payload should be arranged in an ascending manner", Payload[i].Value >= Payload[i-1].Value);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_WithPayload, "System.Mass.EntityCollection.Create.WithPayload");
#endif // WITH_MASSENTITY_DEBUG
//
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
