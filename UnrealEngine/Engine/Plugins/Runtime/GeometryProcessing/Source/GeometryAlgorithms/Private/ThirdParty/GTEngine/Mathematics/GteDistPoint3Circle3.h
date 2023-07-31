// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteCircle3.h>

// The 3D point-circle distance algorithm is described in
// http://www.geometrictools.com/Documentation/DistanceToCircle3.pdf
// The notation used in the code matches that of the document.

namespace gte
{

template <typename Real>
class DCPQuery<Real, Vector3<Real>, Circle3<Real>>
{
public:
    // Either a single point on the circle is closest to 'point', in which
    // case 'equidistant' is false, or the entire circle is closest to
    // 'point', in which case 'equidistant' is true.  In the latter case, the
    // query returns the circle point C+r*U, where C is the circle center, r
    // is the circle radius, and U is a vector perpendicular to the normal N
    // for the plane of the circle.
    struct Result
    {
        Real distance, sqrDistance;
        Vector3<Real> circleClosest;
        bool equidistant;
    };

    Result operator()(Vector3<Real> const& point,
        Circle3<Real> const& circle);
};


template <typename Real>
typename DCPQuery<Real, Vector3<Real>, Circle3<Real>>::Result
DCPQuery<Real, Vector3<Real>, Circle3<Real>>::operator()(
    Vector3<Real> const& point, Circle3<Real> const& circle)
{
    Result result;

    // Projection of P-C onto plane is Q-C = P-C - Dot(N,P-C)*N.
    Vector3<Real> PmC = point - circle.center;
    Vector3<Real> QmC = PmC - Dot(circle.normal, PmC)*circle.normal;
    Real lengthQmC = Length(QmC);
    if (lengthQmC > (Real)0)
    {
        result.circleClosest =
            circle.center + (circle.radius / lengthQmC) * QmC;
        result.equidistant = false;
    }
    else
    {
        // All circle points are equidistant from P.  Return one of them.
        Vector3<Real> basis[3];
        basis[0] = circle.normal;
        ComputeOrthogonalComplement(1, basis);
        result.circleClosest = circle.center + circle.radius * basis[1];
        result.equidistant = true;
    }

    Vector3<Real> diff = point - result.circleClosest;
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);
    return result;
}


}
