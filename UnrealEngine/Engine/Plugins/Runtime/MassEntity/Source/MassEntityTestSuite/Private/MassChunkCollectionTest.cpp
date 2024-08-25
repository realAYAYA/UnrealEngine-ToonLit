// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityManager.h"
#include "MassEntityTestTypes.h"
#include "Algo/RandomShuffle.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

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
		EntitiesSubSet.RemoveAt(10, 1, EAllowShrinking::No);

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

struct FEntityCollection_CreateWithDuplicatesTrivial : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> EntitiesWithDuplicates;
		EntitiesWithDuplicates.Add(Entities[2]);
		EntitiesWithDuplicates.Add(Entities[2]);

		FMassArchetypeEntityCollection EntityCollection(FloatsArchetype, EntitiesWithDuplicates, FMassArchetypeEntityCollection::FoldDuplicates);
		FMassArchetypeEntityCollection::FConstEntityRangeArrayView Ranges = EntityCollection.GetRanges();
		AITEST_EQUAL("The result should have a single subchunk", Ranges.Num(), 1);
		AITEST_EQUAL("The resulting subchunk should be of length 1", Ranges[0].Length, 1);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateWithDuplicatesTrivial, "System.Mass.EntityCollection.Create.TrivialDuplicates");

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

struct FEntityCollection_CreateWithInvalidDuplicates : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		{
			TArray<FMassEntityHandle> EntitiesSubSet;

			EntitiesSubSet.Add(FMassEntityHandle());
			EntitiesSubSet.Add(Entities[0]);
			EntitiesSubSet.Add(Entities[0]);
			EntitiesSubSet.Add(FMassEntityHandle());

			FMassArchetypeEntityCollection Collection(FloatsArchetype, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates);

			// The resulting Collection should have only a single Range consisting of a single entity (matching Entities[0])
			AITEST_EQUAL(TEXT("We expect only a single resulting range"), Collection.GetRanges().Num(), 1);
			AITEST_EQUAL(TEXT("We expect only a single entity in the resulting range"), Collection.GetRanges()[0].SubchunkStart, 0);
			AITEST_EQUAL(TEXT("We expect only a single entity in the resulting range"), Collection.GetRanges()[0].Length, 1);
		}

		{
			TArray<FMassEntityHandle> EntitiesSubSet;

			EntitiesSubSet.Add(Entities[4]);
			EntitiesSubSet.Add(FMassEntityHandle());
			EntitiesSubSet.Add(FMassEntityHandle()); 
			EntitiesSubSet.Add(Entities[3]);
			EntitiesSubSet.Add(FMassEntityHandle());
			EntitiesSubSet.Add(Entities[1]);

			FMassArchetypeEntityCollection Collection(FloatsArchetype, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates);

			// The resulting Collection should have two Ranges for a single archetype, one of them with two entities (3, 4).
			AITEST_EQUAL(TEXT("We expect two resulting range"), Collection.GetRanges().Num(), 2);
			AITEST_EQUAL(TEXT("We expect the first range to consist of a single entity"), Collection.GetRanges()[0].Length, 1);
			AITEST_EQUAL(TEXT("We expect the second range to consist of a two entities"), Collection.GetRanges()[1].Length, 2);
			// the specific composition of resulting ranges is being tested by other tests
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateWithInvalidDuplicates, "System.Mass.EntityCollection.Create.InvalidDuplicates");

struct FEntityCollection_CreateWithInvalidDuplicatesWithPayload : FEntityCollectionTestBase
{
	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> EntitiesSubSet;
		TArray<FTestFragment_Int> Payload;

		EntitiesSubSet.Add(FMassEntityHandle());
		Payload.Add(FTestFragment_Int(int32(2)));

		EntitiesSubSet.Add(Entities[0]);
		Payload.Add(FTestFragment_Int(int32(0)));

		EntitiesSubSet.Add(Entities[0]);
		Payload.Add(FTestFragment_Int(int32(1)));

		EntitiesSubSet.Add(FMassEntityHandle());
		Payload.Add(FTestFragment_Int(int32(3)));

		// transform typed payload array into generic one for sorting purposes
		FStructArrayView PaloadView(Payload);
		TArray<FMassArchetypeEntityCollectionWithPayload> Result;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(*EntityManager, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(MakeArrayView(&PaloadView, 1)), Result);

		AITEST_EQUAL(TEXT("We expect only a single result"), Result.Num(), 1);
		AITEST_EQUAL(TEXT("We expect only a single resulting range"), Result[0].GetEntityCollection().GetRanges().Num(), 1);
		AITEST_EQUAL(TEXT("We expect only a single entity in the resulting range"), Result[0].GetEntityCollection().GetRanges()[0].SubchunkStart, 0);
		AITEST_EQUAL(TEXT("We expect only a single entity in the resulting range"), Result[0].GetEntityCollection().GetRanges()[0].Length, 1);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_CreateWithInvalidDuplicatesWithPayload, "System.Mass.EntityCollection.Create.InvalidDuplicatesWithPayloadWithPayload");

