// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "AssetRegistry/AssetDependencyGatherer.h"

class FGameFeatureDataAssetDependencyGatherer : public IAssetDependencyGatherer
{
public:
	
	FGameFeatureDataAssetDependencyGatherer() = default;
	virtual ~FGameFeatureDataAssetDependencyGatherer() = default;

	virtual void GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const override;
};

#endif
