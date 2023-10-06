// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistRaySegment.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Capsule3.h>

// The queries consider the capsule to be a solid.
//
// The test-intersection queries are based on distance computations.

namespace gte
{

template <typename Real>
class TIQuery<Real, Ray3<Real>, Capsule3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Ray3<Real> const& ray, Capsule3<Real> const& capsule);
};

template <typename Real>
class FIQuery<Real, Ray3<Real>, Capsule3<Real>>
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

    Result operator()(Ray3<Real> const& ray, Capsule3<Real> const& capsule);

protected:
    void DoQuery(Vector3<Real> const& rayOrigin,
        Vector3<Real> const& rayDirection, Capsule3<Real> const& capsule,
        Result& result);
};


template <typename Real>
typename TIQuery<Real, Ray3<Real>, Capsule3<Real>>::Result
TIQuery<Real, Ray3<Real>, Capsule3<Real>>::operator()(
    Ray3<Real> const& ray, Capsule3<Real> const& capsule)
{
    Result result;
    DCPQuery<Real, Ray3<Real>, Segment3<Real>> rsQuery;
    auto rsResult = rsQuery(ray, capsule.segment);
    result.intersect = (rsResult.distance <= capsule.radius);
    return result;
}

template <typename Real>
typename FIQuery<Real, Ray3<Real>, Capsule3<Real>>::Result
FIQuery<Real, Ray3<Real>, Capsule3<Real>>::operator()(
    Ray3<Real> const& ray, Capsule3<Real> const& capsule)
{
    Result result;
    DoQuery(ray.origin, ray.direction, capsule, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = ray.origin + result.parameter[i] * ray.direction;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Ray3<Real>, Capsule3<Real>>::DoQuery(
    Vector3<Real> const& rayOrigin, Vector3<Real> const& rayDirection,
    Capsule3<Real> const& capsule, Result& result)
{
    FIQuery<Real, Line3<Real>, Capsule3<Real>>::DoQuery(rayOrigin,
        rayDirection, capsule, result);

    if (result.intersect)
    {
        // The line containing the ray intersects the capsule; the t-interval
        // is [t0,t1].  The ray intersects the capsule as long as [t0,t1]
        // overlaps the ray t-interval [0,+infinity).
        std::array<Real, 2> rayInterval =
        { (Real)0, std::numeric_limits<Real>::max() };
        FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>> iiQuery;
        auto iiResult = iiQuery(result.parameter, rayInterval);
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
