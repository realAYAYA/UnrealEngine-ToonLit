// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomCacheActions.h"
#include "EditorFramework/AssetImportData.h"
#include "GroomCache.h"

FText FGroomCacheActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GroomCache", "Groom Cache");
}

UClass* FGroomCacheActions::GetSupportedClass() const
{
	return UGroomCache::StaticClass();
}

FColor FGroomCacheActions::GetTypeColor() const
{
	return FColor::White;
}

uint32 FGroomCacheActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FGroomCacheActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (UObject* Asset : TypeAssets)
	{
		const UGroomCache* GroomCache = Cast<UGroomCache>(Asset);
		if (GroomCache && GroomCache->AssetImportData)
		{
			GroomCache->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}
