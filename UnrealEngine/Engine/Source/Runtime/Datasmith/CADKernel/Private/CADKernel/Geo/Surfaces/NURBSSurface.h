// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Geo/Surfaces/NurbsSurfaceData.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Math/BSpline.h"

namespace UE::CADKernel
{
class FNURBSSurface : public FSurface
{
	friend FEntity;

protected:
	int32 PoleUCount;
	int32 PoleVCount;

	int32 UDegree;
	int32 VDegree;

	TArray<double> UNodalVector;
	TArray<double> VNodalVector;

	TArray<double> Weights;

	TArray<FPoint> Poles;

	bool bIsRational;

	/**
	 * Data generated at initialization which are not serialized
	 */
	TArray<double> HomogeneousPoles;

	/**
	 * Build a Non uniform B-Spline surface
	 * @param NodalVectorU Its size is the number of poles in U + the surface degree in U + 1 (PoleUNum + UDegre + 1)
	 * @param NodalVectorV Its size is the number of poles in V + the surface degree in V + 1 (PoleVNum + VDegre + 1)
	 */
	FNURBSSurface(const double InToleranceGeometric, int32 InPoleUCount, int32 InPoleVCount, int32 InDegreU, int32 InDegreV, const TArray<double>& InNodalVectorU, const TArray<double>& InNodalVectorV, const TArray<FPoint>& InPoles)
		: FSurface(InToleranceGeometric)
		, PoleUCount(InPoleUCount)
		, PoleVCount(InPoleVCount)
		, UDegree(InDegreU)
		, VDegree(InDegreV)
		, UNodalVector(InNodalVectorU)
		, VNodalVector(InNodalVectorV)
		, Poles(InPoles)
		, bIsRational(false)
	{
		Finalize();
	}

	/**
	 * Build a Non uniform rational B-Spline surface
	 * @param NodalVectorU Its size is the number of poles in U + the surface degree in U + 1 (PoleUNum + UDegre + 1)
	 * @param NodalVectorV Its size is the number of poles in V + the surface degree in V + 1 (PoleVNum + VDegre + 1)
	 */
	FNURBSSurface(const double InToleranceGeometric, int32 InPoleUCount, int32 InPoleVCount, int32 InDegreU, int32 InDegreV, const TArray<double>& InNodalVectorU, const TArray<double>& InNodalVectorV, const TArray<FPoint>& InPoles, const TArray<double>& InWeights)
		: FSurface(InToleranceGeometric)
		, PoleUCount(InPoleUCount)
		, PoleVCount(InPoleVCount)
		, UDegree(InDegreU)
		, VDegree(InDegreV)
		, UNodalVector(InNodalVectorU)
		, VNodalVector(InNodalVectorV)
		, Weights(InWeights)
		, Poles(InPoles)
		, bIsRational(true)
	{
		ComputeMinToleranceIso();
		Finalize();
	}

	/**
	 * Build a Non uniform B-Spline surface
	 * @param NodalVectorU Its size is the number of poles in U + the surface degree in U + 1 (PoleUNum + UDegre + 1)
	 * @param NodalVectorV Its size is the number of poles in V + the surface degree in V + 1 (PoleVNum + VDegre + 1)
	 */
	FNURBSSurface(const double InToleranceGeometric, FNurbsSurfaceData NurbsData)
		: FSurface(InToleranceGeometric)
		, PoleUCount(NurbsData.PoleUCount)
		, PoleVCount(NurbsData.PoleVCount)
		, UDegree(NurbsData.UDegree)
		, VDegree(NurbsData.VDegree)
		, UNodalVector(NurbsData.UNodalVector)
		, VNodalVector(NurbsData.VNodalVector)
		, Weights(NurbsData.Weights)
		, Poles(NurbsData.Poles)
		, bIsRational(true)
	{
		ComputeMinToleranceIso();
		Finalize();
	}

	/**
	 * Build a Non uniform B-Spline surface
	 * @param NodalVectorU Its size is the number of poles in U + the surface degree in U + 1 (PoleUNum + UDegre + 1)
	 * @param NodalVectorV Its size is the number of poles in V + the surface degree in V + 1 (PoleVNum + VDegre + 1)
	 */
	FNURBSSurface(const double InToleranceGeometric, FNurbsSurfaceHomogeneousData NurbsData)
		: FSurface(InToleranceGeometric)
	{
		FillNurbs(NurbsData);
	}

	FNURBSSurface() = default;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FSurface::Serialize(Ar);
		Ar << PoleUCount;
		Ar << PoleVCount;
		Ar << UDegree;
		Ar << VDegree;
		Ar.Serialize(UNodalVector);
		Ar.Serialize(VNodalVector);
		Ar.Serialize(Weights);
		Ar.Serialize(Poles);
		Ar << bIsRational;

		if (Ar.IsLoading())
		{
			Finalize();
		}
	}

	ESurface GetSurfaceType() const
	{
		return ESurface::Nurbs;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	constexpr const int32 GetDegree(EIso Iso) const
	{
		switch (Iso)
		{
		case EIso::IsoU:
			return UDegree;
		case EIso::IsoV:
		default:
			return VDegree;
		}
	}

	constexpr const int32 GetPoleCount(EIso Iso) const
	{
		switch (Iso)
		{
		case EIso::IsoU:
			return PoleUCount;
		case EIso::IsoV:
		default:
			return PoleVCount;
		}
	}

	const TArray<FPoint>& GetPoles() const
	{
		return Poles;
	}

	const TArray<double>& GetWeights() const
	{
		return Weights;
	}

	TArray<double> GetHPoles() const
	{
		return HomogeneousPoles;
	}

	constexpr const TArray<double>& GetNodalVector(EIso Iso) const
	{
		switch (Iso)
		{
		case EIso::IsoU:
			return UNodalVector;
		case EIso::IsoV:
		default:
			return VNodalVector;
		}
	}

	bool IsRational() const
	{
		return bIsRational;
	}


	virtual TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH& InMatrix) const override;

	virtual void EvaluatePoint(const FPoint2D& InSurfacicCoordinate, FSurfacicPoint& OutPoint3D, int32 InDerivativeOrder = 0) const override
	{
		BSpline::EvaluatePoint(*this, InSurfacicCoordinate, OutPoint3D, InDerivativeOrder);
	}

	void EvaluatePointGrid(const FCoordinateGrid& InSurfacicCoordinates, FSurfacicSampling& OutPoints, bool bComputeNormals) const override
	{
		BSpline::EvaluatePointGrid(*this, InSurfacicCoordinates, OutPoints, bComputeNormals);
	}

	virtual void LinesNotDerivables(const FSurfacicBoundary& InBoundary, int32 InDerivativeOrder, FCoordinateGrid& OutNotDerivableCoordinates) const override
	{
		BSpline::FindNotDerivableParameters(*this, InDerivativeOrder, InBoundary, OutNotDerivableCoordinates);
	}

	void ComputeMinToleranceIso();

private:
	void Finalize();
	void FillNurbs(FNurbsSurfaceHomogeneousData& NurbsData);
};

} // namespace UE::CADKernel

