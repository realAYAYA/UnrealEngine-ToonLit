// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "TestHarness.h"
#include "Serialization/BulkData.h"

namespace BulkDataTest
{
	// Test code paths for BulkData objects that do not reference a file on disk.
	TEST_CASE("CoreUObject::Serialization::FByteBulkData::Transient", "[CoreUObject][Serialization]")
	{
		FByteBulkData BulkData;

		// We should be able to lock for read access but there should be no valid data
		const void* ReadOnlyDataPtr = BulkData.LockReadOnly();
		TEST_NULL(TEXT("Locking an empty BulkData object for reading should return nullptr!"), ReadOnlyDataPtr);
		BulkData.Unlock();

		void* DataPtr = BulkData.Lock(LOCK_READ_WRITE);
		TEST_NULL(TEXT("Locking an empty BulkData object for writing should return nullptr!"), DataPtr);
		BulkData.Unlock();

		void* CopyEmptyPtr = nullptr;
		BulkData.GetCopy(&CopyEmptyPtr, true);
		TEST_NULL(TEXT("Getting a copy of an empty BulkData object for writing should return nullptr!"), CopyEmptyPtr);

		BulkData.Lock(LOCK_READ_WRITE);
		DataPtr = BulkData.Realloc(32 * 32 * 4);
		TEST_NOT_NULL(TEXT("Reallocating an empty BulkData object should return a valid pointer!"), DataPtr);
		BulkData.Unlock();

		TEST_TRUE(TEXT("BulkData should be loaded now that it has been reallocated"), BulkData.IsBulkDataLoaded());

		void* CopyWithDiscardPtr = nullptr;
		BulkData.GetCopy(&CopyWithDiscardPtr, true); // The second parameter should be ignored because the bulkdata cannot be reloaded from disk!
		TEST_NOT_NULL(TEXT("GetCopy should return a valid pointer!"), CopyWithDiscardPtr);
		TEST_NOT_EQUAL(TEXT("GetCopy should return a copy of the data so the pointers should be different!"), DataPtr, CopyWithDiscardPtr);
		TEST_TRUE(TEXT("BulkData should still loaded after taking a copy"), BulkData.IsBulkDataLoaded());

		// Now try GetCopy again but this time without the discard request
		void* CopyNoDiscardPtr = nullptr;
		BulkData.GetCopy(&CopyNoDiscardPtr, false);
		TEST_NOT_NULL(TEXT("GetCopy should return a valid pointer!"), CopyNoDiscardPtr);
		TEST_NOT_EQUAL(TEXT("GetCopy should return a copy of the data so the pointers should be different!"), DataPtr, CopyNoDiscardPtr);
		TEST_NOT_EQUAL(TEXT("GetCopy should return a copy of the data so the pointers should be different!"), CopyWithDiscardPtr, CopyNoDiscardPtr);
		TEST_TRUE(TEXT("BulkData should still loaded after taking a copy"), BulkData.IsBulkDataLoaded());

		// Clean up allocations from GetCopy
		FMemory::Free(CopyWithDiscardPtr);
		FMemory::Free(CopyNoDiscardPtr);

		// Now do one last lock test after GetCopy
		DataPtr = BulkData.Lock(LOCK_READ_WRITE);
		BulkData.Unlock();

		TEST_TRUE(TEXT("BulkData should still loaded after locking for write"), BulkData.IsBulkDataLoaded());
		TEST_NOT_NULL(TEXT("Locking for write should return a valid pointer!"), DataPtr);

		// Now remove the bulkdata and make sure that we cannot access the old data anymore
		BulkData.RemoveBulkData();
		TEST_FALSE(TEXT("RemoveBulkData should've discarded the BulkData"), BulkData.IsBulkDataLoaded());

		DataPtr = BulkData.Lock(LOCK_READ_WRITE);
		BulkData.Unlock();

		TEST_NULL(TEXT("Locking for write after calling ::RemoveBulkData should return a nullptr!"), DataPtr);

		CopyEmptyPtr = nullptr;
		BulkData.GetCopy(&CopyNoDiscardPtr, true);
		TEST_NULL(TEXT("Getting a copy of BulkData object after calling ::RemoveBulkData should return nullptr!"), DataPtr);
	}
}

#endif // WITH_LOW_LEVEL_TESTS
