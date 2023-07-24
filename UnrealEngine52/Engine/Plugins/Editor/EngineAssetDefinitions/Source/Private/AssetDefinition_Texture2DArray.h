// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2DArray.h"
#include "AssetDefinition_Texture.h"

#include "AssetDefinition_Texture2DArray.generated.h"

UCLASS()
class ENGINEASSETDEFINITIONS_API UAssetDefinition_Texture2DArray : public UAssetDefinition_Texture
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Texture2DArray", "Texture 2D Array"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(0, 64, 128)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UTexture2DArray::StaticClass(); }
	// UAssetDefinition End
};
