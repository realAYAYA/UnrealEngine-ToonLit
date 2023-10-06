// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Geo/Curves/Curve.h"

#include "CADKernel/Core/System.h"
#include "CADKernel/Geo/Curves/BoundedCurve.h"
#include "CADKernel/Geo/Sampling/Polyline.h"
#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Geo/Sampling/SurfacicPolyline.h"
#include "CADKernel/Geo/Sampler/SamplerOnChord.h"
#include "CADKernel/Geo/Sampler/SamplerOnParam.h"
#include "CADKernel/UI/Display.h"

namespace UE::CADKernel
{
double FCurve::GetLength(double Tolerance) const
{
	if (!GlobalLength.IsValid())
	{
		switch (Dimension)
		{
		case 3:
			GlobalLength = ComputeLength(Boundary, Tolerance);
			break;
		case 2:
			GlobalLength = ComputeLength2D(Boundary, Tolerance);
			break;
		default:
			GlobalLength = 0;
			break;
		}
	}
	return GlobalLength;
}

void FCurve::EvaluatePoints(const TArray<double>& Coordinates, TArray<FCurvePoint>& OutPoints, int32 DerivativeOrder) const
{
	OutPoints.SetNum(Coordinates.Num());
	for (int32 iPoint = 0; iPoint < Coordinates.Num(); iPoint++)
	{
		EvaluatePoint(Coordinates[iPoint], OutPoints[iPoint], DerivativeOrder);
	}
}

void FCurve::EvaluatePoints(const TArray<double>& Coordinates, TArray<FPoint>& OutPoints) const
{
	OutPoints.Empty(Coordinates.Num());
	for (double Coordinate : Coordinates)
	{
		FCurvePoint Point;
		EvaluatePoint(Coordinate, Point, 0);
		OutPoints.Emplace(Point.Point);
	}
}

void FCurve::Evaluate2DPoints(const TArray<double>& Coordinates, TArray<FCurvePoint2D>& OutPoints, int32 DerivativeOrder) const
{
	OutPoints.SetNum(Coordinates.Num());
	for (int32 iPoint = 0; iPoint < Coordinates.Num(); iPoint++)
	{
		Evaluate2DPoint(Coordinates[iPoint], OutPoints[iPoint], DerivativeOrder);
	}
}

/**
 * Evaluate exact 2D points of the curve at the input Coordinates
 * The function can only be used with 2D curve (Dimension == 2)
 */
void FCurve::Evaluate2DPoints(const TArray<double>& Coordinates, TArray<FPoint2D>& OutPoints) const
{
	OutPoints.SetNum(Coordinates.Num());
	for (int32 iPoint = 0; iPoint < Coordinates.Num(); iPoint++)
	{
		Evaluate2DPoint(Coordinates[iPoint], OutPoints[iPoint]);
	}
}

#ifdef CADKERNEL_DEV
FInfoEntity& FCurve::GetInfo(FInfoEntity& Info) const
{
	return FEntity::GetInfo(Info)
		.Add(TEXT("Curve type"), CurvesTypesNames[(uint8)GetCurveType()])
		.Add(TEXT("Dimension"), (int32)Dimension)
		.Add(TEXT("Boundary"), Boundary)
		.Add(TEXT("Length"), GetLength(0.01));
}
#endif

void FCurve::FindNotDerivableCoordinates(int32 DerivativeOrder, TArray<double>& OutNotDerivableCoordinates) const
{
	FindNotDerivableCoordinates(Boundary, DerivativeOrder, OutNotDerivableCoordinates);
}

void FCurve::FindNotDerivableCoordinates(const FLinearBoundary& InBoundary, int32 DerivativeOrder, TArray<double>& OutNotDerivableCoordinates) const
{
}

TSharedPtr<FCurve> FCurve::Rebound(const FLinearBoundary& InBoundary)
{
	if (FMath::IsNearlyEqual(InBoundary.Min, GetUMin()) && FMath::IsNearlyEqual(InBoundary.Max, GetUMax()))
	{
		FMessage::Printf(Debug, TEXT("Invalid rebound (UMin and UMax are nearly equal) on curve %d\n"), GetId());
		TSharedPtr<FEntity> Entity = AsShared();
		return StaticCastSharedPtr<FCurve>(Entity);
	}

	ensureCADKernel(false);
	return TSharedPtr<FCurve>();
}

TSharedPtr<FCurve> FCurve::MakeBoundedCurve(const FLinearBoundary& InBoundary)
{
	FLinearBoundary NewBoundary = InBoundary;
	if (NewBoundary.Min < GetUMin())
	{
		NewBoundary.Min = GetUMin();
	}

	if (NewBoundary.Max > GetUMax())
	{
		NewBoundary.Max = GetUMax();
	}

	if (NewBoundary.IsDegenerated())
	{
		FMessage::Printf(Log, TEXT("Invalid bounds (u1=%f u2=%f) on curve %d\n"), NewBoundary.Min, NewBoundary.Max, GetId());
		return TSharedPtr<FCurve>();
	}

	if (FMath::IsNearlyEqual(NewBoundary.Min, GetUMin()) && FMath::IsNearlyEqual(NewBoundary.Max, GetUMax()))
	{
		FMessage::Printf(Debug, TEXT("Invalid rebound (UMin and UMax are nearly equal) on curve %d\n"), GetId());
		return TSharedPtr<FCurve>();
	}

	return FEntity::MakeShared<FBoundedCurve>(StaticCastSharedRef<FCurve>(AsShared()), NewBoundary, Dimension);
}

double FCurve::ComputeLength(const FLinearBoundary& InBoundary, double Tolerance) const
{
	FPolyline3D Polyline;
	FCurveSamplerOnChord Sampler(*this, Boundary, Tolerance, Polyline);
	Sampler.Sample();
	return Polyline.GetLength(Boundary);
}

double FCurve::ComputeLength2D(const FLinearBoundary& InBoundary, double Tolerance) const
{
	FPolyline2D Polyline;
	FCurve2DSamplerOnChord Sampler(*this, Boundary, Tolerance, Polyline);
	Sampler.Sample();
	return Polyline.GetLength(Boundary);
}

void FCurve::Presample(const FLinearBoundary& InBoundary, double Tolerance, TArray<double>& OutSampling) const
{
	FPolyline3D Presampling;
	FCurveSamplerOnParam Sampler(*this, Boundary, Tolerance * 10., Tolerance, Presampling);
	Sampler.Sample();

	Presampling.SwapCoordinates(OutSampling);
}

} // namespace UE::CADKernel
