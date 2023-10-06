// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Entity.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Math/MatrixH.h"

namespace UE::CADKernel
{
class FCADKernelArchive;
class FDatabase;
class FEntityGeom;
class FPoint2D;
class FSurfacicPolyline;
struct FCurvePoint2D;
struct FCurvePoint;
struct FLinearBoundary;

class CADKERNEL_API FSurfacicCurve : public FCurve
{
	friend class FEntity;

protected:

	TSharedPtr<FCurve> Curve2D;
	TSharedPtr<FSurface> CarrierSurface;

	FSurfacicCurve(TSharedRef<FCurve> InCurve2D, TSharedRef<FSurface> InSurface)
		: FCurve(InCurve2D->GetBoundary())
		, Curve2D(InCurve2D)
		, CarrierSurface(InSurface)
	{
		ensureCADKernel(Curve2D.IsValid());
	}

	FSurfacicCurve() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FCurve::Serialize(Ar);
		SerializeIdent(Ar, Curve2D);
		SerializeIdent(Ar, CarrierSurface);
	}

	virtual void SpawnIdent(FDatabase& Database) override
	{
		if (!FEntity::SetId(Database))
		{
			return;
		}

		Curve2D->SpawnIdent(Database);
		CarrierSurface->SpawnIdent(Database);
	}

	virtual void ResetMarkersRecursively() const override
	{
		ResetMarkers();
		Curve2D->ResetMarkersRecursively();
		CarrierSurface->ResetMarkersRecursively();
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	virtual ECurve GetCurveType() const override
	{
		return ECurve::Surfacic;
	}

	const TSharedPtr<FCurve>& Get2DCurve() const
	{
		return Curve2D;
	}

	TSharedPtr<FCurve>& Get2DCurve()
	{
		return Curve2D;
	}

	const TSharedRef<FSurface> GetCarrierSurface() const
	{
		return CarrierSurface.ToSharedRef();
	}

	void Set2DCurve(TSharedPtr<FCurve>& NewCurve2D)
	{
		ensureCADKernel(NewCurve2D.IsValid());
		Curve2D = NewCurve2D;
	}

	const TSharedPtr<FSurface>& GetSurface() const
	{
		return CarrierSurface;
	}

	TSharedPtr<FSurface>& GetSurface()
	{
		return CarrierSurface;
	}

	virtual TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH& InMatrix) const override;

	/**
	 * Must not be call
	 */
	virtual void Offset(const FPoint& OffsetDirection) override
	{
		ensure(false);
	}

	virtual void EvaluatePoint(double Coordinate, FCurvePoint& OutPoint, int32 DerivativeOrder = 0) const override;

	virtual void Evaluate2DPoint(double Coordinate, FCurvePoint2D& OutPoint, int32 DerivativeOrder = 0) const override
	{
		Curve2D->Evaluate2DPoint(Coordinate, OutPoint, DerivativeOrder);
	}

	virtual void Evaluate2DPoint(double Coordinate, FPoint2D& OutPoint) const override
	{
		Curve2D->Evaluate2DPoint(Coordinate, OutPoint);
	}

	virtual void EvaluatePoints(const TArray<double>& Coordinates, TArray<FCurvePoint>& OutPoints, int32 DerivativeOrder = 0) const override;

	void EvaluateSurfacicPolyline(FSurfacicPolyline& OutPolyline) const;

	virtual void Evaluate2DPoints(const TArray<double>& Coordinates, TArray<FPoint2D>& OutPoints) const override
	{
		Curve2D->Evaluate2DPoints(Coordinates, OutPoints);
	}

	virtual void Evaluate2DPoints(const TArray<double>& Coordinates, TArray<FCurvePoint2D>& OutPoints, int32 DerivativeOrder = 0) const override
	{
		Curve2D->Evaluate2DPoints(Coordinates, OutPoints, DerivativeOrder);
	}

	virtual void FindNotDerivableCoordinates(const FLinearBoundary& InBoundary, int32 DerivativeOrder, TArray<double>& OutNotDerivableCoordinates) const override;
private:
	void EvaluateSurfacicPolylineWithNormalAndTangent(FSurfacicPolyline& OutPolyline) const;
};

} // namespace UE::CADKernel

