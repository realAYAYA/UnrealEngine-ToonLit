// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Sampling/SurfacicPolyline.h"

#include "CADKernel/Core/System.h"
#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Geo/Sampler/SamplerOnParam.h"
#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Math/SlopeUtils.h"

namespace UE::CADKernel
{

FSurfacicPolyline::FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> Curve2D, const double InTolerance)
	: bWithNormals(true)
	, bWithTangent(false)
{
	FSurfacicCurveSamplerOnParam Sampler(InCarrierSurface.Get(), Curve2D.Get(), Curve2D->GetBoundary(), InTolerance, InTolerance, *this);
	Sampler.Sample();
	BoundingBox.Set(Points2D);
}

FSurfacicPolyline::FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> Curve2D)
	: FSurfacicPolyline(InCarrierSurface, Curve2D, InCarrierSurface->Get3DTolerance())
{
}

FSurfacicPolyline::FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> Curve2D, const double ChordTolerance, const double ParamTolerance, bool bInWithNormals, bool bInWithTangents)
	: bWithNormals(bInWithNormals)
	, bWithTangent(bInWithTangents)
{
	FSurfacicCurveSamplerOnParam Sampler(InCarrierSurface.Get(), Curve2D.Get(), Curve2D->GetBoundary(), ChordTolerance, ParamTolerance, *this);
	Sampler.Sample();
	BoundingBox.Set(Points2D);
}

void FSurfacicPolyline::CheckIfDegenerated(const double Tolerance3D, const FSurfacicTolerance& ToleranceIso, const FLinearBoundary& Boudary, bool& bDegeneration2D, bool& bDegeneration3D, double& Length3D) const
{
	TPolylineApproximator<FPoint> Approximator(Coordinates, Points3D);
	int32 BoundaryIndices[2];
	Approximator.GetStartEndIndex(Boudary, BoundaryIndices);

	TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
	Length3D = Approximator3D.ComputeLengthOfSubPolyline(BoundaryIndices, Boudary);

	if (!FMath::IsNearlyZero(Length3D, Tolerance3D))
	{
		bDegeneration3D = false;
		bDegeneration2D = false;
		return;
	}

	bDegeneration3D = true;
	Length3D = 0.;

	// Tolerance along Iso U/V is very costly to compute and not accurate.
	// To test if a curve is degenerated, its 2d bounding box is compute and its compare to the surface boundary along U and along V
	// Indeed, defining a Tolerance2D has no sense has the boundary length along an Iso could be very very huge compare to the boundary length along the other Iso like [[0, 1000] [0, 1]]
	// The tolerance along ans iso is the length of the boundary along this iso divide by 100 000: if the curve length in 3d is 10m, the tolerance is 0.01mm

	FAABB2D Aabb;
	TPolylineApproximator<FPoint2D> Approximator2D(Coordinates, Points2D);
	Approximator2D.ComputeBoundingBox(BoundaryIndices, Boudary, Aabb);

	bDegeneration2D = (Aabb.GetSize(0) < ToleranceIso[EIso::IsoU] && Aabb.GetSize(1) < ToleranceIso[EIso::IsoV]);
}

void FSurfacicPolyline::GetExtremities(const FLinearBoundary& InBoundary, const double Tolerance3D, const FSurfacicTolerance& MinToleranceIso, FSurfacicCurveExtremities& Extremities) const
{
	FDichotomyFinder Finder(Coordinates);
	const int32 StartIndex = Finder.Find(InBoundary.Min);
	const int32 EndIndex = Finder.Find(InBoundary.Max);

	Extremities[0].Point2D = PolylineTools::ComputePoint(Coordinates, Points2D, StartIndex, InBoundary.Min);
	Extremities[0].Point = PolylineTools::ComputePoint(Coordinates, Points3D, StartIndex, InBoundary.Min);
	Extremities[0].Tolerance = ComputeTolerance(Tolerance3D, MinToleranceIso, StartIndex);

	Extremities[1].Point2D = PolylineTools::ComputePoint(Coordinates, Points2D, EndIndex, InBoundary.Max);
	Extremities[1].Point = PolylineTools::ComputePoint(Coordinates, Points3D, EndIndex, InBoundary.Max);
	if (EndIndex == StartIndex)
	{
		Extremities[1].Tolerance = Extremities[0].Tolerance;
	}
	else
	{
		Extremities[1].Tolerance = ComputeTolerance(Tolerance3D, MinToleranceIso, EndIndex);
	}
}

