// Copyright Epic Games, Inc. All Rights Reserved.
#include "ExternalActorAssetDependencyGatherer.h"

#if WITH_EDITOR

#include "AssetRegistry/ARFilter.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Engine/Level.h"

// Register FExternalActorAssetDependencyGatherer for UWorld class
REGISTER_ASSETDEPENDENCY_GATHERER(FExternalActorAssetDependencyGatherer, UWorld);

void FExternalActorAssetDependencyGatherer::GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const
{
	if (ULevel::GetIsLevelUsingExternalActorsFromAsset(AssetData))
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
			OutDependencies.Emplace(IAssetDependencyGatherer::FGathereredDependency{ FilteredAsset.PackageName,
				UE::AssetRegistry::EDependencyProperty::Game  | UE::AssetRegistry::EDependencyProperty::Build });
		}
	}
}

#endif
