// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>

// The halfspace is represented as Dot(N,X) >= c where N is a unit-length
// normal vector, c is the plane constant, and X is any point in space.
// The user must ensure that the normal vector is unit length.

namespace gte
{

template <int N, typename Real>
class Halfspace
{
public:
    // Construction and destruction.  The default constructor sets the normal
    // to (0,...,0,1) and the constant to zero (halfspace x[N-1] >= 0).
    Halfspace();

    // Specify N and c directly.
    Halfspace(Vector<N, Real> const& inNormal, Real inConstant);

    // Public member access.
    Vector<N, Real> normal;
    Real constant;

public:
    // Comparisons to support sorted containers.
    bool operator==(Halfspace const& halfspace) const;
    bool operator!=(Halfspace const& halfspace) const;
    bool operator< (Halfspace const& halfspace) const;
    bool operator<=(Halfspace const& halfspace) const;
    bool operator> (Halfspace const& halfspace) const;
    bool operator>=(Halfspace const& halfspace) const;
};

// Template alias for convenience.
template <typename Real>
using Halfspace3 = Halfspace<3, Real>;


template <int N, typename Real>
Halfspace<N, Real>::Halfspace()
    :
    constant((Real)0)
{
    normal.MakeUnit(N - 1);
}

template <int N, typename Real>
Halfspace<N, Real>::Halfspace(Vector<N, Real> const& inNormal,
    Real inConstant)
    :
    normal(inNormal),
    constant(inConstant)
{
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator==(Halfspace const& halfspace) const
{
    return normal == halfspace.normal && constant == halfspace.constant;
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator!=(Halfspace const& halfspace) const
{
    return !operator==(halfspace);
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator<(Halfspace const& halfspace) const
{
    if (normal < halfspace.normal)
    {
        return true;
    }

    if (normal > halfspace.normal)
    {
        return false;
    }

    return constant < halfspace.constant;
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator<=(Halfspace const& halfspace) const
{
    return operator<(halfspace) || operator==(halfspace);
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator>(Halfspace const& halfspace) const
{
    return !operator<=(halfspace);
}

template <int N, typename Real>
bool Halfspace<N, Real>::operator>=(Halfspace const& halfspace) const
{
    return !operator<(halfspace);
}


}
