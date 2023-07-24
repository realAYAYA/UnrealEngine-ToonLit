// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/AssetTypeActions_ChaosCacheCollection.h"

#include "Chaos/CacheCollection.h"

#define LOCTEXT_NAMESPACE "AssetActions_ChaosCacheCollection"

FText FAssetTypeActions_ChaosCacheCollection::GetName() const
{
	return LOCTEXT("Name", "Chaos Cache Collection");
}

UClass* FAssetTypeActions_ChaosCacheCollection::GetSupportedClass() const
{
	return UChaosCacheCollection::StaticClass();
}

FColor FAssetTypeActions_ChaosCacheCollection::GetTypeColor() const
{
	return FColor(255, 127, 40);
}

void FAssetTypeActions_ChaosCacheCollection::GetActions(const TArray<UObject*>& InObjects,
														class FMenuBuilder&     MenuBuilder)
{
}

void FAssetTypeActions_ChaosCacheCollection::OpenAssetEditor(
	const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

uint32 FAssetTypeActions_ChaosCacheCollection::GetCategories()
{
	return EAssetTypeCategories::Physics;
}

FText FAssetTypeActions_ChaosCacheCollection::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return LOCTEXT("Description", "A collection of physically active component caches that can be used to record and replay Chaos simulation recordings.");
}

#undef LOCTEXT_NAMESPACE
