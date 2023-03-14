//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#include "ResonanceAudioSpatializationSourceSettingsFactory.h"
#include "ResonanceAudioSpatializationSourceSettings.h"
#include "ResonanceAudioConstants.h"

#include "AudioAnalytics.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ResonanceAudioSpatializationSourceSettingsFactory)

FText FAssetTypeActions_ResonanceAudioSpatializationSourceSettings::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ResonanceAudioSpatializationSourceSettings", "Resonance Audio Spatialization Settings");
}

FColor FAssetTypeActions_ResonanceAudioSpatializationSourceSettings::GetTypeColor() const
{
	return ResonanceAudio::ASSET_COLOR;
}

UClass* FAssetTypeActions_ResonanceAudioSpatializationSourceSettings::GetSupportedClass() const
{
	return UResonanceAudioSpatializationSourceSettings::StaticClass();
}

uint32 FAssetTypeActions_ResonanceAudioSpatializationSourceSettings::GetCategories()
{
	return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_ResonanceAudioSpatializationSourceSettings::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundResonanceSubMenu", "Resonance")
	};

	return SubMenus;
}

UResonanceAudioSpatializationSourceSettingsFactory::UResonanceAudioSpatializationSourceSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UResonanceAudioSpatializationSourceSettings::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UResonanceAudioSpatializationSourceSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("ResonanceAudio.SpatializationSourceSettingsCreated"));
	return Cast<UObject>(NewObject<UResonanceAudioSpatializationSourceSettings>(InParent, InName, Flags));
}

uint32 UResonanceAudioSpatializationSourceSettingsFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}

