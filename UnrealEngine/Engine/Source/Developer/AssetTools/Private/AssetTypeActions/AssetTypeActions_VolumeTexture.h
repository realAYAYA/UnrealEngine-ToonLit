// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Engine/VolumeTexture.h"

class FAssetTypeActions_VolumeTexture : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_VolumeTexture", "Volume Texture"); }
	virtual FColor GetTypeColor() const override { return FColor(192,128,64); }
	virtual UClass* GetSupportedClass() const override { return UVolumeTexture::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
