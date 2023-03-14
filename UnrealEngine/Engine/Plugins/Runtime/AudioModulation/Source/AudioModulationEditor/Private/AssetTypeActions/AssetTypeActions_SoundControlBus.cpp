// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_SoundControlBus.h"

#include "SoundControlBus.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundControlBus::GetSupportedClass() const
{
	return USoundControlBus::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundControlBus::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundModulationSubMenu", "Modulation")
	};
	return SubMenus;
}
#undef LOCTEXT_NAMESPACE
