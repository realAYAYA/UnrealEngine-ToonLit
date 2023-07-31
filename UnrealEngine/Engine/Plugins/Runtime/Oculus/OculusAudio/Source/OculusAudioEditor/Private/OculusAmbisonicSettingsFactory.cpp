// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAmbisonicSettingsFactory.h"
#include "AssetTypeCategories.h"
#include "AudioAnalytics.h"
#include "OculusAmbisonicsSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_OculusAmbisonicsSettings::GetName() const
{
	return LOCTEXT("AssetTypeActions_OculusAmbisonicsSettings", "Oculus Ambisonics Settings");
}

FColor FAssetTypeActions_OculusAmbisonicsSettings::GetTypeColor() const
{
	return FColor(100, 100, 100);
}

const TArray<FText>& FAssetTypeActions_OculusAmbisonicsSettings::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundOculusSubMenu", "Oculus")
	};

	return SubMenus;
}

UClass* FAssetTypeActions_OculusAmbisonicsSettings::GetSupportedClass() const
{
	return UOculusAudioSoundfieldSettings::StaticClass();
}

uint32 FAssetTypeActions_OculusAmbisonicsSettings::GetCategories()
{
	return EAssetTypeCategories::Sounds;
}

UOculusAmbisonicsSettingsFactory::UOculusAmbisonicsSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UOculusAudioSoundfieldSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
}

UObject* UOculusAmbisonicsSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("OculusAudio.AmbisonicsSettingsCreated"));
	return NewObject<UOculusAudioSoundfieldSettings>(InParent, Name, Flags);
}

uint32 UOculusAmbisonicsSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}
#undef LOCTEXT_NAMESPACE
