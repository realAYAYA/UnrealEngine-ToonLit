//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononOcclusionSettingsFactory.h"

#include "AudioAnalytics.h"
#include "PhononOcclusionSourceSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

namespace SteamAudio
{
	FText FAssetTypeActions_PhononOcclusionSettings::GetName() const
	{
		return NSLOCTEXT("SteamAudio", "AssetTypeActions_PhononOcclusionSettings", "Phonon Source Occlusion Settings"); 
	}

	FColor FAssetTypeActions_PhononOcclusionSettings::GetTypeColor() const
	{
		return FColor(245, 195, 101);
	}

	UClass* FAssetTypeActions_PhononOcclusionSettings::GetSupportedClass() const
	{
		return UPhononOcclusionSourceSettings::StaticClass();
	}

	uint32 FAssetTypeActions_PhononOcclusionSettings::GetCategories()
	{
		return EAssetTypeCategories::Sounds;
	}

	const TArray<FText>& FAssetTypeActions_PhononOcclusionSettings::GetSubMenus() const
	{
		static const TArray<FText> PhononSubMenus
		{
			NSLOCTEXT("SteamAudio", "AssetPhononSubMenu", "Phonon")
		};
		return PhononSubMenus;
	}
}

UPhononOcclusionSettingsFactory::UPhononOcclusionSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhononOcclusionSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UPhononOcclusionSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("SteamAudio.PhononOcclusionSettingsCreated"));
	return NewObject<UPhononOcclusionSourceSettings>(InParent, InName, Flags);
}

uint32 UPhononOcclusionSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}