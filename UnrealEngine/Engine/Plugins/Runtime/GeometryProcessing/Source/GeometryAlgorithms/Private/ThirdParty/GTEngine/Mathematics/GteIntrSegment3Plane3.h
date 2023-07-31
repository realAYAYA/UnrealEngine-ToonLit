// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Plane3.h>

namespace gte
{

template <typename Real>
class TIQuery<Real, Segment3<Real>, Plane3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Segment3<Real> const& segment,
        Plane3<Real> const& plane);
};

template <typename Real>
class FIQuery<Real, Segment3<Real>, Plane3<Real>>
    :
    public FIQuery<Real, Line3<Real>, Plane3<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line3<Real>, Plane3<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Segment3<Real> const& segment,
        Plane3<Real> const& plane);

protected:
    void DoQuery(Vector3<Real> const& segOrigin,
        Vector3<Real> const& segDirection, Real segExtent,
        Plane3<Real> const& plane, Result& result);
};


template <typename Real>
typename TIQuery<Real, Segment3<Real>, Plane3<Real>>::Result
TIQuery<Real, Segment3<Real>, Plane3<Real>>::operator()(
    Segment3<Real> const& segment, Plane3<Real> const& plane)
{
    Result result;

    // Compute the (signed) distance from the segment endpoints to the plane.
    DCPQuery<Real, Vector3<Real>, Plane3<Real>> vpQuery;
    Real sdistance0 = vpQuery(segment.p[0], plane).signedDistance;
    if (sdistance0 == (Real)0)
    {
        // Endpoint p[0] is on the plane.
        result.intersect = true;
        return result;
    }

    Real sdistance1 = vpQuery(segment.p[1], plane).signedDistance;
    if (sdistance1 == (Real)0)
    {
        // Endpoint p[1] is on the plane.
        result.intersect = true;
        return result;
    }

    // Test whether the segment transversely intersects the plane.
    result.intersect = (sdistance0 * sdistance1 < (Real)0);
    return result;
}

template <typename Real>
typename FIQuery<Real, Segment3<Real>, Plane3<Real>>::Result
FIQuery<Real, Segment3<Real>, Plane3<Real>>::operator()(
    Segment3<Real> const& segment, Plane3<Real> const& plane)
{
    Vector3<Real> segOrigin, segDirection;
    Real segExtent;
    segment.GetCenteredForm(segOrigin, segDirection, segExtent);

    Result result;
    DoQuery(segOrigin, segDirection, segExtent, plane, result);
    if (result.intersect)
    {
        result.point = segOrigin + result.parameter * segDirection;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Segment3<Real>, Plane3<Real>>::DoQuery(
    Vector3<Real> const& segOrigin, Vector3<Real> const& segDirection,
    Real segExtent, Plane3<Real> const& plane, Result& result)
{
    FIQuery<Real, Line3<Real>, Plane3<Real>>::DoQuery(segOrigin,
        segDirection, plane, result);
    if (result.intersect)
    {
        // The line intersects the plane in a point that might not be on the
        // segment.
        if (std::abs(result.parameter) > segExtent)
        {
            result.intersect = false;
            result.numIntersections = 0;
        }
    }
}


}
