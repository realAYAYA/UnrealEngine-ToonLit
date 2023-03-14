// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_ActorFoliageSettings : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_ActorFoliageSettings(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ActorFoliageSettings", "Actor Foliage"); }
	virtual FColor GetTypeColor() const override { return FColor(12, 65, 12); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return AssetCategoryBit; }

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};
