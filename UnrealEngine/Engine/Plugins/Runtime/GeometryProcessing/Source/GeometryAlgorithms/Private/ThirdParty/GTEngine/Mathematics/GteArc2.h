// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>

namespace gte
{

// The circle containing the arc is represented as |X-C| = R where C is the
// center and R is the radius.  The arc is defined by two points end0 and end1
// on the circle so that end1 is obtained from end0 by traversing
// counterclockwise.  The application is responsible for ensuring that end0
// and end1 are on the circle and that they are properly ordered.

template <typename Real>
class Arc2
{
public:
    // Construction and destruction.  The default constructor sets the center
    // to (0,0), radius to 1, end0 to (1,0), and end1 to (0,1).
    Arc2();
    Arc2(Vector2<Real> const& inCenter, Real inRadius,
        Vector2<Real>const& inEnd0, Vector2<Real>const & inEnd1);

    // Test whether P is on the arc.  The application must ensure that P is on
    // the circle; that is, |P-C| = R.  This test works for any angle between
    // B-C and A-C, not just those between 0 and pi radians.
    bool Contains(Vector2<Real> const& p) const;

    Vector2<Real> center;
    Real radius;
    Vector2<Real> end[2];

public:
    // Comparisons to support sorted containers.
    bool operator==(Arc2 const& arc) const;
    bool operator!=(Arc2 const& arc) const;
    bool operator< (Arc2 const& arc) const;
    bool operator<=(Arc2 const& arc) const;
    bool operator> (Arc2 const& arc) const;
    bool operator>=(Arc2 const& arc) const;
};


template <typename Real>
Arc2<Real>::Arc2()
    :
    center(Vector2<Real>::Zero()),
    radius((Real)1)
{
    end[0] = Vector2<Real>::Unit(0);
    end[1] = Vector2<Real>::Unit(1);
}

template <typename Real>
Arc2<Real>::Arc2(Vector2<Real> const& inCenter, Real inRadius,
    Vector2<Real>const& inEnd0, Vector2<Real>const & inEnd1)
    :
    center(inCenter),
    radius(inRadius)
{
    end[0] = inEnd0;
    end[1] = inEnd1;
}

template <typename Real>
bool Arc2<Real>::Contains(Vector2<Real> const& p) const
{
    // Assert: |P-C| = R where P is the input point, C is the circle center,
    // and R is the circle radius.  For P to be on the arc from A to B, it
    // must be on the side of the plane containing A with normal N = Perp(B-A)
    // where Perp(u,v) = (v,-u).

    Vector2<Real> diffPE0 = p - end[0];
    Vector2<Real> diffE1E0 = end[1] - end[0];
    Real dotPerp = DotPerp(diffPE0, diffE1E0);
    return dotPerp >= (Real)0;
}

template <typename Real>
bool Arc2<Real>::operator==(Arc2 const& arc) const
{
    return center == arc.center && radius == arc.radius
        && end[0] == arc.end[0] && end[1] == arc.end[1];
}

template <typename Real>
bool Arc2<Real>::operator!=(Arc2 const& arc) const
{
    return !operator==(arc);
}

template <typename Real>
bool Arc2<Real>::operator<(Arc2 const& arc) const
{
    if (center < arc.center)
    {
        return true;
    }

    if (center > arc.center)
    {
        return false;
    }

    if (radius < arc.radius)
    {
        return true;
    }

    if (radius > arc.radius)
    {
        return false;
    }

    if (end[0] < arc.end[0])
    {
        return true;
    }

    if (end[0] > arc.end[0])
    {
        return false;
    }

    return end[1] < arc.end[1];
}

template <typename Real>
bool Arc2<Real>::operator<=(Arc2 const& arc) const
{
    return operator<(arc) || operator==(arc);
}

template <typename Real>
bool Arc2<Real>::operator>(Arc2 const& arc) const
{
    return !operator<=(arc);
}

template <typename Real>
bool Arc2<Real>::operator>=(Arc2 const& arc) const
{
    return !operator<(arc);
}


}
