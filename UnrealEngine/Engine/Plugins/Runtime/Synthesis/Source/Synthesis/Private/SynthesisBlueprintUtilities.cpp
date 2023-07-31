// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynthesisBlueprintUtilities.h"
#include "DSP/Dsp.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SynthesisBlueprintUtilities)

float USynthesisUtilitiesBlueprintFunctionLibrary::GetLogFrequency(float InLinearValue, float InDomainMin, float InDomainMax, float InRangeMin, float InRangeMax)
{
	return Audio::GetLogFrequencyClamped(InLinearValue, { InDomainMin, InDomainMax }, { InRangeMin, InRangeMax });
}

float USynthesisUtilitiesBlueprintFunctionLibrary::GetLinearFrequency(float InLogFrequencyValue, float InDomainMin, float InDomainMax, float InRangeMin, float InRangeMax)
{
	return Audio::GetLinearFrequencyClamped(InLogFrequencyValue, { InDomainMin, InDomainMax }, { InRangeMin, InRangeMax });
}



