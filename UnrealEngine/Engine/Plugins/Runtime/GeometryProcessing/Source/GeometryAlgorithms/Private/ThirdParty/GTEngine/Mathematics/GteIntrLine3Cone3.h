// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2018/11/29)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteCone.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>

// The queries consider the cone to be single sided and solid.

namespace gte
{

template <typename Real>
class FIQuery<Real, Line3<Real>, Cone3<Real>>
{
public:
    struct Result
    {
        // Default construction.  Set 'intersect' to false, 'type' to 0,
        // 'parameter' to (0,0), and 'point' to (veczero,veczero).
        Result();

        bool intersect;

        // Because the intersection of line and cone with infinite height
        // h > 0 can be a ray or a line, we use a 'type' value that allows
        // you to decide how to interpret the parameter[] and point[] values.
        //   type  intersect  valid data
        //   0     none       none
        //   1     point      parameter[0] = parameter[1], finite
        //                    point[0] = point[1]
        //   2     segment    parameter[0] < parameter[1], finite
        //                    point[0,1] valid
        //   3     ray        parameter[0] finite, parameter[1] maxReal
        //                    point[0] = rayOrigin, point[1] = lineDirection
        //   4     ray        parameter[0] -maxReal, parameter[1] finite
        //                    point[0] = rayOrigin, point[1] = -lineDirection
        //   5     line       parameter[0] -maxReal, parameter[1] maxReal,
        //                    point[0] = lineOrigin, point[1] = lineDirection
        // If the cone height h is finite, only types 0, 1, or 2 can occur.
        int type;
        std::array<Real, 2> parameter;  // Relative to incoming line.
        std::array<Vector3<Real>, 2> point;
    };

