// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteHalfspace.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <vector>

// The queries consider the box to be a solid and the polygon to be a
// convex solid.

namespace gte
{

template <typename Real>
class FIQuery<Real, Halfspace<2, Real>, std::vector<Vector2<Real>>>
{
public:
    struct Result
    {
        bool intersect;

        // If 'intersect' is true, the halfspace and polygon intersect in a
        // convex polygon.
        std::vector<Vector2<Real>> polygon;
    };

    Result operator()(Halfspace<2, Real> const& halfspace,
        std::vector<Vector2<Real>> const& polygon);
};


template <typename Real>
typename FIQuery<Real, Halfspace<2, Real>, std::vector<Vector2<Real>>>::Result
FIQuery<Real, Halfspace<2, Real>, std::vector<Vector2<Real>>>::operator()(
   Halfspace<2, Real> const& halfspace, std::vector<Vector2<Real>> const& polygon)
{
    Result result;

    // Determine whether the polygon vertices are outside the halfspace,
    // inside the halfspace, or on the halfspace boundary.
    int const numVertices = static_cast<int>(polygon.size());
    std::vector<Real> distance(numVertices);
    int positive = 0, negative = 0, positiveIndex = -1;
    for (int i = 0; i < numVertices; ++i)
    {
        distance[i] = Dot(halfspace.normal, polygon[i]) - halfspace.constant;
        if (distance[i] > (Real)0)
        {
            ++positive;
            if (positiveIndex == -1)
            {
                positiveIndex = i;
            }
        }
        else if (distance[i] < (Real)0)
        {
            ++negative;
        }
    }

    if (positive == 0)
    {
        // The polygon is strictly outside the halfspace.
        result.intersect = false;
        return result;
    }

    if (negative == 0)
    {
        // The polygon is contained in the closed halfspace, so it is
        // fully visible and no clipping is necessary.
        result.intersect = true;
        return result;
    }

    // The line transversely intersects the polygon.  Clip the polygon.
    Vector2<Real> vertex;
    int curr, prev;
    Real t;

    if (positiveIndex > 0)
    {
        // Compute the first clip vertex on the line.
        curr = positiveIndex;
        prev = curr - 1;
        t = distance[curr] / (distance[curr] - distance[prev]);
        vertex = polygon[curr] + t * (polygon[prev] - polygon[curr]);
        result.polygon.push_back(vertex);

        // Include the vertices on the positive side of line.
        while (curr < numVertices && distance[curr] >(Real)0)
        {
            result.polygon.push_back(polygon[curr++]);
        }

        // Compute the kast clip vertex on the line.
        if (curr < numVertices)
        {
            prev = curr - 1;
        }
        else
        {
            curr = 0;
            prev = numVertices - 1;
        }
        t = distance[curr] / (distance[curr] - distance[prev]);
        vertex = polygon[curr] + t * (polygon[prev] - polygon[curr]);
        result.polygon.push_back(vertex);
    }
    else  // positiveIndex is 0
    {
        // Include the vertices on the positive side of line.
        curr = 0;
        while (curr < numVertices && distance[curr] >(Real)0)
        {
            result.polygon.push_back(polygon[curr++]);
        }

        // Compute the last clip vertex on the line.
        prev = curr - 1;
        t = distance[curr] / (distance[curr] - distance[prev]);
        vertex = polygon[curr] + t * (polygon[prev] - polygon[curr]);
        result.polygon.push_back(vertex);

        // Skip the vertices on the negative side of the line.
        while (curr < numVertices && distance[curr] <= (Real)0)
        {
            curr++;
        }

        // Compute the first clip vertex on the line.
        if (curr < numVertices)
        {
            prev = curr - 1;
            t = distance[curr] / (distance[curr] - distance[prev]);
            vertex = polygon[curr] + t * (polygon[prev] - polygon[curr]);
            result.polygon.push_back(vertex);

            // Keep the vertices on the positive side of the line.
            while (curr < numVertices && distance[curr] >(Real)0)
            {
                result.polygon.push_back(polygon[curr++]);
            }
        }
        else
        {
            curr = 0;
            prev = numVertices - 1;
            t = distance[curr] / (distance[curr] - distance[prev]);
            vertex = polygon[curr] + t * (polygon[prev] - polygon[curr]);
            result.polygon.push_back(vertex);
        }
    }

    result.intersect = true;
    return result;
}

}
