// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NaniteDisplacedMesh.h"
#include "NaniteDisplacedMesh.h"

#define LOCTEXT_NAMESPACE "AssetActions_NaniteDisplacedMesh"

FText FAssetTypeActions_NaniteDisplacedMesh::GetName() const
{
	return LOCTEXT("Name", "Nanite Displaced Mesh");
}

UClass* FAssetTypeActions_NaniteDisplacedMesh::GetSupportedClass() const
{
	return UNaniteDisplacedMesh::StaticClass();
}

FColor FAssetTypeActions_NaniteDisplacedMesh::GetTypeColor() const
{
	return FColor(255, 127, 40);
}

void FAssetTypeActions_NaniteDisplacedMesh::GetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder)
{
}

void FAssetTypeActions_NaniteDisplacedMesh::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

uint32 FAssetTypeActions_NaniteDisplacedMesh::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FAssetTypeActions_NaniteDisplacedMesh::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
