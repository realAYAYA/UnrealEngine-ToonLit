// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Math/Point.h"

namespace UE::CADKernel
{
struct FCurvePoint2D
{
	int32 DerivativeOrder = -1;
	FPoint2D Point = FPoint2D::ZeroPoint;
	FPoint2D Gradient = FPoint2D::ZeroPoint;
	FPoint2D Laplacian = FPoint2D::ZeroPoint;
};

struct FSurfacicPoint
{
	int32 DerivativeOrder = -1;
	FPoint Point = FPoint::ZeroPoint;
	FPoint GradientU = FPoint::ZeroPoint;
	FPoint GradientV = FPoint::ZeroPoint;
	FPoint LaplacianU = FPoint::ZeroPoint;
	FPoint LaplacianV = FPoint::ZeroPoint;
	FPoint LaplacianUV = FPoint::ZeroPoint;
};

struct FSurfacicCurvePoint
{
	bool bWithNormals;
	bool bWithTangent;

	FPoint2D Point2D = FPoint2D::ZeroPoint;
	FPoint Point = FPoint::ZeroPoint;
	FPoint Normal = FPoint::ZeroPoint;
	FPoint Tangent = FPoint::ZeroPoint;
};

struct FSurfacicCurvePointWithTolerance
{
	FPoint2D Point2D = FPoint2D::ZeroPoint;
	FPoint Point = FPoint::ZeroPoint;
	FSurfacicTolerance Tolerance = FPoint2D::FarawayPoint;
};

typedef FSurfacicCurvePointWithTolerance FSurfacicCurveExtremities[2];

struct FCurvePoint
{
	int32 DerivativeOrder = -1;
	FPoint Point = FPoint::ZeroPoint;
	FPoint Gradient = FPoint::ZeroPoint;
	FPoint Laplacian = FPoint::ZeroPoint;

	FCurvePoint() = default;

	FCurvePoint(FPoint InPoint)
		: Point(InPoint)
	{
		Gradient = FPoint::ZeroPoint;
		Laplacian = FPoint::ZeroPoint;
	}

	void Init()
	{
		Point = FPoint::ZeroPoint;
		Gradient = FPoint::ZeroPoint;
		Laplacian = FPoint::ZeroPoint;
	}

	/**
	 * Compute the 3D surface curve point property (3D Coordinate, Gradient, Laplacian) according to
	 * its 2D curve point property and the 3D surface point property
	 */
	void Combine(const FCurvePoint2D& Point2D, const FSurfacicPoint& SurfacicPoint)
	{
		ensureCADKernel(Point2D.DerivativeOrder >= 0);
		ensureCADKernel(SurfacicPoint.DerivativeOrder >= 0);

		ensureCADKernel(Point2D.DerivativeOrder <= SurfacicPoint.DerivativeOrder);

		DerivativeOrder = Point2D.DerivativeOrder;
		Point = SurfacicPoint.Point;

		if (DerivativeOrder > 0)
		{
			Gradient = SurfacicPoint.GradientU * Point2D.Gradient.U + SurfacicPoint.GradientV * Point2D.Gradient.V;
		}

		if (DerivativeOrder > 1)
		{
			Laplacian = SurfacicPoint.LaplacianU * FMath::Square(Point2D.Gradient.U)
				+ 2.0 * SurfacicPoint.LaplacianUV * Point2D.Gradient.U * Point2D.Gradient.V
				+ SurfacicPoint.LaplacianV * FMath::Square(Point2D.Gradient.V)
				+ SurfacicPoint.GradientU * Point2D.Laplacian.U
				+ SurfacicPoint.GradientV * Point2D.Laplacian.V;
		}
	}
};

struct FCoordinateGrid
{
	TArray<double> Coordinates[2];

	FCoordinateGrid()
	{
	}

	FCoordinateGrid(const TArray<double>& InUCoordinates, const TArray<double>& InVCoordinates)
	{
		Coordinates[EIso::IsoU] = InUCoordinates;
		Coordinates[EIso::IsoV] = InVCoordinates;
	}

	void Swap(TArray<double>& InUCoordinates, TArray<double>& InVCoordinates)
	{
		::Swap(Coordinates[EIso::IsoU], InUCoordinates);
		::Swap(Coordinates[EIso::IsoV], InVCoordinates);
	}

	int32 Count() const
	{
		return Coordinates[EIso::IsoU].Num() * Coordinates[EIso::IsoV].Num();
	}

	int32 IsoCount(EIso Iso) const
	{
		return Coordinates[Iso].Num();
	}

	void SetNum(int32 UNumber, int32 VNumber)
	{
		Coordinates[0].SetNum(UNumber);
		Coordinates[1].SetNum(VNumber);
	}

	void Empty(int32 UNumber = 0, int32 VNumber = 0)
	{
		Coordinates[0].Empty(UNumber);
		Coordinates[1].Empty(VNumber);
	}

	constexpr TArray<double>& operator[](EIso Iso)
	{
		ensureCADKernel(Iso == 0 || Iso == 1);
		return Coordinates[Iso];
	}

	constexpr const TArray<double>& operator[](EIso Iso) const
	{
		ensureCADKernel(Iso == 0 || Iso == 1);
		return Coordinates[Iso];
	}
};


}