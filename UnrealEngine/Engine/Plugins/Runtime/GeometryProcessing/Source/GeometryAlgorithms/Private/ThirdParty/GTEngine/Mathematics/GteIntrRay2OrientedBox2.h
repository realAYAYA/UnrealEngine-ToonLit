// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrRay2AlignedBox2.h>

// The queries consider the box to be a solid.
//
// The test-intersection queries use the method of separating axes.  The
// find-intersection queries use parametric clipping against the four edges of
// the box.

namespace gte
{

template <typename Real>
class TIQuery<Real, Ray2<Real>, OrientedBox2<Real>>
    :
    public TIQuery<Real, Ray2<Real>, AlignedBox2<Real>>
{
public:
    struct Result
        :
        public TIQuery<Real, Ray2<Real>, AlignedBox2<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Ray2<Real> const& ray, OrientedBox2<Real> const& box);
};

template <typename Real>
class FIQuery<Real, Ray2<Real>, OrientedBox2<Real>>
    :
    public FIQuery<Real, Ray2<Real>, AlignedBox2<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Ray2<Real>, AlignedBox2<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Ray2<Real> const& ray, OrientedBox2<Real> const& box);
};


template <typename Real>
typename TIQuery<Real, Ray2<Real>, OrientedBox2<Real>>::Result
TIQuery<Real, Ray2<Real>, OrientedBox2<Real>>::operator()(
    Ray2<Real> const& ray, OrientedBox2<Real> const& box)
{
    // Transform the ray to the oriented-box coordinate system.
    Vector2<Real> diff = ray.origin - box.center;
    Vector2<Real> rayOrigin
    {
        Dot(diff, box.axis[0]),
        Dot(diff, box.axis[1])
    };
    Vector2<Real> rayDirection = Vector2<Real>
    {
        Dot(ray.direction, box.axis[0]),
            Dot(ray.direction, box.axis[1])
    };

    Result result;
    this->DoQuery(rayOrigin, rayDirection, box.extent, result);
    return result;
}

template <typename Real>
typename FIQuery<Real, Ray2<Real>, OrientedBox2<Real>>::Result
FIQuery<Real, Ray2<Real>, OrientedBox2<Real>>::operator()(
    Ray2<Real> const& ray, OrientedBox2<Real> const& box)
{
    // Transform the ray to the oriented-box coordinate system.
    Vector2<Real> diff = ray.origin - box.center;
    Vector2<Real> rayOrigin
    {
        Dot(diff, box.axis[0]),
        Dot(diff, box.axis[1])
    };
    Vector2<Real> rayDirection = Vector2<Real>
    {
        Dot(ray.direction, box.axis[0]),
            Dot(ray.direction, box.axis[1])
    };

    Result result;
    this->DoQuery(rayOrigin, rayDirection, box.extent, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = ray.origin + result.parameter[i] * ray.direction;
    }
    return result;
}


}
