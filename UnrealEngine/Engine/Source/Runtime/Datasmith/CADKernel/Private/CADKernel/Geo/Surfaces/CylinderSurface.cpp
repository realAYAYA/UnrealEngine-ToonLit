// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Surfaces/CylinderSurface.h"

#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Geo/Sampling/SurfacicSampling.h"

namespace UE::CADKernel
{

FCylinderSurface::FCylinderSurface(const double InToleranceGeometric, const FMatrixH& InMatrix, double InRadius, double InStartLength, double InEndLength, double InStartAngle, double InEndAngle)
	: FCylinderSurface(InToleranceGeometric, InMatrix, InRadius, FSurfacicBoundary(InStartAngle, InEndAngle, InStartLength, InEndLength))
{
}

FCylinderSurface::FCylinderSurface(const double InToleranceGeometric, const FMatrixH& InMatrix, const double InRadius, const FSurfacicBoundary& InBoundary)
: FSurface(InToleranceGeometric, InBoundary)
, Matrix(InMatrix)
, Radius(InRadius)
{
	ComputeMinToleranceIso();
}

void FCylinderSurface::InitBoundary()
{
}

void FCylinderSurface::EvaluatePoint(const FPoint2D& InPoint2D, FSurfacicPoint& OutPoint3D, int32 InDerivativeOrder) const
{
	OutPoint3D.DerivativeOrder = InDerivativeOrder;

	OutPoint3D.Point.Set(Radius*(cos(InPoint2D.U)), Radius*(sin(InPoint2D.U)), InPoint2D.V);
	OutPoint3D.Point = Matrix.Multiply(OutPoint3D.Point);

	if(InDerivativeOrder>0) 
	{
		OutPoint3D.GradientU = FPoint(Radius*(-sin(InPoint2D.U)), Radius*(cos(InPoint2D.U)), 0.0);
		OutPoint3D.GradientV = FPoint(0.0, 0.0, 1.0);

		OutPoint3D.GradientU = Matrix.MultiplyVector(OutPoint3D.GradientU);
		OutPoint3D.GradientV = Matrix.MultiplyVector(OutPoint3D.GradientV);

		if (InDerivativeOrder > 1) 
		{
			OutPoint3D.LaplacianU = FPoint(Radius * (-cos(InPoint2D.U)), Radius * (-sin(InPoint2D.U)), 0.0);
			OutPoint3D.LaplacianU = Matrix.MultiplyVector(OutPoint3D.LaplacianU);

			OutPoint3D.LaplacianV = FPoint(0.0, 0.0, 0.0);
			OutPoint3D.LaplacianUV = FPoint(0.0, 0.0, 0.0);
		}
	}
}

TSharedPtr<FEntityGeom> FCylinderSurface::ApplyMatrix(const FMatrixH& InMatrix) const
{
	FMatrixH NewMatrix = InMatrix * Matrix;
	return FEntity::MakeShared<FCylinderSurface>(Tolerance3D, NewMatrix, Radius, Boundary[EIso::IsoV].Min, Boundary[EIso::IsoU].Max, Boundary[EIso::IsoU].Min, Boundary[EIso::IsoU].Max);
}

#ifdef CADKERNEL_DEV
FInfoEntity& FCylinderSurface::GetInfo(FInfoEntity& Info) const
{
	return FSurface::GetInfo(Info).Add(TEXT("Matrix"), Matrix)
							 .Add(TEXT("Radius"), Radius)
							 .Add(TEXT("StartAngle"), Boundary[EIso::IsoU].Min)
							 .Add(TEXT("EndAngle"), Boundary[EIso::IsoU].Max)
							 .Add(TEXT("StartLength"), Boundary[EIso::IsoV].Min)
							 .Add(TEXT("EndLength"), Boundary[EIso::IsoV].Max);
}
#endif

}