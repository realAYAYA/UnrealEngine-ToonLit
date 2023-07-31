// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ReferenceCluster.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_NAME_ROOT "System.Engine.ReferenceCluster"

namespace ReferenceClusterTests
{
	constexpr const uint32 TestFlags = EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceClusterTest, TEST_NAME_ROOT, TestFlags)
	bool FReferenceClusterTest::RunTest(const FString& Parameters)
	{
#if WITH_EDITOR
		TArray<TPair<FGuid, TArray<FGuid>>> ObjectsWithRefs;
		ObjectsWithRefs.AddDefaulted(10);

		for (auto& ObjectWithRefs : ObjectsWithRefs)
		{
			ObjectWithRefs.Key = FGuid::NewGuid();
		}

		// Basic test
		{
			TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ObjectsWithRefs);

			TestTrue(TEXT("Reference Cluster Basic"), Clusters.Num() == 10);
			for (const TArray<FGuid>& Cluster : Clusters)
			{
				TestTrue(TEXT("Reference Cluster Basic"), Cluster.Num() == 1);
			}
		}

		//
		// This forms 6 clusters:
		// 
		// 0 --> 1     4     6     7     8     9
		// |     ^     |
		// v     |     v
		// 2 --> 3     5
		ObjectsWithRefs[0].Value.Add(ObjectsWithRefs[1].Key);
		ObjectsWithRefs[0].Value.Add(ObjectsWithRefs[2].Key);
		ObjectsWithRefs[2].Value.Add(ObjectsWithRefs[3].Key);
		ObjectsWithRefs[3].Value.Add(ObjectsWithRefs[1].Key);
		ObjectsWithRefs[4].Value.Add(ObjectsWithRefs[5].Key);

		// Simple clustering test
		{
			TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ObjectsWithRefs);
			TestTrue(TEXT("Reference Cluster Simple"), Clusters.Num() == 6);
		}

		//
		// This forms 5 clusters:
		// 
		// 0 --> 1     4     6     7     8     9
		// |     ^     |
		// v     |     v
		// 2 --> 3 <-- 5
		ObjectsWithRefs[5].Value.Add(ObjectsWithRefs[3].Key);

		// Simple clusters merging test
		{
			TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ObjectsWithRefs);
			TestTrue(TEXT("Reference Cluster Simple"), Clusters.Num() == 5);
		}

		//
		// This forms 2 clusters:
		// 
		//                   /-----.-----------\
		//                   |     |           |
		//                   v     v           |
		// 0 --> 1     4     6     7     8 <-- 9
		// |     ^     |                  
		// v     |     v                  
		// 2 --> 3 <-- 5                  
		ObjectsWithRefs[9].Value.Add(ObjectsWithRefs[8].Key);
		ObjectsWithRefs[9].Value.Add(ObjectsWithRefs[7].Key);
		ObjectsWithRefs[9].Value.Add(ObjectsWithRefs[6].Key);

		// Double clusters test
		{
			TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ObjectsWithRefs);
			TestTrue(TEXT("Reference Cluster Double"), Clusters.Num() == 2);
		}

		//
		// This forms a single cluster:
		// 
		//                   /-----.-----------\
		//                   |     |           |
		//                   v     v           |
		// 0 --> 1     4     6     7     8 <-- 9
		// |     ^     |                 |
		// v     |     v                 |
		// 2 --> 3 <-- 5 <---------------/
		ObjectsWithRefs[8].Value.Add(ObjectsWithRefs[5].Key);

		// Complex clusters merging test
		{
			TArray<TArray<FGuid>> Clusters = GenerateObjectsClusters(ObjectsWithRefs);

			TestTrue(TEXT("Reference Cluster Complex Merging"), Clusters.Num() == 1);
			TestTrue(TEXT("Reference Cluster Complex Merging"), Clusters[0].Num() == 10);
		}
#endif
		return true;
	}
}

#undef TEST_NAME_ROOT

#endif 
