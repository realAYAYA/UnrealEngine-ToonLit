// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine2Circle2.h>

// The queries consider the circle to be a solid (disk).

namespace gte
{

template <typename Real>
class TIQuery<Real, Segment2<Real>, Circle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Segment2<Real> const& segment,
        Circle2<Real> const& circle);
};

template <typename Real>
class FIQuery<Real, Segment2<Real>, Circle2<Real>>
    :
    public FIQuery<Real, Line2<Real>, Circle2<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line2<Real>, Circle2<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment2<Real> const& segment,
        Circle2<Real> const& circle);

protected:
    void DoQuery(Vector2<Real> const& segyOrigin,
        Vector2<Real> const& segyDirection, Real segExtent,
        Circle2<Real> const& circle, Result& result);
};


template <typename Real>
typename TIQuery<Real, Segment2<Real>, Circle2<Real>>::Result
TIQuery<Real, Segment2<Real>, Circle2<Real>>::operator()(
    Segment2<Real> const& segment, Circle2<Real> const& circle)
{
    Result result;
    FIQuery<Real, Segment2<Real>, Circle2<Real>> scQuery;
    result.intersect = scQuery(segment, circle).intersect;
    return result;
}

template <typename Real>
typename FIQuery<Real, Segment2<Real>, Circle2<Real>>::Result
FIQuery<Real, Segment2<Real>, Circle2<Real>>::operator()(
    Segment2<Real> const& segment, Circle2<Real> const& circle)
{
    Vector2<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, circle, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = segOrigin + result.parameter[i] * segDirection;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment2<Real>, Circle2<Real>>::DoQuery(
    Vector2<Real> const& segOrigin, Vector2<Real> const& segDirection,
    Real segExtent, Circle2<Real> const& circle, Result& result)
{
    FIQuery<Real, Line2<Real>, Circle2<Real>>::DoQuery(segOrigin,
        segDirection, circle, result);

    if (result.intersect)
    {
        // The line containing the segment intersects the disk; the t-interval
        // is [t0,t1].  The segment intersects the disk as long as [t0,t1]
        // overlaps the segment t-interval [-segExtent,+segExtent].
        std::array<Real, 2> segInterval = {{ -segExtent, segExtent }};
        FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>> iiQuery;
        auto iiResult = iiQuery(result.parameter, segInterval);
        result.intersect = iiResult.intersect;
        result.numIntersections = iiResult.numIntersections;
        result.parameter = iiResult.overlap;
    }
}


}
