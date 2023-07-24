// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_SoundscapeColor.h"
#include "SoundscapeColor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundscapeColor::GetSupportedClass() const
{
	return USoundscapeColor::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundscapeColor::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundscapeSubMenu", "Soundscape")
	};
	return SubMenus;
}


#undef LOCTEXT_NAMESPACE
