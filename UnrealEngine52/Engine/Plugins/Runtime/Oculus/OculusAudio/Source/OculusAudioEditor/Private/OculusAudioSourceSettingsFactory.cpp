// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioSourceSettingsFactory.h"
#include "AssetTypeCategories.h"
#include "AudioAnalytics.h"
#include "OculusAudioSourceSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_OculusAudioSourceSettings::GetName() const
{
    return LOCTEXT("AssetTypeActions_OculusAudioSpatializationSettings", "Oculus Audio Source Settings");
}

const TArray<FText>& FAssetTypeActions_OculusAudioSourceSettings::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundOculusSubMenu", "Oculus")
	};

	return SubMenus;
}

FColor FAssetTypeActions_OculusAudioSourceSettings::GetTypeColor() const
{
    return FColor(100, 100, 100);
}

UClass* FAssetTypeActions_OculusAudioSourceSettings::GetSupportedClass() const
{
    return UOculusAudioSourceSettings::StaticClass();
}

uint32 FAssetTypeActions_OculusAudioSourceSettings::GetCategories()
{
    return EAssetTypeCategories::Sounds;
}

UOculusAudioSourceSettingsFactory::UOculusAudioSourceSettingsFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = UOculusAudioSourceSettings::StaticClass();

    bCreateNew = true;
    bEditorImport = true;
    bEditAfterNew = true;
}

UObject* UOculusAudioSourceSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
    FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("OculusAudio.SourceSettingsCreated"));
    return NewObject<UOculusAudioSourceSettings>(InParent, Name, Flags);
}

uint32 UOculusAudioSourceSettingsFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Sounds;
}

#undef LOCTEXT_NAMESPACE
