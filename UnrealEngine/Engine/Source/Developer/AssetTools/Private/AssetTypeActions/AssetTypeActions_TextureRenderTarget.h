// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Engine/TextureRenderTarget.h"

class FAssetTypeActions_TextureRenderTarget : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTarget", "Texture Render Target"); }
	virtual FColor GetTypeColor() const override { return FColor(128,64,64); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTarget::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual bool IsImportedAsset() const override { return false; }

private:
	/** Handler for when CreateStatic is selected */
	void ExecuteCreateStatic(TArray<TWeakObjectPtr<UTextureRenderTarget>> Objects);
};
