// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundSourceBus.h"

#include "Sound/SoundSourceBus.h"

class UClass;


#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundSourceBus::GetSupportedClass() const
{
	return USoundSourceBus::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundSourceBus::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetSoundSourceSubMenu", "Source"))
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE
