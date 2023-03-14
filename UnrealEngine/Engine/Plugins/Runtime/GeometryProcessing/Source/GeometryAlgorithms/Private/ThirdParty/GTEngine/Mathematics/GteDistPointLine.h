// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDCPQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, Line<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real lineParameter;  // t in (-infinity,+infinity)
        Vector<N, Real> lineClosest;  // origin + t * direction
    };

    Result operator()(Vector<N, Real> const& point,
        Line<N, Real> const& line);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointLine =
DCPQuery<Real, Vector<N, Real>, Line<N, Real>>;

template <typename Real>
using DCPPoint2Line2 = DCPPointLine<2, Real>;

template <typename Real>
using DCPPoint3Line3 = DCPPointLine<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, Line<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, Line<N, Real>>::operator()(
    Vector<N, Real> const& point, Line<N, Real> const& line)
{
    Result result;

    Vector<N, Real> diff = point - line.origin;
    result.lineParameter = Dot(line.direction, diff);
    result.lineClosest = line.origin + result.lineParameter*line.direction;

    diff = point - result.lineClosest;
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);

    return result;
}


}
