// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Cylinder3.h>

// The queries consider the cylinder to be a solid.

namespace gte
{

template <typename Real>
class FIQuery<Real, Segment3<Real>, Cylinder3<Real>>
    :
    public FIQuery<Real, Line3<Real>, Cylinder3<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line3<Real>, Cylinder3<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment3<Real> const& segment,
        Cylinder3<Real> const& cylinder);

protected:
    void DoQuery(Vector3<Real> const& segOrigin,
        Vector3<Real> const& segDirection, Real segExtent,
        Cylinder3<Real> const& cylinder, Result& result);
};


template <typename Real>
typename FIQuery<Real, Segment3<Real>, Cylinder3<Real>>::Result
FIQuery<Real, Segment3<Real>, Cylinder3<Real>>::operator()(
    Segment3<Real> const& segment, Cylinder3<Real> const& cylinder)
{
    Vector3<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, cylinder, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = segOrigin + result.parameter[i] * segDirection;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment3<Real>, Cylinder3<Real>>::DoQuery(
    Vector3<Real> const& segOrigin, Vector3<Real> const& segDirection,
    Real segExtent, Cylinder3<Real> const& cylinder, Result& result)
{
    FIQuery<Real, Line3<Real>, Cylinder3<Real>>::DoQuery(segOrigin,
        segDirection, cylinder, result);

    if (result.intersect)
    {
        // The line containing the segment intersects the cylinder; the
        // t-interval is [t0,t1].  The segment intersects the cylinder as
        // long as [t0,t1] overlaps the segment t-interval
        // [-segExtent,+segExtent].
        std::array<Real, 2> segInterval = { -segExtent, segExtent };
        FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>> iiQuery;
        auto iiResult = iiQuery(result.parameter, segInterval);
        if (iiResult.intersect)
        {
            result.numIntersections = iiResult.numIntersections;
            result.parameter = iiResult.overlap;
        }
        else
        {
            result.intersect = false;
            result.numIntersections = 0;
        }
    }
}


}
