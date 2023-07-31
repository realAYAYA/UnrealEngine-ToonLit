// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundAttenuation.h"

#include "AudioEditorSettings.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/UObjectGlobals.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundAttenuation::GetSupportedClass() const
{
	return USoundAttenuation::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundAttenuation::GetSubMenus() const
{
	if (GetDefault<UAudioEditorSettings>()->bPinSoundAttenuationInAssetMenu)
	{
		static const TArray<FText> AssetTypeActionSubMenu;
		return AssetTypeActionSubMenu;
	}
	static const TArray<FText> AssetTypeActionSubMenu
	{
		LOCTEXT("AssetSoundAttenuationSubMenu", "Spatialization"),
	};
	return AssetTypeActionSubMenu;
}

#undef LOCTEXT_NAMESPACE
