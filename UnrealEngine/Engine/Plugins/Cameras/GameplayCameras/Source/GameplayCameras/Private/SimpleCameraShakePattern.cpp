// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleCameraShakePattern.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimpleCameraShakePattern)

void USimpleCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	if (Duration > 0.f)
	{
		OutInfo.Duration = Duration;
	}
	else
	{
		OutInfo.Duration = FCameraShakeDuration::Infinite();
	}

	OutInfo.BlendIn = BlendInTime;
	OutInfo.BlendOut = BlendOutTime;
}


