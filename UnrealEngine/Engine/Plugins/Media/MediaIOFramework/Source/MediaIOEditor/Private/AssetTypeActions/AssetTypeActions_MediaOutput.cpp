// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MediaOutput.h"

#include "MediaOutput.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_MediaOutput"

bool FAssetTypeActions_MediaOutput::CanFilter()
{
	return false;
}

FColor FAssetTypeActions_MediaOutput::GetTypeColor() const
{
	return FColor::White;
}

uint32 FAssetTypeActions_MediaOutput::GetCategories()
{
	return EAssetTypeCategories::Media;
}

bool FAssetTypeActions_MediaOutput::IsImportedAsset() const
{
	return false;
}

FText FAssetTypeActions_MediaOutput::GetName() const
{
	return FText::GetEmpty(); //Factory will give the name for each asset type
}

UClass* FAssetTypeActions_MediaOutput::GetSupportedClass() const
{
	return UMediaOutput::StaticClass();
}

#undef LOCTEXT_NAMESPACE
