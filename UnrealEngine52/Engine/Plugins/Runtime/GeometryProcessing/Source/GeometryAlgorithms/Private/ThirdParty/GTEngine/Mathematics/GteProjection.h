// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteHyperellipsoid.h>
#include <ThirdParty/GTEngine/Mathematics/GteLine.h>

namespace gte
{

// Project an ellipse onto a line.  The projection interval is [smin,smax]
// and corresponds to the line segment P+s*D, where smin <= s <= smax.
template <typename Real>
void Project(Ellipse2<Real> const& ellipse, Line2<Real> const& line,
    Real& smin, Real& smax);

// Project an ellipsoid onto a line.  The projection interval is [smin,smax]
// and corresponds to the line segment P+s*D, where smin <= s <= smax.
template <typename Real>
void Project(Ellipsoid3<Real> const& ellipsoid,
    Line3<Real> const& line, Real& smin, Real& smax);


template <typename Real>
void Project(Ellipse2<Real> const& ellipse, Line2<Real> const& line,
    Real& smin, Real& smax)
{
    // Center of projection interval.
    Real center = Dot(line.direction, ellipse.center - line.origin);

    // Radius of projection interval.
    Real tmp[2] =
    {
        ellipse.extent[0] * Dot(line.direction, ellipse.axis[0]),
        ellipse.extent[1] * Dot(line.direction, ellipse.axis[1])
    };
    Real rSqr = tmp[0] * tmp[0] + tmp[1] * tmp[1];
    Real radius = std::sqrt(rSqr);

    smin = center - radius;
    smax = center + radius;
}

template <typename Real>
void Project(Ellipsoid3<Real> const& ellipsoid, Line3<Real> const& line,
    Real& smin, Real& smax)
{
    // Center of projection interval.
    Real center = Dot(line.direction, ellipsoid.center - line.origin);

    // Radius of projection interval.
    Real tmp[3] =
    {
        ellipsoid.extent[0] * Dot(line.direction, ellipsoid.axis[0]),
        ellipsoid.extent[1] * Dot(line.direction, ellipsoid.axis[1]),
        ellipsoid.extent[2] * Dot(line.direction, ellipsoid.axis[2])
    };
    Real rSqr = tmp[0] * tmp[0] + tmp[1] * tmp[1] + tmp[2] * tmp[2];
    Real radius = std::sqrt(rSqr);

    smin = center - radius;
    smax = center + radius;
}


}
