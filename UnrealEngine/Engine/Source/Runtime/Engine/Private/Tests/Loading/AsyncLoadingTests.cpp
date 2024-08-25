// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"

#if WITH_DEV_AUTOMATION_TESTS

#undef TEST_NAME_ROOT
#define TEST_NAME_ROOT "System.Engine.Loading"

/**
 * This test demonstrate that LoadPackageAsync is thread-safe and can be called from multiple workers at the same time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreadSafeAsyncLoadingTest, TEXT(TEST_NAME_ROOT ".ThreadSafeAsyncLoadingTest"), EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FThreadSafeAsyncLoadingTest::RunTest(const FString& Parameters)
{
	// We use the asset registry to get a list of asset to load. 
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName(TEXT("AssetRegistry"))).Get();
	AssetRegistry.WaitForCompletion();

	// Limit the number of packages we're going to load for the test in case the project is very big.
	constexpr int32 MaxPackageCount = 5000;

	TSet<FName> UniquePackages;
	AssetRegistry.EnumerateAllAssets(
		[&UniquePackages](const FAssetData& AssetData)
		{
			if (UniquePackages.Num() < MaxPackageCount)
			{
				UniquePackages.FindOrAdd(AssetData.PackageName);
				return true;
			}
			
			return false;
		},
		true /* bIncludeOnlyOnDiskAssets */
	);

	TArray<FName> PackagesToLoad(UniquePackages.Array());
	TArray<int32> RequestIDs;
	RequestIDs.SetNum(PackagesToLoad.Num());

	ParallelFor(PackagesToLoad.Num(),
		[&PackagesToLoad, &RequestIDs](int32 Index)
		{
			RequestIDs[Index] = LoadPackageAsync(PackagesToLoad[Index].ToString());
		}
	);

	FlushAsyncLoading(RequestIDs);

	return true;
}

#undef TEST_NAME_ROOT
#endif // WITH_DEV_AUTOMATION_TESTS