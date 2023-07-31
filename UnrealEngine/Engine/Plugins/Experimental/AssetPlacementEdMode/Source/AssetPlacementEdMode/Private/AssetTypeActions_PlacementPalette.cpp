// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_PlacementPalette.h"

#include "PlacementPaletteAsset.h"

FAssetTypeActions_PlacementPalette::FAssetTypeActions_PlacementPalette(EAssetTypeCategories::Type InAssetCategoryBit)
	: AssetCategoryBit(InAssetCategoryBit)
{
}

FText FAssetTypeActions_PlacementPalette::GetName() const
{
	return NSLOCTEXT("AssetPlacementEdMode", "AssetTypeActions_PlacementPalette", "Placement Palette");
}

FColor FAssetTypeActions_PlacementPalette::GetTypeColor() const
{
	return FColor(222, 132, 255);
}

UClass* FAssetTypeActions_PlacementPalette::GetSupportedClass() const
{
	return UPlacementPaletteAsset::StaticClass();
}

uint32 FAssetTypeActions_PlacementPalette::GetCategories()
{
	return AssetCategoryBit;
}
