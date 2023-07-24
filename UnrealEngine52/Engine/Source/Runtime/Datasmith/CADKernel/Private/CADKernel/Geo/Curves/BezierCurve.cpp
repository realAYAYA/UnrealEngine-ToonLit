// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Geo/Curves/BezierCurve.h"

#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Math/BSpline.h"

namespace UE::CADKernel
{

void FBezierCurve::EvaluatePoint(double Coordinate, FCurvePoint& OutPoint, int32 DerivativeOrder) const
{
	OutPoint.DerivativeOrder = DerivativeOrder;
	OutPoint.Init();

	TArray<double> Bernstein;
	TArray<double> BernsteinD1;
	TArray<double> BernsteinD2;

	BSpline::Bernstein(Poles.Num()-1, Coordinate, Bernstein, BernsteinD1, BernsteinD2);

	for (int32 Index = 0; Index < Poles.Num(); Index++)
	{
		OutPoint.Point += Poles[Index] * Bernstein[Index];
	}

	if (DerivativeOrder > 0)
	{
		for (int32 Index = 0; Index < Poles.Num(); Index++)
		{
			OutPoint.Gradient += Poles[Index] * BernsteinD1[Index];
		}
	}

	if (DerivativeOrder > 1)
	{
		for (int32 Index = 0; Index < Poles.Num(); Index++)
		{
			OutPoint.Laplacian += Poles[Index] * BernsteinD2[Index];
		}
	}
}

TSharedPtr<FEntityGeom> FBezierCurve::ApplyMatrix(const FMatrixH& InMatrix) const
{
	TArray<FPoint> TransformedPoles;
	TransformedPoles.Reserve(Poles.Num());

	for (const FPoint& Pole : Poles)
	{
		TransformedPoles.Emplace(InMatrix.Multiply(Pole));
	}

	return FEntity::MakeShared<FBezierCurve>(TransformedPoles);
}

void FBezierCurve::Offset(const FPoint& OffsetDirection)
{
	for (FPoint& Pole : Poles)
	{
		Pole += OffsetDirection;
	}
}

#ifdef CADKERNEL_DEV
FInfoEntity& FBezierCurve::GetInfo(FInfoEntity& Info) const
{
	return FCurve::GetInfo(Info)
		.Add(TEXT("degre"), Poles.Num() - 1)
		.Add(TEXT("poles"), Poles);
}
#endif

void FBezierCurve::ExtendTo(const FPoint& Point)
{
	PolylineTools::ExtendTo(Poles, Point);
}

} // namespace UE::CADKernel

