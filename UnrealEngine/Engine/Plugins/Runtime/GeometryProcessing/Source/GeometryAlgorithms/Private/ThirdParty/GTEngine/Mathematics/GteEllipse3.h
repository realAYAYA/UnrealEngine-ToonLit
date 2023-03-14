// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>

// The plane containing ellipse is Dot(N,X-C) = 0 where X is any point in the
// plane, C is the ellipse center, and N is a unit-length normal to the plane.
// Vectors A0, A1, and N form an orthonormal right-handed set.  The ellipse in
// the plane is parameterized by X = C + e0*cos(t)*A0 + e1*sin(t)*A1, where A0
// is the major axis, A1 is the minor axis, and e0 and e1 are the extents
// along those axes.  The angle t is in [-pi,pi) and e0 >= e1 > 0.

namespace gte
{

template <typename Real>
class Ellipse3
{
public:
    // Construction and destruction.  The default constructor sets center to
    // (0,0,0), A0 to (1,0,0), A1 to (0,1,0), normal to (0,0,1), e0 to 1, and
    // e1 to 1.
    Ellipse3(); 
    Ellipse3(Vector3<Real> const& inCenter, Vector3<Real> const& inNormal,
        Vector3<Real> const inAxis[2], Vector2<Real> const& inExtent);

    // Public member access.
    Vector3<Real> center, normal;
    Vector3<Real> axis[2];
    Vector2<Real> extent;

public:
    // Comparisons to support sorted containers.
    bool operator==(Ellipse3 const& ellipse) const;
    bool operator!=(Ellipse3 const& ellipse) const;
    bool operator< (Ellipse3 const& ellipse) const;
    bool operator<=(Ellipse3 const& ellipse) const;
    bool operator> (Ellipse3 const& ellipse) const;
    bool operator>=(Ellipse3 const& ellipse) const;
};


template <typename Real>
Ellipse3<Real>::Ellipse3()
    :
    center(Vector3<Real>::Zero()),
    normal(Vector3<Real>::Unit(2)),
    extent({ (Real)1, (Real)1 })
{
    axis[0] = Vector3<Real>::Unit(0);
    axis[1] = Vector3<Real>::Unit(1);
}

template <typename Real>
Ellipse3<Real>::Ellipse3(Vector3<Real> const& inCenter,
    Vector3<Real> const& inNormal, Vector3<Real> const inAxis[2],
    Vector2<Real> const& inExtent)
    :
    center(inCenter),
    normal(inNormal),
    extent(inExtent)
{
    for (int i = 0; i < 2; ++i)
    {
        axis[i] = inAxis[i];
    }
}

template <typename Real>
bool Ellipse3<Real>::operator==(Ellipse3 const& ellipse) const
{
    return center == ellipse.center
        && normal == ellipse.normal
        && axis[0] == ellipse.axis[0]
        && axis[1] == ellipse.axis[1]
        && extent == ellipse.extent;
}

template <typename Real>
bool Ellipse3<Real>::operator!=(Ellipse3 const& ellipse) const
{
    return !operator==(ellipse);
}

template <typename Real>
bool Ellipse3<Real>::operator<(Ellipse3 const& ellipse) const
{
    if (center < ellipse.center)
    {
        return true;
    }

    if (center > ellipse.center)
    {
        return false;
    }

    if (normal < ellipse.normal)
    {
        return true;
    }

    if (normal > ellipse.normal)
    {
        return false;
    }

    if (axis[0] < ellipse.axis[0])
    {
        return true;
    }

    if (axis[0] > ellipse.axis[0])
    {
        return false;
    }

    if (axis[1] < ellipse.axis[1])
    {
        return true;
    }

    if (axis[1] > ellipse.axis[1])
    {
        return false;
    }

    return extent < ellipse.extent;
}

template <typename Real>
bool Ellipse3<Real>::operator<=(Ellipse3 const& ellipse) const
{
    return operator<(ellipse) || operator==(ellipse);
}

template <typename Real>
bool Ellipse3<Real>::operator>(Ellipse3 const& ellipse) const
{
    return !operator<=(ellipse);
}

template <typename Real>
bool Ellipse3<Real>::operator>=(Ellipse3 const& ellipse) const
{
    return !operator<(ellipse);
}


}
