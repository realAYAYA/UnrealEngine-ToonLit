// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteAlignedBox.h>

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Vector<N, Real> boxClosest;
    };

    Result operator()(Vector<N, Real> const& point,
        AlignedBox<N, Real> const& box);

protected:
    // On input, 'point' is the difference of the query point and the box
    // center.  On output, 'point' is the point on the box closest to the
    // query point.
    void DoQuery(Vector<N, Real>& point, Vector<N, Real> const& boxExtent,
        Result& result);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointAlignedBox =
DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>;

template <typename Real>
using DCPPoint2AlignedBox2 = DCPPointAlignedBox<2, Real>;

template <typename Real>
using DCPPoint3AlignedBox3 = DCPPointAlignedBox<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>::operator()(
    Vector<N, Real> const& point, AlignedBox<N, Real> const& box)
{
    // Translate the point and box so that the box has center at the origin.
    Vector<N, Real> boxCenter, boxExtent;
    box.GetCenteredForm(boxCenter, boxExtent);
    Vector<N, Real> closest = point - boxCenter;

    Result result;
    DoQuery(closest, boxExtent, result);

    // Compute the closest point on the box.
    result.boxClosest = boxCenter + closest;
    return result;
}

template <int N, typename Real>
void DCPQuery<Real, Vector<N, Real>, AlignedBox<N, Real>>::DoQuery(
    Vector<N, Real>& point, Vector<N, Real> const& boxExtent, Result& result)
{
    result.sqrDistance = (Real)0;
    for (int i = 0; i < N; ++i)
    {
        if (point[i] < -boxExtent[i])
        {
            Real delta = point[i] + boxExtent[i];
            result.sqrDistance += delta * delta;
            point[i] = -boxExtent[i];
        }
        else if (point[i] > boxExtent[i])
        {
            Real delta = point[i] - boxExtent[i];
            result.sqrDistance += delta * delta;
            point[i] = boxExtent[i];
        }
    }
    result.distance = std::sqrt(result.sqrDistance);
}


}
