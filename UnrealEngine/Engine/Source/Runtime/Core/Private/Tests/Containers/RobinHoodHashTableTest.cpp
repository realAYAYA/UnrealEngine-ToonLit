// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Experimental/Containers/RobinHoodHashTable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRobinHoodHashTableTest, "System.Core.Containers.RobinHoodShashTable.Basic", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FRobinHoodHashTableTest::RunTest(const FString& Parameters)
{
	Experimental::TRobinHoodHashMap<int32, int32> Table;
	static constexpr int32 N = 1999; // Some reasonable prime number of elements

	// Insert some unique values into the table
	for (int32 Index = 0; Index < N; ++Index)
	{
		bool bAlreadyExists = false;
		Table.FindOrAdd(Index, Index, bAlreadyExists);
		TestEqual(TEXT("bAlreadyExists"), bAlreadyExists, false);
	}

	// Confirm that everything was added
	for (int32 Index = 0; Index < N; ++Index)
	{
		bool bAlreadyExists = false;
		Table.FindOrAdd(Index, Index, bAlreadyExists);
		TestEqual(TEXT("bAlreadyExists"), bAlreadyExists, true);
	}

	// Check that everything can be accessed via const Find()
	{
		const Experimental::TRobinHoodHashMap<int32, int32>& TableConst = Table;
		for (int32 Index = 0; Index < N; ++Index)
		{
			bool bAlreadyExists = false;
			const int32* FoundPtr = TableConst.Find(Index);
			TestNotNull(TEXT("FoundPtr"), FoundPtr);
			TestEqual(TEXT("*FoundPtr"), *FoundPtr, Index);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
