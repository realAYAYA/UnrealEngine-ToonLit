// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Core/HaveStates.h"
#include "CADKernel/Math/Boundary.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Math/SlopeUtils.h"

namespace UE::CADKernel
{
class FTopologicalLoop;
class FTopologicalEdge;
class FGrid;
class FThinZone;
class FThinZone2DFinder;
class FTopologicalVertex;

class FEdgeSegment : public FHaveStates
{
private:
	TSharedPtr<FTopologicalEdge> Edge;
	double Coordinates[2];
	FPoint2D Points[2];

	FEdgeSegment* NextSegment;
	FEdgeSegment* PreviousSegment;

	FEdgeSegment* ClosedSegment[2] = { nullptr, nullptr };

	FSurfacicBoundary Boundary;
	double AxisMin;

	double SquareDistanceToClosedSegment;
	double Length;

	FIdent ChainIndex;

	FIdent Id;
	static FIdent LastId;

public:
	FEdgeSegment()
		: NextSegment(nullptr)
		, PreviousSegment(nullptr)
		, AxisMin(0.)
		, SquareDistanceToClosedSegment(HUGE_VALUE)
		, Length(-1.)
		, ChainIndex(0)
		, Id(0)
	{
	};

	virtual ~FEdgeSegment() = default;

	void SetBoundarySegment(bool bInIsInnerLoop, const TSharedPtr<FTopologicalEdge>& InEdge, double InStartU, double InEndU, const FPoint2D& InStartPoint, const FPoint2D& InEndPoint)
	{
		if (bInIsInnerLoop)
		{
			SetInner();
		}

		Edge = InEdge;
		Coordinates[ELimit::Start] = InStartU;
		Coordinates[ELimit::End] = InEndU;
		Points[ELimit::Start] = InStartPoint;
		Points[ELimit::End] = InEndPoint;
		NextSegment = nullptr;
		PreviousSegment = nullptr;
		ClosedSegment[0] = nullptr;
		ClosedSegment[1] = nullptr;

		SquareDistanceToClosedSegment = HUGE_VAL;
		Length = Points[ELimit::Start].Distance(Points[ELimit::End]);

		Id = ++LastId;
		ChainIndex = 0;

		Boundary.Set(Points[ELimit::Start], Points[ELimit::End]);

		AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min;
	};

	double GetAxeMin() const
	{
		return AxisMin;
	}

	FIdent GetChainIndex() const
	{
		return ChainIndex;
	}

	void SetChainIndex(FIdent index)
	{
		ChainIndex = index;
	}

	bool IsInner() const
	{
		return ((States & EHaveStates::IsInner) == EHaveStates::IsInner);
	}

	void SetInner()
	{
		States |= EHaveStates::IsInner;
	}

	FIdent GetId()
	{
		return Id;
	}

	const TSharedPtr<FTopologicalEdge> GetEdge() const
	{
		return Edge;
	}

	//const TSharedPtr<FTopologicalLoop>& GetLoop() const
	//{
	//	return Edge->GetLoop();
	//}

	double GetLength() const
	{
		return Length;
	}

	FPoint GetCenter() const
	{
		return (Points[ELimit::Start] + Points[ELimit::End]) * 0.5;
	}

	FPoint ComputeEdgePoint(double EdgeParamU) const
	{
		double SegmentParamS = (EdgeParamU - Coordinates[ELimit::Start]) / (Coordinates[ELimit::End] - Coordinates[ELimit::Start]);
		return Points[ELimit::Start] + (Points[ELimit::End] - Points[ELimit::Start]) * SegmentParamS;
	}

	constexpr const FPoint2D& GetExtemity(const ELimit Limit) const
	{
		if (Limit == Start)
		{
			return Points[ELimit::Start];
		}
		else
		{
			return Points[ELimit::End];
		}
	}

	constexpr double GetCoordinate(const ELimit Limit) const
	{
		if (Limit == Start)
		{
			return Coordinates[ELimit::Start];
		}
		else
		{
			return Coordinates[ELimit::End];
		}
	}

	/**
	 * Compute the slope of the input Segment according to this.
	 */
	double ComputeUnorientedSlopeOf(FEdgeSegment* Segment)
	{
		double ReferenceSlope = ComputeSlope(Points[ELimit::Start], Points[ELimit::End]);
		return ComputeUnorientedSlope(Segment->Points[ELimit::Start], Segment->Points[ELimit::End], ReferenceSlope);
	}

	/**
	 * Compute the slope of the Segment defined by the two input points according to this.
	 */
	double ComputeUnorientedSlopeOf(FPoint2D& Middle, FPoint2D& Projection)
	{
		double ReferenceSlope = ComputeSlope(Points[ELimit::Start], Points[ELimit::End]);
		return ComputeUnorientedSlope(Projection, Middle, ReferenceSlope);
	}

	FEdgeSegment* GetNext() const
	{
		return NextSegment;
	}

	FEdgeSegment* GetPrevious() const
	{
		return PreviousSegment;
	}

	FEdgeSegment* GetClosedSegment() const
	{
		return ClosedSegment[0];
	}

	void ResetClosedData()
	{
		if (ClosedSegment[0]->ClosedSegment[0] == this)
		{
			ClosedSegment[0]->ClosedSegment[0] = ClosedSegment[0]->ClosedSegment[1];
		}
		else
		{
			ClosedSegment[0]->ClosedSegment[1] = nullptr;
		}

		ClosedSegment[0] = nullptr;
		SquareDistanceToClosedSegment = HUGE_VAL;
	}

	void SetClosedSegment(FEdgeSegment* InSegmentA, FEdgeSegment* InSegmentB, double InDistance, bool bFirstChoice)
	{
		ClosedSegment[0] = InSegmentA;
		SquareDistanceToClosedSegment = InDistance;

		if (bFirstChoice && InDistance < InSegmentA->SquareDistanceToClosedSegment)
		{
			InSegmentA->ClosedSegment[0] = this;
			InSegmentA->SquareDistanceToClosedSegment = InDistance;
		}
		if (InSegmentB && InDistance < InSegmentB->SquareDistanceToClosedSegment)
		{
			InSegmentB->ClosedSegment[0] = this;
			InSegmentB->SquareDistanceToClosedSegment = InDistance;
		}
	}

	double GetClosedSquareDistance() const
	{
		return SquareDistanceToClosedSegment;
	}

	void SetNext(FEdgeSegment* Segment)
	{
		NextSegment = Segment;
		Segment->SetPrevious(this);
	}

	double ComputeEdgeCoordinate(const double SegmentU) const
	{
		return Coordinates[ELimit::Start] + (Coordinates[ELimit::End] - Coordinates[ELimit::Start]) * SegmentU;
	}

	FPoint2D ProjectPoint(const FPoint2D& PointToProject, double& SegmentU) const
	{
		return ProjectPointOnSegment(PointToProject, Points[ELimit::Start], Points[ELimit::End], SegmentU, true);
	}

private:
	void SetPrevious(FEdgeSegment* Segment)
	{

		PreviousSegment = Segment;
	}
};
}

