// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorWiggle.h"

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "PropertyAnimatorShared.h"

UPropertyAnimatorWiggle::UPropertyAnimatorWiggle()
{
	static int32 SeedIncrement = 0;

	SetAnimatorDisplayName(DefaultControllerName);

	bRandomTimeOffset = true;
	Seed = SeedIncrement++;
}

float UPropertyAnimatorWiggle::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	const float Frequency = InOptions->GetFrequency() * GlobalFrequency;

	// Apply random wave based on time and frequency
	const double WaveResult = UE::PropertyAnimator::Wave::Perlin(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());

	// Remap from [-1, 1] to user amplitude from [Min, Max]
	return FMath::GetMappedRangeValueClamped(FVector2D(-1, 1), FVector2D(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), WaveResult);
}
