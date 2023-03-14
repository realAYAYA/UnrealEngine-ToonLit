// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Engine/TextureCubeArray.h"

class FAssetTypeActions_TextureCubeArray : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureCubeArray", "Texture Cube Array"); }
	virtual FColor GetTypeColor() const override { return FColor(192, 192, 128); }
	virtual UClass* GetSupportedClass() const override { return UTextureCubeArray::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};