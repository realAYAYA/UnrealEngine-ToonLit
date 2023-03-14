// Copyright Epic Games, Inc. All Rights Reserved.

#include "Intersection/IntersectionQueries2.h"

using namespace UE::Geometry;

template<typename Real>
bool UE::Geometry::TestIntersection(const TSegment2<Real>& Segment, const TAxisAlignedBox2<Real>& Box)
{
	// TODO Port Wild Magic IntrSegment2Box2 (which requires porting IntrLine2Box2) so we can call segment-box intersection here
	
	// If either endpoint is inside, then definitely (at least partially) contained
	if (Box.Contains(Segment.StartPoint()) || Box.Contains(Segment.EndPoint()))
	{
		return true;
	}

	// If both outside, have to do some intersections with the box sides

	if (Segment.Intersects(TSegment2<Real>(Box.GetCorner(0), Box.GetCorner(1))))
	{
		return true;
	}

	if (Segment.Intersects(TSegment2<Real>(Box.GetCorner(1), Box.GetCorner(2))))
	{
		return true;
	}

	return Segment.Intersects(TSegment2<Real>(Box.GetCorner(3), Box.GetCorner(2)));

	// Don't need to intersect with the fourth side because segment would have to intersect two sides
	// of box if both endpoints are outside the box.
}

namespace UE
{
namespace Geometry
{

template bool GEOMETRYCORE_API TestIntersection(const TSegment2<float>& Segment, const TAxisAlignedBox2<float>& Box);
template bool GEOMETRYCORE_API TestIntersection(const TSegment2<double>& Segment, const TAxisAlignedBox2<double>& Box);
		
} // namespace UE::Geometry
} // namespace UE
