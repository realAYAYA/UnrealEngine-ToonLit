// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundMix.h"

#include "Sound/SoundMix.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundMix::GetSupportedClass() const
{
	return USoundMix::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundMix::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetSoundClassSubMenu", "Classes"))
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE