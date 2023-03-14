// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

class FAssetTypeActions_DataLayer : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataLayer", "Data Layer"); }
	virtual FColor GetTypeColor() const override { return FColor(12, 65, 12); }
	virtual UClass* GetSupportedClass() const override { return UDataLayerAsset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};