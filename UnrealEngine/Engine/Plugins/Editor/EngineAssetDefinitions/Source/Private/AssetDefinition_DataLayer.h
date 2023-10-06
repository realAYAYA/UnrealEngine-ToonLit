// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_DataLayer.generated.h"

UCLASS()
class UAssetDefinition_DataLayer : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataLayer", "Data Layer"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(12, 65, 12)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UDataLayerAsset::StaticClass(); }
	// UAssetDefinition End
};
