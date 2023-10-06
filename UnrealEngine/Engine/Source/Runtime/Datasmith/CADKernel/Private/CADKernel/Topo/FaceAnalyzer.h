// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/HaveStates.h"
#include "CADKernel/Core/Factory.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Math/Geometry.h"


//#define DEBUG_THIN_FACE
//#define DISPLAY_THIN_FACE // to display found thin zone


namespace UE::CADKernel
{
class FTopologicalFace;
class FTopologicalEdge;
class FTopologicalLoop;

namespace Topo
{

struct FFaceAnalyzerChronos
{
	FDuration BuildLoopSegmentsTime;
	FDuration FindClosedSegmentTime;
	FDuration AnalyzeClosedSegmentTime;
};

class FEdgeSegment : public FHaveStates
{
private:
	const FTopologicalEdge* Edge = nullptr;
	double Coordinates[2] = {0. ,0.};
	FPoint Points[2];
	FPoint Middle;
	FPoint Vector;

	FEdgeSegment* ClosedSegment = nullptr;

	double MiddleAxis = 0.;
	double SquareDistanceToClosedSegment = 0.;

	double Length;

	bool bIsThinZone = false;

#ifdef CADKERNEL_DEV
	FIdent Id;
	static FIdent LastId;
#endif

public:
	FEdgeSegment() = default;

	virtual ~FEdgeSegment() = default;

	void SetBoundarySegment(const FTopologicalEdge* InEdge, double InStartU, double InEndU, const FPoint& InStartPoint, const FPoint& InEndPoint)
	{
		Edge = InEdge;
		Coordinates[ELimit::Start] = InStartU;
		Coordinates[ELimit::End] = InEndU;
		Points[ELimit::Start] = InStartPoint;
		Points[ELimit::End] = InEndPoint;
		Middle = Points[ELimit::Start].Middle(Points[ELimit::End]);
		Vector = Points[ELimit::End] - Points[ELimit::Start];

		ClosedSegment = nullptr;

		SquareDistanceToClosedSegment = -1.;
		Length = Points[ELimit::Start].Distance(Points[ELimit::End]);

		MiddleAxis = Middle.DiagonalAxisCoordinate();

#ifdef CADKERNEL_DEV
		Id = LastId++;
#endif
	};

	const FPoint GetVector() const
	{
		return Vector;
	}

#ifdef CADKERNEL_DEV
	FIdent GetId() const
	{
		return Id;
	}
#endif

	const FTopologicalEdge* GetEdge() const
	{
		return Edge;
	}

	double GetLength() const
	{
		return Length;
	}

	FPoint GetMiddle() const
	{
		return Middle;
	}

	constexpr const FPoint& GetExtemity(const ELimit Limit) const
	{
		return Points[Limit];
	}

	constexpr double GetCoordinate(const ELimit Limit) const
	{
		return Coordinates[Limit];
	}

	/**
	 * Compute the slope of the input Segment according to this.
	 */
	double ComputeCosAngleOf(const FEdgeSegment* Segment)
	{
		return GetVector().ComputeCosinus(Segment->GetVector());
	}

	FEdgeSegment* GetClosedSegment() const
	{
		return ClosedSegment;
	}

	void SetClosedSegment(FEdgeSegment* InSegmentA, double InSquareDistance)
	{
		ClosedSegment = InSegmentA;
		SquareDistanceToClosedSegment = InSquareDistance;
	}

	double GetClosedSquareDistance() const
	{
		return SquareDistanceToClosedSegment;
	}

	FPoint ProjectPoint(const FPoint& PointToProject, double& SegmentU) const
	{
		return ProjectPointOnSegment(PointToProject, Points[ELimit::Start], Points[ELimit::End], SegmentU, true);
	}

	bool IsThinZone()
	{
		return bIsThinZone;
	}

	void SetAsThinZone()
	{
		bIsThinZone = true;
	}
};

struct FThinFaceContext
{
	const FTopologicalLoop& Loop;

	TArray<Topo::FEdgeSegment*> LoopSegments;
	double ExternalLoopLength = 0.0;
	TFactory<Topo::FEdgeSegment> SegmentFatory;

	TArray<double> EdgeSquareDistance;
	TArray<double> EdgeMaxSquareDistance;

	double MaxSquareDistance = 0;
	double ThinSideEdgeLength = 0;
	double OppositSideEdgeLength = 0;

	FThinFaceContext(const FTopologicalLoop& InLoop)
		: Loop(InLoop)
	{
	}
};

}

class FFaceAnalyzer
{

public:
	Topo::FFaceAnalyzerChronos Chronos;

protected:
	const double Tolerance;
	const double SquareTolerance;
	const double MaxOppositSideLength;

	FTopologicalFace& Face;

public:

	FFaceAnalyzer(FTopologicalFace& InFace, double InTol)
		: Tolerance(InTol)
		, SquareTolerance(FMath::Square(InTol))
		, MaxOppositSideLength(4. * InTol)
		, Face(InFace)
	{
 	}

	bool IsThinFace(double& OutGapSize);

private:

	void BuildLoopSegments(Topo::FThinFaceContext& Context);
	void FindClosedSegments(Topo::FThinFaceContext& Context);
	void Analyze(Topo::FThinFaceContext& Context);

#ifdef CADKERNEL_DEV
	void DisplayCloseSegments(Topo::FThinFaceContext& Context);
	void DisplayLoopSegments(Topo::FThinFaceContext& Context);
#endif
};
}
