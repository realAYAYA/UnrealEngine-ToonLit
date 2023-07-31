// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteLine.h>

// The cylinder axis is a line.  The origin of the cylinder is chosen to be
// the line origin.  The cylinder wall is at a distance R units from the axis.
// An infinite cylinder has infinite height.  A finite cylinder has center C
// at the line origin and has a finite height H.  The segment for the finite
// cylinder has endpoints C-(H/2)*D and C+(H/2)*D where D is a unit-length
// direction of the line.

namespace gte
{

template <typename Real>
class Cylinder3
{
public:
    // Construction and destruction.  The default constructor sets axis to
    // (0,0,1), radius to 1, and height to 1.
    Cylinder3();
    Cylinder3(Line3<Real> const& inAxis, Real inRadius, Real inHeight);

    Line3<Real> axis;
    Real radius, height;

public:
    // Comparisons to support sorted containers.
    bool operator==(Cylinder3 const& cylinder) const;
    bool operator!=(Cylinder3 const& cylinder) const;
    bool operator< (Cylinder3 const& cylinder) const;
    bool operator<=(Cylinder3 const& cylinder) const;
    bool operator> (Cylinder3 const& cylinder) const;
    bool operator>=(Cylinder3 const& cylinder) const;
};


template <typename Real>
Cylinder3<Real>::Cylinder3()
    :
    axis(Line3<Real>()),
    radius((Real)1),
    height((Real)1)
{
}

template <typename Real>
Cylinder3<Real>::Cylinder3(Line3<Real> const& inAxis, Real inRadius,
    Real inHeight)
    :
    axis(inAxis),
    radius(inRadius),
    height(inHeight)
{
}

template <typename Real>
bool Cylinder3<Real>::operator==(Cylinder3 const& cylinder) const
{
    return axis == cylinder.axis
        && radius == cylinder.radius
        && height == cylinder.height;
}

template <typename Real>
bool Cylinder3<Real>::operator!=(Cylinder3 const& cylinder) const
{
    return !operator==(cylinder);
}

template <typename Real>
bool Cylinder3<Real>::operator<(Cylinder3 const& cylinder) const
{
    if (axis < cylinder.axis)
    {
        return true;
    }

    if (axis > cylinder.axis)
    {
        return false;
    }

    if (radius < cylinder.radius)
    {
        return true;
    }

    if (radius > cylinder.radius)
    {
        return false;
    }

    return height < cylinder.height;
}

template <typename Real>
bool Cylinder3<Real>::operator<=(Cylinder3 const& cylinder) const
{
    return operator<(cylinder) || operator==(cylinder);
}

template <typename Real>
bool Cylinder3<Real>::operator>(Cylinder3 const& cylinder) const
{
    return !operator<=(cylinder);
}

template <typename Real>
bool Cylinder3<Real>::operator>=(Cylinder3 const& cylinder) const
{
    return !operator<(cylinder);
}


}
