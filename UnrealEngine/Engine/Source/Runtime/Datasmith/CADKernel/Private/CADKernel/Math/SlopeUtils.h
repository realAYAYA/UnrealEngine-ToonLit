// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/Point.h"
#include "Algo/AllOf.h"

namespace UE::CADKernel
{

namespace Slope
{

constexpr double NullSlope = 0.;
constexpr double FullSlope = 8.;

/**
 * RightSlope i.e. Right angle i.e Pi / 2
 */
constexpr double RightSlope = 2.;

/**
 * ThreeRightSlope i.e. 3Pi / 2
 */
constexpr double ThreeRightSlope = 6.;

/**
 * MinusRightSlope i.e. -Pi / 2
 */
constexpr double MinusRightSlope = -2.;

/**
 * PiSlope i.e. Pi angle
 */
constexpr double PiSlope = 4.;
}

typedef TFunction<double(const FPoint2D&, const FPoint2D&, double)> SlopMethod;


/**
 * Transform a positive slope into an oriented slope [-4, 4] i.e. an equivalent angle between [-Pi, Pi]
 * @return a slope between [-4, 4]
 */
inline double TransformIntoOrientedSlope(double Slope)
{
	if (Slope > 4.) Slope -= 8;
	if (Slope < -4.) Slope += 8;
	return Slope;
}

/**
 * Transform a slope into a positive slope [0, *] i.e. an equivalent angle between [0, 2.Pi]
 * @return a slope between [0, 8]
 */
inline double TransformIntoPositiveSlope(double Slope)
{
	if (Slope < 0) Slope += 8;
	return Slope;
}

/**
 * Fast angle approximation by a "slope" :
 * This method is used to compute an approximation of the angle between the input segment defined by two points and [0, u) axis.
 * The return value is a real in the interval [0, 8] for an angle in the interval [0, 2Pi]
 * Warning, it's only an approximation... The conversion is not linear but the error is small near the integer value of slope (0, 1, 2, 3, ...8)
 *
 * This approximation is very good when only comparison of angles is needed.
 * This method is not adapted to compute angle
 *
 * To compute an angle value between two segments, the call of acos (and asin for an oriented angle) is necessary while with this approximation, only a division is useful.
 *
 * [0 - 2Pi] is divide into 8 angular sector i.e. [0, Pi/4] = [0,1], [Pi/4, Pi/2] = [1,2], ...
 *
 * @return a slope between [0, 8] i.e. an equivalent angle between [0, 2Pi]
 *
 * Angle (Degree) to Slop
 *		  0   = 0
 *		  7.  = 0.125
 *		 14.  = 0.25
 *		 30   = 0.5
 *		 36.8 = 0.75
 *		 45   = 1
 *		 53.2 = 1.25
 *		 60   = 1.5
 *		 76.  = 1.75
 *		 90   = 2
 *		135   = 3
 *		180   = 4
 *		360   = 8
 */
inline double ComputeSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
{
	double DeltaU = EndPoint.U - StartPoint.U;
	double DeltaV = EndPoint.V - StartPoint.V;
	double Delta;

	if (FMath::Abs(DeltaU) < DOUBLE_SMALL_NUMBER && FMath::Abs(DeltaV) < DOUBLE_SMALL_NUMBER)
	{
		return 0;
	}

	if (DeltaU > DOUBLE_SMALL_NUMBER)
	{
		if (DeltaV > DOUBLE_SMALL_NUMBER)
		{
			if (DeltaU > DeltaV)
			{
				Delta = DeltaV / DeltaU;
			}
			else
			{
				Delta = 2 - DeltaU / DeltaV;
			}
		}
		else
		{
			if (DeltaU > -DeltaV)
			{
				Delta = 8 + DeltaV / DeltaU;
			}
			else
			{
				Delta = 6 - DeltaU / DeltaV; // deltaU/deltaV <0
			}
		}
	}
	else
	{
		if (DeltaV > DOUBLE_SMALL_NUMBER)
		{
			if (-DeltaU > DeltaV)
			{
				Delta = 4 + DeltaV / DeltaU;
			}
			else
			{
				Delta = 2 - DeltaU / DeltaV;
			}
		}
		else
		{
			if (-DeltaU > -DeltaV)
			{
				Delta = 4 + DeltaV / DeltaU;
			}
			else
			{
				Delta = 6 - DeltaU / DeltaV;
			}
		}
	}

	return Delta;
}

/**
 * Compute the oriented slope of a segment according to a reference slope
 * This method is used to compute an approximation of the angle between two segments in 2D.
 * return a slope between [0, 8] i.e. an equivalent angle between [0, 2Pi]
 */
inline double ComputePositiveSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceSlope)
{
	double Slope = ComputeSlope(StartPoint, EndPoint);
	Slope -= ReferenceSlope;
	if (Slope < 0) Slope += 8;
	return Slope;
}

