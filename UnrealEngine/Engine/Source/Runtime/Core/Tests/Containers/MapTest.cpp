// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "Containers/Map.h"

#include "Tests/TestHarnessAdapter.h"

TEST_CASE_NAMED(FMapIteratorTest, "System::Core::Containers::TMap::TIterator::RemoveCurrent", "[Core][Containers][TMap]")
{
	TMap<int, int> MapUnderTest;

	MapUnderTest.Add(5, 55);
	MapUnderTest.Add(1, 11);
	MapUnderTest.Add(3, 33);
	MapUnderTest.Add(4, 44);

	int VisitedItemsCount = 0;
	for (auto It = MapUnderTest.CreateIterator(); It; ++It)
	{
		++VisitedItemsCount;
		if (It->Key == 1)
		{
			It.RemoveCurrent();
		}
	}

	CHECK(MapUnderTest.Num() == 3);
	
	CHECK(VisitedItemsCount == 4);
	
	CHECK(MapUnderTest.Find(5) != nullptr);
	CHECK(MapUnderTest.Find(1) == nullptr);
	CHECK(MapUnderTest.Find(3) != nullptr);
	CHECK(MapUnderTest.Find(4) != nullptr);
}


#endif //WITH_LOW_LEVEL_TESTS
