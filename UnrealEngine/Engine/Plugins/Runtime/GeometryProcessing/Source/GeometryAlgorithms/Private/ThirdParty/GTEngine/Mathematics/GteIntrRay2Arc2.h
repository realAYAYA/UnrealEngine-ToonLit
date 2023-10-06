// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteArc2.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrRay2Circle2.h>

// The queries consider the arc to be a 1-dimensional object.

namespace gte
{

template <typename Real>
class TIQuery<Real, Ray2<Real>, Arc2<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Ray2<Real> const& ray, Arc2<Real> const& arc);
};

template <typename Real>
class FIQuery<Real, Ray2<Real>, Arc2<Real>>
{
public:
    struct Result
    {
        bool intersect;
        int numIntersections;
        std::array<Real, 2> parameter;
        std::array<Vector2<Real>, 2> point;
    };

    Result operator()(Ray2<Real> const& ray, Arc2<Real> const& arc);
};


template <typename Real>
typename TIQuery<Real, Ray2<Real>, Arc2<Real>>::Result
TIQuery<Real, Ray2<Real>, Arc2<Real>>::operator()(
    Ray2<Real> const& ray, Arc2<Real> const& arc)
{
    Result result;
    FIQuery<Real, Ray2<Real>, Arc2<Real>> raQuery;
    auto raResult = raQuery(ray, arc);
    result.intersect = raResult.intersect;
    return result;
}

template <typename Real>
typename FIQuery<Real, Ray2<Real>, Arc2<Real>>::Result
FIQuery<Real, Ray2<Real>, Arc2<Real>>::operator()(
    Ray2<Real> const& ray, Arc2<Real> const& arc)
{
    Result result;

    FIQuery<Real, Ray2<Real>, Circle2<Real>> rcQuery;
    Circle2<Real> circle(arc.center, arc.radius);
    auto rcResult = rcQuery(ray, circle);
    if (rcResult.intersect)
    {
        // Test whether ray-circle intersections are on the arc.
        result.numIntersections = 0;
        for (int i = 0; i < rcResult.numIntersections; ++i)
        {
            if (arc.Contains(rcResult.point[i]))
            {
                result.intersect = true;
                result.parameter[result.numIntersections]
                    = rcResult.parameter[i];
                result.point[result.numIntersections++]
                    = rcResult.point[i];
            }
        }
    }
    else
    {
        result.intersect = false;
        result.numIntersections = 0;
    }

    return result;
}


}
