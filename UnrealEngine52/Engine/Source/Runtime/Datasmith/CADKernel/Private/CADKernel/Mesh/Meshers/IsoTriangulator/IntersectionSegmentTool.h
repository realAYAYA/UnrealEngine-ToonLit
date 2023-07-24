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
class FIsoInnerNode;
class FIsoSegment;
class FPoint2D;

namespace IntersectionToolBase
{
struct FSegment
{
	const TSegment<FPoint2D> Segment2D;

	/**
	 * Segment's axis aligned bounding box
	 */
	const FSurfacicBoundary Boundary;

	/**
	 * Uses as criterion to sort segments to optimize
	 * AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min
	 */
	double AxisMin;
	double AxisMax;

	/**
	 * WARNING StartPoint, EndPoint must be defined in EGridSpace::UniformScaled
	 */
	FSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
		: Segment2D(StartPoint, EndPoint)
		, Boundary(StartPoint, EndPoint)
	{
		AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min;
		AxisMax = Boundary[EIso::IsoU].Max + Boundary[EIso::IsoV].Max;
	}

	virtual ~FSegment() = default;

	virtual bool IsValid() const = 0;

	virtual const FIsoNode* GetFirstNode() const = 0;
	virtual const FIsoNode* GetSecondNode() const = 0;

	virtual const FIsoSegment* GetIsoSegment() const 
	{
		return nullptr;
	}

	bool DoesItStartFrom(const FIsoNode* StartNode, const FIsoNode* EndNode) const
	{
		if (GetFirstNode() == StartNode || GetSecondNode() == StartNode)
		{
			return true;
		}

		if (GetFirstNode() == EndNode || GetSecondNode() == EndNode)
		{
			return true;
		}

		return false;
	}

	bool DoesItStartFrom(const FIsoNode* StartNode, const FPoint2D* EndPoint) const
	{
		if (GetFirstNode() == StartNode || GetSecondNode() == StartNode)
		{
			return true;
		}

		return false;
	}

	bool DoesItStartFrom(const FPoint2D* StartPoint, const FPoint2D* EndPoint) const
	{
		return false;
	}

	bool IsFullyBefore(const FSegment& Segment) const
	{
		return AxisMax < Segment.AxisMin;
	}

	bool IsFullyAfter(const FSegment& Segment) const
	{
		return Segment.AxisMax < AxisMin;
	}

	bool CouldItIntersect(const FSurfacicBoundary& SegmentBoundary) const
	{
		if ((Boundary[EIso::IsoU].Min > SegmentBoundary[EIso::IsoU].Max) || (Boundary[EIso::IsoV].Min > SegmentBoundary[EIso::IsoV].Max))
		{
			return false;
		}

		if ((Boundary[EIso::IsoU].Max < SegmentBoundary[EIso::IsoU].Min) || (Boundary[EIso::IsoV].Max < SegmentBoundary[EIso::IsoV].Min))
		{
			return false;
		}
		return true;
	}

	bool DoesItIntersect(const FSegment& Segment) const
	{
		if (!CouldItIntersect(Segment.Boundary))
		{
			return false;
		}

		return IntersectSegments2D(Segment2D, Segment.Segment2D);
	}
};
}

namespace IntersectionSegmentTool
{

struct FSegment : public IntersectionToolBase::FSegment
{
	const FIsoSegment* IsoSegment;

	FSegment(const FGrid& Grid, const FIsoSegment& InSegment);
	FSegment(const FGrid& Grid, const FIsoNode& StartNode, const FIsoNode& EndNode);
	FSegment(const FGrid& Grid, const FIsoNode& StartNode, const FPoint2D& EndPoint);
	FSegment(const FGrid& Grid, const FPoint2D& StartPoint, const FPoint2D& EndPoint)
		: IntersectionToolBase::FSegment(StartPoint, EndPoint)
		, IsoSegment(nullptr)
	{
	}

	virtual bool IsValid() const override;

