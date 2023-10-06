// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteDistPointLine.h>
#include <ThirdParty/GTEngine/Mathematics/GteHypersphere.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

// The queries consider the circle to be a solid (disk).

namespace gte
{

template <typename Real>
class TIQuery<Real, Line2<Real>, Circle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Line2<Real> const& line, Circle2<Real> const& circle);
};

template <typename Real>
class FIQuery<Real, Line2<Real>, Circle2<Real>>
{
public:
    struct Result
    {
        bool intersect;
        int numIntersections;
        std::array<Real,2> parameter;
        std::array<Vector2<Real>, 2> point;
    };

    Result operator()(Line2<Real> const& line, Circle2<Real> const& circle);

protected:
    void DoQuery(Vector2<Real> const& lineOrigin,
        Vector2<Real> const& lineDirection, Circle2<Real> const& circle,
        Result& result);
};


template <typename Real>
typename TIQuery<Real, Line2<Real>, Circle2<Real>>::Result
TIQuery<Real, Line2<Real>, Circle2<Real>>::operator()(
    Line2<Real> const& line, Circle2<Real> const& circle)
{
    Result result;
    DCPQuery<Real, Vector2<Real>, Line2<Real>> plQuery;
    auto plResult = plQuery(circle.center, line);
    result.intersect = (plResult.distance <= circle.radius);
    return result;
}

template <typename Real>
typename FIQuery<Real, Line2<Real>, Circle2<Real>>::Result
FIQuery<Real, Line2<Real>, Circle2<Real>>::operator()(
    Line2<Real> const& line, Circle2<Real> const& circle)
{
    Result result;
    DoQuery(line.origin, line.direction, circle, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = line.origin + result.parameter[i] * line.direction;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Line2<Real>, Circle2<Real>>::DoQuery(
    Vector2<Real> const& lineOrigin, Vector2<Real> const& lineDirection,
    Circle2<Real> const& circle, Result& result)
{
    // Intersection of a the line P+t*D and the circle |X-C| = R.  The line
    // direction is unit length. The t-value is a real-valued root to the
    // quadratic equation
    //   0 = |t*D+P-C|^2 - R^2
    //     = t^2 + 2*Dot(D,P-C)*t + |P-C|^2-R^2
    //     = t^2 + 2*a1*t + a0
    // If there are two distinct roots, the order is t0 < t1.
    Vector2<Real> diff = lineOrigin - circle.center;
    Real a0 = Dot(diff, diff) - circle.radius * circle.radius;
    Real a1 = Dot(lineDirection, diff);
    Real discr = a1 * a1 - a0;
    if (discr > (Real)0)
    {
        Real root = std::sqrt(discr);
        result.intersect = true;
        result.numIntersections = 2;
        result.parameter[0] = -a1 - root;
        result.parameter[1] = -a1 + root;
    }
    else if (discr < (Real)0)
    {
        result.intersect = false;
        result.numIntersections = 0;
    }
    else  // discr == 0
    {
        result.intersect = true;
        result.numIntersections = 1;
        result.parameter[0] = -a1;
    }
}


}
