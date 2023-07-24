// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine2Triangle2.h>

// The queries consider the triangle to be a solid.

namespace gte
{

template <typename Real>
class TIQuery<Real, Segment2<Real>, Triangle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Segment2<Real> const& segment,
        Triangle2<Real> const& triangle);
};

template <typename Real>
class FIQuery <Real, Segment2<Real>, Triangle2<Real>>
    :
    public FIQuery<Real, Line2<Real>, Triangle2<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line2<Real>, Triangle2<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment2<Real> const& segment,
        Triangle2<Real> const& triangle);

protected:
    void DoQuery(Vector2<Real> const& segOrigin,
        Vector2<Real> const& segDirection, Real segExtent,
        Triangle2<Real> const& triangle, Result& result);
};


template <typename Real>
typename TIQuery<Real, Segment2<Real>, Triangle2<Real>>::Result
TIQuery<Real, Segment2<Real>, Triangle2<Real>>::operator()(
    Segment2<Real> const& segment, Triangle2<Real> const& triangle)
{
    Result result;
    FIQuery<Real, Segment2<Real>, Triangle2<Real>> stQuery;
    result.intersect = stQuery(segment, triangle).intersect;
    return result;
}

template <typename Real>
typename FIQuery<Real, Segment2<Real>, Triangle2<Real>>::Result
FIQuery<Real, Segment2<Real>, Triangle2<Real>>::operator()(
    Segment2<Real> const& segment, Triangle2<Real> const& triangle)
{
    Vector2<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, triangle, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = segOrigin + result.parameter[i] * segDirection;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment2<Real>, Triangle2<Real>>::DoQuery(
    Vector2<Real> const& segOrigin, Vector2<Real> const& segDirection,
    Real segExtent, Triangle2<Real> const& triangle, Result& result)
{
    FIQuery<Real, Line2<Real>, Triangle2<Real>>::DoQuery(segOrigin,
        segDirection, triangle, result);

    if (result.intersect)
    {
        // The line containing the segment intersects the disk; the t-interval
        // is [t0,t1].  The segment intersects the disk as long as [t0,t1]
        // overlaps the segment t-interval [-segExtent,+segExtent].
        std::array<Real, 2> segInterval = {{ -segExtent, segExtent }};
        FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>> iiQuery;
        result.parameter = iiQuery(result.parameter, segInterval).overlap;
    }
}


}
