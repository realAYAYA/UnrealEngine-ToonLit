// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>

// The circle is the intersection of the sphere |X-C|^2 = r^2 and the
// plane Dot(N,X-C) = 0, where C is the circle center, r is the radius,
// and N is a unit-length plane normal.

namespace gte
{

template <typename Real>
class Circle3
{
public:

    // Construction and destruction.  The default constructor sets center to
    // (0,0,0), normal to (0,0,1), and radius to 1.
    Circle3();
    Circle3(Vector3<Real> const& inCenter, Vector3<Real> const& inNormal,
        Real inRadius);

    // Public member access.
    Vector3<Real> center, normal;
    Real radius;

public:
    // Comparisons to support sorted containers.
    bool operator==(Circle3 const& circle) const;
    bool operator!=(Circle3 const& circle) const;
    bool operator< (Circle3 const& circle) const;
    bool operator<=(Circle3 const& circle) const;
    bool operator> (Circle3 const& circle) const;
    bool operator>=(Circle3 const& circle) const;
};


template <typename Real>
Circle3<Real>::Circle3()
    :
    center(Vector3<Real>::Zero()),
    normal(Vector3<Real>::Unit(2)),
    radius((Real)1)
{
}

template <typename Real>
Circle3<Real>::Circle3(Vector3<Real> const& inCenter,
    Vector3<Real> const& inNormal, Real inRadius)
    :
    center(inCenter),
    normal(inNormal),
    radius(inRadius)
{
}

template <typename Real>
bool Circle3<Real>::operator==(Circle3 const& circle) const
{
    return center == circle.center
        && normal == circle.normal
        && radius == circle.radius;
}

template <typename Real>
bool Circle3<Real>::operator!=(Circle3 const& circle) const
{
    return !operator==(circle);
}

template <typename Real>
bool Circle3<Real>::operator<(Circle3 const& circle) const
{
    if (center < circle.center)
    {
        return true;
    }

    if (center > circle.center)
    {
        return false;
    }

    if (normal < circle.normal)
    {
        return true;
    }

    if (normal > circle.normal)
    {
        return false;
    }

    return radius < circle.radius;
}

template <typename Real>
bool Circle3<Real>::operator<=(Circle3 const& circle) const
{
    return operator<(circle) || operator==(circle);
}

template <typename Real>
bool Circle3<Real>::operator>(Circle3 const& circle) const
{
    return !operator<=(circle);
}

template <typename Real>
bool Circle3<Real>::operator>=(Circle3 const& circle) const
{
    return !operator<(circle);
}


}
