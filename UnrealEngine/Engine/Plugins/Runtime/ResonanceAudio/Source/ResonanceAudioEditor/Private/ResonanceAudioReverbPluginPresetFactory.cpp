//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#include "ResonanceAudioReverbPluginPresetFactory.h"
#include "ResonanceAudioReverb.h"

#include "AudioAnalytics.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ResonanceAudioReverbPluginPresetFactory)

FText FAssetTypeActions_ResonanceAudioReverbPluginPreset::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ResonanceAudioReverbPluginPreset", "Resonance Audio Reverb Settings");
}

FColor FAssetTypeActions_ResonanceAudioReverbPluginPreset::GetTypeColor() const
{
	return ResonanceAudio::ASSET_COLOR;
}

UClass* FAssetTypeActions_ResonanceAudioReverbPluginPreset::GetSupportedClass() const
{
	return UResonanceAudioReverbPluginPreset::StaticClass();
}

uint32 FAssetTypeActions_ResonanceAudioReverbPluginPreset::GetCategories()
{
	return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_ResonanceAudioReverbPluginPreset::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundResonanceSubMenu", "Resonance")
	};

	return SubMenus;
}

UResonanceAudioReverbPluginPresetFactory::UResonanceAudioReverbPluginPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UResonanceAudioReverbPluginPreset::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UResonanceAudioReverbPluginPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	Audio::Analytics::RecordEvent_Usage(TEXT("ResonanceAudio.ReverbPresetCreated"));
	return NewObject<UResonanceAudioReverbPluginPreset>(InParent, InName, Flags);
}

uint32 UResonanceAudioReverbPluginPresetFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Sounds;
}