void FSurfacicPolyline::ComputeIntersectionsWithIsos(const FLinearBoundary& InBoundary, const TArray<double>& IsoCoordinates, const EIso TypeIso, const FSurfacicTolerance& ToleranceIso, TArray<double>& Intersection) const
{
	if (BoundingBox.Length(TypeIso) < ToleranceIso[TypeIso])
	{
		// the edge is on an IsoCurve on Iso axis
		return;
	}

#ifdef DEBUG_COMPUTEINTERSECTIONSWITHISOS
	F3DDebugSession _(TEXT("IntersectEdgeIsos"));
#endif
 
	Intersection.Reserve(IsoCoordinates.Num());

	double UMin;
	double UMax;

	int32 IsoCoordinateIndex = 0;
	double IsoCoordinate = IsoCoordinates[0];

	for (int32 Index = 0; Index < Points2D.Num() - 1; ++Index)
	{
		// if the segment is outside edge boundary, go to the next one
		if (Coordinates[Index + 1] < InBoundary.GetMin())
		{
			continue;
		}

		if (Coordinates[Index] > InBoundary.GetMax())
		{
			break;
		}

		// if the segment is too parallel to Iso, go to the next one
		double EdgeLocalSlope = ComputeUnorientedSlope(Points2D[Index], Points2D[Index + 1], 0);
		if (TypeIso == EIso::IsoV)
		{
			if (EdgeLocalSlope < 0.1 || EdgeLocalSlope > 3.9)
			{
				continue;
			}
		}
		else
		{
			if (EdgeLocalSlope < 2.1 && EdgeLocalSlope > 1.9)
			{
				continue;
			}
		}

		GetMinMax(Points2D[Index][TypeIso], Points2D[Index + 1][TypeIso], UMin, UMax);
		UMin -= DOUBLE_SMALL_NUMBER;
		UMax += DOUBLE_SMALL_NUMBER;

		if (IsoCoordinate < UMin || IsoCoordinate > UMax)
		{
			if (IsoCoordinateIndex > 0 && IsoCoordinateIndex < IsoCoordinates.Num() - 1 && IsoCoordinates[IsoCoordinateIndex - 1] < UMin && UMax < IsoCoordinates[IsoCoordinateIndex + 1])
			{
				continue;
			}
		}

		if (IsoCoordinate > UMin)
		{
			while (IsoCoordinateIndex > 0 && IsoCoordinates[IsoCoordinateIndex - 1] > UMin)
			{
				IsoCoordinateIndex--;
			}
			IsoCoordinate = IsoCoordinates[IsoCoordinateIndex];
		}

		while (IsoCoordinateIndex < IsoCoordinates.Num() - 1 && IsoCoordinates[IsoCoordinateIndex] < UMin)
		{
			IsoCoordinateIndex++;
		}
		IsoCoordinate = IsoCoordinates[IsoCoordinateIndex];

		if (UMax < IsoCoordinate)
		{
			continue;
		}

		for (;; ++IsoCoordinateIndex)
		{
			if (IsoCoordinateIndex == IsoCoordinates.Num() || IsoCoordinates[IsoCoordinateIndex] > UMax)
			{
				--IsoCoordinateIndex;
				break;
			}
			IsoCoordinate = IsoCoordinates[IsoCoordinateIndex];

			if ((IsoCoordinate > UMin) && (IsoCoordinate < UMax))
			{
				double EdgeCoord = 0.;
				const double Delta = (Points2D[Index + 1][TypeIso] - Points2D[Index][TypeIso]);
				if (FMath::IsNearlyZero(Delta))
				{
					EdgeCoord = Coordinates[Index];
				}
				else
				{
					const double LocalCoord = (IsoCoordinate - Points2D[Index][TypeIso]) / Delta;
					EdgeCoord = PolylineTools::LinearInterpolation(Coordinates, Index, LocalCoord);
				}

#ifdef DEBUG_COMPUTEINTERSECTIONSWITHISOS
				::DisplayPoint(Points2D[Index + 1], EVisuProperty::RedPoint);
				::DisplayPoint(Points2D[Index], EVisuProperty::RedPoint);
				FPoint2D IntersectionPoint = PolylineTools::LinearInterpolation(Points2D, Index, LocalCoord);
				::DisplayPoint(IntersectionPoint);
				Wait();
#endif // #ifdef DEBUG_COMPUTEINTERSECTIONSWITHISOS

				if (!(Intersection.Num() && FMath::IsNearlyEqual(EdgeCoord, Intersection.Last())))
				{
					Intersection.Add(EdgeCoord);
				}
			}
		}
	}
}

} //namespace UE::CADKernel
