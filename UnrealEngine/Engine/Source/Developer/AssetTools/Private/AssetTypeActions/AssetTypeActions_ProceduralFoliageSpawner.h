// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_ProceduralFoliageSpawner : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_ProceduralFoliageSpawner(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ProceduralFoliageSpawner", "Procedural Foliage Spawner"); }
	virtual FColor GetTypeColor() const override { return FColor(7, 103, 7); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return AssetCategoryBit; }
	virtual bool CanFilter() override;

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};
