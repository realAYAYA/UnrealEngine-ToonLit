// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>

// The hypersphere is represented as |X-C| = R where C is the center and R is
// the radius.  The hypersphere is a circle for dimension 2 or a sphere for
// dimension 3.

namespace gte
{

template <int N, typename Real>
class Hypersphere
{
public:
    // Construction and destruction.  The default constructor sets the center
    // to (0,...,0) and the radius to 1.
    Hypersphere();
    Hypersphere(Vector<N, Real> const& inCenter, Real inRadius);

    // Public member access.
    Vector<N, Real> center;
    Real radius;

public:
    // Comparisons to support sorted containers.
    bool operator==(Hypersphere const& hypersphere) const;
    bool operator!=(Hypersphere const& hypersphere) const;
    bool operator< (Hypersphere const& hypersphere) const;
    bool operator<=(Hypersphere const& hypersphere) const;
    bool operator> (Hypersphere const& hypersphere) const;
    bool operator>=(Hypersphere const& hypersphere) const;
};

// Template aliases for convenience.
template <typename Real>
using Circle2 = Hypersphere<2, Real>;

template <typename Real>
using Sphere3 = Hypersphere<3, Real>;


template <int N, typename Real>
Hypersphere<N, Real>::Hypersphere()
    :
    radius((Real)1)
{
    center.MakeZero();
}

template <int N, typename Real>
Hypersphere<N, Real>::Hypersphere(Vector<N, Real> const& inCenter,
    Real inRadius)
    :
    center(inCenter),
    radius(inRadius)
{
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator==(Hypersphere const& hypersphere) const
{
    return center == hypersphere.center && radius == hypersphere.radius;
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator!=(Hypersphere const& hypersphere) const
{
    return !operator==(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator<(Hypersphere const& hypersphere) const
{
    if (center < hypersphere.center)
    {
        return true;
    }

    if (center > hypersphere.center)
    {
        return false;
    }

    return radius < hypersphere.radius;
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator<=(Hypersphere const& hypersphere) const
{
    return operator<(hypersphere) || operator==(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator>(Hypersphere const& hypersphere) const
{
    return !operator<=(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator>=(Hypersphere const& hypersphere) const
{
    return !operator<(hypersphere);
}


}
