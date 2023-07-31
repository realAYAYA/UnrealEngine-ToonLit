// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/Boundary.h"
#include "CADKernel/Math/Geometry.h"
#include "CADKernel/UI/Message.h"
#include "CADKernel/UI/Visu.h"

namespace UE::CADKernel
{
class FGrid;
class FIsoNode;
class FIsoSegment;
class FPoint2D;

struct FSegment4IntersectionTools
{
	const TSegment<FPoint2D> Segment2D;

	/**
	 * Segment's axis aligned bounding box
	 */
	const FSurfacicBoundary Boundary;

	const FIsoSegment* IsoSegment;

	/**
	 * Uses as criterion to sort segments to optimize
	 * AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min
	 */
	double AxisMin;

	FSegment4IntersectionTools(const FGrid& Grid, const FIsoSegment& InSegment);

	/**
	 * WARNING StartPoint, EndPoint must be defined in EGridSpace::UniformScaled
	 */
	FSegment4IntersectionTools(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
		: Segment2D(StartPoint, EndPoint)
		, Boundary(StartPoint, EndPoint)
		, IsoSegment(nullptr)
	{
		AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min;
	}

	/**
	 * @return false if their Boundaries are not intersecting
	 */
	bool CouldItIntersect(const FSegment4IntersectionTools& Segment) const
	{
		if ((Boundary[EIso::IsoU].Min > Segment.Boundary[EIso::IsoU].Max) || (Boundary[EIso::IsoV].Min > Segment.Boundary[EIso::IsoV].Max))
		{
			return false;
		}

		if ((Boundary[EIso::IsoU].Max < Segment.Boundary[EIso::IsoU].Min) || (Boundary[EIso::IsoV].Max < Segment.Boundary[EIso::IsoV].Min))
		{
			return false;
		}
		return true;
	}
};

class FIntersectionSegmentTool
{
	TArray<FSegment4IntersectionTools> Segments;
	const FGrid& Grid;
	bool bSegmentsAreSorted;

public:
	FIntersectionSegmentTool(const FGrid& InGrid)
		: Grid(InGrid)
		, bSegmentsAreSorted(false)
	{
	}

	void Empty(int32 InMaxNum)
	{
		Segments.Empty(InMaxNum);
		bSegmentsAreSorted = false;
	}

	void Reserve(int32 InMaxNum)
	{
		Segments.Reserve(InMaxNum);
	}

	void RemoveLast()
	{
		ensureCADKernel(!bSegmentsAreSorted);
		Segments.RemoveAt(Segments.Num() - 1);
		bSegmentsAreSorted = false;
	}

	void SetCount(int32 NewCount)
	{
		ensureCADKernel(NewCount < Segments.Num() && !bSegmentsAreSorted);
		while (NewCount != Segments.Num())
		{
			Segments.RemoveAt(Segments.Num() - 1);
		}
	}

	/**
	 * @return true if the segment is found and removed
	 */
	bool Remove(const FIsoSegment* Segment)
	{
		int32 SegmentIndex = Segments.IndexOfByPredicate([Segment](const FSegment4IntersectionTools& SegmentIter)
			{
				return SegmentIter.IsoSegment == Segment;
			});

		if (SegmentIndex != INDEX_NONE)
		{
			Segments.RemoveAt(SegmentIndex);
			return true;
		}
		return false;
	}

	int32 Count()
	{
		return Segments.Num();
	}

	bool Update(const FIsoSegment* Segment)
	{
		int32 SegmentIndex = Segments.IndexOfByPredicate([Segment](const FSegment4IntersectionTools& SegmentIter)
			{
				return SegmentIter.IsoSegment == Segment;
			});

		if (SegmentIndex != INDEX_NONE)
		{
			Segments.RemoveAt(SegmentIndex);
			Segments.EmplaceAt(SegmentIndex, Grid, *Segment);
			bSegmentsAreSorted = false;
			return true;
		}
		return false;

	}

	void AddSegments(FIsoSegment** InNewSegments, int32 Count)
	{
		bSegmentsAreSorted = false;
		Segments.Reserve(Count + Segments.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FIsoSegment* NewSegment = InNewSegments[Index];
			AddSegment(*NewSegment);
		}
	}

	void AddSegments(const TArray<FIsoSegment*>& InNewSegments)
	{
		bSegmentsAreSorted = false;
		Segments.Reserve(InNewSegments.Num() + Segments.Num());
		for (FIsoSegment* Segment : InNewSegments)
		{
			AddSegment(*Segment);
		}
	}

	void AddSegment(const FIsoSegment& Segment)
	{
		bSegmentsAreSorted = false;
		Segments.Emplace(Grid, Segment);
	}

	void AddSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
	{
		bSegmentsAreSorted = false;
		Segments.Emplace(StartPoint, EndPoint);
	}

	const FIsoSegment* DoesIntersect(const FIsoSegment& Segment) const;
	FIsoSegment* DoesIntersect(const FIsoSegment& Segment);

	/**
	 * WARNING StartPoint, EndPoint must be defined in EGridSpace::UniformScaled
	 */
	bool DoesIntersect(const FPoint2D& StartPoint, const FPoint2D& EndPoint) const;

	/**
	 * Allow StartNode to be connected to one segment
	 * WARNING EndPoint must be defined in EGridSpace::UniformScaled
	 */
	bool DoesIntersect(const FIsoNode& StartNode, const FPoint2D& EndPoint) const;

	/**
	 * Allow StartNode and EndNode to be connected to one segment
	 */
	const FIsoSegment* DoesIntersect(const FIsoNode& StartNode, const FIsoNode& EndNode) const;

	bool FindIntersections(const FIsoNode& StartNode, const FIsoNode& EndNode, TArray<const FIsoSegment*>& OutIntersections) const;

	/**
	 * segments are sorted by DMin increasing
	 */
	void Sort()
	{
		Algo::Sort(Segments, [](const FSegment4IntersectionTools& Segment1, const FSegment4IntersectionTools& Segment2) { return Segment1.AxisMin < Segment2.AxisMin; });
		bSegmentsAreSorted = true;
	}

#ifdef CADKERNEL_DEV
	void Display(const TCHAR* Message, EVisuProperty Property = EVisuProperty::BlueCurve) const;
#endif

};
} // namespace UE::CADKernel