/**
 * Compute the positive slope between the segments [StartPoint, EndPoint1] and [StartPoint, EndPoint2]
 * @return a slope between [0, 8] i.e. an equivalent angle between [0, 2Pi]
 */
inline double ComputePositiveSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint1, const FPoint2D& EndPoint2)
{
	double ReferenceSlope = ComputeSlope(StartPoint, EndPoint1);
	double Slope = ComputeSlope(StartPoint, EndPoint2);
	Slope -= ReferenceSlope;
	return TransformIntoPositiveSlope(Slope);
}

inline double ClockwiseSlop(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceSlope)
{
	return 8. - ComputePositiveSlope(StartPoint, EndPoint, ReferenceSlope);
}

inline double CounterClockwiseSlop(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceSlope)
{
	return ComputePositiveSlope(StartPoint, EndPoint, ReferenceSlope);
}

/**
 * Compute the oriented slope of a segment according to a reference slope
 * @return a slope between [-4, 4] i.e. an equivalent angle between [-Pi, Pi]
 */
inline double ComputeOrientedSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceSlope)
{
	return TransformIntoOrientedSlope(ComputePositiveSlope(StartPoint, EndPoint, ReferenceSlope));
}

/**
 * Compute the positive slope between the segments [StartPoint, EndPoint1] and [StartPoint, EndPoint2]
 * @return a slope between [-4, 4] i.e. an equivalent angle between [-Pi, Pi]
 */
inline double ComputeOrientedSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint1, const FPoint2D& EndPoint2)
{
	return TransformIntoOrientedSlope(ComputePositiveSlope(StartPoint, EndPoint1, EndPoint2));
}

/**
 * return a slope between [0, 4] i.e. an angle between [0, Pi]
 */
inline double ComputeUnorientedSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceSlope)
{
	return FMath::Abs(ComputeOrientedSlope(StartPoint, EndPoint, ReferenceSlope));
}

/**
 * return a slope between [0, 1] relative to the nearest axis between horizontal or vertical axis i.e.
 * ComputeUnorientedSlope => 0.5 return 0.5
 * ComputeUnorientedSlope => 2.3 return 0.3
 * ComputeUnorientedSlope => 3.6 return 0.4
 */
