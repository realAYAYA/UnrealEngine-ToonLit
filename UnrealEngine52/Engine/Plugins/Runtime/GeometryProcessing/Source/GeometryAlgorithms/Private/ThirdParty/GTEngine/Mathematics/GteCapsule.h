// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteSegment.h>

namespace gte
{

// A capsule is the set of points that are equidistant from a segment, the
// common distance called the radius.

template <int N, typename Real>
class Capsule
{
public:
    // Construction and destruction.  The default constructor sets the segment
    // to have endpoints p0 = (-1,0,...,0) and p1 = (1,0,...,0), and the
    // radius is 1.
    Capsule();
    Capsule(Segment<N, Real> const& inSegment, Real inRadius);

    // Public member access.
    Segment<N, Real> segment;
    Real radius;

public:
    // Comparisons to support sorted containers.
    bool operator==(Capsule const& capsule) const;
    bool operator!=(Capsule const& capsule) const;
    bool operator< (Capsule const& capsule) const;
    bool operator<=(Capsule const& capsule) const;
    bool operator> (Capsule const& capsule) const;
    bool operator>=(Capsule const& capsule) const;
};

// Template alias for convenience.
template <typename Real>
using Capsule3 = Capsule<3, Real>;


template <int N, typename Real>
Capsule<N, Real>::Capsule()
    :
    radius((Real)1)
{
}

template <int N, typename Real>
Capsule<N, Real>::Capsule(Segment<N, Real> const& inSegment, Real inRadius)
    :
    segment(inSegment),
    radius(inRadius)
{
}

template <int N, typename Real>
bool Capsule<N, Real>::operator==(Capsule const& capsule) const
{
    return segment == capsule.segment && radius == capsule.radius;
}

template <int N, typename Real>
bool Capsule<N, Real>::operator!=(Capsule const& capsule) const
{
    return !operator==(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator<(Capsule const& capsule) const
{
    if (segment < capsule.segment)
    {
        return true;
    }

    if (segment > capsule.segment)
    {
        return false;
    }

    return radius < capsule.radius;
}

template <int N, typename Real>
bool Capsule<N, Real>::operator<=(Capsule const& capsule) const
{
    return operator<(capsule) || operator==(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator>(Capsule const& capsule) const
{
    return !operator<=(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator>=(Capsule const& capsule) const
{
    return !operator<(capsule);
}


}
