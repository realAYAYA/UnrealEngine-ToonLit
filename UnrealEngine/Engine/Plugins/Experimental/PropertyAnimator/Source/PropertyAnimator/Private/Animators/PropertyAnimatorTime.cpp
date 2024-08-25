// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorTime.h"

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"

UPropertyAnimatorTime::UPropertyAnimatorTime()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

float UPropertyAnimatorTime::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	const float Frequency = InOptions->GetFrequency() * GlobalFrequency;

	const double TimePeriod = 1.f / Frequency;

	const double TimeProgress = FMath::Fmod(InTimeElapsed + InOptions->GetTimeOffset(), TimePeriod);

	return FMath::GetMappedRangeValueClamped(FVector2f(0, TimePeriod), FVector2f(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), TimeProgress);
}
