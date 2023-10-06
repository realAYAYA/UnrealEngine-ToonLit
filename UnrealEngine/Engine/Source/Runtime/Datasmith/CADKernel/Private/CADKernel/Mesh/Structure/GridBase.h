// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/HaveStates.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Mesh/MeshEnum.h"
#include "CADKernel/UI/Display.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/UI/DefineForDebug.h"
#endif


namespace UE::CADKernel
{

struct FCuttingPoint;
struct FCuttingGrid;

class FIsoNode;
class FIsoSegment;
class FLoopNode;
class FModelMesh;
class FPoint;
class FPoint2D;
class FThinZone2DFinder;
class FTopologicalFace;

struct FGridChronos
{
	FDuration DefineCuttingParametersDuration;
	FDuration GeneratePointCloudDuration;
	FDuration ProcessPointCloudDuration;
	FDuration FindInnerDomainPointsDuration;
	FDuration Build2DLoopDuration;
	FDuration RemovePointsClosedToLoopDuration;
	FDuration FindPointsCloseToLoopDuration;
	FDuration ScaleGridDuration;

	FGridChronos()
		: DefineCuttingParametersDuration(FChrono::Init())
		, GeneratePointCloudDuration(FChrono::Init())
		, ProcessPointCloudDuration(FChrono::Init())
		, FindInnerDomainPointsDuration(FChrono::Init())
		, Build2DLoopDuration(FChrono::Init())
		, RemovePointsClosedToLoopDuration(FChrono::Init())
		, FindPointsCloseToLoopDuration(FChrono::Init())
		, ScaleGridDuration(FChrono::Init())
	{}

	void PrintTimeElapse() const
	{
		FDuration GridDuration = FChrono::Elapse(FChrono::Now());
		GridDuration += DefineCuttingParametersDuration;
		GridDuration += GeneratePointCloudDuration;
		GridDuration += FindInnerDomainPointsDuration;
		GridDuration += Build2DLoopDuration;
		GridDuration += RemovePointsClosedToLoopDuration;
		GridDuration += FindPointsCloseToLoopDuration;
		FChrono::PrintClockElapse(Log, TEXT(""), TEXT("Grid"), GridDuration);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("DefineCuttingParameters"), DefineCuttingParametersDuration);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("GeneratePointCloud"), GeneratePointCloudDuration);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("GenerateDomainPoints"), ProcessPointCloudDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("FindInnerDomainPointsDuration"), FindInnerDomainPointsDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("FindPointsCloseToLoop"), FindPointsCloseToLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("RemovePointsClosedToLoop"), RemovePointsClosedToLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Build2DLoopDuration"), Build2DLoopDuration);
	}
};

class FGridBase : public FHaveStates
{

protected:

	FTopologicalFace& Face;
	const double Tolerance3D;

	/**
	 * grid point cloud size
	 */
	int32 CuttingCount[2] = { 0 , 0 };
	int32 CuttingSize = 0;

	FCoordinateGrid UniformCuttingCoordinates;

	/**
	 * 2D Coordinate of grid nodes in each space
	 */
	TArray<FPoint2D> Points2D[EGridSpace::EndGridSpace];

	/**
	 * 3D Coordinate of inner nodes
	 */
	TArray<FPoint> Points3D;

	/**
	 * Surface Normal at each inner nodes
	 */
	TArray<FVector3f> Normals;

public:
	FGridChronos Chronos;

public:
	FGridBase(FTopologicalFace& InFace);

	virtual ~FGridBase()
	{
#ifdef DEBUG_ONLY_SURFACE_TO_DEBUG
		Close3DDebugSession(bDisplay);
#endif
	}

	/**
	 * Convert an array of points of "DefaultParametric" space into a scaled parametric space
	 * @see FThinZone2DFinder::BuildBoundarySegments()
	 */
	void TransformPoints(EGridSpace DestinationSpace, const TArray<FPoint2D>& InPointsToScale, TArray<FPoint2D>& OutScaledPoints) const;

	/**
	 * Builds the scaled parametric spaces
	 * @see Points2D
	 * @see GeneratePointCloud (ended GeneratePointCloud process)
	 * @return false if the scaled grid is degenerated
	 */
	bool ScaleGrid();

	double GetTolerance() const
	{
		return Tolerance3D;
	}

protected:

	virtual const FCoordinateGrid& GetCoordinateGrid() const = 0;

	void EvaluatePointGrid(const FCoordinateGrid& CoordinateGrid, bool bWithNormals);

