// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Line<N, Real>, Ray<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real parameter[2];
        Vector<N, Real> closestPoint[2];
    };

    Result operator()(Line<N, Real> const& line, Ray<N, Real> const& ray);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPLineRay = DCPQuery<Real, Line<N, Real>, Ray<N, Real>>;

template <typename Real>
using DCPLine2Ray2 = DCPLineRay<2, Real>;

template <typename Real>
using DCPLine3Ray3 = DCPLineRay<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Line<N, Real>, Ray<N, Real>>::Result
DCPQuery<Real, Line<N, Real>, Ray<N, Real>>::operator()(
    Line<N, Real> const& line, Ray<N, Real> const& ray)
{
    Result result;

    Vector<N, Real> diff = line.origin - ray.origin;
    Real a01 = -Dot(line.direction, ray.direction);
    Real b0 = Dot(diff, line.direction);
    Real s0, s1;

    if (std::abs(a01) < (Real)1)
    {
        Real b1 = -Dot(diff, ray.direction);
        s1 = a01 * b0 - b1;

        if (s1 >= (Real)0)
        {
            // Two interior points are closest, one on line and one on ray.
            Real det = (Real)1 - a01 * a01;
            s0 = (a01 * b1 - b0) / det;
            s1 /= det;
        }
        else
        {
            // Origin of ray and interior point of line are closest.
            s0 = -b0;
            s1 = (Real)0;
        }
    }
    else
    {
        // Lines are parallel, closest pair with one point at ray origin.
        s0 = -b0;
        s1 = (Real)0;
    }

    result.parameter[0] = s0;
    result.parameter[1] = s1;
    result.closestPoint[0] = line.origin + s0 * line.direction;
    result.closestPoint[1] = ray.origin + s1 * ray.direction;
    diff = result.closestPoint[0] - result.closestPoint[1];
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);
    return result;
}


}
