// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Cone3.h>

// The queries consider the cone to be single sided and solid.

namespace gte
{

template <typename Real>
class FIQuery<Real, Segment3<Real>, Cone3<Real>>
    :
    public FIQuery<Real, Line3<Real>, Cone3<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line3<Real>, Cone3<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment3<Real> const& segment, Cone3<Real> const& cone);

protected:
    void DoQuery(Vector3<Real> const& segOrigin,
        Vector3<Real> const& segDirection, Real segExtent,
        Cone3<Real> const& cone, Result& result);
};


template <typename Real>
typename FIQuery<Real, Segment3<Real>, Cone3<Real>>::Result
FIQuery<Real, Segment3<Real>, Cone3<Real>>::operator()(
    Segment3<Real> const& segment, Cone3<Real> const& cone)
{
    Vector3<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, cone, result);
    switch (result.type)
    {
    case 1:  // point
        result.point[0] = segOrigin + result.parameter[0] * segDirection;
        result.point[1] = result.point[0];
        break;
    case 2:  // segment
        result.point[0] = segOrigin + result.parameter[0] * segDirection;
        result.point[1] = segOrigin + result.parameter[1] * segDirection;
        break;
    default:  // no intersection
        break;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment3<Real>, Cone3<Real>>::DoQuery(
    Vector3<Real> const& segOrigin, Vector3<Real> const& segDirection,
    Real segExtent, Cone3<Real> const& cone, Result& result)
{
    FIQuery<Real, Line3<Real>, Cone3<Real>>::DoQuery(segOrigin,
        segDirection, cone, result);

    if (result.intersect)
    {
        // The line containing the segment intersects the cone; the
        // t-interval is [t0,t1].  The segment intersects the cone as
        // long as [t0,t1] overlaps the segment t-interval
        // [-segExtent,+segExtent].
        std::array<Real, 2> segInterval = {{ -segExtent, segExtent }};
        FIIntervalInterval<Real> iiQuery;
        auto iiResult = iiQuery(result.parameter, segInterval);
        if (iiResult.intersect)
        {
            result.parameter = iiResult.overlap;
            result.type = iiResult.numIntersections;
        }
        else
        {
            result.intersect = false;
            result.type = 0;
        }
    }
}


}
