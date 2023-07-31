// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteCapsule.h>
#include <ThirdParty/GTEngine/Mathematics/GteHalfspace.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

// Queries for intersection of objects with halfspaces.  These are useful for
// containment testing, object culling, and clipping.

namespace gte
{

template <typename Real>
class TIQuery<Real, Halfspace3<Real>, Capsule3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Halfspace3<Real> const& halfspace,
        Capsule3<Real> const& capsule);
};


template <typename Real>
typename TIQuery<Real, Halfspace3<Real>, Capsule3<Real>>::Result
TIQuery<Real, Halfspace3<Real>, Capsule3<Real>>::operator()(
    Halfspace3<Real> const& halfspace, Capsule3<Real> const& capsule)
{
    Result result;

    // Project the capsule onto the normal line.  The plane of the halfspace
    // occurs at the origin (zero) of the normal line.
    Real e0 =
        Dot(halfspace.normal, capsule.segment.p[0]) - halfspace.constant;
    Real e1 =
        Dot(halfspace.normal, capsule.segment.p[1]) - halfspace.constant;

    // The capsule and halfspace intersect when the projection interval
    // maximum is nonnegative.
    result.intersect = (std::max(e0, e1) + capsule.radius >= (Real)0);
    return result;
}


}
