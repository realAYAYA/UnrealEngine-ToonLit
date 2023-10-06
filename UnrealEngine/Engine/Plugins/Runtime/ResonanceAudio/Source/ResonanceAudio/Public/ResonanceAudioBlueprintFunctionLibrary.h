//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ResonanceAudioBlueprintFunctionLibrary.generated.h"

class UResonanceAudioReverbPluginPreset;

UCLASS()
class RESONANCEAUDIO_API UResonanceAudioBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// This function overrides the Global Reverb Preset for Resonance Audio
	UFUNCTION(BlueprintCallable, Category = "ResonanceAudio|GlobalReverbPreset")
	static void SetGlobalReverbPreset(UResonanceAudioReverbPluginPreset* InPreset);

	// This function retrieves the Global Reverb Preset for Resonance Audio
	UFUNCTION(BlueprintCallable, Category = "ResonanceAudio|GlobalReverbPreset")
	static UResonanceAudioReverbPluginPreset* GetGlobalReverbPreset();
};
