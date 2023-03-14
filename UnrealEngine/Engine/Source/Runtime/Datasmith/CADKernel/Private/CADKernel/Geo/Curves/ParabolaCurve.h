// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Geo/Curves/Curve.h"

namespace UE::CADKernel
{

class CADKERNEL_API FParabolaCurve : public FCurve
{
	friend class FIGESEntity104;
	friend class FEntity;

protected:
	FMatrixH Matrix;
	double FocalDistance;

	FParabolaCurve(const FMatrixH& InMatrix, double InFocalDistance, const FLinearBoundary& InBounds, int8 InDimension = 3)
		: FCurve(InBounds, InDimension)
		, Matrix(InMatrix)
		, FocalDistance(InFocalDistance)
	{
	}

	FParabolaCurve() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FCurve::Serialize(Ar);
		Ar << Matrix;
		Ar << FocalDistance;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	virtual ECurve GetCurveType() const override
	{
		return ECurve::Parabola;
	}

	virtual TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH& InMatrix) const override;
	virtual void Offset(const FPoint& OffsetDirection) override;

	virtual void ExtendTo(const FPoint& Point) override
	{
		ensureCADKernel(false);
	}

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

		{
			PointType Result(Coordinate * Coordinate * FocalDistance, Coordinate);
			OutPoint.Point = Matrix.Multiply(Result);
		}

		if (DerivativeOrder > 0)
		{
			FPoint Result(2.0 * Coordinate * FocalDistance, 1);
			OutPoint.Gradient = Matrix.MultiplyVector(Result);
		}

		if (DerivativeOrder > 1)
		{
			FPoint Result(2.0 * FocalDistance, 0.);
			OutPoint.Laplacian = Matrix.MultiplyVector(Result);
		}
	}
};

} // namespace UE::CADKernel

