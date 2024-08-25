// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include "Templates/SharedPointer.h"

TEST_CASE("SharedPointer.PreviouslyAllocated")
{
	TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(42));

	AutoRTFM::Commit([&]
		{
			// Make a copy to bump the reference count.
			TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;

			*Copy = 13;
		});

	REQUIRE(13 == *Foo);
	REQUIRE(1 == Foo.GetSharedReferenceCount());
}

TEST_CASE("SharedPointer.AbortWithPreviouslyAllocated")
{
	TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(42));

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
		{
			// Make a copy to bump the reference count.
			TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;

			*Copy = 13;

			AutoRTFM::AbortTransaction();
		}));

	REQUIRE(42 == *Foo);
	REQUIRE(1 == Foo.GetSharedReferenceCount());
}

TEST_CASE("SharedPointer.NewlyAllocated")
{
	int Copy = 42;

	AutoRTFM::Commit([&]
		{
			TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(13));

			Copy = *Foo;
		});

	REQUIRE(13 == Copy);
}

TEST_CASE("SharedPointer.AbortWithNewlyAllocated")
{
	int Result = 42;

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
		{
			TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(13));
			TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;
			Result = *Copy;
			AutoRTFM::AbortTransaction();
		}));

	REQUIRE(42 == Result);
}

TEST_CASE("SharedPointer.NestedTransactionWithPreviouslyAllocated")
{
	TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(42));

	AutoRTFM::Commit([&]
		{
			AutoRTFM::Commit([&]
				{
					// Make a copy to bump the reference count.
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;

					*Copy = 13;
				});
		});

	REQUIRE(13 == *Foo);
	REQUIRE(1 == Foo.GetSharedReferenceCount());
}

TEST_CASE("SharedPointer.AbortNestedTransactionWithPreviouslyAllocated")
{
	TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(42));

	AutoRTFM::Commit([&]
		{
			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
				{
					// Make a copy to bump the reference count.
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;

					*Copy = 13;

					AutoRTFM::AbortTransaction();
				}));
		});

	REQUIRE(42 == *Foo);
	REQUIRE(1 == Foo.GetSharedReferenceCount());
}

TEST_CASE("SharedPointer.NestedTransactionWithNewlyAllocated")
{
	int Result = 42;

	AutoRTFM::Commit([&]
		{
			AutoRTFM::Commit([&]
				{
					TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(13));
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;
					Result = *Copy;
				});
		});

	REQUIRE(13 == Result);
}

TEST_CASE("SharedPointer.AbortNestedTransactionWithNewlyAllocated")
{
	int Result = 42;

	AutoRTFM::Commit([&]
		{
			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
				{
					TSharedPtr<int, ESPMode::ThreadSafe> Foo(new int(13));
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = Foo;
					Result = *Copy;
					AutoRTFM::AbortTransaction();
				}));
		});

	REQUIRE(42 == Result);
}

template<typename T> FORCENOINLINE void* MakeMemoryForT()
{
	return malloc(sizeof(T));
}

TEST_CASE("SharedPointer.NestedTransactionWithPlacementNewlyAllocated")
{
	int Result = 42;

	AutoRTFM::Commit([&]
		{
			AutoRTFM::Commit([&]
				{
					void* const Memory = MakeMemoryForT<TSharedPtr<int, ESPMode::ThreadSafe>>();
					TSharedPtr<int, ESPMode::ThreadSafe>* const Foo = new (Memory) TSharedPtr<int, ESPMode::ThreadSafe>(new int(13));
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = *Foo;
					Result = *Copy;
					free(Memory);
				});
		});

	REQUIRE(13 == Result);
}

TEST_CASE("SharedPointer.AbortNestedTransactionWithPlacementNewlyAllocated")
{
	int Result = 42;

	AutoRTFM::Commit([&]
		{
			REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
				{
					void* const Memory = MakeMemoryForT<TSharedPtr<int, ESPMode::ThreadSafe>>();
					TSharedPtr<int, ESPMode::ThreadSafe>* const Foo = new (Memory) TSharedPtr<int, ESPMode::ThreadSafe>(new int(13));
					TSharedPtr<int, ESPMode::ThreadSafe> Copy = *Foo;
					Result = *Copy;
					AutoRTFM::AbortTransaction();
				}));
		});

	REQUIRE(42 == Result);
}