	virtual const FIsoNode* GetFirstNode() const override;
	virtual const FIsoNode* GetSecondNode() const override;

	virtual const FIsoSegment* GetIsoSegment() const override
	{
		return IsoSegment;
	}

};

}

namespace IntersectionNodePairTool
{

struct FSegment : public IntersectionToolBase::FSegment
{
	const FIsoNode* StartNode;
	const FIsoNode* EndNode;

	FSegment(const FIsoNode* StartNode, const FIsoNode* EndNode, const FPoint2D& StartPoint, const FPoint2D& EndPoint);
	FSegment(const FGrid& Grid, const FIsoNode& StartNode, const FIsoNode& EndNode);

	virtual bool IsValid() const override
	{
		return true;
	}

	virtual const FIsoNode* GetFirstNode() const override
	{
		return StartNode;
	}

	virtual const FIsoNode* GetSecondNode() const override
	{
		return EndNode;
	}

};

}

template<typename SegmentType>
class TIntersectionSegmentTool
{
protected:
	const FGrid& Grid;

	TArray<SegmentType> Segments;
	bool bSegmentsAreSorted;

public:
	TIntersectionSegmentTool(const FGrid& InGrid)
		: Grid(InGrid)
		, bSegmentsAreSorted(false)
	{
	}

	virtual ~TIntersectionSegmentTool() = default;

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
		Segments.SetNum(NewCount);
	}

	int32 Count()
	{
		return Segments.Num();
	}

	/**
	 * segments are sorted by DMin increasing
	 */
	void Sort()
	{
		Algo::Sort(Segments, [](const SegmentType& Segment1, const SegmentType& Segment2) { return Segment1.AxisMin < Segment2.AxisMin; });
		bSegmentsAreSorted = true;
	}

	template<typename ExtremityType1, typename ExtremityType2>
	const FIsoSegment* FindIntersectingSegment(const ExtremityType1* StartExtremity, const ExtremityType2* EndExtremity) const
	{
		using namespace IntersectionSegmentTool;
		FSegment InSegment(Grid, *StartExtremity, *EndExtremity);

		for (const FSegment& Segment : Segments)
		{
			if (!Segment.IsValid())
			{
				continue;
			}

			if (bSegmentsAreSorted)
			{
				if (Segment.IsFullyBefore(InSegment))
				{
					continue;
				}

				if (Segment.IsFullyAfter(InSegment))
				{
					break;
				}
}

			if (Segment.DoesItStartFrom(StartExtremity, EndExtremity))
			{
				continue;
			}

			if (Segment.DoesItIntersect(InSegment))
			{
				return Segment.IsoSegment;
			}
		}

		return nullptr;
	}

	template<typename ExtremityType1, typename ExtremityType2>
	int32 FindIntersectingSegments(const ExtremityType1* StartExtremity, const ExtremityType2* EndExtremity, TArray<const FIsoSegment*>* OutIntersectedSegments) const
	{
		SegmentType InSegment(Grid, *StartExtremity, *EndExtremity);
		if(OutIntersectedSegments)
		{
			OutIntersectedSegments->Empty(10);
		}

		int32 IntersectionCount = 0;
		for (const SegmentType& Segment : Segments)
		{
			if (!Segment.IsValid())
			{
				continue;
			}

			if (bSegmentsAreSorted)
			{
				if (Segment.IsFullyBefore(InSegment))
				{
					continue;
				}

				if (Segment.IsFullyAfter(InSegment))
				{
					break;
				}
}

			if (Segment.DoesItStartFrom(StartExtremity, EndExtremity))
			{
				continue;
			}

			if (Segment.DoesItIntersect(InSegment))
			{
				++IntersectionCount;
				if (OutIntersectedSegments)
				{
					OutIntersectedSegments->Add(Segment.GetIsoSegment());
				}
			}
		}

		return IntersectionCount;
	}

