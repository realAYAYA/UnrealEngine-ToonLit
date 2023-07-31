// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteRectangle.h>

namespace gte
{

template <typename Real>
class DCPQuery<Real, Vector3<Real>, Rectangle3<Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real rectangleParameter[2];
        Vector3<Real> rectangleClosestPoint;
    };

    Result operator()(Vector3<Real> const& point,
        Rectangle3<Real> const& rectangle);
};


template <typename Real>
typename DCPQuery<Real, Vector3<Real>, Rectangle3<Real>>::Result
DCPQuery<Real, Vector3<Real>, Rectangle3<Real>>::operator()(
    Vector3<Real> const& point, Rectangle3<Real> const& rectangle)
{
    Result result;

    Vector3<Real> diff = rectangle.center - point;
    Real b0 = Dot(diff, rectangle.axis[0]);
    Real b1 = Dot(diff, rectangle.axis[1]);
    Real s0 = -b0, s1 = -b1;
    result.sqrDistance = Dot(diff, diff);

    if (s0 < -rectangle.extent[0])
    {
        s0 = -rectangle.extent[0];
    }
    else if (s0 > rectangle.extent[0])
    {
        s0 = rectangle.extent[0];
    }
    result.sqrDistance += s0*(s0 + ((Real)2)*b0);

    if (s1 < -rectangle.extent[1])
    {
        s1 = -rectangle.extent[1];
    }
    else if (s1 > rectangle.extent[1])
    {
        s1 = rectangle.extent[1];
    }
    result.sqrDistance += s1*(s1 + ((Real)2)*b1);

    // Account for numerical round-off error.
    if (result.sqrDistance < (Real)0)
    {
        result.sqrDistance = (Real)0;
    }

    result.distance = std::sqrt(result.sqrDistance);
    result.rectangleParameter[0] = s0;
    result.rectangleParameter[1] = s1;
    result.rectangleClosestPoint = rectangle.center;
    for (int i = 0; i < 2; ++i)
    {
        result.rectangleClosestPoint +=
            result.rectangleParameter[i] * rectangle.axis[i];
    }
    return result;
}


}
