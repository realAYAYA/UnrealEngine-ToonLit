// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinition_PCGGraphInterface.h"

#include "AssetDefinition_PCGGraph.generated.h"

UCLASS()
class UAssetDefinition_PCGGraph : public UAssetDefinition_PCGGraphInterface
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const override;
};