#ifdef CADKERNEL_DEV
	virtual void Display(bool bDisplay, const TCHAR* Message, EVisuProperty Property = EVisuProperty::BlueCurve) const
	{
		if (!bDisplay)
		{
			return;
		}

		int32 Index = 0;
		Open3DDebugSession(Message);
		for (const SegmentType& Segment : Segments)
		{
			DisplaySegment(Segment.Segment2D[0] * DisplayScale, Segment.Segment2D[1] * DisplayScale, Index++, Property);
		}
		Close3DDebugSession();
	}
#endif

};

class FIntersectionSegmentTool : public TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>
{
public:
	FIntersectionSegmentTool(const FGrid& InGrid)
		: TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>(InGrid)
	{
	}

	bool Update(const FIsoSegment* Segment)
	{
		int32 SegmentIndex = Segments.IndexOfByPredicate([Segment](const IntersectionSegmentTool::FSegment& SegmentIter)
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
		Segments.Emplace(Grid, StartPoint, EndPoint);
	}

	/**
	 * @return true if the segment is found and removed
	 */
	bool Remove(const FIsoSegment* Segment)
	{
		int32 SegmentIndex = Segments.IndexOfByPredicate([Segment](const IntersectionSegmentTool::FSegment& SegmentIter)
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

	const FIsoSegment* FindIntersectingSegment(const FIsoSegment& Segment) const;
	FIsoSegment* FindIntersectingSegment(const FIsoSegment& Segment);

	bool DoesIntersect(const FIsoSegment& Segment) const
	{
		return FindIntersectingSegment(Segment) != nullptr;
	}

	/**
	 * WARNING StartPoint, EndPoint must be defined in EGridSpace::UniformScaled
	 */
	bool DoesIntersect(const FPoint2D& StartPoint, const FPoint2D& EndPoint) const
	{
		return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&StartPoint, &EndPoint) != nullptr;
	}

	/**
	 * Allow StartNode to be connected to one segment
	 * WARNING EndPoint must be defined in EGridSpace::UniformScaled
	 */
	bool DoesIntersect(const FIsoNode& StartNode, const FPoint2D& EndPoint) const
	{
		return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&StartNode, &EndPoint) != nullptr;
	}

	bool DoesIntersect(const FIsoNode& StartNode, const FIsoNode& EndNode) const
	{
		return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&StartNode, &EndNode) != nullptr;
	}

	/**
	 * Allow StartNode and EndNode to be connected to one segment
	 */
	const FIsoSegment* FindIntersectingSegment(const FIsoNode& StartNode, const FIsoNode& EndNode) const
	{
		return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&StartNode, &EndNode);
	}

	bool FindIntersectingSegments(const FIsoNode& StartNode, const FIsoNode& EndNode, TArray<const FIsoSegment*>& OutIntersections) const
	{
		return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegments(&StartNode, &EndNode, &OutIntersections) > 0;
	}

};

class FIntersectionNodePairTool : public TIntersectionSegmentTool<IntersectionNodePairTool::FSegment>
{

public:
	FIntersectionNodePairTool(const FGrid& InGrid)
		: TIntersectionSegmentTool<IntersectionNodePairTool::FSegment>(InGrid)
	{
	}

	void AddSegment(const FIsoNode& StartNode, const FIsoNode& EndNode)
	{
		bSegmentsAreSorted = false;
		Segments.Emplace(Grid, StartNode, EndNode);
	}

	void AddSegment(const FIsoNode* StartNode, const FIsoNode* EndNode, const FPoint2D& StartPoint, const FPoint2D& EndPoint)
	{
		bSegmentsAreSorted = false;
		Segments.Emplace(StartNode, EndNode, StartPoint, EndPoint);
	}

	int32 CountIntersections(const FIsoNode& StartNode, const FIsoNode& EndNode) const
	{
		return TIntersectionSegmentTool<IntersectionNodePairTool::FSegment>::FindIntersectingSegments(&StartNode, &EndNode, nullptr);
	}

};

} // namespace UE::CADKernel

