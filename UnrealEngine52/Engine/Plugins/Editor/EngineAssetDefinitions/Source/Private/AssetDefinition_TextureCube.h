// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TextureCube.h"
#include "AssetDefinition_Texture.h"

#include "AssetDefinition_TextureCube.generated.h"

UCLASS()
class ENGINEASSETDEFINITIONS_API UAssetDefinition_TextureCube : public UAssetDefinition_Texture
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureCube", "Texture Cube"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(192,64,128)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UTextureCube::StaticClass(); }
	// UAssetDefinition End
};
