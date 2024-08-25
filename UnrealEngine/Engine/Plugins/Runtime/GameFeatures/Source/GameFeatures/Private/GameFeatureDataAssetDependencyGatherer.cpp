// Copyright Epic Games, Inc. All Rights Reserved.
#include "GameFeatureDataAssetDependencyGatherer.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR

#include "AssetRegistry/ARFilter.h"
#include "GameFeatureData.h"
#include "AssetRegistry/AssetRegistryState.h"

// Register FGameFeatureDataAssetDependencyGatherer for UGameFeatureData class
REGISTER_ASSETDEPENDENCY_GATHERER(FGameFeatureDataAssetDependencyGatherer, UGameFeatureData);

void FGameFeatureDataAssetDependencyGatherer::GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const
{
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = true;

	TArray<FString> DependencyDirectories;
	UGameFeatureData::GetDependencyDirectoriesFromAssetData(AssetData, DependencyDirectories);
	for (const FString& DependencyDirectory : DependencyDirectories)
	{
		OutDependencyDirectories.Add(DependencyDirectory);
		Filter.PackagePaths.Add(*DependencyDirectory);
	}

	if (Filter.PackagePaths.Num() > 0)
	{
		TArray<FAssetData> FilteredAssets;
		AssetRegistryState.GetAssets(CompileFilterFunc(Filter), {}, FilteredAssets, true);

		for (const FAssetData& FilteredAsset : FilteredAssets)
		{
			OutDependencies.Emplace(IAssetDependencyGatherer::FGathereredDependency{ FilteredAsset.PackageName,
				UE::AssetRegistry::EDependencyProperty::Game | UE::AssetRegistry::EDependencyProperty::Build });
		}
	}
}

#endif