	/**
	 * Convert Coordinate of "DefaultParametric" space into a scaled parametric space
	 * @see ScaleLoops
	 */
	void ComputeNewCoordinate(const TArray<FPoint2D>& NewGrid, int32 IndexU, int32 IndexV, const FPoint2D& InPoint, FPoint2D& OutNewScaledPoint) const
	{
		const FCoordinateGrid& CoordinateGrid = GetCoordinateGrid();

		const FPoint2D& PointU0V0 = NewGrid[(IndexV + 0) * CuttingCount[EIso::IsoU] + (IndexU + 0)];
		const FPoint2D& PointU1V0 = NewGrid[(IndexV + 0) * CuttingCount[EIso::IsoU] + (IndexU + 1)];
		const FPoint2D& PointU0V1 = NewGrid[(IndexV + 1) * CuttingCount[EIso::IsoU] + (IndexU + 0)];
		const FPoint2D& PointU1V1 = NewGrid[(IndexV + 1) * CuttingCount[EIso::IsoU] + (IndexU + 1)];

		const double U1MinusU0 = CoordinateGrid[EIso::IsoU][IndexU + 1] - CoordinateGrid[EIso::IsoU][IndexU];
		const double U0MinusU  = CoordinateGrid[EIso::IsoU][IndexU] - InPoint.U;
		const double V1MinusV0 = CoordinateGrid[EIso::IsoV][IndexV + 1] - CoordinateGrid[EIso::IsoV][IndexV];
		const double V0MinusV  = CoordinateGrid[EIso::IsoV][IndexV] - InPoint.V;

		OutNewScaledPoint =
			PointU0V0 +
			(PointU0V0 - PointU1V0) * (U0MinusU / U1MinusU0) +
			(PointU0V0 - PointU0V1) * (V0MinusV / V1MinusV0) +
			(PointU0V0 - PointU0V1 + PointU1V1 - PointU1V0) * ((U0MinusU * V0MinusV) / (U1MinusU0 * V1MinusV0));
	};

public:

	/**
	 * @return the FPoint2D (parametric coordinates) of the point at the Index of the grid in the defined grid space
	 * @see EGridSpace
	 * @see Points2D
	 */
	const FPoint2D& GetInner2DPoint(EGridSpace Space, int32 Index) const
	{
		return Points2D[(int32)Space][Index];
	}

	/**
	 * @return the FPoint2D (parametric coordinates) of the point at the Index of the grid in the defined grid space @see EGridSpace
	 */
	const FPoint2D& GetInner2DPoint(EGridSpace Space, int32 IndexU, int32 IndexV) const
	{
		return Points2D[(int32)Space][GobalIndex(IndexU, IndexV)];
	}

	/**
	 * @return the FPoint (3D coordinates) of the point at the Index of the grid
	 */
	const FPoint& GetInner3DPoint(int32 Index) const
	{
		return Points3D[Index];
	}

	constexpr const TArray<double>& GetUniformCuttingCoordinatesAlongIso(EIso Iso) const
	{
		return UniformCuttingCoordinates[Iso];
	}

	const FCoordinateGrid& GetUniformCuttingCoordinates() const
	{
		return UniformCuttingCoordinates;
	}

	/**
	 * @return the array of 3d points of the grid
	 */
	TArray<FPoint>& GetInner3DPoints()
	{
		return Points3D;
	}

	/**
	 * @return the array of 3d points of the grid
	 */
	const TArray<FPoint>& GetInner3DPoints() const
	{
		return Points3D;
	}

	/**
	 * @return the array of 2d points of the grid in the defined space
	 */
	const TArray<FPoint2D>& GetInner2DPoints(EGridSpace Space) const
	{
		return Points2D[(int32)Space];
	}

	const FTopologicalFace& GetFace() const
	{
		return Face;
	}

	FTopologicalFace& GetFace()
	{
		return Face;
	}

	/**
	 * @return the Index of the position in the arrays of a point [IndexU, IndexV] of the grid
	 */
	int32 GobalIndex(int32 IndexU, int32 IndexV) const
	{
		return IndexV * CuttingCount[EIso::IsoU] + IndexU;
	}

#ifdef CADKERNEL_DEBUG

	// ======================================================================================================================================================================================================================
	// Display Methodes   ================================================================================================================================================================================================
	// ======================================================================================================================================================================================================================
	mutable bool bDisplay = true;

	void DisplayIsoNode(EGridSpace Space, const int32 PointIndex, FIdent Ident = 0, EVisuProperty Property = EVisuProperty::BluePoint) const;
	virtual void DisplayGridPoints(EGridSpace DisplaySpace) const;
	void DisplayInnerPoints(TCHAR* Message, EGridSpace DisplaySpace) const;
#endif

#ifdef CADKERNEL_DEV
	void PrintTimeElapse() const;
#endif
};

}
