// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Engine/Texture2DArray.h"

class FAssetTypeActions_Texture2DArray : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Texture2DArray", "Texture 2D Array"); }
	virtual FColor GetTypeColor() const override { return FColor(0,64,128); }
	virtual UClass* GetSupportedClass() const override{ return UTexture2DArray::StaticClass();}
	virtual bool CanFilter() override { return true;}
}; 