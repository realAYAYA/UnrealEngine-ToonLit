// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/GeoEnum.h"

namespace UE::CADKernel
{

class CADKERNEL_API FSegmentCurve : public FCurve
{
	friend class FEntity;

protected:
	FPoint StartPoint;
	FPoint EndPoint;

	FSegmentCurve(const FPoint& InStartPoint, const FPoint& InEndPoint, int8 InDimension = 3)
		: FCurve(InDimension)
		, StartPoint(InStartPoint)
		, EndPoint(InEndPoint)
	{
	}

	FSegmentCurve() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FCurve::Serialize(Ar);
		Ar << StartPoint;
		Ar << EndPoint;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	virtual ECurve GetCurveType() const override
	{
		return ECurve::Segment;
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

	const FPoint& GetStartPoint() const
	{
		return StartPoint;
	}

	const FPoint& GetEndPoint() const
	{
		return EndPoint;
	}

	virtual void ExtendTo(const FPoint& DesiredPosition) override
	{
		double DistanceToStartPoint = DesiredPosition.SquareDistance(StartPoint);
		double DistanceToEndPoint = DesiredPosition.SquareDistance(EndPoint);
		if (DistanceToEndPoint < DistanceToStartPoint)
		{
			EndPoint = DesiredPosition;
		}
		else
		{
			StartPoint = DesiredPosition;
		}
	}

private:
	template <typename CurvePointType, typename PointType>
	void Evaluate(double Coordinate, CurvePointType& OutPoint, int32 DerivativeOrder) const
	{
		OutPoint.DerivativeOrder = DerivativeOrder;

		PointType Tangent = (EndPoint - StartPoint);
		OutPoint.Point = Tangent * Coordinate + StartPoint;

		if (DerivativeOrder > 0)
		{
			OutPoint.Gradient = Tangent;
		}
	}

};

} // namespace UE::CADKernel

