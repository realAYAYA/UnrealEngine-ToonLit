//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "PhononCommon.h"
#include "PhononReverbSourceSettings.generated.h"

UCLASS()
class STEAMAUDIO_API UPhononReverbSourceSettings : public UReverbPluginSourceSettingsBase
{
	GENERATED_BODY()

public:
	UPhononReverbSourceSettings();

	// How to simulate source-centric reverb.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReverbSettings, meta = (DisplayName = "Source-Centric Reverb Simulation"))
	EIplSimulationType SourceReverbSimulationType;

	// Scale factor to make the indirect contribution louder or softer.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = ReverbSettings, meta = (DisplayName = "Source-Centric Reverb Contribution", ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float SourceReverbContribution;
};
