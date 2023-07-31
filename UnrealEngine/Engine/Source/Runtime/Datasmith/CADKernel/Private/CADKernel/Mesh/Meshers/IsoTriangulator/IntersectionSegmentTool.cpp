// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Meshers/IsoTriangulator/IntersectionSegmentTool.h"

#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/Mesh/Structure/Grid.h"
#include "CADKernel/Utils/Util.h"

namespace UE::CADKernel
{ 

const FIsoSegment* FIntersectionSegmentTool::DoesIntersect(const FIsoSegment& Segment) const
{
	return DoesIntersect(Segment.GetFirstNode(), Segment.GetSecondNode());
}

FIsoSegment* FIntersectionSegmentTool::DoesIntersect(const FIsoSegment& Segment)
{
	return const_cast<FIsoSegment*> (static_cast<const FIntersectionSegmentTool*>(this)->DoesIntersect(Segment.GetFirstNode(), Segment.GetSecondNode()));
}

bool FIntersectionSegmentTool::FindIntersections(const FIsoNode& StartNode, const FIsoNode& EndNode, TArray<const FIsoSegment*>& OutIntersectedSegments) const
{
	const FPoint2D& StartPoint = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
	const FPoint2D& EndPoint = EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
	const FSegment4IntersectionTools StartEndSegment(StartPoint, EndPoint);

	OutIntersectedSegments.Empty(10);

	for (const FSegment4IntersectionTools& Segment : Segments)
	{
		if (!Segment.IsoSegment)
		{
			continue;
		}

		if (&Segment.IsoSegment->GetFirstNode() == &StartNode || &Segment.IsoSegment->GetSecondNode() == &StartNode)
		{
			continue;
		}

		if (&Segment.IsoSegment->GetFirstNode() == &EndNode || &Segment.IsoSegment->GetSecondNode() == &EndNode)
		{
			continue;
		}

		if (IntersectSegments2D(Segment.Segment2D, StartEndSegment.Segment2D))
		{
			OutIntersectedSegments.Add(Segment.IsoSegment);
		}
	}

	return OutIntersectedSegments.Num() > 0;
}

const FIsoSegment* FIntersectionSegmentTool::DoesIntersect(const FIsoNode& StartNode, const FIsoNode& EndNode) const
{
	const FPoint2D& StartPoint = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
	const FPoint2D& EndPoint = EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
	const FSegment4IntersectionTools StartEndSegment(StartPoint, EndPoint);

	double AxisMax = StartEndSegment.Boundary[EIso::IsoU].Max + StartEndSegment.Boundary[EIso::IsoV].Max;

	for (const FSegment4IntersectionTools& Segment : Segments)
	{
		if (!Segment.IsoSegment || Segment.IsoSegment->IsDelete())
		{
			continue;
		}

		if (bSegmentsAreSorted && AxisMax < Segment.AxisMin)
		{
			break;
		}

		if (&Segment.IsoSegment->GetFirstNode() == &StartNode || &Segment.IsoSegment->GetSecondNode() == &StartNode)
		{
			continue;
		}

		if (&Segment.IsoSegment->GetFirstNode() == &EndNode || &Segment.IsoSegment->GetSecondNode() == &EndNode)
		{
			continue;
		}

		if (IntersectSegments2D(Segment.Segment2D, StartEndSegment.Segment2D))
		{
			//DisplaySegment(Segment.Segment2D.Point0, Segment.Segment2D.Point1, 0, RedCurve);
			return Segment.IsoSegment;
		}
	}

	return nullptr;
}

bool FIntersectionSegmentTool::DoesIntersect(const FIsoNode& StartNode, const FPoint2D& EndPoint) const
{
	const FPoint2D& StartPoint = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
	const FSegment4IntersectionTools StartEndSegment(StartPoint, EndPoint);

	double DMax = StartEndSegment.Boundary[EIso::IsoU].Max + StartEndSegment.Boundary[EIso::IsoV].Max;

	for (const FSegment4IntersectionTools& Segment : Segments)
	{
		if (!Segment.IsoSegment)
		{
			continue;
		}

		if (bSegmentsAreSorted && DMax < Segment.AxisMin)
		{
			break;
		}

		if (&Segment.IsoSegment->GetFirstNode() == &StartNode || &Segment.IsoSegment->GetSecondNode() == &StartNode)
		{
			continue;
		}

		if (!StartEndSegment.CouldItIntersect(Segment))
		{
			continue;
		}

		if (IntersectSegments2D(Segment.Segment2D, StartEndSegment.Segment2D))
		{
			return true;
		}
	}

	return false;
}

bool FIntersectionSegmentTool::DoesIntersect(const FPoint2D& StartPoint, const FPoint2D& EndPoint) const
{
	const FSegment4IntersectionTools StartEndSegment(StartPoint, EndPoint);
	double DMax = StartEndSegment.Boundary[EIso::IsoU].Max + StartEndSegment.Boundary[EIso::IsoV].Max;

	for (const FSegment4IntersectionTools& Segment : Segments)
	{
		if (bSegmentsAreSorted && DMax < Segment.AxisMin)
		{
			break;
		}

		if (!StartEndSegment.CouldItIntersect(Segment))
		{
			continue;
		}

		if (IntersectSegments2D(Segment.Segment2D, StartEndSegment.Segment2D))
		{
			return true;
		}
	}
	return false;
}

FSegment4IntersectionTools::FSegment4IntersectionTools(const FGrid& Grid, const FIsoSegment& InSegment)
	: Segment2D(InSegment.GetFirstNode().Get2DPoint(EGridSpace::UniformScaled, Grid), InSegment.GetSecondNode().Get2DPoint(EGridSpace::UniformScaled, Grid))
	, Boundary(Segment2D.Point0, Segment2D.Point1)
	, IsoSegment(&InSegment)
{
	AxisMin = Boundary[EIso::IsoU].Min + Boundary[EIso::IsoV].Min;
}

} // namespace UE::CADKernel
