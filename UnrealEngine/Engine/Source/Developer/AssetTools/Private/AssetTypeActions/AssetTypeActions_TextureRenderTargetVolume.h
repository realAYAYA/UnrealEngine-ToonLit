// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget.h"
#include "Engine/TextureRenderTargetVolume.h"

class FAssetTypeActions_TextureRenderTargetVolume : public FAssetTypeActions_TextureRenderTarget
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTargetVolume", "Volume Render Target"); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTargetVolume::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
