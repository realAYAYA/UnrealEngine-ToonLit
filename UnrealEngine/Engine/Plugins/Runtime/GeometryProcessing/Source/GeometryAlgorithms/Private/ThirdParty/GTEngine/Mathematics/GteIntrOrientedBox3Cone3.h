// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/11/29)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteCone.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrAlignedBox3Cone3.h>

// Test for intersection of a box and a cone.  The cone can be infinite
//   0 <= minHeight < maxHeight = std::numeric_limits<Real>::max()
// or finite (cone frustum)
//   0 <= minHeight < maxHeight < std::numeric_limits<Real>::max().
// The algorithm is described in
//   http://www.geometrictools.com/Documentation/IntersectionBoxCone.pdf
// and reports an intersection only when th intersection set has positive
// volume.  For example, let the box be outside the cone.  If the box is
// below the minHeight plane at the cone vertex and just touches the cone
// vertex, no intersection is reported.  If the box is above the maxHeight
// plane and just touches the disk capping the cone, either at a single
// point, a line segment of points or a polygon of points, no intersection
// is reported.  However, if the box straddles the minHeight plane (part of
// the box strictly above the plane and part of the box strictly below the
// plane) and just touches the cone vertex, an intersection is reported.

namespace gte
{
    template <typename Real>
    class TIQuery<Real, OrientedBox<3, Real>, Cone<3, Real>>
        :
        public TIQuery<Real, AlignedBox<3, Real>, Cone<3, Real>>
    {
    public:
        struct Result
            :
            public TIQuery<Real, AlignedBox<3, Real>, Cone<3, Real>>::Result
        {
            // No additional information to compute.
        };

        Result operator()(OrientedBox<3, Real> const& box, Cone<3, Real> const& cone)
        {
            // Transform the cone and box so that the cone vertex is at the
            // origin and the box is axis aligned.  This allows us to call the
            // base class operator()(...).
            Vector<3, Real> diff = box.center - cone.ray.origin;
            Vector<3, Real> xfrmBoxCenter
            {
                Dot(box.axis[0], diff),
                Dot(box.axis[1], diff),
                Dot(box.axis[2], diff)
            };
            AlignedBox<3, Real> xfrmBox;
            xfrmBox.min = xfrmBoxCenter - box.extent;
            xfrmBox.max = xfrmBoxCenter + box.extent;

            Cone<3, Real> xfrmCone = cone;
            for (int i = 0; i < 3; ++i)
            {
                xfrmCone.ray.origin[i] = (Real)0;
                xfrmCone.ray.direction[i] = Dot(box.axis[i], cone.ray.direction);
            }

            // Test for intersection between the aligned box and the cone.
            auto bcResult = TIQuery<Real, AlignedBox<3, Real>, Cone<3, Real>>::operator()(xfrmBox, xfrmCone);
            Result result;
            result.intersect = bcResult.intersect;
            return result;
        }
    };

    // Template alias for convenience.
    template <typename Real>
    using TIOrientedBox3Cone3 = TIQuery<Real, OrientedBox<3, Real>, Cone<3, Real>>;
}
