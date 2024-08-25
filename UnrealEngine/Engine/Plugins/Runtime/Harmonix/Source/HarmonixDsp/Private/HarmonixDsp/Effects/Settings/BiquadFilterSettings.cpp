// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/Effects/Settings/BiquadFilterSettings.h"
#include "HarmonixDsp/Effects/BiquadFilter.h"

void FBiquadFilterSettings::GetMagnitudeResponse(
	float const* FrequenciesOfInterest, 
	int32 NumFrequencies,
	float* MagnitudeResponse, 
	float Fs)
{
	Harmonix::Dsp::Effects::FBiquadFilterCoefs Coefs(*this, Fs);
	Coefs.GetMagnitudeResponse(FrequenciesOfInterest, NumFrequencies, MagnitudeResponse);
}