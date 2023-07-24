// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>

// A box has center C, axis directions U[i], and extents e[i].  The set
// {U[0],...,U[N-1]} is orthonormal, which means the vectors are
// unit-length and mutually perpendicular.  The extents are nonnegative;
// zero is allowed, meaning the box is degenerate in the corresponding
// direction.  A point X is represented in box coordinates by
// X = C + y[0]*U[0] + y[1]*U[1].  This point is inside or on the
// box whenever |y[i]| <= e[i] for all i.

namespace gte
{

template <int N, typename Real>
class OrientedBox
{
public:
    // Construction and destruction.  The default constructor sets the center
    // to (0,...,0), axis d to Vector<N,Real>::Unit(d), and extent d to +1.
    OrientedBox();
    OrientedBox(Vector<N, Real> const& inCenter,
        std::array<Vector<N, Real>, N> const& inAxis,
        Vector<N, Real> const& inExtent);

    // Compute the vertices of the box.  If index i has the bit pattern
    // i = b[N-1]...b[0], then
    //    vertex[i] = center + sum_{d=0}^{N-1} sign[d] * extent[d] * axis[d]
    // where sign[d] = 2*b[d] - 1.
    void GetVertices(std::array<Vector<N, Real>, (1 << N)>& vertex) const;

    // Public member access.  It is required that extent[i] >= 0.
    Vector<N, Real> center;
    std::array<Vector<N, Real>, N> axis;
    Vector<N, Real> extent;

public:
    // Comparisons to support sorted containers.
    bool operator==(OrientedBox const& box) const;
    bool operator!=(OrientedBox const& box) const;
    bool operator< (OrientedBox const& box) const;
    bool operator<=(OrientedBox const& box) const;
    bool operator> (OrientedBox const& box) const;
    bool operator>=(OrientedBox const& box) const;
};

// Template aliases for convenience.
template <typename Real>
using OrientedBox2 = OrientedBox<2, Real>;

template <typename Real>
using OrientedBox3 = OrientedBox<3, Real>;


template <int N, typename Real>
OrientedBox<N, Real>::OrientedBox()
{
    center.MakeZero();
    for (int i = 0; i < N; ++i)
    {
        axis[i].MakeUnit(i);
        extent[i] = (Real)1;
    }
}

template <int N, typename Real>
OrientedBox<N, Real>::OrientedBox(Vector<N, Real> const& inCenter,
    std::array<Vector<N, Real>, N> const& inAxis,
    Vector<N, Real> const& inExtent)
    :
    center(inCenter),
    axis(inAxis),
    extent(inExtent)
{
}

template <int N, typename Real>
void OrientedBox<N, Real>::GetVertices(
    std::array<Vector<N, Real>, (1 << N)>& vertex) const
{
    unsigned int const dsup = static_cast<unsigned int>(N);
    std::array<Vector<N, Real>, N> product;
    for (unsigned int d = 0; d < dsup; ++d)
    {
        product[d] = extent[d] * axis[d];
    }

    int const imax = (1 << N);
    for (int i = 0; i < imax; ++i)
    {
        vertex[i] = center;
        for (unsigned int d = 0, mask = 1; d < dsup; ++d, mask <<= 1)
        {
            Real sign = (i & mask ? (Real)1 : (Real)-1);
            vertex[i] += sign * product[d];
        }
    }
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator==(OrientedBox const& box) const
{
    return center == box.center && axis == box.axis && extent == box.extent;
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator!=(OrientedBox const& box) const
{
    return !operator==(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator<(OrientedBox const& box) const
{
    if (center < box.center)
    {
        return true;
    }

    if (center > box.center)
    {
        return false;
    }

    if (axis < box.axis)
    {
        return true;
    }

    if (axis > box.axis)
    {
        return false;
    }

    return extent < box.extent;
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator<=(OrientedBox const& box) const
{
    return operator<(box) || operator==(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator>(OrientedBox const& box) const
{
    return !operator<=(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator>=(OrientedBox const& box) const
{
    return !operator<(box);
}


}
