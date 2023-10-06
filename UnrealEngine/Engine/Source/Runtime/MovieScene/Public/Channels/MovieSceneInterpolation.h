// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "Misc/FrameRate.h"
#include "Misc/TVariant.h"


/**
 * This file contains structures that represent specific interpolation algorithms for either a continuous or discrete range.
 *
 * They are used by Sequencer evaluation to bypass expensive piecewise data searching every frame by caching the resulting
 * interpolation over the relevant time range which allows it to perform only the minimum computation required to find a result.
 *
 * FCachedInterpolation is a variant type that can represent any one of the supported interpolation modes in this file.
 */
namespace UE::MovieScene::Interpolation
{

/**
 * Sentinal type that represents an invalid interpolation value.
 * Only used when a curve has no data whatsoever, and therefore cannot be evaluated
 */
struct FInvalidValue
{
};


/**
 * Structure representing a constant value.
 * Temporarily includes a flag to determine whether it needs to be re-evaluated for legacy fallback.
 */
struct FConstantValue
{
	/** The constant value */
	double Value;

	FConstantValue(double InValue)
		: Value(InValue)
	{}
};

/**
 * Structure representing a linear interpolation of the form f(t) = a(t-o) + b.
 */
struct FLinearInterpolation
{
	/** The coeffient 'a' in f(t) = a(t-o) + b */
	double Coefficient;
	/** The constant 'c' in f(t) = a(t-o) + b */
	double Constant;
	/** The origin 'o' in f(t) = a(t-o) + b */
	FFrameNumber Origin;

	FLinearInterpolation(FFrameNumber InOrigin, double InCoefficient, double InConstant)
		: Coefficient(InCoefficient)
		, Constant(InConstant)
		, Origin(InOrigin)
	{}

	/**
	 * Evaluate the expression
	 */
	double Evaluate(FFrameTime InTime) const
	{
		return Coefficient*(InTime - Origin).AsDecimal() + Constant;
	}
};

/**
 * A cubic bezier interpolation between 2 control points with tangents, represented as 4 control points on a Bezier curve
 */
struct FCubicInterpolation
{
	/** The delta value between the two control points in the time-domain */
	double DX;
	/** The four control points that should be passed to BezierInterp */
	double P0, P1, P2, P3;
	/** The origin time of the first control point */
	FFrameNumber Origin;

	MOVIESCENE_API FCubicInterpolation(
		FFrameNumber InOrigin,
		double InDX,
		double InStartValue,
		double InEndValue,
		double InStartTangent,
		double InEndTangent);

	/**
	 * Evaluate the expression
	 */
	MOVIESCENE_API double Evaluate(FFrameTime InTime) const;
};

/**
 * A weighted cubic bezier interpolation between 2 control points with weighted tangents
 */
struct FWeightedCubicInterpolation
{
	double DX;
	double StartKeyValue;
	double NormalizedStartTanDX;
	double StartKeyTanY;
	double StartWeight;

	double EndKeyValue;
	double NormalizedEndTanDX;
	double EndKeyTanY;
	double EndWeight;

	FFrameNumber Origin;

	FWeightedCubicInterpolation(
		FFrameRate TickResolution,
		FFrameNumber InOrigin,

		FFrameNumber StartTime, 
		double StartValue,
		double StartTangent,
		double StartTangentWeight,
		bool bStartIsWeighted,

		FFrameNumber EndTime, 
		double EndValue,
		double EndTangent,
		double EndTangentWeight,
		bool bEndIsWeighted);

	double Evaluate(FFrameTime InTime) const;
};

/**
 * Simple 1 dimensional range based off a FFrameNumber to define the range within which a cached interpolation is valid
 */
struct FCachedInterpolationRange
{
	/** Make an empty range */
	static MOVIESCENE_API FCachedInterpolationRange Empty();
	/** Make finite range from InStart to InEnd, not including the end frame */
	static MOVIESCENE_API FCachedInterpolationRange Finite(FFrameNumber InStart, FFrameNumber InEnd);
	/** Make an infinite range */
	static MOVIESCENE_API FCachedInterpolationRange Infinite();

	/** Make a range that only contains the specified time */
	static MOVIESCENE_API FCachedInterpolationRange Only(FFrameNumber InTime);
	/** Make a range that covers all times from (and including) the specified start */
	static MOVIESCENE_API FCachedInterpolationRange From(FFrameNumber InStart);
	/** Make a range that covers all times up to (but not including) the specified end */
	static MOVIESCENE_API FCachedInterpolationRange Until(FFrameNumber InEnd);

	MOVIESCENE_API bool Contains(FFrameNumber FrameNumber) const;

	/** Inclusive start frame */
	FFrameNumber Start;
	/** Exclusive end frame (unless End == Max()) */
	FFrameNumber End;
};

/**
 * Variant structure that wraps an interpolation and the range within which it is valid.
 * ~96 bytes
 */
struct FCachedInterpolation
{
	/** Default construction to an invalid state. Calling Evaluate will always return false */
	MOVIESCENE_API FCachedInterpolation();
	/** Construction as a constant value */
	MOVIESCENE_API FCachedInterpolation(const FCachedInterpolationRange& InRange, const FConstantValue& Constant);
	/** Construction as a linear interpolation */
	MOVIESCENE_API FCachedInterpolation(const FCachedInterpolationRange& InRange, const FLinearInterpolation& Linear);
	/** Construction as a cubic interpolation */
	MOVIESCENE_API FCachedInterpolation(const FCachedInterpolationRange& InRange, const FCubicInterpolation& Cubic);
	/** Construction as a weighted cubic interpolation */
	MOVIESCENE_API FCachedInterpolation(const FCachedInterpolationRange& InRange, const FWeightedCubicInterpolation& WeightedCubic);

	/**
	 * Check whether this cache is still valid for the specified frame number.
	 * @return true if this interpolation is relevant to the frame, or false if it should be re-generated
	 */
	MOVIESCENE_API bool IsCacheValidForTime(FFrameNumber FrameNumber) const;

	/**
	 * Evaluate this interpolation for the specified frame time
	 *
	 * @param FrameTime  The time to evaluate at
	 * @param OutResult  Reference to recieve the resulting value. Only written to if this function returns true.
	 * @return true if this interpolation evaluated successfully (and the result written to OutResult), false otherwise.
	 */
	MOVIESCENE_API bool Evaluate(FFrameTime FrameTime, double& OutResult) const;

private:

	/** Variant containing the actual interpolation implementation */
	TVariant<FInvalidValue,
		FConstantValue,
		FLinearInterpolation,
		FCubicInterpolation,
		FWeightedCubicInterpolation> Data;

	/** Structure representint the range of times this interpolation applies to */
	FCachedInterpolationRange Range;
};

} // namespace UE::MovieScene
