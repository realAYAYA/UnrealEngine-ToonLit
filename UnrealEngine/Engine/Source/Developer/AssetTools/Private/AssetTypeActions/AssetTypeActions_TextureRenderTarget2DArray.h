// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2DArray.h"

class FAssetTypeActions_TextureRenderTarget2DArray : public FAssetTypeActions_TextureRenderTarget
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTarget2DArray", "2D Array Render Target"); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTarget2DArray::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
