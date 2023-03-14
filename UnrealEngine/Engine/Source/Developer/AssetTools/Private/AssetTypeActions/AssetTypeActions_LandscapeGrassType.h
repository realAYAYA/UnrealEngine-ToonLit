// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "LandscapeGrassType.h"

class FAssetTypeActions_LandscapeGrassType : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_LandscapeGrassType(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LandscapeGrassType", "Landscape Grass Type"); }
	virtual FColor GetTypeColor() const override { return FColor(128,255,128); }
	virtual UClass* GetSupportedClass() const override { return ULandscapeGrassType::StaticClass(); }
	virtual uint32 GetCategories() override { return AssetCategoryBit; }

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};
