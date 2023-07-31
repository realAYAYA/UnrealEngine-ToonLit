// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistLine3Triangle3.h>
#include <ThirdParty/GTEngine/Mathematics/GteDistPointTriangle.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>

namespace gte
{

template <typename Real>
class DCPQuery<Real, Ray3<Real>, Triangle3<Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real rayParameter, triangleParameter[3];
        Vector3<Real> closestPoint[2];
    };

    Result operator()(Ray3<Real> const& ray, Triangle3<Real> const& triangle);
};


template <typename Real>
typename DCPQuery<Real, Ray3<Real>, Triangle3<Real>>::Result
DCPQuery<Real, Ray3<Real>, Triangle3<Real>>::operator()(
    Ray3<Real> const& ray, Triangle3<Real> const& triangle)
{
    Result result;

    Line3<Real> line(ray.origin, ray.direction);
    DCPQuery<Real, Line3<Real>, Triangle3<Real>> ltQuery;
    auto ltResult = ltQuery(line, triangle);

    if (ltResult.lineParameter >= (Real)0)
    {
        result.distance = ltResult.distance;
        result.sqrDistance = ltResult.sqrDistance;
        result.rayParameter = ltResult.lineParameter;
        result.triangleParameter[0] = ltResult.triangleParameter[0];
        result.triangleParameter[1] = ltResult.triangleParameter[1];
        result.triangleParameter[2] = ltResult.triangleParameter[2];
        result.closestPoint[0] = ltResult.closestPoint[0];
        result.closestPoint[1] = ltResult.closestPoint[1];
    }
    else
    {
        DCPQuery<Real, Vector3<Real>, Triangle3<Real>> ptQuery;
        auto ptResult = ptQuery(ray.origin, triangle);
        result.distance = ptResult.distance;
        result.sqrDistance = ptResult.sqrDistance;
        result.rayParameter = (Real)0;
        result.triangleParameter[0] = ptResult.parameter[0];
        result.triangleParameter[1] = ptResult.parameter[1];
        result.triangleParameter[2] = ptResult.parameter[2];
        result.closestPoint[0] = ray.origin;
        result.closestPoint[1] = ptResult.closest;
    }
    return result;
}


}
