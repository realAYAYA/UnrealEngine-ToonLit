// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDAssetCacheAssetActions.h"

#include "USDAssetCache2.h"
#include "USDAssetCacheAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetCacheAssetActions"

uint32 FUsdAssetCacheAssetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FUsdAssetCacheAssetActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetCache", "USD Asset Cache");
}

UClass* FUsdAssetCacheAssetActions::GetSupportedClass() const
{
	return UUsdAssetCache2::StaticClass();
}

FColor FUsdAssetCacheAssetActions::GetTypeColor() const
{
	return FColor(32, 145, 208);
}

void FUsdAssetCacheAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UUsdAssetCache2* Asset = Cast<UUsdAssetCache2>(*ObjIt))
		{
			TSharedRef<FUsdAssetCacheAssetEditorToolkit> NewCustomAssetEditor = MakeShared<FUsdAssetCacheAssetEditorToolkit>();
			NewCustomAssetEditor->Initialize(Mode, EditWithinLevelEditor, Asset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
