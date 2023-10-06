// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteHyperplane.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

namespace gte
{

template <typename Real>
class TIQuery<Real, Plane3<Real>, Plane3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Plane3<Real> const& plane0, Plane3<Real> const& plane1);
};

template <typename Real>
class FIQuery<Real, Plane3<Real>, Plane3<Real>>
{
public:
    struct Result
    {
        bool intersect;

        // If 'intersect' is true, the intersection is either a line or the
        // planes are the same.  When a line, 'line' is valid.  When a plane,
        // 'plane' is set to one of the planes.
        bool isLine;
        Line3<Real> line;
        Plane3<Real> plane;
    };

    Result operator()(Plane3<Real> const& plane0, Plane3<Real> const& plane1);
};


template <typename Real>
typename TIQuery<Real, Plane3<Real>, Plane3<Real>>::Result
TIQuery<Real, Plane3<Real>, Plane3<Real>>::operator()(
    Plane3<Real> const& plane0, Plane3<Real> const& plane1)
{
    // If Cross(N0,N1) is zero, then either planes are parallel and separated
    // or the same plane.  In both cases, 'false' is returned.  Otherwise, the
    // planes intersect.  To avoid subtle differences in reporting between
    // Test() and Find(), the same parallel test is used.  Mathematically,
    //   |Cross(N0,N1)|^2 = Dot(N0,N0)*Dot(N1,N1)-Dot(N0,N1)^2
    //                    = 1 - Dot(N0,N1)^2
    // The last equality is true since planes are required to have unit-length
    // normal vectors.  The test |Cross(N0,N1)| = 0 is the same as
    // |Dot(N0,N1)| = 1.

    Result result;
    Real dot = Dot(plane0.normal, plane1.normal);
    if (std::abs(dot) < (Real)1)
    {
        result.intersect = true;
        return result;
    }

    // The planes are parallel.  Check whether they are coplanar.
    Real cDiff;
    if (dot >= (Real)0)
    {
        // Normals are in same direction, need to look at c0-c1.
        cDiff = plane0.constant - plane1.constant;
    }
    else
    {
        // Normals are in opposite directions, need to look at c0+c1.
        cDiff = plane0.constant + plane1.constant;
    }

    result.intersect = (std::abs(cDiff) == (Real)0);
    return result;
}



template <typename Real>
typename FIQuery<Real, Plane3<Real>, Plane3<Real>>::Result
FIQuery<Real, Plane3<Real>, Plane3<Real>>::operator()(
    Plane3<Real> const& plane0, Plane3<Real> const& plane1)
{
    // If N0 and N1 are parallel, either the planes are parallel and separated
    // or the same plane.  In both cases, 'false' is returned.  Otherwise,
    // the intersection line is
    //   L(t) = t*Cross(N0,N1)/|Cross(N0,N1)| + c0*N0 + c1*N1
    // for some coefficients c0 and c1 and for t any real number (the line
    // parameter).  Taking dot products with the normals,
    //   d0 = Dot(N0,L) = c0*Dot(N0,N0) + c1*Dot(N0,N1) = c0 + c1*d
    //   d1 = Dot(N1,L) = c0*Dot(N0,N1) + c1*Dot(N1,N1) = c0*d + c1
    // where d = Dot(N0,N1).  These are two equations in two unknowns.  The
    // solution is
    //   c0 = (d0 - d*d1)/det
    //   c1 = (d1 - d*d0)/det
    // where det = 1 - d^2.

    Result result;

    Real dot = Dot(plane0.normal, plane1.normal);
    if (std::abs(dot) >= (Real)1)
    {
        // The planes are parallel.  Check if they are coplanar.
        Real cDiff;
        if (dot >= (Real)0)
        {
            // Normals are in same direction, need to look at c0-c1.
            cDiff = plane0.constant - plane1.constant;
        }
        else
        {
            // Normals are in opposite directions, need to look at c0+c1.
            cDiff = plane0.constant + plane1.constant;
        }

        if (std::abs(cDiff) == (Real)0)
        {
            // The planes are coplanar.
            result.intersect = true;
            result.isLine = false;
            result.plane = plane0;
            return result;
        }

        // The planes are parallel but distinct.
        result.intersect = false;
        return result;
    }

    Real invDet = ((Real)1) / ((Real)1 - dot*dot);
    Real c0 = (plane0.constant - dot*plane1.constant)*invDet;
    Real c1 = (plane1.constant - dot*plane0.constant)*invDet;
    result.intersect = true;
    result.isLine = true;
    result.line.origin = c0*plane0.normal + c1*plane1.normal;
    result.line.direction = UnitCross(plane0.normal, plane1.normal);
    return result;
}


}
