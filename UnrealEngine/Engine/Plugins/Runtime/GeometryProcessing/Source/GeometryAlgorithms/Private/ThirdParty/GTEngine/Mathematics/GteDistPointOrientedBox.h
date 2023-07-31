// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistPointAlignedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, OrientedBox<N, Real>>
    :
    public DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>
{
public:
    struct Result
        :
        public DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Vector<N, Real> const& point,
        OrientedBox<N, Real> const& box);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointOrientedBox =
DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>;

template <typename Real>
using DCPPoint2OrientedBox2 = DCPPointOrientedBox<2, Real>;

template <typename Real>
using DCPPoint3OrientedBox3 = DCPPointOrientedBox<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, OrientedBox<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, OrientedBox<N, Real>>::operator()(
    Vector<N, Real> const& point, OrientedBox<N, Real> const& box)
{
    // Translate the point to the coordinate system of the box.  In this
    // system, the box is axis-aligned with center at the origin.
    Vector<N, Real> diff = point - box.center;
    Vector<N, Real> closest;
    for (int i = 0; i < N; ++i)
    {
        closest[i] = Dot(diff, box.axis[i]);
    }

    Result result;
    this->DoQuery(closest, box.extent, result);

    // Compute the closest point on the box.
    result.boxClosest = box.center;
    for (int i = 0; i < N; ++i)
    {
        result.boxClosest += closest[i] * box.axis[i];
    }
    return result;
}


}