    Result operator()(Line3<Real> const& line, Cone3<Real> const& cone);

protected:
    void DoQuery(Vector3<Real> const& lineOrigin,
        Vector3<Real> const& lineDirection, Cone3<Real> const& cone,
        Result& result);
};


template <typename Real>
FIQuery<Real, Line3<Real>, Cone3<Real>>::Result::Result()
    :
    intersect(false),
    type(0)
{
    parameter.fill((Real)0);
    point.fill({ (Real)0, (Real)0, (Real)0 });
}

template <typename Real>
typename FIQuery<Real, Line3<Real>, Cone3<Real>>::Result
FIQuery<Real, Line3<Real>, Cone3<Real>>::operator()(Line3<Real> const& line,
    Cone3<Real> const& cone)
{
    Result result;
    DoQuery(line.origin, line.direction, cone, result);
    switch (result.type)
    {
    case 1:  // point
        result.point[0] = line.origin + result.parameter[0] * line.direction;
        result.point[1] = result.point[0];
        break;
    case 2:  // segment
        result.point[0] = line.origin + result.parameter[0] * line.direction;
        result.point[1] = line.origin + result.parameter[1] * line.direction;
        break;
    case 3:  // ray
        result.point[0] = line.origin + result.parameter[0] * line.direction;
        result.point[1] = line.direction;
        break;
    case 4:  // ray
        result.point[0] = line.origin + result.parameter[1] * line.direction;
        result.point[1] = -line.direction;
        break;
    case 5:  // line
        result.point[0] = line.origin;
        result.point[1] = line.direction;
        break;
    default:  // no intersection
        break;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Line3<Real>, Cone3<Real>>::DoQuery(
    Vector3<Real> const& lineOrigin, Vector3<Real> const& lineDirection,
    Cone3<Real> const& cone, Result& result)
{
    // The cone has vertex V, unit-length axis direction D, angle theta in
    // (0,pi/2), and height h in (0,+infinity).  The line is P + t*U, where U
    // is a unit-length direction vector.  Define g = cos(theta).  The cone
    // is represented by
    //   (X-V)^T * (D*D^T - g^2*I) * (X-V) = 0,  0 <= Dot(D,X-V) <= h
    // The first equation defines a double-sided cone.  The first inequality
    // in the second equation limits this to a single-sided cone containing
    // the ray V + s*D with s >= 0.  We will call this the 'positive cone'.
    // The single-sided cone containing ray V + s * t with s <= 0 is called
    // the 'negative cone'.  The double-sided cone is the union of the
    // positive cone and negative cone.  The second inequality in the second
    // equation limits the single-sided cone to the region bounded by the
    // height.  Setting X(t) = P + t*U, the equations are
    //   c2*t^2 + 2*c1*t + c0 = 0,  0 <= Dot(D,U)*t + Dot(D,P-V) <= h
    // where
    //   c2 = Dot(D,U)^2 - g^2
    //   c1 = Dot(D,U)*Dot(D,P-V) - g^2*Dot(U,P-V)
    //   c0 = Dot(D,P-V)^2 - g^2*Dot(P-V,P-V)
    // The following code computes the t-interval that satisfies the quadratic
    // equation subject to the linear inequality constraints.

    Vector3<Real> PmV = lineOrigin - cone.ray.origin;
    Real DdU = Dot(cone.ray.direction, lineDirection);
    Real DdPmV = Dot(cone.ray.direction, PmV);
    Real UdPmV = Dot(lineDirection, PmV);
    Real PmVdPmV = Dot(PmV, PmV);
    Real cosAngleSqr = cone.cosAngle * cone.cosAngle;
    Real c2 = DdU * DdU - cosAngleSqr;
    Real c1 = DdU * DdPmV - cosAngleSqr * UdPmV;
    Real c0 = DdPmV * DdPmV - cosAngleSqr * PmVdPmV;
    Real t;

    if (c2 != (Real)0)
    {
        Real discr = c1 * c1 - c0 * c2;
        if (discr < (Real)0)
        {
            // The quadratic has no real-valued roots.  The line does not
            // intersect the double-sided cone.
            result.intersect = false;
            result.type = 0;
            return;
        }
        else if (discr > (Real)0)
        {
            // The quadratic has two distinct real-valued roots.  However, one
            // or both of them might intersect the negative cone.  We are
            // interested only in those intersections with the positive cone.
            Real root = std::sqrt(discr);
            Real invC2 = ((Real)1) / c2;
            int numParameters = 0;

            t = (-c1 - root) * invC2;
            if (DdU * t + DdPmV >= (Real)0)
            {
                result.parameter[numParameters++] = t;
            }

            t = (-c1 + root) * invC2;
            if (DdU * t + DdPmV >= (Real)0)
            {
                result.parameter[numParameters++] = t;
            }

            if (numParameters == 2)
            {
                // The line intersects the positive cone in two distinct
                // points.
                result.intersect = true;
                result.type = 2;
                if (result.parameter[0] > result.parameter[1])
                {
                    std::swap(result.parameter[0], result.parameter[1]);
                }
            }
            else if (numParameters == 1)
            {
                // The line intersects the positive cone in a single point and
                // the negative cone in a single point.  We report only the
                // intersection with the positive cone.
                result.intersect = true;
                if (DdU > (Real)0)
                {
                    result.type = 3;
                    result.parameter[1] = std::numeric_limits<Real>::max();
                }
                else
                {
                    result.type = 4;
                    result.parameter[1] = result.parameter[0];
                    result.parameter[0] = -std::numeric_limits<Real>::max();

                }
            }
            else
            {
                // The line intersects the negative cone in two distinct
                // points, but we are interested only in the intersections
                // with the positive cone.
                result.intersect = false;
                result.type = 0;
                return;
            }
        }
        else  // discr == 0
        {
            // One repeated real root; the line is tangent to the double-sided
            // cone at a single point.  Report only the point if it is on the
            // positive cone.
            t = -c1 / c2;
            if (DdU * t + DdPmV >= (Real)0)
            {
                result.intersect = true;
                result.type = 1;
                result.parameter[0] = t;
                result.parameter[1] = t;
            }
            else
            {
                result.intersect = false;
                result.type = 0;
                return;
            }
        }
    }
    else if (c1 != (Real)0)
    {
        // c2 = 0, c1 != 0; U is a direction vector on the cone boundary
        t = -((Real)0.5)*c0 / c1;
        if (DdU * t + DdPmV >= (Real)0)
        {
            // The line intersects the positive cone and the ray of
            // intersection is interior to the positive cone.
            result.intersect = true;
            if (DdU > (Real)0)
            {
                result.type = 3;
                result.parameter[0] = t;
                result.parameter[1] = std::numeric_limits<Real>::max();
            }
            else
            {
                result.type = 4;
                result.parameter[0] = -std::numeric_limits<Real>::max();
                result.parameter[1] = t;
            }
        }
        else
        {
            // The line intersects the negative cone and the ray of
            // intersection is interior to the positive cone.
            result.intersect = false;
            result.type = 0;
            return;
        }
    }
    else if (c0 != (Real)0)
    {
        // c2 = c1 = 0, c0 != 0.  Cross(D,U) is perpendicular to Cross(P-V,U)
        result.intersect = false;
        result.type = 0;
        return;
    }
    else
    {
        // c2 = c1 = c0 = 0; the line is on the cone boundary.
        result.intersect = true;
        result.type = 5;
        result.parameter[0] = -std::numeric_limits<Real>::max();
        result.parameter[1] = +std::numeric_limits<Real>::max();
    }

    if (cone.maxHeight < std::numeric_limits<Real>::max())
    {
        if (DdU != (Real)0)
        {
            // Clamp the intersection to the height of the cone.
            Real invDdU = ((Real)1) / DdU;
            std::array<Real, 2> hInterval;
            if (DdU >(Real)0)
            {
                hInterval[0] = -DdPmV * invDdU;
                hInterval[1] = (cone.maxHeight - DdPmV) * invDdU;
            }
            else // (DdU < (Real)0)
            {
                hInterval[0] = (cone.maxHeight - DdPmV) * invDdU;
                hInterval[1] = -DdPmV * invDdU;
            }

            FIIntervalInterval<Real> iiQuery;
            auto iiResult = iiQuery(result.parameter, hInterval);
            result.intersect = (iiResult.numIntersections > 0);
            result.type = iiResult.numIntersections;
            result.parameter = iiResult.overlap;
        }
        else if (result.intersect)
        {
            if (DdPmV > cone.maxHeight)
            {
                result.intersect = false;
                result.type = 0;
            }
        }
    }
}


}
