// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistSegmentSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Capsule3.h>

// The queries consider the capsule to be a solid.
//
// The test-intersection queries are based on distance computations.

namespace gte
{

template <typename Real>
class TIQuery<Real, Segment3<Real>, Capsule3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Segment3<Real> const& segment,
        Capsule3<Real> const& capsule);
};

template <typename Real>
class FIQuery<Real, Segment3<Real>, Capsule3<Real>>
    :
    public FIQuery<Real, Line3<Real>, Capsule3<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line3<Real>, Capsule3<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment3<Real> const& segment,
        Capsule3<Real> const& capsule);

protected:
    void DoQuery(Vector3<Real> const& segOrigin,
        Vector3<Real> const& segDirection, Real segExtent,
        Capsule3<Real> const& capsule, Result& result);
};


template <typename Real>
typename TIQuery<Real, Segment3<Real>, Capsule3<Real>>::Result
TIQuery<Real, Segment3<Real>, Capsule3<Real>>::operator()(
    Segment3<Real> const& segment, Capsule3<Real> const& capsule)
{
    Result result;
    DCPQuery<Real, Segment3<Real>, Segment3<Real>> ssQuery;
    auto ssResult = ssQuery(segment, capsule.segment);
    result.intersect = (ssResult.distance <= capsule.radius);
    return result;
}

template <typename Real>
typename FIQuery<Real, Segment3<Real>, Capsule3<Real>>::Result
FIQuery<Real, Segment3<Real>, Capsule3<Real>>::operator()(
    Segment3<Real> const& segment, Capsule3<Real> const& capsule)
{
    Vector3<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, capsule, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = segOrigin + result.parameter[i] * segDirection;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment3<Real>, Capsule3<Real>>::DoQuery(
    Vector3<Real> const& segOrigin, Vector3<Real> const& segDirection,
    Real segExtent, Capsule3<Real> const& capsule, Result& result)
{
    FIQuery<Real, Line3<Real>, Capsule3<Real>>::DoQuery(segOrigin,
        segDirection, capsule, result);

    if (result.intersect)
    {
        // The line containing the segment intersects the capsule; the
        // t-interval is [t0,t1].  The segment intersects the capsule as
        // long as [t0,t1] overlaps the segment t-interval
        // [-segExtent,+segExtent].
        std::array<Real, 2> segInterval = {{ -segExtent, segExtent }};
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
