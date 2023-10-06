// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Math/Point.h"

namespace UE::CADKernel
{
class FCurve;

class CADKERNEL_API FTabulatedCylinderSurface : public FSurface
{
	friend FEntity;

protected:
	TSharedPtr<FCurve> GuideCurve;
	FPoint DirectorVector;

	/**
	 * A tabulated cylinder is a surface formed by moving a curve segment called GuideCurve parallel to
	 * itself along a vector called DirectorVector.
	 *
	 * The bounds of the cylinder are defined as follow:
	 * Bounds[EIso::IsoU] = GuideCurve bounds;
	 * Bounds[EIso::IsoV].Min = InVMin;
	 * Bounds[EIso::IsoV].Max = InVMax;
	 */
	FTabulatedCylinderSurface(const double InToleranceGeometric, TSharedPtr<FCurve> InGuideCurve, const FPoint& InDirectorVector, double InVMin = 0., double InVMax = 1.)
		: FSurface(InToleranceGeometric, 0.0, 1.0, InVMin, InVMax)
		, GuideCurve(InGuideCurve)
		, DirectorVector(InDirectorVector)
	{
		ComputeDefaultMinToleranceIso();
	}

	FTabulatedCylinderSurface() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FSurface::Serialize(Ar);
		SerializeIdent(Ar, GuideCurve);
		Ar << DirectorVector;
	}

	virtual void SpawnIdent(FDatabase& Database) override;
	virtual void ResetMarkersRecursively() const override;

	ESurface GetSurfaceType() const
	{
		return ESurface::TabulatedCylinder;
	}

	TSharedPtr<FCurve> GetGuideCurve() const
	{
		return GuideCurve;
	}

	FPoint GetDirectorVector() const
	{
		return DirectorVector;
	}

	virtual void EvaluatePoint(const FPoint2D& InSurfacicCoordinate, FSurfacicPoint& OutPoint3D, int32 InDerivativeOrder = 0) const override;
	virtual void EvaluatePointGrid(const FCoordinateGrid& Coordinates, FSurfacicSampling& OutPoints, bool bComputeNormals = false) const override;

	virtual void LinesNotDerivables(const FSurfacicBoundary& Bounds, int32 InDerivativeOrder, FCoordinateGrid& OutNotDerivables) const override;

	virtual void Presample(const FSurfacicBoundary& InBoundaries, FCoordinateGrid& OutCoordinates) override;

	virtual TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH& InMatrix) const override;

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

};

} // namespace UE::CADKernel
