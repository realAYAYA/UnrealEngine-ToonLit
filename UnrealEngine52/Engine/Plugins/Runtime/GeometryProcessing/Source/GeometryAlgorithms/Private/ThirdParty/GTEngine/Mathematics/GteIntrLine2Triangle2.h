// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>
#include <ThirdParty/GTEngine/Mathematics/GteTriangle.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

// The queries consider the triangle to be a solid.

namespace gte
{

template <typename Real>
class TIQuery<Real, Line2<Real>, Triangle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Line2<Real> const& line,
        Triangle2<Real> const& triangle);
};

template <typename Real>
class FIQuery<Real, Line2<Real>, Triangle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
        int numIntersections;
        std::array<Real, 2> parameter;
        std::array<Vector2<Real>, 2> point;
    };

    Result operator()(Line2<Real> const& line,
        Triangle2<Real> const& triangle);

protected:
    void DoQuery(Vector2<Real> const& lineOrigin,
        Vector2<Real> const& lineDirection, Triangle2<Real> const& triangle,
        Result& result);
};


template <typename Real>
typename TIQuery<Real, Line2<Real>, Triangle2<Real>>::Result
TIQuery<Real, Line2<Real>, Triangle2<Real>>::operator()(
    Line2<Real> const& line, Triangle2<Real> const& triangle)
{
    Result result;

    // Determine on which side of the line the vertices lie.  The table of
    // possibilities is listed next with n = numNegative, p = numPositive, and
    // z = numZero.
    //
    //   n p z  intersection
    //   ------------------------------------
    //   0 3 0  none
    //   0 2 1  vertex
    //   0 1 2  edge
    //   0 0 3  none (degenerate triangle)
    //   1 2 0  segment (2 edges clipped)
    //   1 1 1  segment (1 edge clipped)
    //   1 0 2  edge
    //   2 1 0  segment (2 edges clipped)
    //   2 0 1  vertex
    //   3 0 0  none

    Real s[3];
    int numPositive = 0, numNegative = 0, numZero = 0;
    for (int i = 0; i < 3; ++i)
    {
        Vector2<Real> diff = triangle.v[i] - line.origin;
        s[i] = DotPerp(line.direction, diff);
        if (s[i] >(Real)0)
        {
            ++numPositive;
        }
        else if (s[i] < (Real)0)
        {
            ++numNegative;
        }
        else
        {
            ++numZero;
        }
    }

    result.intersect =
        (numZero == 0 && (numPositive == 0 || numNegative == 0)) ||
        (numZero == 3);

    return result;
}

template <typename Real>
typename FIQuery<Real, Line2<Real>, Triangle2<Real>>::Result
FIQuery<Real, Line2<Real>, Triangle2<Real>>::operator()(
    Line2<Real> const& line, Triangle2<Real> const& triangle)
{
    Result result;
    DoQuery(line.origin, line.direction, triangle, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = line.origin + result.parameter[i] * line.direction;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Line2<Real>, Triangle2<Real>>::DoQuery(
    Vector2<Real> const& lineOrigin, Vector2<Real> const& lineDirection,
    Triangle2<Real> const& triangle, Result& result)
{
    // Determine on which side of the line the vertices lie.  The table of
    // possibilities is listed next with n = numNegative, p = numPositive, and
    // z = numZero.
    //
    //   n p z  intersection
    //   ------------------------------------
    //   0 3 0  none
    //   0 2 1  vertex
    //   0 1 2  edge
    //   0 0 3  none (degenerate triangle)
    //   1 2 0  segment (2 edges clipped)
    //   1 1 1  segment (1 edge clipped)
    //   1 0 2  edge
    //   2 1 0  segment (2 edges clipped)
    //   2 0 1  vertex
    //   3 0 0  none

    Real s[3];
    int numPositive = 0, numNegative = 0, numZero = 0;
    for (int i = 0; i < 3; ++i)
    {
        Vector2<Real> diff = triangle.v[i] - lineOrigin;
        s[i] = DotPerp(lineDirection, diff);
        if (s[i] >(Real)0)
        {
            ++numPositive;
        }
        else if (s[i] < (Real)0)
        {
            ++numNegative;
        }
        else
        {
            ++numZero;
        }
    }

    if (numZero == 0 && numPositive > 0 && numNegative > 0)
    {
        result.intersect = true;
        result.numIntersections = 2;
        Real sign = (Real)3 - numPositive * (Real)2;
        for (int i0 = 0; i0 < 3; ++i0)
        {
            if (sign * s[i0] >(Real)0)
            {
                int i1 = (i0 + 1) % 3, i2 = (i0 + 2) % 3;
                Real s1 = s[i1] / (s[i1] - s[i0]);
                Vector2<Real> p1 = (triangle.v[i1] - lineOrigin) +
                    s1 * (triangle.v[i0] - triangle.v[i1]);
                result.parameter[0] = Dot(lineDirection, p1);
                Real s2 = s[i2] / (s[i2] - s[i0]);
                Vector2<Real> p2 = (triangle.v[i2] - lineOrigin) +
                    s2 * (triangle.v[i0] - triangle.v[i2]);
                result.parameter[1] = Dot(lineDirection, p2);
                break;
            }
        }
        return;
    }

    if (numZero == 1)
    {
        result.intersect = true;
        for (int i0 = 0; i0 < 3; ++i0)
        {
            if (s[i0] == (Real)0)
            {
                int i1 = (i0 + 1) % 3, i2 = (i0 + 2) % 3;
                result.parameter[0] =
                    Dot(lineDirection, triangle.v[i0] - lineOrigin);
                if (numPositive == 2 || numNegative == 2)
                {
                    result.numIntersections = 1;

                    // Used by derived classes.
                    result.parameter[1] = result.parameter[0];
                }
                else
                {
                    result.numIntersections = 2;
                    Real s1 = s[i1] / (s[i1] - s[i2]);
                    Vector2<Real> p1 = (triangle.v[i1] - lineOrigin) +
                        s1 * (triangle.v[i2] - triangle.v[i1]);
                    result.parameter[1] = Dot(lineDirection, p1);
                }
                break;
            }
        }
        return;
    }

    if (numZero == 2)
    {
        result.intersect = true;
        result.numIntersections = 2;
        for (int i0 = 0; i0 < 3; ++i0)
        {
            if (s[i0] != (Real)0)
            {
                int i1 = (i0 + 1) % 3, i2 = (i0 + 2) % 3;
                result.parameter[0] =
                    Dot(lineDirection, triangle.v[i1] - lineOrigin);
                result.parameter[1] =
                    Dot(lineDirection, triangle.v[i2] - lineOrigin);
                break;
            }
        }
        return;
    }

    // (n,p,z) one of (3,0,0), (0,3,0), (0,0,3)
    result.intersect = false;
    result.numIntersections = 0;
}


}
