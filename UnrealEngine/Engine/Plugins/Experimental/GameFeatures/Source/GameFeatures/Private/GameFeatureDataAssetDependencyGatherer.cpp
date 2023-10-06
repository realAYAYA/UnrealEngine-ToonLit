// Copyright Epic Games, Inc. All Rights Reserved.
#include "GameFeatureDataAssetDependencyGatherer.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR

#include "AssetRegistry/ARFilter.h"
#include "GameFeatureData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "Engine/Level.h"

// Register FGameFeatureDataAssetDependencyGatherer for UGameFeatureData class
REGISTER_ASSETDEPENDENCY_GATHERER(FGameFeatureDataAssetDependencyGatherer, UGameFeatureData);

void FGameFeatureDataAssetDependencyGatherer::GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const
{
	TArray<FGuid> ContentBundleGuids;
	UGameFeatureData::GetContentBundleGuidsFromAsset(AssetData, ContentBundleGuids);
	if (ContentBundleGuids.Num() > 0)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;

		FName MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString());
		for (const FGuid& ContentBundleGuid : ContentBundleGuids)
		{
			FString ContentBundleExternalActorPath;
			if (ContentBundlePaths::BuildContentBundleExternalActorPath(MountPoint.ToString(), ContentBundleGuid, ContentBundleExternalActorPath))
			{
				const FString ExternalActorsPath = ULevel::GetExternalActorsPath(ContentBundleExternalActorPath);

				OutDependencyDirectories.Add(ExternalActorsPath);
				Filter.PackagePaths.Add(*ExternalActorsPath);
			}
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
}

#endif