inline double ComputeSlopeRelativeToNearestAxis(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
{
	double Slope = FMath::Abs(TransformIntoOrientedSlope(ComputeSlope(StartPoint, EndPoint)));
	if (Slope > 2)
	{
		Slope = 4 - Slope;
	}

	// if slope close to 2 means segment close to IsoU, otherwise segment close to IsoV
	// Wants a slope between 0 and 1 to manage either IsoU and IsoV
	// Close to 0 means close to IsoU or IsoV
	if (Slope > 1)
	{
		Slope = 2 - Slope;
	}

	return Slope;
}

/**
 * return a slope between [0, 2] relative to reference Axis i.e.
 * ComputeUnorientedSlope => 0.5 return 0.5
 * ComputeUnorientedSlope => 2.3 return 1.7
 */
inline double ComputeSlopeRelativeToReferenceAxis(const FPoint2D& StartPoint, const FPoint2D& EndPoint, double ReferenceAxisSlope)
{
	double Slope = ComputeUnorientedSlope(StartPoint, EndPoint, ReferenceAxisSlope);
	if (Slope > 2)
	{
		Slope = 4 - Slope;
	}

	return Slope;
}

/**
 * return a slope between [0, 4] i.e. an angle between [0, Pi]
 */
inline double ComputeUnorientedSlope(const FPoint2D& StartPoint, const FPoint2D& EndPoint1, const FPoint2D& EndPoint2)
{
	return FMath::Abs(ComputeOrientedSlope(StartPoint, EndPoint1, EndPoint2));
}

/**
 *                         P1
 *          inside        /
 *                       /   inside
 *                      /
 *    A -------------- B --------------- C
 *                      \
 *           Outside     \  Outside
 *                        \
 *                         P2
 *
 * Return true if the segment BP is inside the sector defined the half-lines [BA) and [BC) in the counterclockwise.
 * Return false if ABP angle or PBC angle is too flat (smaller than FlatAngle)
 */
inline bool IsPointPInsideSectorABC(const FPoint2D& PointA, const FPoint2D& PointB, const FPoint2D& PointC, const FPoint2D& PointP, const double FlatAngle)
{
	double SlopWithNextBoundary = ComputeSlope(PointB, PointC);
	double BoundaryDeltaSlope = ComputePositiveSlope(PointB, PointA, SlopWithNextBoundary);
	double SegmentSlope = ComputePositiveSlope(PointB, PointP, SlopWithNextBoundary);
	if (SegmentSlope < FlatAngle || SegmentSlope + FlatAngle > BoundaryDeltaSlope)
	{
		return false;
	}
	return true;
}

/**
 *                         P1
 *          inside        /
 *                       /   inside
 *                      /
 *    A -------------- B --------------- C
 *                      \
 *           Outside     \  Outside
 *                        \
 *                         P2
 *
 * Return true if all of the segment BPi is inside the sector defined the half-lines [BA) and [BC) in the counterclockwise.
 */
inline bool ArePointsInsideSectorABC(const FPoint2D& PointA, const FPoint2D& PointB, const FPoint2D& PointC, const TArray<const FPoint2D*>& Points, const double FlatAngle = -DOUBLE_SMALL_NUMBER)
{
	double SlopWithNextBoundary = ComputeSlope(PointB, PointC);
	double BoundaryDeltaSlope = ComputePositiveSlope(PointB, PointA, SlopWithNextBoundary);

	return Algo::AllOf(Points, [&](const FPoint2D* PointP) {
		double DeltaU = PointB.U - PointP->U;
		double DeltaV = PointB.V - PointP->V;

		if (FMath::Abs(DeltaU) < DOUBLE_SMALL_NUMBER && FMath::Abs(DeltaV) < DOUBLE_SMALL_NUMBER)
		{
			return true;
		}

		double SegmentSlope = ComputePositiveSlope(PointB, *PointP, SlopWithNextBoundary);
		if (SegmentSlope < FlatAngle || SegmentSlope + FlatAngle > BoundaryDeltaSlope)
		{
			return false;
		}
		return true;
		});
}

inline FPoint2D SlopeToVector(const double Slope)
{
	int32 SlopeStep = (int32)(Slope);
	FPoint2D Vector;
	switch (SlopeStep)
	{
	case 0:
		// Delta = DeltaV / DeltaU;
		Vector[0] = 1.;
		Vector[1] = Slope;
		break;
	case 1:
		// 2 - DeltaU / DeltaV;
		Vector[0] = 2. - Slope;
		Vector[1] = 1.;
		break;
	case 2:
		// 2 - DeltaU / DeltaV;
		Vector[0] = 2. - Slope;
		Vector[1] = 1.;
		break;
	case 3:
		// 4 + DeltaV / DeltaU;
		Vector[0] = -1.;
		Vector[1] = 4. - Slope;
		break;
	case 4:
		// 4 + DeltaV / DeltaU;
		Vector[0] = -1.;
		Vector[1] = 4. - Slope;
		break;
	case 5:
		// 6 - DeltaU / DeltaV;
		Vector[0] = Slope - 6.;
		Vector[1] = -1.;
		break;
	case 6:
		// 6 - DeltaU / DeltaV // deltaU/deltaV <0
		Vector[0] = Slope - 6.;
		Vector[1] = -1;
		break;
	case 7:
		// 8 + DeltaV / DeltaU; // deltaU/deltaV <0
		Vector[0] = 1.;
		Vector[1] = Slope - 8.;
		break;
	default:
		break;
	}

	return Vector;
}

} // namespace UE::CADKernel	