#if WITH_MASSENTITY_DEBUG
struct FEntityCollection_WithPayloadBase : FEntityCollectionTestBase
{
	virtual bool SetUp() override
	{
		// skipping FEntityCollectionTestBase::SetUp on purpose to manually create entities
		FEntityTestBase::SetUp();

		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsArchetype);
		EntityManager->BatchCreateEntities(FloatsArchetype, EntitiesPerChunk * 2, Entities);
		return true;
	}
};

struct FEntityCollection_TrivialDuplicatesWithPayload : FEntityCollection_WithPayloadBase
{
	virtual bool InstantTest() override
	{
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsArchetype);
		const int32 TotalCount = Entities.Num();
		const int32 SubSetCount = int(0.6 * Entities.Num()); // using >0.5 to ensure some entities picked being in sequence and/or in different chunks
		TArray<FMassEntityHandle> EntitiesSubSet;
		TArray<FTestFragment_Int> Payload;

		EntitiesSubSet.Add(Entities[EntitiesPerChunk + 20]); 
		Payload.Add(FTestFragment_Int(int32(0)));
		EntitiesSubSet.Add(Entities[EntitiesPerChunk + 20]);
		Payload.Add(FTestFragment_Int(int32(1)));

		// transform typed payload array into generic one for sorting purposes
		FStructArrayView PaloadView(Payload);
		TArray<FMassArchetypeEntityCollectionWithPayload> Result;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(*EntityManager, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(MakeArrayView(&PaloadView, 1)), Result);

		AITEST_EQUAL(TEXT("We expect only a single result"), Result.Num(), 1);
		AITEST_EQUAL(TEXT("We expect only a single resulting range"), Result[0].GetEntityCollection().GetRanges().Num(), 1);
		AITEST_EQUAL(TEXT("We expect only a single entity in the resulting range"), Result[0].GetEntityCollection().GetRanges()[0].Length, 1);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_TrivialDuplicatesWithPayload, "System.Mass.EntityCollection.Create.TrivialDuplicatesWithPayload");

// @todo also add another archetype
struct FEntityCollection_MultiDuplicatesWithPayload : FEntityCollection_WithPayloadBase
{
	virtual bool InstantTest() override
	{
		const int32 EntitiesPerChunk = EntityManager->DebugGetArchetypeEntitiesCountPerChunk(FloatsArchetype);
		const int32 TotalCount = Entities.Num();
		const int32 SubSetCount = int(0.6 * Entities.Num()); // using >0.5 to ensure some entities picked being in sequence and/or in different chunks
		TArray<FMassEntityHandle> EntitiesSubSet;
		TArray<FTestFragment_Int> Payload;

		constexpr int32 NumUniques = 3;
		constexpr int32 NumDuplicatesEach = 4;
		int32 FragmentValue = 0;
		for (int32 Iteration = 0; Iteration < NumDuplicatesEach; ++Iteration, ++FragmentValue)
		{
			for (int32 Unique = 0; Unique < NumUniques; ++Unique)
			{
				EntitiesSubSet.Add(Entities[EntitiesPerChunk + 20 + Unique]);
				Payload.Add(FTestFragment_Int(Unique));
			}
		}

		// transform typed payload array into generic one for sorting purposes
		FStructArrayView PaloadView(Payload);
		TArray<FMassArchetypeEntityCollectionWithPayload> Result;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(*EntityManager, EntitiesSubSet, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(MakeArrayView(&PaloadView, 1)), Result);

		AITEST_EQUAL(TEXT("We expect only a single result"), Result.Num(), 1);
		AITEST_EQUAL(TEXT("We expect only a single resulting range"), Result[0].GetEntityCollection().GetRanges().Num(), 1);
		AITEST_EQUAL(TEXT("We expect exatly NumUniques entities in the resulting range"), Result[0].GetEntityCollection().GetRanges()[0].Length, NumUniques);
		const FMassGenericPayloadViewSlice& PayloadSlice = Result[0].GetPayload();
		for (int32 Unique = 0; Unique < NumUniques; ++Unique)
		{
			AITEST_EQUAL("The surviving payload value should match the expected", PayloadSlice[0].GetAt<FTestFragment_Int>(Unique).Value, Unique);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCollection_MultiDuplicatesWithPayload, "System.Mass.EntityCollection.Create.MultiDuplicatesWithPayload");

struct FEntityCollection_WithPayload : FEntityCollection_WithPayloadBase
{
	virtual bool InstantTest() override
	{
		const int32 TotalCount = Entities.Num();
		const int32 SubSetCount = int(0.6 * Entities.Num()); // using >0.5 to ensure some entities picked being in sequence and/or in different chunks
		TArray<FMassEntityHandle> EntitiesSubSet;
		TArray<FTestFragment_Int> Payload;

		TArray<int32> Indices;
		Indices.AddUninitialized(TotalCount);
		for (int32 i = 0; i < Indices.Num(); ++i)
		{
			Indices[i] = i;
		}

		FMath::SRandInit(TotalCount);
		Algo::RandomShuffle(Indices);
		Indices.SetNum(SubSetCount);

		for (int32 i : Indices)
		{
			EntitiesSubSet.Add(Entities[i]);
			Payload.Add(FTestFragment_Int(i));
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

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
