// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Surfaces/PlaneSurface.h"

#include "CADKernel/Core/System.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Geo/Sampling/SurfacicSampling.h"

namespace UE::CADKernel
{

FPlaneSurface::FPlaneSurface(const double InToleranceGeometric, const FMatrixH& InMatrix, const FSurfacicBoundary& InBoundary)
	: FSurface(InToleranceGeometric, InBoundary)
{
	Matrix = InMatrix;

	ensureCADKernel(FMath::IsNearlyZero(Matrix.Get(3, 0)) && FMath::IsNearlyZero(Matrix.Get(3, 1)) && FMath::IsNearlyZero(Matrix.Get(3, 2)));

	InverseMatrix = Matrix;
	InverseMatrix.Inverse();
	ComputeMinToleranceIso();
}

FPlaneSurface::FPlaneSurface(const double InToleranceGeometric, const FPoint& InPosition, FPoint InNormal, const FSurfacicBoundary& InBoundary)
	: FSurface(InToleranceGeometric, InBoundary)
{
	InNormal.Normalize();
	Matrix.FromAxisOrigin(InNormal, InPosition);

	InverseMatrix = Matrix;
	InverseMatrix.Inverse();
	ComputeMinToleranceIso();
}

FPlane FPlaneSurface::GetPlane() const
{
	FSurfacicPoint Point3D;
	EvaluatePoint(FPoint2D::ZeroPoint, Point3D, 0);
	FPoint Normal = Matrix.Column(2);
	return FPlane(Point3D.Point, Normal);
}

void FPlaneSurface::EvaluatePoint(const FPoint2D& InSurfacicCoordinate, FSurfacicPoint& OutPoint3D, int32 InDerivativeOrder) const
{
	OutPoint3D.DerivativeOrder = InDerivativeOrder;
	OutPoint3D.Point = Matrix.Multiply(InSurfacicCoordinate);
	if(InDerivativeOrder>0) 
	{
		OutPoint3D.GradientU = Matrix.Column(0);
		OutPoint3D.GradientV = Matrix.Column(1);
	}

	if(InDerivativeOrder>1) 
	{
		OutPoint3D.LaplacianU = FPoint::ZeroPoint;
		OutPoint3D.LaplacianV = FPoint::ZeroPoint;
		OutPoint3D.LaplacianUV = FPoint::ZeroPoint;
	}
}

void FPlaneSurface::EvaluatePoints(const TArray<FPoint2D>& InSurfacicCoordinates, TArray<FSurfacicPoint>& OutPoint3D, int32 InDerivativeOrder) const
{
	int32 PointNum = InSurfacicCoordinates.Num();
	OutPoint3D.SetNum(PointNum);

	for (int32 Index = 0; Index < PointNum; ++Index)
	{
		OutPoint3D[Index].DerivativeOrder = InDerivativeOrder;
	}

	for (int32 Index = 0; Index < PointNum; ++Index)
	{
		OutPoint3D[Index].Point = Matrix.Multiply(InSurfacicCoordinates[Index]);
	}

	if (InDerivativeOrder > 0)
	{
		FPoint GradientU(Matrix.Column(0));
		FPoint GradientV(Matrix.Column(1));

		for (int32 Index = 0; Index < PointNum; ++Index)
		{
			OutPoint3D[Index].GradientU = GradientU;
		}

		for (int32 Index = 0; Index < PointNum; ++Index)
		{
			OutPoint3D[Index].GradientV = GradientV;
		}
	}
}

void FPlaneSurface::EvaluatePointGrid(const FCoordinateGrid& Coordinates, FSurfacicSampling& OutPoints, bool bComputeNormals) const
{
	OutPoints.bWithNormals = bComputeNormals;

	int32 PointNum = Coordinates.Count();
	OutPoints.Reserve(PointNum);

	OutPoints.Set2DCoordinates(Coordinates);

	for (FPoint Point : OutPoints.Points2D)
	{
		OutPoints.Points3D.Emplace(Matrix.Multiply(Point));
	}

	if(bComputeNormals)
	{
		FPoint Normal(Matrix.Column(2));
		OutPoints.Normals.Init(Normal, PointNum);
	}
}

FPoint FPlaneSurface::ProjectPoint(const FPoint& Point, FPoint* OutProjectedPoint) const
{
	FPoint PointCoordinate = InverseMatrix.Multiply(Point);
	PointCoordinate.Z = 0.0;

	if(OutProjectedPoint)
	{
		*OutProjectedPoint = Matrix.Multiply(PointCoordinate);
	}

	return PointCoordinate;
}

void FPlaneSurface::ProjectPoints(const TArray<FPoint>& Points, TArray<FPoint>* PointCoordinates, TArray<FPoint>* OutProjectedPointS) const
{
	PointCoordinates->Reserve(Points.Num());
	if(OutProjectedPointS) 
	{
		OutProjectedPointS->Reserve(Points.Num());
	}

	for (const FPoint& Point : Points)
	{
		FPoint& PointCoordinate = PointCoordinates->Emplace_GetRef(InverseMatrix.Multiply(Point));
		PointCoordinate.Z = 0.0;
	}

	if (OutProjectedPointS)
	{
		for (const FPoint& PointCoordinate : *PointCoordinates)
		{
			OutProjectedPointS->Emplace(Matrix.Multiply(PointCoordinate));
		}
	}
}

TSharedPtr<FEntityGeom> FPlaneSurface::ApplyMatrix(const FMatrixH& InMatrix) const
{
	FMatrixH NewMatrix = InMatrix * Matrix;
	return FEntity::MakeShared<FPlaneSurface>(Tolerance3D, NewMatrix, Boundary);
}

#ifdef CADKERNEL_DEV
FInfoEntity& FPlaneSurface::GetInfo(FInfoEntity& Info) const
{
	return FSurface::GetInfo(Info)
		.Add(TEXT("Matrix"), Matrix)
		.Add(TEXT("Inverse"), InverseMatrix);
}
#endif

}