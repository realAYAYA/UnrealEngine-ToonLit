// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>

TEST_CASE("Statics.Int")
{
	auto GetAndIncrement = []
		{
			static int Thing = 42;
			return Thing++;
		};

	REQUIRE(
		AutoRTFM::ETransactionResult::Committed ==
		AutoRTFM::Transact([&]()
			{
				GetAndIncrement();
			}));

	// The transactional effect of incrementing the static will have
	// been committed, and since we are accessing the exact same
	// static we should see its side effects.
	REQUIRE(43 == GetAndIncrement());
}

TEST_CASE("Statics.IntAbort")
{
	auto GetAndIncrement = []
		{
			static int Thing = 42;
			return Thing++;
		};

    REQUIRE(
        AutoRTFM::ETransactionResult::AbortedByRequest ==
        AutoRTFM::Transact([&] ()
        {
			if (42 == GetAndIncrement())
			{
				AutoRTFM::AbortTransaction();
			}
        }));
    
	// The transactional effect of incrementing the static will have
	// been rolled back, but it should still be initialized correctly.
	REQUIRE(42 == GetAndIncrement());
}

struct SomeStruct
{
	int Payload[42];
	int Current = 0;
};

TEST_CASE("Statics.Struct")
{
	auto GetSlot = []
		{
			static SomeStruct S;
			int* const Result = &S.Payload[S.Current];
			*Result = ++S.Current;
			return Result;
		};

	REQUIRE(
		AutoRTFM::ETransactionResult::Committed ==
		AutoRTFM::Transact([&]()
			{
				int* const Slot = GetSlot();
				*Slot = 13;
			}));

	// The transactional effect of incrementing the static will have
	// been committed, so we should see the side effects.
	int* const Slot = GetSlot();
	REQUIRE(2 == *Slot);

	// The transaction would have written to the previous slot.
	REQUIRE(13 == Slot[-1]);
}

TEST_CASE("Statics.StructAbort")
{
	auto GetSlot = []
		{
			static SomeStruct S;
			int* const Result = &S.Payload[S.Current];
			*Result = ++S.Current;
			return Result;
		};

	REQUIRE(
		AutoRTFM::ETransactionResult::AbortedByRequest ==
		AutoRTFM::Transact([&]()
			{
				int* const Slot = GetSlot();
				*Slot = 13;
				AutoRTFM::AbortTransaction();
			}));

	// The transactional effect of incrementing the static will have
	// been rolled back, but it should still be initialized correctly.
	REQUIRE(1 == *GetSlot());
}
