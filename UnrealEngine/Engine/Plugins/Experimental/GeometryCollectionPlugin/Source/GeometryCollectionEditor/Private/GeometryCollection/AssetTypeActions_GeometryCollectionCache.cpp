// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/AssetTypeActions_GeometryCollectionCache.h"
#include "GeometryCollection/GeometryCollectionCache.h"

#define LOCTEXT_NAMESPACE "AssetActions_GeometryCollectionCache"

FText FAssetTypeActions_GeometryCollectionCache::GetName() const
{
	return LOCTEXT("Name", "Geometry Collection Cache");
}

UClass* FAssetTypeActions_GeometryCollectionCache::GetSupportedClass() const
{
	return UGeometryCollectionCache::StaticClass();
}

FColor FAssetTypeActions_GeometryCollectionCache::GetTypeColor() const
{
	return FColor(255, 127, 40);
}

void FAssetTypeActions_GeometryCollectionCache::GetActions(const TArray<UObject *>& InObjects, class FMenuBuilder& MenuBuilder)
{
	
}

void FAssetTypeActions_GeometryCollectionCache::OpenAssetEditor(const TArray<UObject *>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	// No editor for caches
}

uint32 FAssetTypeActions_GeometryCollectionCache::GetCategories()
{
	return EAssetTypeCategories::Physics;
}

FText FAssetTypeActions_GeometryCollectionCache::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
