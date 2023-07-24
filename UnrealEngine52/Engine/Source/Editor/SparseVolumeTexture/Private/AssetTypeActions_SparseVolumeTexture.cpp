// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_SparseVolumeTexture.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_SparseVolumeTexture"

void FAssetTypeActions_SparseVolumeTexture::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	// Checkout AssetTypeActions_Texture.cpp for some exemples
}


void FAssetTypeActions_SparseVolumeTexture::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	// Checkout AssetTypeActions_Texture.cpp for some exemples
}


void FAssetTypeActions_SparseVolumeTexture::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// Checkout AssetTypeActions_Texture.cpp for some exemples
}

#undef LOCTEXT_NAMESPACE
