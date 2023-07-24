// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioSettings.h"

UOculusAudioSettings::UOculusAudioSettings() :
	ReverbWetLevel(0.0f)
	, EarlyReflections(true)
	, LateReverberation(false)
	, PropagationQuality(1.0f)
	, Width(8.0f)
	, Height(3.0f)
	, Depth(5.0f)
	, ReflectionCoefRight(0.25f)
	, ReflectionCoefLeft(0.25f)
	, ReflectionCoefUp(0.5f)
	, ReflectionCoefDown(0.1f)
	, ReflectionCoefBack(0.25f)
	, ReflectionCoefFront(0.25f)
{
	OutputSubmix = FString(TEXT("/OculusAudio/OculusSubmixDefault.OculusSubmixDefault"));
}

#if WITH_EDITOR
void UOculusAudioSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// TODO notify

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UOculusAudioSettings::CanEditChange(const FProperty* InProperty) const
{
	// TODO disable settings when reflection engine is disabled

	return Super::CanEditChange(InProperty);
}
#endif