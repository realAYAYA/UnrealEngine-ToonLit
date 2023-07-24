// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteRay.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Cone3.h>

// The queries consider the cone to be single sided and solid.

namespace gte
{

template <typename Real>
class FIQuery<Real, Ray3<Real>, Cone3<Real>>
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

    Result operator()(Ray3<Real> const& ray, Cone3<Real> const& cone);

protected:
    void DoQuery(Vector3<Real> const& rayOrigin,
        Vector3<Real> const& rayDirection, Cone3<Real> const& cone,
        Result& result);
};


template <typename Real>
typename FIQuery<Real, Ray3<Real>, Cone3<Real>>::Result
FIQuery<Real, Ray3<Real>, Cone3<Real>>::operator()(Ray3<Real> const& ray,
    Cone3<Real> const& cone)
{
    Result result;
    DoQuery(ray.origin, ray.direction, cone, result);
    switch (result.type)
    {
    case 1:  // point
        result.point[0] = ray.origin + result.parameter[0] * ray.direction;
        result.point[1] = result.point[0];
        break;
    case 2:  // segment
        result.point[0] = ray.origin + result.parameter[0] * ray.direction;
        result.point[1] = ray.origin + result.parameter[1] * ray.direction;
        break;
    case 3:  // ray
        result.point[0] = ray.origin + result.parameter[0] * ray.direction;
        result.point[1] = ray.direction;
        break;
    default:  // no intersection
        break;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Ray3<Real>, Cone3<Real>>::DoQuery(
    Vector3<Real> const& rayOrigin, Vector3<Real> const& rayDirection,
    Cone3<Real> const& cone, Result& result)
{
    FIQuery<Real, Line3<Real>, Cone3<Real>>::DoQuery(rayOrigin,
        rayDirection, cone, result);

    if (result.intersect)
    {
        // The line containing the ray intersects the cone; the t-interval
        // is [t0,t1].  The ray intersects the cone as long as [t0,t1]
        // overlaps the ray t-interval [0,+infinity).
        std::array<Real, 2> rayInterval = {{
            (Real)0, std::numeric_limits<Real>::max() }};
        FIIntervalInterval<Real> iiQuery;
        auto iiResult = iiQuery(result.parameter, rayInterval);
        if (iiResult.intersect)
        {
            result.parameter = iiResult.overlap;
            if (result.parameter[1] < std::numeric_limits<Real>::max())
            {
                result.type = iiResult.numIntersections;
            }
            else
            {
                result.type = 3;
            }
        }
        else
        {
            result.intersect = false;
            result.type = 0;
        }
    }
}


}
