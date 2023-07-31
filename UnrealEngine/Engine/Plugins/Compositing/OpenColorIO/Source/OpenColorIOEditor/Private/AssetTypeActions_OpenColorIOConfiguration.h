// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

#include "CoreMinimal.h"


/** Asset type actions for UOpenColorIOConfiguration class */
class FAssetTypeActions_OpenColorIOConfiguration : public FAssetTypeActions_Base
{
public:
	// Begin IAssetTypeActions interface
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return false; }
	
	virtual FText GetName() const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	// End IAssetTypeActions interface
};
