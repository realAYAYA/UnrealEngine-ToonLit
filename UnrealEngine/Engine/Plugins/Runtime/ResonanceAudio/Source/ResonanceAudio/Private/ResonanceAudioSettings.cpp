//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#include "ResonanceAudioSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ResonanceAudioSettings)

UResonanceAudioSettings::UResonanceAudioSettings()
	: QualityMode(ERaQualityMode::BINAURAL_HIGH)
	, GlobalReverbPreset(nullptr)
{
	OutputSubmix = FString(TEXT("/ResonanceAudio/ResonanceSubmixDefault.ResonanceSubmixDefault"));
}

