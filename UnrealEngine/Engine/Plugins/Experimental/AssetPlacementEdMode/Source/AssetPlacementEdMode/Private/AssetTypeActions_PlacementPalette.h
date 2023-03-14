// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_PlacementPalette : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_PlacementPalette(EAssetTypeCategories::Type InAssetCategoryBit);

	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};
