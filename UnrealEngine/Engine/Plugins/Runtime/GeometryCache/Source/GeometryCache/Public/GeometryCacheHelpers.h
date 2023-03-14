// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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