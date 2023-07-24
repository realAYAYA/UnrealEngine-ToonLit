// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteApprQuadratic3.h>

// Least-squares fit of a sphere to a set of points.  A successful fit is
// indicated by the return value of 'true'.  The return value is the number
// of iterations used.  If the number is the maximum, you can either accept
// the result or try increasing the maximum number and calling the function
// again.  TODO: Currently, the test for terminating the algorithm is exact
// (diff == (0,0,0)).  Expose an epsilon for this?
//
// If initialCenterIsAverage is set to 'true', the initial guess for the
// sphere center is the average of the data points.  If the data points are
// clustered along a solid angle, ApprSphere32 is very slow to converge.  If
// initialCenterIsAverage is set to 'false', the initial guess for the
// sphere center is computed using a least-squares estimate of the
// coefficients for a quadratic equation that represents a sphere.  This
// approach tends to converge rapidly.

namespace gte
{

template <typename Real>
class ApprSphere3
{
public:
    unsigned int operator()(int numPoints, Vector3<Real> const* points,
        unsigned int maxIterations, bool initialCenterIsAverage,
        Sphere3<Real>& sphere);
};


template <typename Real>
unsigned int ApprSphere3<Real>::operator()(int numPoints,
    Vector3<Real> const* points, unsigned int maxIterations,
    bool initialCenterIsAverage, Sphere3<Real>& sphere)
{
    // Compute the average of the data points.
    Vector3<Real> average = points[0];
    for (int i = 1; i < numPoints; ++i)
    {
        average += points[i];
    }
    Real invNumPoints = ((Real)1) / static_cast<Real>(numPoints);
    average *= invNumPoints;

    // The initial guess for the center.
    if (initialCenterIsAverage)
    {
        sphere.center = average;
    }
    else
    {
        ApprQuadraticSphere3<Real>()(numPoints, points, sphere);
    }

    unsigned int iteration;
    for (iteration = 0; iteration < maxIterations; ++iteration)
    {
        // Update the iterates.
        Vector3<Real> current = sphere.center;

        // Compute average L, dL/da, dL/db, dL/dc.
        Real lenAverage = (Real)0;
        Vector3<Real> derLenAverage = Vector3<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            Vector3<Real> diff = points[i] - sphere.center;
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

        sphere.center = average + lenAverage * derLenAverage;
        sphere.radius = lenAverage;

        Vector3<Real> diff = sphere.center - current;
        if (diff == Vector3<Real>::Zero())
        {
            break;
        }
    }

    return ++iteration;
}


}
