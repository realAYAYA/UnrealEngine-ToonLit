// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

#include "CADKernel/Core/CADKernelArchive.h"
#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Math/Boundary.h"
#include "CADKernel/Math/MatrixH.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/UI/Display.h"
#include "CADKernel/Utils/IndexOfCoordinateFinder.h"

#include "Serialization/Archive.h"
#include "Algo/AllOf.h"

namespace UE::CADKernel
{

class FCurve;
class FEntityGeom;
class FInfoEntity;
class FSurface;

class CADKERNEL_API FSurfacicPolyline
{

public:

	TArray<double> Coordinates;
	TArray<FPoint2D> Points2D;
	TArray<FPoint> Points3D;
	TArray<FVector3f> Normals;
	TArray<FPoint> Tangents;

	FSurfacicBoundary BoundingBox;

	bool bWithNormals;
	bool bWithTangent;

	FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> InCurve2D);

	FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> InCurve2D, const double Tolerance);

	FSurfacicPolyline(TSharedRef<FSurface> InCarrierSurface, TSharedRef<FCurve> InCurve2D, const double ChordTolerance, const double ParamTolerance, bool bInWithNormals/* = false*/, bool bWithTangent/* = false*/);

	FSurfacicPolyline(bool bInWithNormals = false, bool bInWithTangent = false)
		: bWithNormals(bInWithNormals)
		, bWithTangent(bInWithTangent)
	{
	}

	void Serialize(FCADKernelArchive& Ar)
	{
		Ar.Serialize(Points3D);
		Ar.Serialize(Points2D);
		Ar.Serialize(Normals);
		Ar.Serialize(Coordinates);
		Ar.Serialize(BoundingBox);
		Ar << bWithNormals;
		Ar << bWithTangent;
	}

	FInfoEntity& GetInfo(FInfoEntity&) const;

	TSharedPtr<FEntityGeom> ApplyMatrix(const FMatrixH&) const;

	void CheckIfDegenerated(const double Tolerance3D, const FSurfacicTolerance& Tolerances2D, const FLinearBoundary& Boudary, bool& bDegeneration2D, bool& bDegeneration3D, double& Length3D) const;

	void GetExtremities(const FLinearBoundary& InBoundary, const double Tolerance3D, const FSurfacicTolerance& Tolerances2D, FSurfacicCurveExtremities& Extremities) const;

	FPoint Approximate3DPoint(double InCoordinate) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		return Approximator3D.ApproximatePoint(InCoordinate);
	}

	void Approximate3DPoints(const TArray<double>& InCoordinates, TArray<FPoint>& OutPoints) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.ApproximatePoints(InCoordinates, OutPoints);
	}

	FPoint2D Approximate2DPoint(double InCoordinate) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		return Approximator.ApproximatePoint(InCoordinate);
	}

	FPoint GetTangentAt(double InCoordinate) const
	{
		FDichotomyFinder Finder(Coordinates);
		int32 Index = Finder.Find(InCoordinate);
		return Points3D[Index + 1] - Points3D[Index];
	}

	FPoint2D GetTangent2DAt(double InCoordinate) const
	{
		FDichotomyFinder Finder(Coordinates);
		int32 Index = Finder.Find(InCoordinate);
		return Points2D[Index + 1] - Points2D[Index];
	}

	FSurfacicTolerance ComputeTolerance(const double Tolerance3D, const FSurfacicTolerance& MinToleranceIso, const int32 Index) const
	{
		double Distance3D = Points3D[Index].Distance(Points3D[Index + 1]);
		if (FMath::IsNearlyZero(Distance3D, (double)DOUBLE_SMALL_NUMBER))
		{
			return FPoint2D::FarawayPoint;
		}
		else
		{
			FSurfacicTolerance Tolerance2D = Points2D[Index] - Points2D[Index + 1];
			return Max(Abs(Tolerance2D) * Tolerance3D / Distance3D, MinToleranceIso);
		}
	};

	double ComputeLinearToleranceAt(const double Tolerance3D, const double MinLinearTolerance, const int32 Index) const
	{
		double Distance3D = Points3D[Index].Distance(Points3D[Index + 1]);
		if (FMath::IsNearlyZero(Distance3D, (double)DOUBLE_SMALL_NUMBER))
		{
			return (Coordinates.Last() - Coordinates[0]) / 10.;
		}
		else
		{
			double LinearDistance = Coordinates[Index + 1] - Coordinates[Index];
			return FMath::Max(LinearDistance / Distance3D * Tolerance3D, MinLinearTolerance);
		}
	};

	void Approximate2DPoints(const TArray<double>& InCoordinates, TArray<FPoint2D>& OutPoints) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		Approximator.ApproximatePoints(InCoordinates, OutPoints);
	}

	void ApproximatePolyline(FSurfacicPolyline& OutPolyline) const
	{
		if (OutPolyline.Coordinates.IsEmpty())
		{
			return;
		}

		TFunction<void(FIndexOfCoordinateFinder&)> ComputePoints = [&](FIndexOfCoordinateFinder& Finder)
		{
			int32 CoordinateCount = OutPolyline.Coordinates.Num();

			TArray<int32> SegmentIndexes;
			SegmentIndexes.Reserve(CoordinateCount);

			TArray<double> SegmentCoordinates;
			SegmentCoordinates.Reserve(CoordinateCount);


			//for (int32 Index = 0; Index < CoordinateCount; ++Index)
			for (const double Coordinate : OutPolyline.Coordinates)
			{
				const int32& Index = SegmentIndexes.Emplace_GetRef(Finder.Find(Coordinate));
				SegmentCoordinates.Emplace(PolylineTools::SectionCoordinate(Coordinates, Index, Coordinate));
			}

			OutPolyline.Points2D.Reserve(CoordinateCount);
			for (int32 Index = 0; Index < CoordinateCount; ++Index)
			{
				int32 SegmentIndex = SegmentIndexes[Index];
				double SegmentCoordinate = SegmentCoordinates[Index];
				OutPolyline.Points2D.Emplace(PolylineTools::LinearInterpolation(Points2D, SegmentIndex, SegmentCoordinate));
			}

			OutPolyline.Points3D.Reserve(CoordinateCount);
			for (int32 Index = 0; Index < CoordinateCount; ++Index)
			{
				int32 SegmentIndex = SegmentIndexes[Index];
				double SegmentCoordinate = SegmentCoordinates[Index];
				OutPolyline.Points3D.Emplace(PolylineTools::LinearInterpolation(Points3D, SegmentIndex, SegmentCoordinate));
			}

			if (bWithNormals)
			{
				for (int32 Index = 0; Index < CoordinateCount; ++Index)
				{
					int32 SegmentIndex = SegmentIndexes[Index];
					double SegmentCoordinate = SegmentCoordinates[Index];
					OutPolyline.Normals.Emplace(PolylineTools::LinearInterpolation(Normals, SegmentIndex, SegmentCoordinate));
				}
			}

			if (bWithTangent)
			{
				for (int32 Index = 0; Index < CoordinateCount; ++Index)
				{
					int32 SegmentIndex = SegmentIndexes[Index];
					double SegmentCoordinate = SegmentCoordinates[Index];
					OutPolyline.Tangents.Emplace(PolylineTools::LinearInterpolation(Tangents, SegmentIndex, SegmentCoordinate));
				}
			}
		};

		FDichotomyFinder DichotomyFinder(Coordinates);
		int32 StartIndex = 0;
		int32 EndIndex;

		bool bUseDichotomy = false;
		StartIndex = DichotomyFinder.Find(OutPolyline.Coordinates[0]);
		EndIndex = DichotomyFinder.Find(OutPolyline.Coordinates.Last());
		bUseDichotomy = PolylineTools::IsDichotomyToBePreferred(EndIndex - StartIndex, Coordinates.Num());


		if (bUseDichotomy)
		{
			DichotomyFinder.StartLower = StartIndex;
			DichotomyFinder.StartUpper = EndIndex;
			ComputePoints(DichotomyFinder);
		}
		else
		{
			FLinearFinder LinearFinder(Coordinates, StartIndex);
			ComputePoints(LinearFinder);
		}
	}


	void Sample(const FLinearBoundary& Boundary, const double DesiredSegmentLength, TArray<double>& OutCoordinates) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.SamplePolyline(Boundary, DesiredSegmentLength, OutCoordinates);
	}

	double GetCoordinateOfProjectedPoint(const FLinearBoundary& Boundary, const FPoint& PointOnEdge, FPoint& ProjectedPoint) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		return Approximator3D.ProjectPointToPolyline(Boundary, PointOnEdge, ProjectedPoint);
	}

	double GetCoordinateOfProjectedPoint(const FLinearBoundary& Boundary, const FPoint2D& PointOnEdge, FPoint2D& ProjectedPoint) const
	{
		TPolylineApproximator<FPoint2D> Approximator2D(Coordinates, Points2D);
		return Approximator2D.ProjectPointToPolyline(Boundary, PointOnEdge, ProjectedPoint);
	}

	void ProjectPoints(const FLinearBoundary& InBoundary, const TArray<FPoint>& InPointsToProject, TArray<double>& ProjectedPointCoordinates, TArray<FPoint>& ProjectedPoints) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.ProjectPointsToPolyline(InBoundary, InPointsToProject, ProjectedPointCoordinates, ProjectedPoints);
	}

	void ProjectPoints(const FLinearBoundary& InBoundary, const TArray<FPoint2D>& InPointsToProject, TArray<double>& ProjectedPointCoordinates, TArray<FPoint2D>& ProjectedPoints) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		Approximator.ProjectPointsToPolyline(InBoundary, InPointsToProject, ProjectedPointCoordinates, ProjectedPoints);
	}

	/**
	 * Project each point of a coincidental polyline on the Polyline.
	 * @param ToleranceOfProjection: Max error between the both curve to stop the search of projection
	 */
	void ProjectCoincidentalPolyline(const FLinearBoundary& InBoundary, const TArray<FPoint>& InPointsToProject, bool bSameOrientation, TArray<double>& OutProjectedPointCoordinates, double ToleranceOfProjection) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.ProjectCoincidentalPolyline(InBoundary, InPointsToProject, bSameOrientation, OutProjectedPointCoordinates, ToleranceOfProjection);
	}

	/**
	 * The main idea of this algorithm is to process starting for the beginning of the curve to the end of the curve.
	 */
	void ComputeIntersectionsWithIsos(const FLinearBoundary& InBoundary, const TArray<double>& InIsoCoordinates, const EIso InTypeIso, const FSurfacicTolerance& ToleranceIso, TArray<double>& OutIntersection) const;

	const TArray<double>& GetCoordinates() const
	{
		return Coordinates;
	}

	const TArray<FPoint2D>& Get2DPoints() const
	{
		return Points2D;
	}

	const FPoint& GetPointAt(int32 Index) const
	{
		return Points3D[Index];
	}

	const TArray<FPoint>& GetPoints() const
	{
		return Points3D;
	}

	const TArray<FVector3f>& GetNormals() const
	{
		return Normals;
	}

	const TArray<FPoint>& GetTangents() const
	{
		return Tangents;
	}

	void SwapCoordinates(TArray<double>& NewCoordinates)
	{
		Swap(NewCoordinates, Coordinates);
	}

	/**
	 * @return the size of the polyline i.e. the count of points.
	 */
	int32 Size() const
	{
		return Points2D.Num();
	}

	/**
	 * Get the sub 2d polyline bounded by the input InBoundary in the orientation of the input InOrientation and append it to the output OutPoints
	 */
	void GetSubPolyline(const FLinearBoundary& InBoundary, const EOrientation InOrientation, TArray<FPoint2D>& OutPoints) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		Approximator.GetSubPolyline(InBoundary, InOrientation, OutPoints);
	}

	/**
	 * Get the sub polyline bounded by the input InBoundary in the orientation of the input InOrientation and append it to the output OutPoints
	 */
	void GetSubPolyline(const FLinearBoundary& InBoundary, TArray<double>& OutCoordinates, TArray<FPoint2D>& OutPoints) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		Approximator.GetSubPolyline(InBoundary, OutCoordinates, OutPoints);
	}

	/**
	 * Get the sub polyline bounded by the input InBoundary in the orientation of the input InOrientation and append it to the output OutPoints
	 */
	void GetSubPolyline(const FLinearBoundary& InBoundary, const EOrientation InOrientation, TArray<FPoint>& OutPoints) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.GetSubPolyline(InBoundary, InOrientation, OutPoints);
	}

	/**
	 * Get the sub polyline bounded by the input InBoundary in the orientation of the input InOrientation and append it to the output OutPoints
	 */
	void GetSubPolyline(const FLinearBoundary& InBoundary, TArray<double>& OutCoordinates, TArray<FPoint>& OutPoints) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		Approximator3D.GetSubPolyline(InBoundary, OutCoordinates, OutPoints);
	}

	/**
	 * Reserves memory such that the polyline can contain at least Number elements.
	 *
	 * @param Number The number of elements that the polyline should be able to contain after allocation.
	 */
	void Reserve(int32 Number)
	{
		Points3D.Reserve(Number);
		Points2D.Reserve(Number);
		Coordinates.Reserve(Number);
		if (bWithNormals)
		{
			Normals.Reserve(Number);
		}
	}

	/**
	 * Empties the polyline.
	 *
	 * @param Slack (Optional) The expected usage size after empty operation. Default is 0.
	 */
	void Empty(int32 Slack = 0)
	{
		Points3D.Empty(Slack);
		Points2D.Empty(Slack);
		Normals.Empty(Slack);
		Coordinates.Empty(Slack);
	}

	void EmplaceAt(int32 Index, FSurfacicPolyline& Polyline, int32 PointIndex)
	{
		Coordinates.EmplaceAt(Index, Polyline.Coordinates[PointIndex]);

		Points2D.EmplaceAt(Index, Polyline.Points2D[PointIndex]);
		Points3D.EmplaceAt(Index, Polyline.Points3D[PointIndex]);
		if (bWithNormals)
		{
			Normals.EmplaceAt(Index, Polyline.Normals[PointIndex]);
		}
		if (bWithTangent)
		{
			Tangents.EmplaceAt(Index, Polyline.Tangents[PointIndex]);
		}
	}

	void RemoveAt(int32 Index)
	{
		Coordinates.RemoveAt(Index);
		Points2D.RemoveAt(Index);
		Points3D.RemoveAt(Index);
		if (bWithNormals)
		{
			Normals.RemoveAt(Index);
		}
		if (bWithTangent)
		{
			Tangents.RemoveAt(Index);
		}
	}

	void Pop()
	{
		Coordinates.Pop(EAllowShrinking::No);
		Points2D.Pop(EAllowShrinking::No);
		Points3D.Pop(EAllowShrinking::No);
		if (bWithNormals)
		{
			Normals.Pop(EAllowShrinking::No);
		}
		if (bWithTangent)
		{
			Tangents.Pop(EAllowShrinking::No);
		}
	}

	void Reverse()
	{
		Algo::Reverse(Coordinates);
		Algo::Reverse(Points2D);
		Algo::Reverse(Points3D);
		if (bWithNormals)
		{
			Algo::Reverse(Normals);
		}
		if (bWithTangent)
		{
			Algo::Reverse(Tangents);
		}
	}

	double GetLength(const FLinearBoundary& InBoundary) const
	{
		TPolylineApproximator<FPoint> Approximator3D(Coordinates, Points3D);
		return Approximator3D.ComputeLengthOfSubPolyline(InBoundary);
	}

	double Get2DLength(const FLinearBoundary& InBoundary) const
	{
		TPolylineApproximator<FPoint2D> Approximator(Coordinates, Points2D);
		return Approximator.ComputeLengthOfSubPolyline(InBoundary);
	}

	bool IsIso(EIso Iso, double ErrorTolerance = DOUBLE_SMALL_NUMBER) const
	{
		FPoint2D StartPoint = Points2D[0];
		return Algo::AllOf(Points2D, [&](const FPoint2D& Point) { 
			return FMath::IsNearlyEqual(Point[Iso], StartPoint[Iso], ErrorTolerance);
			});
	}

};

} // ns UE::CADKernel
