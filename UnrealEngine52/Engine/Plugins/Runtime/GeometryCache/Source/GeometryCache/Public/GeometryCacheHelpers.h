// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/UnrealMathUtility.h"

struct GeometyCacheHelpers 
{
	/**
		Use this instead of fmod when working with looping animations as fmod gives incorrect results when using negative times.
	*/
	static inline float WrapAnimationTime(float Time, float Duration)
	{
		return Time - Duration * FMath::FloorToFloat(Time / Duration);
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Math/UnrealMath.h"
#endif
