// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistPoint3Plane3.h>
#include <ThirdParty/GTEngine/Mathematics/GteCircle3.h>
#include <ThirdParty/GTEngine/Mathematics/GteHypersphere.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

namespace gte
{

template <typename Real>
class TIQuery<Real, Plane3<Real>, Sphere3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Plane3<Real> const& plane, Sphere3<Real> const& sphere);
};

template <typename Real>
class FIQuery<Real, Plane3<Real>, Sphere3<Real>>
{
public:
    struct Result
    {
        bool intersect;

        // If 'intersect' is true, the intersection is either a point or a
        // circle.  When 'isCircle' is true, 'circle' is valid.  When
        // 'isCircle' is false, 'point' is valid.
        bool isCircle;
        Circle3<Real> circle;
        Vector3<Real> point;
    };

    Result operator()(Plane3<Real> const& plane, Sphere3<Real> const& sphere);
};


template <typename Real>
typename TIQuery<Real, Plane3<Real>, Sphere3<Real>>::Result
TIQuery<Real, Plane3<Real>, Sphere3<Real>>::operator()(
    Plane3<Real> const& plane, Sphere3<Real> const& sphere)
{
    Result result;
    DCPQuery<Real, Vector3<Real>, Plane3<Real>> ppQuery;
    auto ppResult = ppQuery(sphere.center, plane);
    result.intersect = (ppResult.distance <= sphere.radius);
    return result;
}


template <typename Real>
typename FIQuery<Real, Plane3<Real>, Sphere3<Real>>::Result
FIQuery<Real, Plane3<Real>, Sphere3<Real>>::operator()(
    Plane3<Real> const& plane, Sphere3<Real> const& sphere)
{
    Result result;
    DCPQuery<Real, Vector3<Real>, Plane3<Real>> ppQuery;
    auto ppResult = ppQuery(sphere.center, plane);
    if (ppResult.distance < sphere.radius)
    {
        result.intersect = true;
        result.isCircle = true;
        result.circle.center = sphere.center - ppResult.signedDistance * plane.normal;
        result.circle.normal = plane.normal;
        Real sum = sphere.radius + ppResult.distance;  // > 0
        Real dif = sphere.radius - ppResult.distance;  // > 0
        Real arg = sum * dif;  // sqr(sphere.radius) - sqr(ppResult.distance)
        result.circle.radius = std::sqrt(arg);
        return result;
    }
    else if (ppResult.distance == sphere.radius)
    {
        result.intersect = true;
        result.isCircle = false;
        result.point = sphere.center - ppResult.signedDistance * plane.normal;
        return result;
    }
    else
    {
        result.intersect = false;
        return result;
    }
}

}
