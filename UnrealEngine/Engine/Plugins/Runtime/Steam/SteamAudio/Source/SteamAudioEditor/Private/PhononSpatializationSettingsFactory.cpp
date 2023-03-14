//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSpatializationSettingsFactory.h"

#include "AudioAnalytics.h"
#include "PhononSpatializationSourceSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

namespace SteamAudio
{
	FText FAssetTypeActions_PhononSpatializationSettings::GetName() const
	{
		return NSLOCTEXT("SteamAudio", "AssetTypeActions_PhononSpatializationSettings", "Phonon Source Spatialization Settings");
	}

	FColor FAssetTypeActions_PhononSpatializationSettings::GetTypeColor() const
	{
		return FColor(245, 195, 101);
	}

	UClass* FAssetTypeActions_PhononSpatializationSettings::GetSupportedClass() const
	{
		return UPhononSpatializationSourceSettings::StaticClass();
	}

	uint32 FAssetTypeActions_PhononSpatializationSettings::GetCategories()
	{
		return EAssetTypeCategories::Sounds;
	}

	const TArray<FText>& FAssetTypeActions_PhononSpatializationSettings::GetSubMenus() const
	{
		static const TArray<FText> PhononSubMenus
		{
			NSLOCTEXT("SteamAudio", "AssetPhononSubMenu", "Phonon")
		};
		return PhononSubMenus;
	}
}

UPhononSpatializationSettingsFactory::UPhononSpatializationSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPhononSpatializationSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UPhononSpatializationSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("SteamAudio.PhononSpatializationSettingsCreated"));
	return NewObject<UPhononSpatializationSourceSettings>(InParent, InName, Flags);
}

uint32 UPhononSpatializationSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}