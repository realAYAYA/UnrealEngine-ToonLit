// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLODLayerAssetTypeActions.h"
#include "WorldPartition/HLOD/HLODLayer.h"

FText FHLODLayerAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_HLODLayer", "HLOD Layer");
}

FColor FHLODLayerAssetTypeActions::GetTypeColor() const
{
	return FColor(0, 200, 200);
}

UClass* FHLODLayerAssetTypeActions::GetSupportedClass() const
{
	return UHLODLayer::StaticClass();
}

uint32 FHLODLayerAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FHLODLayerAssetTypeActions::CanLocalize() const
{
	return false;
}
