// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Meshers/IsoTriangulator/IntersectionSegmentTool.h"

#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/Mesh/Structure/Grid.h"
#include "CADKernel/Utils/Util.h"

namespace UE::CADKernel
{ 

const FIsoSegment* FIntersectionSegmentTool::FindIntersectingSegment(const FIsoSegment& Segment) const
{
	using namespace IntersectionSegmentTool;
	return TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&Segment.GetFirstNode(), &Segment.GetSecondNode());
}

FIsoSegment* FIntersectionSegmentTool::FindIntersectingSegment(const FIsoSegment& Segment)
{
	return const_cast<FIsoSegment*> (static_cast<const FIntersectionSegmentTool*>(this)->TIntersectionSegmentTool<IntersectionSegmentTool::FSegment>::FindIntersectingSegment(&Segment.GetFirstNode(), &Segment.GetSecondNode()));
}

IntersectionSegmentTool::FSegment::FSegment(const FGrid& Grid, const FIsoNode& StartNode, const FIsoNode& EndNode)
	: IntersectionToolBase::FSegment(StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid), EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid))
	, IsoSegment(nullptr)
{
}

IntersectionSegmentTool::FSegment::FSegment(const FGrid& Grid, const FIsoNode& StartNode, const FPoint2D& EndPoint)
	: IntersectionToolBase::FSegment(StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid), EndPoint)
	, IsoSegment(nullptr)
{
}

IntersectionSegmentTool::FSegment::FSegment(const FGrid& Grid, const FIsoSegment& InSegment)
	: IntersectionToolBase::FSegment(InSegment.GetFirstNode().Get2DPoint(EGridSpace::UniformScaled, Grid), InSegment.GetSecondNode().Get2DPoint(EGridSpace::UniformScaled, Grid))
	, IsoSegment(&InSegment)
{
}

IntersectionNodePairTool::FSegment::FSegment(const FIsoNode* InStartNode, const FIsoNode* InEndNode, const FPoint2D& InStartPoint, const FPoint2D& InEndPoint)
	: IntersectionToolBase::FSegment(InStartPoint, InEndPoint)
	, StartNode(InStartNode)
	, EndNode(InEndNode)
{
}

IntersectionNodePairTool::FSegment::FSegment(const FGrid& Grid, const FIsoNode& InStartNode, const FIsoNode& InEndNode)
	: IntersectionToolBase::FSegment(InStartNode.Get2DPoint(EGridSpace::UniformScaled, Grid), InEndNode.Get2DPoint(EGridSpace::UniformScaled, Grid))
	, StartNode(&InStartNode)
	, EndNode(&InEndNode)
{
}

bool IntersectionSegmentTool::FSegment::IsValid() const
{
	return IsoSegment != nullptr && !IsoSegment->IsDelete();
}


const FIsoNode* IntersectionSegmentTool::FSegment::GetFirstNode() const
{
	return &IsoSegment->GetFirstNode();
}

const FIsoNode* IntersectionSegmentTool::FSegment::GetSecondNode() const
{
	return &IsoSegment->GetSecondNode();
}

} // namespace UE::CADKernel
