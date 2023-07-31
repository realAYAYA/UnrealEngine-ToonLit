// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, Ray<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real rayParameter;  // t in [0,+infinity)
        Vector<N, Real> rayClosest;  // origin + t * direction
    };

    Result operator()(Vector<N, Real> const& point, Ray<N, Real> const& ray);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointRay =
DCPQuery<Real, Vector<N, Real>, Ray<N, Real>>;

template <typename Real>
using DCPPoint2Ray2 = DCPPointRay<2, Real>;

template <typename Real>
using DCPPoint3Ray3 = DCPPointRay<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, Ray<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, Ray<N, Real>>::operator()(
    Vector<N, Real> const& point, Ray<N, Real> const& ray)
{
    Result result;

    Vector<N, Real> diff = point - ray.origin;
    result.rayParameter = Dot(ray.direction, diff);
    if (result.rayParameter > (Real)0)
    {
        result.rayClosest = ray.origin + result.rayParameter*ray.direction;
    }
    else
    {
        result.rayClosest = ray.origin;
    }

    diff = point - result.rayClosest;
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);

    return result;
}


}
