// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteRay.h>
#include <ThirdParty/GTEngine/Mathematics/GteTriangle.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>

namespace gte
{

template <typename Real>
class TIQuery<Real, Ray3<Real>, Triangle3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(Ray3<Real> const& ray, Triangle3<Real> const& triangle);
};

template <typename Real>
class FIQuery<Real, Ray3<Real>, Triangle3<Real>>
{
public:
    struct Result
    {
        Result();

        bool intersect;
        Real parameter;
        Real triangleBary[3];
        Vector3<Real> point;
    };

    Result operator()(Ray3<Real> const& ray, Triangle3<Real> const& triangle);
};


template <typename Real>
typename TIQuery<Real, Ray3<Real>, Triangle3<Real>>::Result
TIQuery<Real, Ray3<Real>, Triangle3<Real>>::operator()(
    Ray3<Real> const& ray, Triangle3<Real> const& triangle)
{
    Result result;

    // Compute the offset origin, edges, and normal.
    Vector3<Real> diff = ray.origin - triangle.v[0];
    Vector3<Real> edge1 = triangle.v[1] - triangle.v[0];
    Vector3<Real> edge2 = triangle.v[2] - triangle.v[0];
    Vector3<Real> normal = Cross(edge1, edge2);

    // Solve Q + t*D = b1*E1 + b2*E2 (Q = kDiff, D = ray direction,
    // E1 = edge1, E2 = edge2, N = Cross(E1,E2)) by
    //   |Dot(D,N)|*b1 = sign(Dot(D,N))*Dot(D,Cross(Q,E2))
    //   |Dot(D,N)|*b2 = sign(Dot(D,N))*Dot(D,Cross(E1,Q))
    //   |Dot(D,N)|*t = -sign(Dot(D,N))*Dot(Q,N)
    Real DdN = Dot(ray.direction, normal);
    Real sign;
    if (DdN > (Real)0)
    {
        sign = (Real)1;
    }
    else if (DdN < (Real)0)
    {
        sign = (Real)-1;
        DdN = -DdN;
    }
    else
    {
        // Ray and triangle are parallel, call it a "no intersection"
        // even if the ray does intersect.
        result.intersect = false;
        return result;
    }

    Real DdQxE2 = sign*DotCross(ray.direction, diff, edge2);
    if (DdQxE2 >= (Real)0)
    {
        Real DdE1xQ = sign*DotCross(ray.direction, edge1, diff);
        if (DdE1xQ >= (Real)0)
        {
            if (DdQxE2 + DdE1xQ <= DdN)
            {
                // Line intersects triangle, check whether ray does.
                Real QdN = -sign*Dot(diff, normal);
                if (QdN >= (Real)0)
                {
                    // Ray intersects triangle.
                    result.intersect = true;
                    return result;
                }
                // else: t < 0, no intersection
            }
            // else: b1+b2 > 1, no intersection
        }
        // else: b2 < 0, no intersection
    }
    // else: b1 < 0, no intersection

    result.intersect = false;
    return result;
}

template <typename Real>
FIQuery<Real, Ray3<Real>, Triangle3<Real>>::Result::Result()
    :
    parameter((Real)0)
{
    triangleBary[0] = (Real)0;
    triangleBary[1] = (Real)0;
    triangleBary[2] = (Real)0;
}

template <typename Real>
typename FIQuery<Real, Ray3<Real>, Triangle3<Real>>::Result
FIQuery<Real, Ray3<Real>, Triangle3<Real>>::operator()(
    Ray3<Real> const& ray, Triangle3<Real> const& triangle)
{
    Result result;

    // Compute the offset origin, edges, and normal.
    Vector3<Real> diff = ray.origin - triangle.v[0];
    Vector3<Real> edge1 = triangle.v[1] - triangle.v[0];
    Vector3<Real> edge2 = triangle.v[2] - triangle.v[0];
    Vector3<Real> normal = Cross(edge1, edge2);

    // Solve Q + t*D = b1*E1 + b2*E2 (Q = kDiff, D = ray direction,
    // E1 = edge1, E2 = edge2, N = Cross(E1,E2)) by
    //   |Dot(D,N)|*b1 = sign(Dot(D,N))*Dot(D,Cross(Q,E2))
    //   |Dot(D,N)|*b2 = sign(Dot(D,N))*Dot(D,Cross(E1,Q))
    //   |Dot(D,N)|*t = -sign(Dot(D,N))*Dot(Q,N)
    Real DdN = Dot(ray.direction, normal);
    Real sign;
    if (DdN > (Real)0)
    {
        sign = (Real)1;
    }
    else if (DdN < (Real)0)
    {
        sign = (Real)-1;
        DdN = -DdN;
    }
    else
    {
        // Ray and triangle are parallel, call it a "no intersection"
        // even if the ray does intersect.
        result.intersect = false;
        return result;
    }

    Real DdQxE2 = sign*DotCross(ray.direction, diff, edge2);
    if (DdQxE2 >= (Real)0)
    {
        Real DdE1xQ = sign*DotCross(ray.direction, edge1, diff);
        if (DdE1xQ >= (Real)0)
        {
            if (DdQxE2 + DdE1xQ <= DdN)
            {
                // Line intersects triangle, check whether ray does.
                Real QdN = -sign*Dot(diff, normal);
                if (QdN >= (Real)0)
                {
                    // Ray intersects triangle.
                    result.intersect = true;
                    Real inv = ((Real)1) / DdN;
                    result.parameter = QdN*inv;
                    result.triangleBary[1] = DdQxE2*inv;
                    result.triangleBary[2] = DdE1xQ*inv;
                    result.triangleBary[0] = (Real)1 - result.triangleBary[1]
                        - result.triangleBary[2];
                    result.point = ray.origin +
                        result.parameter * ray.direction;
                    return result;
                }
                // else: t < 0, no intersection
            }
            // else: b1+b2 > 1, no intersection
        }
        // else: b2 < 0, no intersection
    }
    // else: b1 < 0, no intersection

    result.intersect = false;
    return result;
}


}
