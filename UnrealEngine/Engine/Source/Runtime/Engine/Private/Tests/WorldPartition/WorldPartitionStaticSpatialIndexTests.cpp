// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_NAME_ROOT "System.Engine.WorldPartition"

namespace WorldPartitionTests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPartitionStaticSpatialIndexTest, TEST_NAME_ROOT ".StaticSpatialIndex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#if WITH_EDITOR
	template <class Class>
	void PerformTests(FWorldPartitionStaticSpatialIndexTest* Test, const TCHAR* Name, const TArray<TPair<FBox, int32>>& Elements, const TArray<FSphere>& Tests, TArray<int32>& Results)
	{
		Class SpatialIndex;
		SpatialIndex.Init(Elements);

		Results.Reserve(Tests.Num());

		const double StartTime = FPlatformTime::Seconds();

		for (int32 ListNumTests = 0; ListNumTests < Tests.Num(); ListNumTests++)
		{
			SpatialIndex.ForEachIntersectingElement(Tests[ListNumTests], [&Results](const int32& Value) { Results.Add(Value); });
		}

		const double RunTime = FPlatformTime::Seconds() - StartTime;
		Test->AddInfo(FString::Printf(TEXT("%s: %d tests in %s (%.2f/s)"), Name, Tests.Num(), *FPlatformTime::PrettyTime(RunTime), Tests.Num() / RunTime), 0);
	}
#endif

	bool FWorldPartitionStaticSpatialIndexTest::RunTest(const FString& Parameters)
	{
#if WITH_EDITOR
		const int32 NumBoxes = 100000;
		const int32 NumTests = 10000;

		TArray<TPair<FBox, int32>> Elements;
		Elements.Reserve(NumBoxes);
		for (int32 i=0; i<NumBoxes; i++)
		{
			const FSphere Sphere(FMath::VRand() * 10000000, FMath::RandRange(1, 100000));
			Elements.Emplace(FBoxSphereBounds(Sphere).GetBox(), i);
		}

		TArray<FSphere> Tests;
		Tests.Reserve(NumTests);
		for (int32 i=0; i<NumTests; i++)
		{
			const FSphere Sphere(FMath::VRand() * 10000000, FMath::RandRange(1, 100000));
			Tests.Add(Sphere);
		}

		TArray<int32> ListResults;
		PerformTests<TStaticSpatialIndexList<int32>>(this, TEXT("TStaticSpatialIndexList"), Elements, Tests, ListResults);
		ListResults.Sort();

		TArray<int32> FRTreeResults;
		PerformTests<TStaticSpatialIndexRTree<int32>>(this, TEXT("TStaticSpatialIndexRTree"), Elements, Tests, FRTreeResults);
		FRTreeResults.Sort();

		TestTrue(TEXT("TStaticSpatialIndexRTree"), ListResults == FRTreeResults);
#endif
		return true;
	}
}

#undef TEST_NAME_ROOT

#endif