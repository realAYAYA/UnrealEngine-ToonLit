// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMath.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>

namespace gte
{

// A solid sector is the intersection of a disk and a 2D cone.  The disk
// has center C, radius R, and contains points X for which |X-C| <= R.  The
// 2D cone has vertex C, unit-length axis direction D, angle A in (0,pi)
// measured from D, and contains points X for which Dot(D,(X-C)/|X-C|) >= cos(A).
// Sector points X satisfy both inequality constraints.

template <typename Real>
class Sector2
{
public:
    // Construction and destruction.  The default constructor sets the vertex
    // to (0,0), radius to 1, axis direction to (1,0), and angle to pi,
    // all of which define a disk.
    Sector2();
    Sector2(Vector2<Real> const& inVertex, Real inRadius,
        Vector2<Real> const& inDirection, Real inAngle);

    // Set the angle and cos(angle) simultaneously.
    void SetAngle(Real inAngle);

    // Test whether P is in the sector. 
    bool Contains(Vector2<Real> const& p) const;

    // The cosine and sine of the angle are used in queries, so all o
    // angle, cos(angle), and sin(angle) are stored. If you set 'angle'
    // via the public members, you must set all to be consistent.  You
    // can also call SetAngle(...) to ensure consistency.
    Vector2<Real> vertex;
    Real radius;
    Vector2<Real> direction;
    Real angle, cosAngle, sinAngle;

public:
    // Comparisons to support sorted containers.
    bool operator==(Sector2 const& sector) const;
    bool operator!=(Sector2 const& sector) const;
    bool operator< (Sector2 const& sector) const;
    bool operator<=(Sector2 const& sector) const;
    bool operator> (Sector2 const& sector) const;
    bool operator>=(Sector2 const& sector) const;
};


template <typename Real>
Sector2<Real>::Sector2()
    :
    vertex(Vector2<Real>::Zero()),
    radius((Real)1),
    direction(Vector2<Real>::Unit(0)),
    angle((Real)GTE_C_PI),
    cosAngle((Real)-1),
    sinAngle((Real)0)
{
}

template <typename Real>
Sector2<Real>::Sector2(Vector2<Real> const& inCenter, Real inRadius,
    Vector2<Real> const& inDirection, Real inAngle)
    :
    vertex(inCenter),
    radius(inRadius),
    direction(inDirection)
{
    SetAngle(inAngle);
}

template <typename Real>
void Sector2<Real>::SetAngle(Real inAngle)
{
    angle = inAngle;
    cosAngle = std::cos(angle);
    sinAngle = std::sin(angle);
}

template <typename Real>
bool Sector2<Real>::Contains(Vector2<Real> const& p) const
{
    Vector2<Real> diff = p - vertex;
    Real length = Length(diff);
    return length <= radius && Dot(direction, diff) >= length * cosAngle;
}

template <typename Real>
bool Sector2<Real>::operator==(Sector2 const& sector) const
{
    return vertex == sector.vertex && radius == sector.radius
        && direction == sector.direction && angle == sector.angle;
}

template <typename Real>
bool Sector2<Real>::operator!=(Sector2 const& sector) const
{
    return !operator==(sector);
}

template <typename Real>
bool Sector2<Real>::operator<(Sector2 const& sector) const
{
    if (vertex < sector.vertex)
    {
        return true;
    }

    if (vertex > sector.vertex)
    {
        return false;
    }

    if (radius < sector.radius)
    {
        return true;
    }

    if (radius > sector.radius)
    {
        return false;
    }

    if (direction < sector.direction)
    {
        return true;
    }

    if (direction > sector.direction)
    {
        return false;
    }

    return angle < sector.angle;
}

template <typename Real>
bool Sector2<Real>::operator<=(Sector2 const& sector) const
{
    return operator<(sector) || operator==(sector);
}

template <typename Real>
bool Sector2<Real>::operator>(Sector2 const& sector) const
{
    return !operator<=(sector);
}

template <typename Real>
bool Sector2<Real>::operator>=(Sector2 const& sector) const
{
    return !operator<(sector);
}

}
