// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorBounce.h"

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "PropertyAnimatorShared.h"

UPropertyAnimatorBounce::UPropertyAnimatorBounce()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorBounce::SetInvertEffect(bool bInvert)
{
	if (bInvertEffect == bInvert)
	{
		return;
	}

	bInvertEffect = bInvert;
	OnInvertEffect();
}

float UPropertyAnimatorBounce::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	const float Frequency = InOptions->GetFrequency() * GlobalFrequency;
	double TimeProgress = FMath::Fmod(InTimeElapsed + InOptions->GetTimeOffset(), 1.f / Frequency);

	TimeProgress = bInvertEffect ? TimeProgress : 1 - TimeProgress;

	const double EasingValue = UE::PropertyAnimator::Easing::Bounce(TimeProgress, EPropertyAnimatorEasingType::In);

	// Remap from [0, 1] to user amplitude from [Min, Max]
	return FMath::GetMappedRangeValueClamped(FVector2D(0, 1), FVector2D(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), EasingValue);
}
