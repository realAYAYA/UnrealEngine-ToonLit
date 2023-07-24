// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

class FAssetTypeActions_SparseVolumeTexture : public FAssetTypeActions_Base
{
public:

	//~ Begin IAssetTypeActions Interface

	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SparseVolumeTexture", "Base"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 128, 64); }
	virtual UClass* GetSupportedClass() const override { return USparseVolumeTexture::StaticClass(); }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Textures; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

	//~ End IAssetTypeActions Interface

}; 
