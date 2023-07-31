// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteApprQuadratic2.h>

// Least-squares fit of a circle to a set of points.  A successful fit is
// indicated by the return value of 'true'.  The return value is the number
// of iterations used.  If the number is the maximum, you can either accept
// the result or try increasing the maximum number and calling the function
// again.  TODO: Currently, the test for terminating the algorithm is exact
// (diff == (0,0)).  Expose an epsilon for this?
//
// If initialCenterIsAverage is set to 'true', the initial guess for the
// circle center is the average of the data points.  If the data points are
// clustered along a small arc, ApprCircle2 is very slow to converge.  If
// initialCenterIsAverage is set to 'false', the initial guess for the
// circle center is computed using a least-squares estimate of the
// coefficients for a quadratic equation that represents a circle.  This
// approach tends to converge rapidly.

namespace gte
{

template <typename Real>
class ApprCircle2
{
public:
    unsigned int operator()(int numPoints, Vector2<Real> const* points,
        unsigned int maxIterations, bool initialCenterIsAverage,
        Circle2<Real>& circle);
};


template <typename Real>
unsigned int ApprCircle2<Real>::operator()(int numPoints,
    Vector2<Real> const* points, unsigned int maxIterations,
    bool initialCenterIsAverage, Circle2<Real>& circle)
{
    // Compute the average of the data points.
    Vector2<Real> average = points[0];
    for (int i = 1; i < numPoints; ++i)
    {
        average += points[i];
    }
    Real invNumPoints = ((Real)1) / static_cast<Real>(numPoints);
    average *= invNumPoints;

    // The initial guess for the center.
    if (initialCenterIsAverage)
    {
        circle.center = average;
    }
    else
    {
        ApprQuadraticCircle2<Real>()(numPoints, points, circle);
    }

    unsigned int iteration;
    for (iteration = 0; iteration < maxIterations; ++iteration)
    {
        // Update the iterates.
        Vector2<Real> current = circle.center;

        // Compute average L, dL/da, dL/db.
        Real lenAverage = (Real)0;
        Vector2<Real> derLenAverage = Vector2<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            Vector2<Real> diff = points[i] - circle.center;
            Real length = Length(diff);
            if (length >(Real)0)
            {
                lenAverage += length;
                Real invLength = ((Real)1) / length;
                derLenAverage -= invLength * diff;
            }
        }
        lenAverage *= invNumPoints;
        derLenAverage *= invNumPoints;

        circle.center = average + lenAverage * derLenAverage;
        circle.radius = lenAverage;

        Vector2<Real> diff = circle.center - current;
        if (diff == Vector2<Real>::Zero())
        {
            break;
        }
    }

    return ++iteration;
}


}
