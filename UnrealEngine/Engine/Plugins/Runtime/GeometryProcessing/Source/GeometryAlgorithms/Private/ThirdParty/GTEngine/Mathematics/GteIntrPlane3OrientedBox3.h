// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistPoint3Plane3.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

namespace gte
{

template <typename Real>
class TIQuery<Real, Plane3<Real>, OrientedBox3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Plane3<Real> const& plane,
        OrientedBox3<Real> const& box);
};


template <typename Real>
typename TIQuery<Real, Plane3<Real>, OrientedBox3<Real>>::Result
TIQuery<Real, Plane3<Real>, OrientedBox3<Real>>::operator()(
    Plane3<Real> const& plane, OrientedBox3<Real> const& box)
{
    Result result;

    Real radius =
        std::abs(box.extent[0] * Dot(plane.normal, box.axis[0])) +
        std::abs(box.extent[1] * Dot(plane.normal, box.axis[1])) +
        std::abs(box.extent[2] * Dot(plane.normal, box.axis[2]));

    DCPQuery<Real, Vector3<Real>, Plane3<Real>> ppQuery;
    auto ppResult = ppQuery(box.center, plane);
    result.intersect = (ppResult.distance <= radius);
    return result;
}


}
