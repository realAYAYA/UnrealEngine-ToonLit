// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.4.0 (2016/11/08)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteIntrAlignedBox3Cylinder3.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>

// The query considers the cylinder and box to be solids.

namespace gte
{

template <typename Real>
class TIQuery<Real, OrientedBox3<Real>, Cylinder3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(OrientedBox3<Real> const& box, Cylinder3<Real> const& cylinder);
};


template <typename Real>
typename TIQuery<Real, OrientedBox3<Real>, Cylinder3<Real>>::Result
TIQuery<Real, OrientedBox3<Real>, Cylinder3<Real>>::operator()(
    OrientedBox3<Real> const& box, Cylinder3<Real> const& cylinder)
{
    // Transform the box and cylinder so that the box is axis-aligned.
    AlignedBox3<Real> aabb(-box.extent, box.extent);
    Vector3<Real> diff = cylinder.axis.origin - box.center;
    Cylinder3<Real> transformedCylinder;
    transformedCylinder.radius = cylinder.radius;
    transformedCylinder.height = cylinder.height;
    for (int i = 0; i < 3; ++i)
    {
        transformedCylinder.axis.origin[i] = Dot(box.axis[i], diff);
        transformedCylinder.axis.direction[i] = Dot(box.axis[i], cylinder.axis.direction);
    }

    TIQuery<Real, AlignedBox3<Real>, Cylinder3<Real>> aabbCylinderQuery;
    auto aabbCylinderResult = aabbCylinderQuery(aabb, transformedCylinder);
    Result result;
    result.intersect = aabbCylinderResult.intersect;
    return result;
}

}
