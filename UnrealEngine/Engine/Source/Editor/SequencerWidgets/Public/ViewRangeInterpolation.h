// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** Enum specifying how to interpolate to a new view range */
enum class EViewRangeInterpolation
{
	/** Use an externally defined animated interpolation */
	Animated,
	/** Set the view range immediately */
	Immediate,
};
