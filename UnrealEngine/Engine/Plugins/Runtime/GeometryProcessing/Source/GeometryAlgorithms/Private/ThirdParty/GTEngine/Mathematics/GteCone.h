// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2019/02/15)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteMath.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>

// An acute cone is Dot(A,X-V) = |X-V| cos(t) where V is the vertex, A is the
// unit-length direction of the axis of the cone, and T is the cone angle with
// 0 < t < pi/2.  The cone interior is defined by the inequality
// Dot(A,X-V) >= |X-V| cos(t).  Since cos(t) > 0, we can avoid computing
// square roots.  The solid cone is defined by the inequality
// Dot(A,X-V)^2 >= Dot(X-V,X-V) cos(t)^2.  I refer to this object as an
// "infinite cone."  Cone axis points are V + h * A, where h is referred to
// as height and 0 <= h < +infinity.
//
// The cone can be truncated by a plane perpendicular to its axis at a height
// hmax with 0 < hmax < +infinity.  I refer to this object as a "finite cone."
// The finite cone is capped by has a circular disk opposite the vertex; the
// disk has radius hmax*tan(t).
//
// The finite cone can be additionally truncated at a height hmin with
// 0 < hmin < hmax < +infinity.  I refer to this a a "cone frustum."

namespace gte
{
    template <int N, typename Real>
    class Cone
    {
    public:
        // The default constructor creates an infinite cone with
        //   vertex = (0,...,0)
        //   axis = (0,...,0,1)
        //   angle = pi/4
        //   minimum height = 0
        //   maximum height = std::numeric_limits<Real>::max()
        Cone()
            :
            minHeight((Real)0),
            maxHeight(std::numeric_limits<Real>::max())
        {
            ray.origin.MakeZero();
            ray.direction.MakeUnit(N - 1);
            SetAngle((Real)GTE_C_QUARTER_PI);
        }

        // This constructor creates an infinite cone with the specified
        // vertex, axis direction and angle, and with heights
        //   minimum height = 0
        //   maximum height = std::numeric_limits<Real>::max()
        Cone(Ray<N, Real> const& inRay, Real inAngle)
            :
            ray(inRay),
            minHeight((Real)0),
            maxHeight(std::numeric_limits<Real>::max())
        {
            SetAngle(inAngle);
        }

        // This constructor creates a cone with all parameters specified.
        Cone(Ray<N, Real> const& inRay, Real inAngle, Real inMinHeight, Real inMaxHeight)
            :
            ray(inRay),
            minHeight(inMinHeight),
            maxHeight(inMaxHeight)
        {
            LogAssert((Real)0 <= minHeight && minHeight < maxHeight, "Invalid height interval.");
            SetAngle(inAngle);
        }

        // The angle must be in (0,pi/2).  The function sets 'angle' and
        // computes 'cosAngle', 'sinAngle', 'tanAngle', 'cosAngleSqr',
        // 'sinAngleSqr' and 'invSinAngle'.
        void SetAngle(Real inAngle)
        {
            LogAssert((Real)0 < inAngle && inAngle < (Real)GTE_C_HALF_PI, "Invalid angle.");
            angle = inAngle;
            cosAngle = std::cos(angle);
            sinAngle = std::sin(angle);
            tanAngle = std::tan(angle);
            cosAngleSqr = cosAngle * cosAngle;
            sinAngleSqr = sinAngle * sinAngle;
            invSinAngle = (Real)1 / sinAngle;
        }

        // The cone axis direction must be unit length.  The angle must
        // be in (0,pi/2).  The heights must satisfy
        // 0 <= minHeight < maxHeight <= std::numeric_limits<Real>::max().
        Ray<N, Real> ray;
        Real angle;
        Real minHeight, maxHeight;

        // Members derived from 'angle', to avoid calling trigonometric
        // functions in geometric queries (for speed).  You may set 'angle'
        // and compute these by calling SetAngle(inAngle).
        Real cosAngle, sinAngle, tanAngle;
        Real cosAngleSqr, sinAngleSqr, invSinAngle;

    public:
        // Comparisons to support sorted containers.  These based only on
        // 'ray', 'angle', 'minHeight' and 'maxHeight'.
        bool operator==(Cone const& cone) const
        {
            return ray == cone.ray
                && angle == cone.angle
                && minHeight == cone.minHeight
                && maxHeight == cone.maxHeight;
        }

        bool operator!=(Cone const& cone) const
        {
            return !operator==(cone);
        }

        bool operator< (Cone const& cone) const
        {
            if (ray < cone.ray)
            {
                return true;
            }

            if (ray > cone.ray)
            {
                return false;
            }

            if (angle < cone.angle)
            {
                return true;
            }

            if (angle > cone.angle)
            {
                return false;
            }

            if (minHeight < cone.minHeight)
            {
                return true;
            }

            if (minHeight > cone.minHeight)
            {
                return false;
            }

            return maxHeight < cone.maxHeight;
        }

        bool operator<=(Cone const& cone) const
        {
            return !cone.operator<(*this);
        }

        bool operator> (Cone const& cone) const
        {
            return cone.operator<(*this);
        }

        bool operator>=(Cone const& cone) const
        {
            return !operator<(cone);
        }
    };

    // Template alias for convenience.
    template <typename Real>
    using Cone3 = Cone<3, Real>;
}
