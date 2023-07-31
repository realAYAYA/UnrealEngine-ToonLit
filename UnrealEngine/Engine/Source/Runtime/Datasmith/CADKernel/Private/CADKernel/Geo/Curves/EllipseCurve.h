// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Math/MathConst.h"

namespace UE::CADKernel
{

class CADKERNEL_API FEllipseCurve : public FCurve
{
	friend class FEntity;

protected:
	FMatrixH Matrix;
	double   RadiusU;
	double   RadiusV;

	FEllipseCurve(const FMatrixH& InMatrix, double InRadiusU, double InRadiusV, int8 InDimension = 3)
		: FCurve(FLinearBoundary(0, PI * 2.), InDimension)
		, Matrix(InMatrix)
		, RadiusU(InRadiusU)
		, RadiusV(InRadiusV)
	{
	}

	FEllipseCurve(const FMatrixH& InMatrix, double InRadiusU, double InRadiusV, const FLinearBoundary& InBounds, int8 InDimension = 3)
		: FCurve(InBounds, InDimension)
		, Matrix(InMatrix)
		, RadiusU(InRadiusU)
		, RadiusV(InRadiusV)
	{
	}

	FEllipseCurve() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FCurve::Serialize(Ar);
		Ar << Matrix;
		Ar << RadiusU;
		Ar << RadiusV;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	virtual ECurve GetCurveType() const override
	{
		return ECurve::Ellipse;
	}

	const FMatrixH& GetMatrix() const
	{
		return Matrix;
	}

	bool IsCircular() const
	{
		return FMath::IsNearlyZero(RadiusU - RadiusV);
	}

	virtual TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH& InMatrix) const override;
	virtual void Offset(const FPoint& OffsetDirection) override;

	virtual void EvaluatePoint(double Coordinate, FCurvePoint& OutPoint, int32 DerivativeOrder = 0) const override
	{
		ensure(Dimension == 3);
		Evaluate<FCurvePoint, FPoint>(Coordinate, OutPoint, DerivativeOrder);
	}

	virtual void Evaluate2DPoint(double Coordinate, FCurvePoint2D& OutPoint, int32 DerivativeOrder = 0) const override
	{
		ensure(Dimension == 2);
		Evaluate<FCurvePoint2D, FPoint2D>(Coordinate, OutPoint, DerivativeOrder);
	}

private:

	template <typename CurvePointType, typename PointType>
	void Evaluate(double Coordinate, CurvePointType& OutPoint, int32 DerivativeOrder) const
	{
		OutPoint.DerivativeOrder = DerivativeOrder;

		double CosU = cos(Coordinate);
		double SinU = sin(Coordinate);

		{
			PointType Result(RadiusU * CosU, RadiusV * SinU);
			OutPoint.Point = Matrix.Multiply(Result);
		}

		if (DerivativeOrder > 0)
		{
			PointType Result(RadiusU * (-SinU), RadiusV * CosU);
			OutPoint.Gradient = Matrix.MultiplyVector(Result);
		}

		if (DerivativeOrder > 1)
		{
			PointType Result(RadiusU * (-CosU), RadiusV * (-SinU));
			OutPoint.Laplacian = Matrix.MultiplyVector(Result);
		}
	}
};

} // namespace UE::CADKernel

