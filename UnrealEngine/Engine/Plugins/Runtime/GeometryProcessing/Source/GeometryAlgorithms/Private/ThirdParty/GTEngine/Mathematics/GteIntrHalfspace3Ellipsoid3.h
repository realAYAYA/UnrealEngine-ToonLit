// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMatrix3x3.h>
#include <ThirdParty/GTEngine/Mathematics/GteHalfspace.h>
#include <ThirdParty/GTEngine/Mathematics/GteHyperellipsoid.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

// Queries for intersection of objects with halfspaces.  These are useful for
// containment testing, object culling, and clipping.

namespace gte
{

template <typename Real>
class TIQuery<Real, Halfspace3<Real>, Ellipsoid3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Halfspace3<Real> const& halfspace,
        Ellipsoid3<Real> const& ellipsoid);
};


template <typename Real>
typename TIQuery<Real, Halfspace3<Real>, Ellipsoid3<Real>>::Result
TIQuery<Real, Halfspace3<Real>, Ellipsoid3<Real>>::operator()(
    Halfspace3<Real> const& halfspace, Ellipsoid3<Real> const& ellipsoid)
{
    // Project the ellipsoid onto the normal line.  The plane of the
    // halfspace occurs at the origin (zero) of the normal line.
    Result result;
    Matrix3x3<Real> MInverse;
    ellipsoid.GetMInverse(MInverse);
    Real discr = Dot(halfspace.normal, MInverse * halfspace.normal);
    Real extent = std::sqrt(std::max(discr, (Real)0));
    Real center = Dot(halfspace.normal, ellipsoid.center) - halfspace.constant;
    Real tmax = center + extent;

    // The ellipsoid and halfspace intersect when the projection interval
    // maximum is nonnegative.
    result.intersect = (tmax >= (Real)0);
    return result;
}


}
