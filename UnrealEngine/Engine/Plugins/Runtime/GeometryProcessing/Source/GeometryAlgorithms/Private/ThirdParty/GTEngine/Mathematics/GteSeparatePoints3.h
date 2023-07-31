// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteConvexHull3.h>

// Separate two point sets, if possible, by computing a plane for which the
// point sets lie on opposite sides.  The algorithm computes the convex hull
// of the point sets, then uses the method of separating axes to determine if
// the two convex polyhedra are disjoint.  The ComputeType is for the
// ConvexHull3 class.

namespace gte
{

template <typename Real, typename ComputeType>
class SeparatePoints3
{
public:
    // The return value is 'true' if and only if there is a separation.  If
    // 'true', the returned plane is a separating plane.  The code assumes
    // that each point set has at least 4 noncoplanar points.
    bool operator()(int numPoints0, Vector3<Real> const* points0,
        int numPoints1, Vector3<Real> const* points1,
        Plane3<Real>& separatingPlane) const;

private:
    int OnSameSide(Plane3<Real> const& plane, int numTriangles,
        int const* indices, Vector3<Real> const* points) const;

    int WhichSide(Plane3<Real> const& plane, int numTriangles,
        int const* indices, Vector3<Real> const* points) const;
};


template <typename Real, typename ComputeType>
bool SeparatePoints3<Real, ComputeType>::operator()(int numPoints0,
    Vector3<Real> const* points0, int numPoints1,
    Vector3<Real> const* points1, Plane3<Real>& separatingPlane) const
{
    // Construct convex hull of point set 0.
    ConvexHull3<Real, ComputeType> ch0;
    ch0(numPoints0, points0, (Real)0);
    if (ch0.GetDimension() != 3)
    {
        return false;
    }

    // Construct convex hull of point set 1.
    ConvexHull3<Real, ComputeType> ch1;
    ch1(numPoints1, points1, (Real)0);
    if (ch1.GetDimension() != 3)
    {
        return false;
    }

    auto const& hull0 = ch0.GetHullUnordered();
    auto const& hull1 = ch1.GetHullUnordered();
    int numTriangles0 = static_cast<int>(hull0.size());
    int const* indices0 = reinterpret_cast<int const*>(&hull0[0]);
    int numTriangles1 = static_cast<int>(hull1.size());
    int const* indices1 = reinterpret_cast<int const*>(&hull1[0]);

    // Test faces of hull 0 for possible separation of points.
    int i, i0, i1, i2, side0, side1;
    Vector3<Real> diff0, diff1;
    for (i = 0; i < numTriangles0; ++i)
    {
        // Look up face (assert: i0 != i1 && i0 != i2 && i1 != i2).
        i0 = indices0[3 * i];
        i1 = indices0[3 * i + 1];
        i2 = indices0[3 * i + 2];

        // Compute potential separating plane (assert: normal != (0,0,0)).
        separatingPlane = Plane3<Real>({ points0[i0], points0[i1],
            points0[i2] });

        // Determine whether hull 1 is on same side of plane.
        side1 = OnSameSide(separatingPlane, numTriangles1, indices1, points1);

        if (side1)
        {
            // Determine on which side of plane hull 0 lies.
            side0 = WhichSide(separatingPlane, numTriangles0, indices0,
                points0);
            if (side0*side1 <= 0)  // Plane separates hulls.
            {
                return true;
            }
        }
    }

    // Test faces of hull 1 for possible separation of points.
    for (i = 0; i < numTriangles1; ++i)
    {
        // Look up edge (assert: i0 != i1 && i0 != i2 && i1 != i2).
        i0 = indices1[3 * i];
        i1 = indices1[3 * i + 1];
        i2 = indices1[3 * i + 2];

        // Compute perpendicular to face (assert: normal != (0,0,0)).
        separatingPlane = Plane3<Real>({ points1[i0], points1[i1],
            points1[i2] });

        // Determine whether hull 0 is on same side of plane.
        side0 = OnSameSide(separatingPlane, numTriangles0, indices0, points0);
        if (side0)
        {
            // Determine on which side of plane hull 1 lies.
            side1 = WhichSide(separatingPlane, numTriangles1, indices1,
                points1);
            if (side0*side1 <= 0)  // Plane separates hulls.
            {
                return true;
            }
        }
    }

    // Build edge set for hull 0.
    std::set<std::pair<int, int>> edgeSet0;
    for (i = 0; i < numTriangles0; ++i)
    {
        // Look up face (assert: i0 != i1 && i0 != i2 && i1 != i2).
        i0 = indices0[3 * i];
        i1 = indices0[3 * i + 1];
        i2 = indices0[3 * i + 2];
        edgeSet0.insert(std::make_pair(i0, i1));
        edgeSet0.insert(std::make_pair(i0, i2));
        edgeSet0.insert(std::make_pair(i1, i2));
    }

    // Build edge list for hull 1.
    std::set<std::pair<int, int>> edgeSet1;
    for (i = 0; i < numTriangles1; ++i)
    {
        // Look up face (assert: i0 != i1 && i0 != i2 && i1 != i2).
        i0 = indices1[3 * i];
        i1 = indices1[3 * i + 1];
        i2 = indices1[3 * i + 2];
        edgeSet1.insert(std::make_pair(i0, i1));
        edgeSet1.insert(std::make_pair(i0, i2));
        edgeSet1.insert(std::make_pair(i1, i2));
    }

    // Test planes whose normals are cross products of two edges, one from
    // each hull.
    for (auto const& e0 : edgeSet0)
    {
        // Get edge.
        diff0 = points0[e0.second] - points0[e0.first];

        for (auto const& e1 : edgeSet1)
        {
            diff1 = points1[e1.second] - points1[e1.first];

            // Compute potential separating plane.
            separatingPlane.normal = UnitCross(diff0, diff1);
            separatingPlane.constant = Dot(separatingPlane.normal,
                points0[e0.first]);

            // Determine if hull 0 is on same side of plane.
            side0 = OnSameSide(separatingPlane, numTriangles0, indices0,
                points0);
            side1 = OnSameSide(separatingPlane, numTriangles1, indices1,
                points1);

            if (side0*side1 < 0)  // Plane separates hulls.
            {
                return true;
            }
        }
    }

    return false;
}

template <typename Real, typename ComputeType>
int SeparatePoints3<Real, ComputeType>::OnSameSide(Plane3<Real> const& plane,
    int numTriangles, int const* indices, Vector3<Real> const* points) const
{
    // test if all points on same side of plane (nx,ny,nz)*(x,y,z) = c
    int posSide = 0, negSide = 0;

    for (int t = 0; t < numTriangles; ++t)
    {
        for (int i = 0; i < 3; ++i)
        {
            int v = indices[3 * t + i];
            Real c0 = Dot(plane.normal, points[v]);
            if (c0 > plane.constant)
            {
                ++posSide;
            }
            else if (c0 < plane.constant)
            {
                ++negSide;
            }

            if (posSide && negSide)
            {
                // Plane splits point set.
                return 0;
            }
        }
    }

    return (posSide ? +1 : -1);
}

template <typename Real, typename ComputeType>
int SeparatePoints3<Real, ComputeType>::WhichSide(Plane3<Real> const& plane,
    int numTriangles, int const* indices, Vector3<Real> const* points) const
{
    // Establish which side of plane hull is on.
    for (int t = 0; t < numTriangles; ++t)
    {
        for (int i = 0; i < 3; ++i)
        {
            int v = indices[3 * t + i];
            Real c0 = Dot(plane.normal, points[v]);
            if (c0 > plane.constant)
            {
                // Positive side.
                return +1;
            }
            if (c0 < plane.constant)
            {
                // Negative side.
                return -1;
            }
        }
    }

    // Hull is effectively collinear.
    return 0;
}


}
