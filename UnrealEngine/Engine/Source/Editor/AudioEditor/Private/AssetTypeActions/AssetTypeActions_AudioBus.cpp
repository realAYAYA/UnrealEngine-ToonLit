// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_AudioBus.h"

#include "Sound/AudioBus.h"

class UClass;


#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_AudioBus::GetSupportedClass() const
{
	return UAudioBus::StaticClass();
}

const TArray<FText>& FAssetTypeActions_AudioBus::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetAudioBusMenu", "Mix"))
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE
