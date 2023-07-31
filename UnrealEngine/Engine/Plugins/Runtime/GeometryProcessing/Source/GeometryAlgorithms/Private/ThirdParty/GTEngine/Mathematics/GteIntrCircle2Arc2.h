// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteArc2.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrCircle2Circle2.h>

namespace gte
{

template <typename Real>
class FIQuery<Real, Circle2<Real>, Arc2<Real>>
{
public:
    struct Result
    {
        bool intersect;

        // The number of intersections is 0, 1, 2, or maxInt =
        // std::numeric_limits<int>::max().  When 1, the arc and circle
        // intersect in a single point.  When 2, the arc is not on the circle
        // and they intersect in two points.  When maxInt, the arc is on the
        // circle.
        int numIntersections;

        // Valid only when numIntersections = 1 or 2.
        Vector2<Real> point[2];

        // Valid only when numIntersections = maxInt.
        Arc2<Real> arc;
    };

    Result operator()(Circle2<Real> const& circle, Arc2<Real> const& arc);
};


template <typename Real>
typename FIQuery<Real, Circle2<Real>, Arc2<Real>>::Result
FIQuery<Real, Circle2<Real>, Arc2<Real>>::operator()(
    Circle2<Real> const& circle, Arc2<Real> const& arc)
{
    Result result;

    Circle2<Real> circleOfArc(arc.center, arc.radius);
    FIQuery<Real, Circle2<Real>, Circle2<Real>> ccQuery;
    auto ccResult = ccQuery(circle, circleOfArc);
    if (!ccResult.intersect)
    {
        result.intersect = false;
        result.numIntersections = 0;
        return result;
    }

    if (ccResult.numIntersections == std::numeric_limits<int>::max())
    {
        // The arc is on the circle.
        result.intersect = true;
        result.numIntersections = std::numeric_limits<int>::max();
        result.arc = arc;
        return result;
    }

    // Test whether circle-circle intersection points are on the arc.
    for (int i = 0; i < ccResult.numIntersections; ++i)
    {
        result.numIntersections = 0;
        if (arc.Contains(ccResult.point[i]))
        {
            result.point[result.numIntersections++] = ccResult.point[i];
            result.intersect = true;
        }
    }
    return result;
}


}
