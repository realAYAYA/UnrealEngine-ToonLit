// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/ImgMediaSourceActions.h"

#include "ImgMediaSource.h"

#define LOCTEXT_NAMESPACE "ImageMediaSourceAssetTypeActions"

bool FImgMediaSourceActions::CanFilter()
{
	return true;
}

FText FImgMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_ImgMediaSource", "Img Media Source");
}

UClass* FImgMediaSourceActions::GetSupportedClass() const
{
	return UImgMediaSource::StaticClass();
}

#undef LOCTEXT_NAMESPACE
