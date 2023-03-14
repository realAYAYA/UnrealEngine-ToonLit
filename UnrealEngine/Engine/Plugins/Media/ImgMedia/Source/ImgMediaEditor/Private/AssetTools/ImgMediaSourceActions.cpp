// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/ImgMediaSourceActions.h"

#include "AssetRegistry/AssetData.h"
#include "ImgMediaSource.h"


#define LOCTEXT_NAMESPACE "ImageMediaSourceAssetTypeActions"

bool FImgMediaSourceActions::CanFilter()
{
	return true;
}

FText FImgMediaSourceActions::GetAssetDescription(const struct FAssetData& AssetData) const
{
	const UImgMediaSource* ImgMediaSource = Cast<UImgMediaSource>(AssetData.GetAsset());

	if (ImgMediaSource != nullptr)
	{
		const FString Url = ImgMediaSource->GetUrl();

		if (Url.IsEmpty())
		{
			return LOCTEXT("ImgAssetTypeActions_URLMissing", "Warning: Missing URL detected!");
		}

		if (!ImgMediaSource->Validate())
		{
			return LOCTEXT("AssetTypeActions_ImgMediaSourceInvalid", "Warning: Invalid settings detected!");
		}
	}

	return FText::GetEmpty();
}

uint32 FImgMediaSourceActions::GetCategories()
{
	return EAssetTypeCategories::Media;
}

FText FImgMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_ImgMediaSource", "Img Media Source");
}

UClass* FImgMediaSourceActions::GetSupportedClass() const
{
	return UImgMediaSource::StaticClass();
}

FColor FImgMediaSourceActions::GetTypeColor() const
{
	return FColor::White;
}

#undef LOCTEXT_NAMESPACE
