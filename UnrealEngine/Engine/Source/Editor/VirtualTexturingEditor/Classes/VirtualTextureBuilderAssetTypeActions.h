// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UVirtualTextureBuilder;

/** Asset actions setup for URuntimeVirtualTexture */
class FAssetTypeActions_VirtualTextureBuilder : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_VirtualTextureBuilder() {}

protected:
	//~ Begin IAssetTypeActions Interface.
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
	//~ End IAssetTypeActions Interface.
};
