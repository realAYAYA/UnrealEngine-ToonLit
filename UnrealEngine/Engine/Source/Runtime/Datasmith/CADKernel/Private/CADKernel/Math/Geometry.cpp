// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Math/Geometry.h"

#include "CADKernel/Core/KernelParameters.h" 
#include "CADKernel/Core/System.h"
#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Math/MatrixH.h"
#include "CADKernel/Utils/Util.h"

namespace UE::CADKernel
{
double ComputeCurvature(const FPoint& Gradient, const FPoint& Laplacian)
{
	FPoint GradientCopy = Gradient;
	FPoint LaplacianCopy = Laplacian;
	GradientCopy.Normalize();
	LaplacianCopy.Normalize();
	FPoint Normal = GradientCopy ^ LaplacianCopy;
	return (Normal.Length() * Laplacian.Length()) / (Gradient.Length() * Gradient.Length());
}

double ComputeCurvature(const FPoint& Normal, const FPoint& Gradient, const FPoint& Laplacian)
{
	FPoint GradientCopy = Gradient;
	FPoint LaplacianCopy = Laplacian;
	FPoint NormalCopy = Normal;
	GradientCopy.Normalize();
	LaplacianCopy.Normalize();
	NormalCopy.Normalize();
	FPoint Coef = (LaplacianCopy ^ GradientCopy) ^ NormalCopy;
	return (Coef.Length() * Laplacian.Length()) / Gradient.SquareLength();
}

void FindLoopIntersectionsWithIso(const EIso Iso, const double IsoParameter, const TArray<TArray<FPoint2D>>& Loops, TArray<double>& OutIntersections)
{
	OutIntersections.Empty(8);

	int32 UIndex = Iso == EIso::IsoU ? 0 : 1;
	int32 VIndex = Iso == EIso::IsoU ? 1 : 0;

	TFunction<void(const FPoint2D&, const FPoint2D&)> ComputeIntersection = [&](const FPoint2D& Point1, const FPoint2D& Point2)
	{
		if (IsoParameter > Point1[UIndex] && IsoParameter <= Point2[UIndex])
		{
			double Intersection = (IsoParameter - Point1[UIndex]) / (Point2[UIndex] - Point1[UIndex]) * (Point2[VIndex] - Point1[VIndex]) + Point1[VIndex];
			OutIntersections.Add((IsoParameter - Point1[UIndex]) / (Point2[UIndex] - Point1[UIndex]) * (Point2[VIndex] - Point1[VIndex]) + Point1[VIndex]);
		}
	};

	for (const TArray<FPoint2D>& Loop : Loops)
	{
		const FPoint2D* Point1 = &Loop.Last();
		for (const FPoint2D& Point2 : Loop)
		{
			if (!FMath::IsNearlyEqual((*Point1)[UIndex], Point2[UIndex]))
			{
				if ((*Point1)[UIndex] < Point2[UIndex])
				{
					ComputeIntersection(*Point1, Point2);
				}
				else
				{
					ComputeIntersection(Point2, *Point1);
				}
			}
			Point1 = &Point2;
		}
	}
	Algo::Sort(OutIntersections);
}

bool IntersectSegments2D(const TSegment<FPoint2D>& SegmentAB, const TSegment<FPoint2D>& SegmentCD)
{
	constexpr const double Min = -DOUBLE_SMALL_NUMBER;
	constexpr const double Max = 1. + DOUBLE_SMALL_NUMBER;

	TFunction<bool(double, double, double, double)> Intersect = [](double A, double B, double C, double D)
	{
		return !((D < A) || (B < C));
	};

	TFunction<bool(double, double, double, double)> TestWhenParallel = [&](double A, double B, double C, double D)
	{
		if (A < B)
		{
			if (C < D)
			{
				return Intersect(A, B, C, D);
			}
			else
			{
				return Intersect(A, B, D, C);
			}
		}
		else
		{
			if (C < D)
			{
				return Intersect(B, A, C, D);
			}
			else
			{
				return Intersect(B, A, D, C);
			}
		}
	};

	FPoint2D AB = SegmentAB[1] - SegmentAB[0];
	FPoint2D CD = SegmentCD[1] - SegmentCD[0];
	FPoint2D CA = SegmentAB[0] - SegmentCD[0];

	const double ParallelCoef = CD ^ AB;
	if (FMath::IsNearlyZero(ParallelCoef))
	{
		const double ParallelCoef2 = CA ^ AB;
		if (!FMath::IsNearlyZero(ParallelCoef2))
		{
			return false;
		}

		if (fabs(AB.U) > fabs(AB.V))
		{
			return TestWhenParallel(SegmentAB[0].U, SegmentAB[1].U, SegmentCD[0].U, SegmentCD[1].U);
		}
		else
		{
			return TestWhenParallel(SegmentAB[0].V, SegmentAB[1].V, SegmentCD[0].V, SegmentCD[1].V);
		}
	}

	const double ABIntersectionCoordinate = (CA ^ CD) / ParallelCoef;
	const double CDIntersectionCoordinate = (CA ^ AB) / ParallelCoef;
	return (ABIntersectionCoordinate <= Max && ABIntersectionCoordinate >= Min && CDIntersectionCoordinate <= Max && CDIntersectionCoordinate >= Min);
}
}
