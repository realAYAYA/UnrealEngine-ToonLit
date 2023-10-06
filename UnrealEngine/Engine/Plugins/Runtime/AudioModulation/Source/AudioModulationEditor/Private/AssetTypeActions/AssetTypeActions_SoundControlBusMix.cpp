// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_SoundControlBusMix.h"

#include "SoundControlBusMix.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundControlBusMix::GetSupportedClass() const
{
	return USoundControlBusMix::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundControlBusMix::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundModulationSubMenu", "Modulation")
	};

	return SubMenus;
}

#undef LOCTEXT_NAMESPACE
