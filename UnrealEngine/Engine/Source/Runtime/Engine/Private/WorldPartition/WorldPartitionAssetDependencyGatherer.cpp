// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartitionAssetDependencyGatherer.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "Engine/Level.h"
#include "HAL/FileManager.h"

// Register FWorldPartitionCookPackageSplitter for UWorld class
REGISTER_ASSETDEPENDENCY_GATHERER(FWorldPartitionAssetDependencyGatherer, UWorld);

void FWorldPartitionAssetDependencyGatherer::GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const
{
	if (ULevel::GetIsLevelPartitionedFromAsset(AssetData))
	{
		const FString ExternalActorsPath = ULevel::GetExternalActorsPath(AssetData.PackageName.ToString());
		OutDependencyDirectories.Add(ExternalActorsPath);

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackagePaths.Add(*ExternalActorsPath);

		TArray<FAssetData> FilteredAssets;
		AssetRegistryState.GetAssets(CompileFilterFunc(Filter), {}, FilteredAssets, true);

		for (const FAssetData& FilteredAsset : FilteredAssets)
		{
			OutDependencies.Emplace(IAssetDependencyGatherer::FGathereredDependency{ FilteredAsset.PackageName, UE::AssetRegistry::EDependencyProperty::Game });
		}
	}
}

#endif