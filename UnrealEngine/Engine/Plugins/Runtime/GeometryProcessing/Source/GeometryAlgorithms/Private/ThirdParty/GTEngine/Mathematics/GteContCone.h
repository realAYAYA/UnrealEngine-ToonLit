// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.19.0 (2018/11/30)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteCone.h>

namespace gte
{
    // Test for containment of a point by a cone.
    template <int N, typename Real>
    bool InContainer(Vector<N, Real> const& point, Cone<N, Real> const& cone)
    {
        Vector<N, Real> diff = point - cone.ray.origin;
        Real h = Dot(cone.ray.direction, diff);
        return cone.minHeight <= h && h <= cone.maxHeight && h >= cone.cosAngle * Length(diff);
    }
}
