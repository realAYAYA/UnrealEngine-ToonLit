// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"


/**
 * Implements an action for ImgMediaSource assets.
 */
class FImgMediaSourceActions : public FAssetTypeActions_Base
{
public:

	//~ FAssetTypeActions_Base interface
	virtual bool CanFilter() override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
};
