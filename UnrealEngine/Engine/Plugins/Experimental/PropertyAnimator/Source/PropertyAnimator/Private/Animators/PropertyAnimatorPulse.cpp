// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorPulse.h"

#include "Properties/PropertyAnimatorFloatContext.h"
#include "PropertyAnimatorShared.h"

UPropertyAnimatorPulse::UPropertyAnimatorPulse()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorPulse::SetEasingFunction(EPropertyAnimatorEasingFunction InEasingFunction)
{
	EasingFunction = InEasingFunction;
}

void UPropertyAnimatorPulse::SetEasingType(EPropertyAnimatorEasingType InEasingType)
{
	EasingType = InEasingType;
}

float UPropertyAnimatorPulse::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	const float Frequency = InOptions->GetFrequency() * GlobalFrequency;

	const double WaveResult = UE::PropertyAnimator::Wave::Triangle(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());

	// Result of wave functions is [-1, 1] -> remap to [0, 1] range for easing functions
	const float NormalizedWaveProgress = FMath::GetMappedRangeValueClamped(FVector2D(-1, 1), FVector2D(0, 1), WaveResult);

	// Apply easing function on normalized progress
	const float EasingResult = UE::PropertyAnimator::Easing::Ease(NormalizedWaveProgress, EasingFunction, EasingType);

	// Remap from [0, 1] to user amplitude from [Min, Max]
	return FMath::GetMappedRangeValueClamped(FVector2D(0, 1), FVector2D(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), EasingResult);
}
