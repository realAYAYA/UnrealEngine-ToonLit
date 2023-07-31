// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>

namespace gte
{

// Axis-angle representation for N = 3 or N = 4.  When N = 4, the axis
// must be a vector of the form (x,y,z,0) [affine representation of the
// 3-tuple direction].

template <int N, typename Real>
class AxisAngle
{
public:
    AxisAngle();
    AxisAngle(Vector<N,Real> const& inAxis, Real inAngle);

    Vector<N,Real> axis;
    Real angle;
};


template <int N, typename Real>
AxisAngle<N, Real>::AxisAngle()
{
    static_assert(N == 3 || N == 4, "Dimension must be 3 or 4.");
    // Uninitialized.
}

template <int N, typename Real>
AxisAngle<N, Real>::AxisAngle(Vector<N, Real> const& inAxis, Real inAngle)
    :
    axis(inAxis),
    angle(inAngle)
{
    static_assert(N == 3 || N == 4, "Dimension must be 3 or 4.");
}


}
