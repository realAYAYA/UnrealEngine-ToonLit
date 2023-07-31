//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once
#include "ResonanceAudioEnums.h"
#include "ResonanceAudioSettings.generated.h"


UCLASS(config = Engine, defaultconfig)
class RESONANCEAUDIO_API UResonanceAudioSettings : public UObject
{
	GENERATED_BODY()

public:

	UResonanceAudioSettings();

	// Reference to submix where reverb plugin audio is routed.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb", meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
	FSoftObjectPath OutputSubmix;

	// Global Quality mode to use for directional sound sources.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General)
	ERaQualityMode QualityMode;

	// Default settings for global reverb: This is overridden when a player enters Audio Volumes.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General, meta = (AllowedClasses = "/Script/ResonanceAudio.ResonanceAudioReverbPluginPreset"))
	FSoftObjectPath GlobalReverbPreset;

	// Default settings for global source settings
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General, meta = (AllowedClasses = "/Script/ResonanceAudio.ResonanceAudioSpatializationSourceSettings"))
	FSoftObjectPath GlobalSourcePreset;
};
