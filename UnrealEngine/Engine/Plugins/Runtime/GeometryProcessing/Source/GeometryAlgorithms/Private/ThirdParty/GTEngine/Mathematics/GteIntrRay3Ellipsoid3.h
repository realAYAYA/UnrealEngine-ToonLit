// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMatrix3x3.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrIntervals.h>
#include <ThirdParty/GTEngine/Mathematics/GteIntrLine3Ellipsoid3.h>

// The queries consider the ellipsoid to be a solid.

namespace gte
{

template <typename Real>
class TIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Ray3<Real> const& ray,
        Ellipsoid3<Real> const& ellipsoid);
};

template <typename Real>
class FIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>
    :
    public FIQuery<Real, Line3<Real>, Ellipsoid3<Real>>
{
public:
    struct Result
        :
        public FIQuery<Real, Line3<Real>, Ellipsoid3<Real>>::Result
    {
        // No additional information to compute.
    };

    Result operator()(Ray3<Real> const& ray,
        Ellipsoid3<Real> const& ellipsoid);

protected:
    void DoQuery(Vector3<Real> const& rayOrigin,
        Vector3<Real> const& rayDirection, Ellipsoid3<Real> const& ellipsoid,
        Result& result);
};


template <typename Real>
typename TIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>::Result
TIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>::operator()(
    Ray3<Real> const& ray, Ellipsoid3<Real> const& ellipsoid)
{
    // The ellipsoid is (X-K)^T*M*(X-K)-1 = 0 and the line is X = P+t*D.
    // Substitute the line equation into the ellipsoid equation to obtain
    // a quadratic equation Q(t) = a2*t^2 + 2*a1*t + a0 = 0, where
    // a2 = D^T*M*D, a1 = D^T*M*(P-K), and a0 = (P-K)^T*M*(P-K)-1.
    Result result;

    Matrix3x3<Real> M;
    ellipsoid.GetM(M);

    Vector3<Real> diff = ray.origin - ellipsoid.center;
    Vector3<Real> matDir = M*ray.direction;
    Vector3<Real> matDiff = M*diff;
    Real a2 = Dot(ray.direction, matDir);
    Real a1 = Dot(ray.direction, matDiff);
    Real a0 = Dot(diff, matDiff) - (Real)1;

    Real discr = a1*a1 - a0*a2;
    if (discr >= (Real)0)
    {
        // Test whether ray origin is inside ellipsoid.
        if (a0 <= (Real)0)
        {
            result.intersect = true;
        }
        else
        {
            // At this point, Q(0) = a0 > 0 and Q(t) has real roots.  It is
            // also the case that a2 > 0, since M is positive definite,
            // implying that D^T*M*D > 0 for any nonzero vector D.  Thus,
            // an intersection occurs only when Q'(0) < 0.
            result.intersect = (a1 < (Real)0);
        }
    }
    else
    {
        // No intersection if Q(t) has no real roots.
        result.intersect = false;
    }

    return result;
}

template <typename Real>
typename FIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>::Result
FIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>::operator()(
    Ray3<Real> const& ray, Ellipsoid3<Real> const& ellipsoid)
{
    Result result;
    DoQuery(ray.origin, ray.direction, ellipsoid, result);
    for (int i = 0; i < result.numIntersections; ++i)
    {
        result.point[i] = ray.origin + result.parameter[i] * ray.direction;
    }
    return result;
}

template <typename Real>
void FIQuery<Real, Ray3<Real>, Ellipsoid3<Real>>::DoQuery(
    Vector3<Real> const& rayOrigin, Vector3<Real> const& rayDirection,
    Ellipsoid3<Real> const& ellipsoid, Result& result)
{
    FIQuery<Real, Line3<Real>, Ellipsoid3<Real>>::DoQuery(rayOrigin,
        rayDirection, ellipsoid, result);

    if (result.intersect)
    {
        // The line containing the ray intersects the ellipsoid; the
        // t-interval is [t0,t1].  The ray intersects the capsule as long as
        // [t0,t1] overlaps the ray t-interval [0,+infinity).
        std::array<Real, 2> rayInterval =
        { (Real)0, std::numeric_limits<Real>::max() };
        FIQuery<Real, std::array<Real, 2>, std::array<Real, 2>> iiQuery;
        auto iiResult = iiQuery(result.parameter, rayInterval);
        if (iiResult.intersect)
        {
            result.numIntersections = iiResult.numIntersections;
            result.parameter = iiResult.overlap;
        }
        else
        {
            result.intersect = false;
            result.numIntersections = 0;
        }
    }
}


}
