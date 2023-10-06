// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/11/28)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrRay3AlignedBox3.h>

// The test-intersection queries use the method of separating axes.  The
// find-intersection queries use parametric clipping against the six faces of
// the box.  The find-intersection queries use Liang-Barsky clipping.  The
// queries consider the box to be a solid.  The algorithms are described in
//   https://www.geometrictools.com/Documentation/IntersectionLineBox.pdf

namespace gte
{
    template <typename Real>
    class TIQuery<Real, Ray3<Real>, OrientedBox3<Real>>
        :
        public TIQuery<Real, Ray3<Real>, AlignedBox3<Real>>
    {
    public:
        struct Result
            :
            public TIQuery<Real, Ray3<Real>, AlignedBox3<Real>>::Result
        {
            // No additional information to compute.
        };

        Result operator()(Ray3<Real> const& ray, OrientedBox3<Real> const& box)
        {
            // Transform the ray to the oriented-box coordinate system.
            Vector3<Real> diff = ray.origin - box.center;
            Vector3<Real> rayOrigin
            {
                Dot(diff, box.axis[0]),
                Dot(diff, box.axis[1]),
                Dot(diff, box.axis[2])
            };
            Vector3<Real> rayDirection = Vector3<Real>
            {
                Dot(ray.direction, box.axis[0]),
                Dot(ray.direction, box.axis[1]),
                Dot(ray.direction, box.axis[2])
            };

            Result result;
            this->DoQuery(rayOrigin, rayDirection, box.extent, result);
            return result;
        }
    };

    template <typename Real>
    class FIQuery<Real, Ray3<Real>, OrientedBox3<Real>>
        :
        public FIQuery<Real, Ray3<Real>, AlignedBox3<Real>>
    {
    public:
        struct Result
            :
            public FIQuery<Real, Ray3<Real>, AlignedBox3<Real>>::Result
        {
            // No additional information to compute.
        };

        Result operator()(Ray3<Real> const& ray, OrientedBox3<Real> const& box)
        {
            // Transform the ray to the oriented-box coordinate system.
            Vector3<Real> diff = ray.origin - box.center;
            Vector3<Real> rayOrigin
            {
                Dot(diff, box.axis[0]),
                Dot(diff, box.axis[1]),
                Dot(diff, box.axis[2])
            };
            Vector3<Real> rayDirection = Vector3<Real>
            {
                Dot(ray.direction, box.axis[0]),
                Dot(ray.direction, box.axis[1]),
                Dot(ray.direction, box.axis[2])
            };

            Result result;
            this->DoQuery(rayOrigin, rayDirection, box.extent, result);
            for (int i = 0; i < result.numPoints; ++i)
            {
                result.point[i] = ray.origin + result.lineParameter[i] * ray.direction;
            }
            return result;
        }
    };
}
