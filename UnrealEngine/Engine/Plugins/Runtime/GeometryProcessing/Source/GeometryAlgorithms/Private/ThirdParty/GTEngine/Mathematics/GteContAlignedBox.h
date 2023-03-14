// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.13.0 (2018/04/15)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteAlignedBox.h>

namespace gte
{

// Compute the minimum size aligned bounding box of the points.  The extreme
// values are the minima and maxima of the point coordinates.
template <int N, typename Real>
bool GetContainer(int numPoints, Vector<N, Real> const* points,
     AlignedBox<N, Real>& box);

// Test for containment.
template <int N, typename Real>
bool InContainer(Vector<N, Real> const& point, AlignedBox<N, Real> const& box);

// Construct an aligned box that contains two other aligned boxes.  The
// result is the minimum size box containing the input boxes.
template <int N, typename Real>
bool MergeContainers(AlignedBox<N, Real> const& box0,
    AlignedBox<N,Real> const& box1, AlignedBox<N, Real>& merge);


template <int N, typename Real>
bool GetContainer(int numPoints, Vector<N, Real> const* points,
    AlignedBox<N, Real>& box)
{
    return ComputeExtremes(numPoints, points, box.min, box.max);
}

template <int N, typename Real>
bool InContainer(Vector<N, Real> const& point, AlignedBox<N, Real> const& box)
{
    for (int i = 0; i < N; ++i)
    {
        Real value = point[i];
        if (value < box.min[i] || value > box.max[i])
        {
            return false;
        }
    }
    return true;
}

template <int N, typename Real>
bool MergeContainers(AlignedBox<N, Real> const& box0,
    AlignedBox<N, Real> const& box1, AlignedBox<N, Real>& merge)
{
    for (int i = 0; i < N; ++i)
    {
        merge.min[i] = std::min(box0.min[i], box1.min[i]);
        merge.max[i] = std::max(box0.max[i], box1.max[i]);
    }
    return true;
}

}
