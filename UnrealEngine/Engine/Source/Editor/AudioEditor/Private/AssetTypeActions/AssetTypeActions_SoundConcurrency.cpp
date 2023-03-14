// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundConcurrency.h"

#include "AudioEditorSettings.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/UObjectGlobals.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundConcurrency::GetSupportedClass() const
{
	return USoundConcurrency::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundConcurrency::GetSubMenus() const 
{
	if (GetDefault<UAudioEditorSettings>()->bPinSoundConcurrencyInAssetMenu)
	{
		static const TArray<FText> AssetTypeActionSubMenu;
		return AssetTypeActionSubMenu;
	}
	static const TArray<FText> AssetTypeActionSubMenu
	{
		LOCTEXT("AssetSoundConcurrencySubMenu", "Mix"),
	};
	return AssetTypeActionSubMenu;
}

#undef LOCTEXT_NAMESPACE
