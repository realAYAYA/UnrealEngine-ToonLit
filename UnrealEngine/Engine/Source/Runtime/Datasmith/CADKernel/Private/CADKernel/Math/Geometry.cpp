// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Math/Geometry.h"

#include "CADKernel/Core/System.h"
#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Math/MatrixH.h"
#include "CADKernel/Utils/Util.h"

namespace UE::CADKernel
{
namespace IntersectionTool
{
static double IntersectionToolTolerance = 0.01;

void SetTolerance(const double Tolerance)
{
	IntersectionToolTolerance = Tolerance;
}

struct FIntersectionContext
{
	const FSegment2D& SegmentAB;
	const FSegment2D& SegmentCD;
	const FPoint2D AB;
	const FPoint2D CD;
	const FPoint2D CA;

	double NormAB = 0;
	double NormCD = 0;

	FIntersectionContext(const FSegment2D& InSegmentAB, const FSegment2D& InSegmentCD)
		: SegmentAB(InSegmentAB)
		, SegmentCD(InSegmentCD)
		, AB(SegmentAB.GetVector())
		, CD(SegmentCD.GetVector())
		, CA(SegmentAB[0] - SegmentCD[0])
	{
	}
};

bool DoCoincidentSegmentsIntersectInside(double A, double B, double C, double D)
{
	return !((D < A + DOUBLE_KINDA_SMALL_NUMBER) || (B < C + DOUBLE_KINDA_SMALL_NUMBER));
}

bool DoCoincidentSegmentsIntersect(double A, double B, double C, double D)
{
	return !((D < A) || (B < C));
}

constexpr double MinValue(bool OnlyInside)
{
	return OnlyInside ? DOUBLE_KINDA_SMALL_NUMBER : -DOUBLE_KINDA_SMALL_NUMBER;
}

constexpr double MaxValue(bool OnlyInside)
{
	return OnlyInside ? 1. - DOUBLE_KINDA_SMALL_NUMBER : 1. + DOUBLE_KINDA_SMALL_NUMBER;
}

bool ConfirmIntersectionWhenNearlyCoincident(const FPoint2D& AB, const FPoint2D& AC, const FPoint2D& AD, const double NormAB)
{
	double HeightC = (AB ^ AC) / NormAB;
	double HeightD = (AB ^ AD) / NormAB;

	if (fabs(HeightC) < IntersectionToolTolerance || fabs(HeightD) < IntersectionToolTolerance)
	{
		return true;
	}
	return HeightC * HeightD < 0;
};

bool ConfirmIntersectionWhenNearlyCoincident(const FIntersectionContext& Context)
{
	if (Context.NormAB > Context.NormCD)
	{
		const FPoint2D DA = Context.SegmentAB[0] - Context.SegmentCD[1];
		return ConfirmIntersectionWhenNearlyCoincident(Context.AB, Context.CA, DA, Context.NormAB);
	}
	else
	{
		const FPoint2D CB = Context.SegmentAB[1] - Context.SegmentCD[0];
		return ConfirmIntersectionWhenNearlyCoincident(Context.CD, Context.CA, CB, Context.NormCD);
	}
}

}


double ComputeCurvature(const FPoint& Gradient, const FPoint& Laplacian)
{
	const FPoint GradientCopy = Gradient.Normalize();
	const FPoint LaplacianCopy = Laplacian.Normalize();
	const FPoint Normal = GradientCopy ^ LaplacianCopy;
	return (Normal.Length() * Laplacian.Length()) / (Gradient.Length() * Gradient.Length());
}

double ComputeCurvature(const FPoint& Normal, const FPoint& Gradient, const FPoint& Laplacian)
{
	const FPoint GradientCopy = Gradient.Normalize();
	const FPoint LaplacianCopy = Laplacian.Normalize();
	const FPoint NormalCopy = Normal.Normalize();
	const FPoint Coef = (LaplacianCopy ^ GradientCopy) ^ NormalCopy;
	return (Coef.Length() * Laplacian.Length()) / Gradient.SquareLength();
}

void FindLoopIntersectionsWithIso(const EIso Iso, const double IsoParameter, const TArray<TArray<FPoint2D>>& Loops, TArray<double>& OutIntersections)
{
	OutIntersections.Empty(8);

	const int32 UIndex = Iso == EIso::IsoU ? 0 : 1;
	const int32 VIndex = Iso == EIso::IsoU ? 1 : 0;

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

bool DoIntersect(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD, TFunction<bool(double, double, double, double)> DoCoincidentSegmentsIntersect, const double Min, const double Max)
{
	using namespace IntersectionTool;

	TFunction<bool(double, double, double, double)> FastIntersectionTestWhenCoincident = [&DoCoincidentSegmentsIntersect](double A, double B, double C, double D) -> bool
	{
		if (A < B)
		{
			if (C < D)
			{
				return DoCoincidentSegmentsIntersect(A, B, C, D);
			}
			else
			{
				return DoCoincidentSegmentsIntersect(A, B, D, C);
			}
		}
		else
		{
			if (C < D)
			{
				return DoCoincidentSegmentsIntersect(B, A, C, D);
			}
			else
			{
				return DoCoincidentSegmentsIntersect(B, A, D, C);
			}
		}
	};

	FIntersectionContext Context(SegmentAB, SegmentCD);

	const double ParallelCoef = Context.CD ^ Context.AB;
	if (FMath::IsNearlyZero(ParallelCoef, DOUBLE_KINDA_SMALL_NUMBER))
	{
		// double check with normalized vectors
		{
			FPoint2D NormalizedAB = Context.AB;
			FPoint2D NormalizedCD = Context.CD;
			FPoint2D NormalizedCA = Context.CA;

			NormalizedAB.Normalize(Context.NormAB);
			NormalizedCD.Normalize(Context.NormCD);
			NormalizedCA.Normalize();

			const double NormalizedParallelCoef = NormalizedCD ^ NormalizedAB;
			if (FMath::IsNearlyZero(NormalizedParallelCoef, DOUBLE_KINDA_SMALL_NUMBER))
			{
				const double NormalizedParallelCoef2 = NormalizedCA ^ NormalizedAB;
				if (!FMath::IsNearlyZero(NormalizedParallelCoef2, DOUBLE_KINDA_SMALL_NUMBER))
				{
					return false;
				}

				if (fabs(Context.AB.U) > fabs(Context.AB.V))
				{
					if (FastIntersectionTestWhenCoincident(SegmentAB[0].U, SegmentAB[1].U, SegmentCD[0].U, SegmentCD[1].U))
					{
						return ConfirmIntersectionWhenNearlyCoincident(Context);
					}
					else
					{
						return false;
					}
				}
				else
				{
					if (FastIntersectionTestWhenCoincident(SegmentAB[0].V, SegmentAB[1].V, SegmentCD[0].V, SegmentCD[1].V))
					{
						return ConfirmIntersectionWhenNearlyCoincident(Context);
					}
					else
					{
						return false;
					}
				}
			}
		}
	}

	const double ABIntersectionCoordinate = (Context.CA ^ Context.CD) / ParallelCoef;
	const double CDIntersectionCoordinate = (Context.CA ^ Context.AB) / ParallelCoef;
	return (ABIntersectionCoordinate <= Max && ABIntersectionCoordinate >= Min && CDIntersectionCoordinate <= Max && CDIntersectionCoordinate >= Min);
}

bool DoIntersectInside(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD)
{
	return DoIntersect(SegmentAB, SegmentCD, IntersectionTool::DoCoincidentSegmentsIntersectInside, IntersectionTool::MinValue(true), IntersectionTool::MaxValue(true));
}

bool DoIntersect(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD)
{
	return DoIntersect(SegmentAB, SegmentCD, IntersectionTool::DoCoincidentSegmentsIntersect, IntersectionTool::MinValue(false), IntersectionTool::MaxValue(false));
}

}
