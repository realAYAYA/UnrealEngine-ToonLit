// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.21.0 (2019/01/21)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteDistPointTriangle.h>
#include <ThirdParty/GTEngine/Mathematics/GteHypersphere.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteQuadraticField.h>
#include <ThirdParty/GTEngine/Mathematics/GteFIQuery.h>

namespace gte
{
    // Currently, only a dynamic query is supported.  A static query will
    // need to compute the intersection set of triangle and sphere.

    template <typename Real>
    class FIQuery<Real, Sphere3<Real>, Triangle3<Real>>
    {
    public:
        // The implementation for floating-point types.
        struct Result
        {
            // The cases are
            // 1. Objects initially overlapping.  The contactPoint is only one
            //    of infinitely many points in the overlap.
            //      intersectionType = -1
            //      contactTime = 0
            //      contactPoint = triangle point closest to sphere.center
            // 2. Objects initially separated but do not intersect later.  The
            //      contactTime and contactPoint are invalid.
            //      intersectionType = 0
            //      contactTime = 0
            //      contactPoint = (0,0,0)
            // 3. Objects initially separated but intersect later.
            //      intersectionType = +1
            //      contactTime = first time T > 0
            //      contactPoint = corresponding first contact
            int intersectionType;
            Real contactTime;
            Vector3<Real> contactPoint;
        };

        template <typename Dummy = Real>
        typename std::enable_if<!is_arbitrary_precision<Dummy>::value, Result>::type
        operator()(Sphere3<Real> const& sphere, Vector3<Real> const& sphereVelocity,
            Triangle3<Real> const& triangle, Vector3<Real> const& triangleVelocity)
        {
            Result result = { 0, (Real)0, { (Real)0, (Real)0, (Real)0 } };

            // Test for initial overlap or contact.
            DCPQuery<Real, Vector3<Real>, Triangle3<Real>> ptQuery;
            auto ptResult = ptQuery(sphere.center, triangle);
            Real rsqr = sphere.radius * sphere.radius;
            if (ptResult.sqrDistance <= rsqr)
            {
                result.intersectionType = (ptResult.sqrDistance < rsqr ? -1 : +1);
                result.contactTime = (Real)0;
                result.contactPoint = ptResult.closest;
                return result;
            }

            // To reach here, the sphere and triangle are initially separated.
            // Compute the velocity of the sphere relative to the triangle.
            Vector3<Real> V = sphereVelocity - triangleVelocity;
            Real sqrLenV = Dot(V, V);
            if (sqrLenV == (Real)0)
            {
                // The sphere and triangle are separated and the sphere is not
                // moving relative to the triangle, so there is no contact.
                // The 'result' is already set to the correct state for this
                // case.
                return result;
            }

            // Compute the triangle edge directions E[], the vector U normal
            // to the plane of the triangle,  and compute the normals to the
            // edges in the plane of the triangle.  TODO: For a nondeforming
            // triangle (or mesh of triangles), these quantities can all be
            // precomputed to reduce the computational cost of the query.  Add
            // another operator()-query that accepts the precomputed values.
            // TODO: When the triangle is deformable, these quantities must be
            // computed, either by the caller or here.  Optimize the code to
            // compute the quantities on-demand (i.e. only when they are
            // needed, but cache them for later use).
            Vector3<Real> E[3] =
            {
                triangle.v[1] - triangle.v[0],
                triangle.v[2] - triangle.v[1],
                triangle.v[0] - triangle.v[2]
            };
            Real sqrLenE[3] = { Dot(E[0], E[0]), Dot(E[1], E[1]), Dot(E[2], E[2]) };
            Vector3<Real> U = UnitCross(E[0], E[1]);
            Vector3<Real> ExU[3] =
            {
                Cross(E[0], U),
                Cross(E[1], U),
                Cross(E[2], U)
            };

            // Compute the vectors from the triangle vertices to the sphere
            // center.
            Vector3<Real> Delta[3] =
            {
                sphere.center - triangle.v[0],
                sphere.center - triangle.v[1],
                sphere.center - triangle.v[2]
            };

            // Determine where the sphere center is located relative to the
            // planes of the triangle offset faces of the sphere-swept volume.
            Real dotUDelta0 = Dot(U, Delta[0]);
            if (dotUDelta0 >= sphere.radius)
            {
                // The sphere is on the positive side of Dot(U,X-C) = r.  If
                // the sphere will contact the sphere-swept volume at a
                // triangular face, it can do so only on the face of the
                // aforementioned plane.
                Real dotUV = Dot(U, V);
                if (dotUV >= (Real)0)
                {
                    // The sphere is moving away from, or parallel to, the
                    // plane of the triangle.  The 'result' is already set to
                    // the correct state for this case.
                    return result;
                }

                Real tbar = (sphere.radius - dotUDelta0) / dotUV;
                bool foundContact = true;
                for (int i = 0; i < 3; ++i)
                {
                    Real phi = Dot(ExU[i], Delta[i]);
                    Real psi = Dot(ExU[i], V);
                    if (phi + psi * tbar > (Real)0)
                    {
                        foundContact = false;
                        break;
                    }
                }
                if (foundContact)
                {
                    result.intersectionType = 1;
                    result.contactTime = tbar;
                    result.contactPoint = sphere.center + tbar * sphereVelocity;
                    return result;
                }
            }
            else if (dotUDelta0 <= -sphere.radius)
            {
                // The sphere is on the positive side of Dot(-U,X-C) = r.  If
                // the sphere will contact the sphere-swept volume at a
                // triangular face, it can do so only on the face of the
                // aforementioned plane.
                Real dotUV = Dot(U, V);
                if (dotUV <= (Real)0)
                {
                    // The sphere is moving away from, or parallel to, the
                    // plane of the triangle.  The 'result' is already set to
                    // the correct state for this case.
                    return result;
                }

                Real tbar = (-sphere.radius - dotUDelta0) / dotUV;
                bool foundContact = true;
                for (int i = 0; i < 3; ++i)
                {
                    Real phi = Dot(ExU[i], Delta[i]);
                    Real psi = Dot(ExU[i], V);
                    if (phi + psi * tbar > (Real)0)
                    {
                        foundContact = false;
                        break;
                    }
                }
                if (foundContact)
                {
                    result.intersectionType = 1;
                    result.contactTime = tbar;
                    result.contactPoint = sphere.center + tbar * sphereVelocity;
                    return result;
                }
            }
            // else: The ray-sphere-swept-volume contact point (if any) cannot
            // be on a triangular face of the sphere-swept-volume.

            // The sphere is moving towards the slab between the two planes
            // of the sphere-swept volume triangular faces.  Determine whether
            // the ray intersects the half cylinders or sphere wedges of the
            // sphere-swept volume.

            // Test for contact with half cylinders of the sphere-swept
            // volume.  First, precompute some dot products required in the
            // computations.  TODO: Optimize the code to compute the quantities
            // on-demand (i.e. only when they are needed, but cache them for
            // later use).
            Real del[3], delp[3], nu[3];
            for (int im1 = 2, i = 0; i < 3; im1 = i++)
            {
                del[i] = Dot(E[i], Delta[i]);
                delp[im1] = Dot(E[im1], Delta[i]);
                nu[i] = Dot(E[i], V);
            }

            for (int i = 2, ip1 = 0; ip1 < 3; i = ip1++)
            {
                Vector3<Real> hatV = V - E[i] * nu[i] / sqrLenE[i];
                Real sqrLenHatV = Dot(hatV, hatV);
                if (sqrLenHatV > (Real)0)
                {
                    Vector3<Real> hatDelta = Delta[i] - E[i] * del[i] / sqrLenE[i];
                    Real alpha = -Dot(hatV, hatDelta);
                    if (alpha >= (Real)0)
                    {
                        Real sqrLenHatDelta = Dot(hatDelta, hatDelta);
                        Real beta = alpha * alpha - sqrLenHatV * (sqrLenHatDelta - rsqr);
                        if (beta >= (Real)0)
                        {
                            Real tbar = (alpha - std::sqrt(beta)) / sqrLenHatV;

                            Real mu = Dot(ExU[i], Delta[i]);
                            Real omega = Dot(ExU[i], hatV);
                            if (mu + omega * tbar >= (Real)0)
                            {
                                if (del[i] + nu[i] * tbar >= (Real)0)
                                {
                                    if (delp[i] + nu[i] * tbar <= (Real)0)
                                    {
                                        // The constraints are satisfied, so
                                        // tbar is the first time of contact.
                                        result.intersectionType = 1;
                                        result.contactTime = tbar;
                                        result.contactPoint = sphere.center + tbar * sphereVelocity;
                                        return result;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Test for contact with sphere wedges of the sphere-swept
            // volume.  We know that |V|^2 > 0 because of a previous
            // early-exit test.
            for (int im1 = 2, i = 0; i < 3; im1 = i++)
            {
                Real alpha = -Dot(V, Delta[i]);
                if (alpha >= (Real)0)
                {
                    Real sqrLenDelta = Dot(Delta[i], Delta[i]);
                    Real beta = alpha * alpha - sqrLenV * (sqrLenDelta - rsqr);
                    if (beta >= (Real)0)
                    {
                        Real tbar = (alpha - std::sqrt(beta)) / sqrLenV;
                        if (delp[im1] + nu[im1] * tbar >= (Real)0)
                        {
                            if (del[i] + nu[i] * tbar <= (Real)0)
                            {
                                // The constraints are satisfied, so tbar
                                // is the first time of contact.
                                result.intersectionType = 1;
                                result.contactTime = tbar;
                                result.contactPoint = sphere.center + tbar * sphereVelocity;
                                return result;
                            }
                        }
                    }
                }
            }

            // The ray and sphere-swept volume do not intersect, so the sphere
            // and triangle do not come into contact.  The 'result' is already
            // set to the correct state for this case.
            return result;
        }


        // The implementation for arbitrary-precision types.
        typedef typename QuadraticField<Real>::Element QFElement;

        struct ExactResult
        {
            // The cases are
            // 1. Objects initially overlapping.  The contactPoint is only one
            //    of infinitely many points in the overlap.
            //      intersectionType = -1
            //      contactTime = 0
            //      contactPoint = triangle point closest to sphere.center
            // 2. Objects initially separated but do not intersect later.  The
            //      contactTime and contactPoint are invalid.
            //      intersectionType = 0
            //      contactTime = 0
            //      contactPoint = (0,0,0)
            // 3. Objects initially separated but intersect later.
            //      intersectionType = +1
            //      contactTime = first time T > 0
            //      contactPoint = corresponding first contact
            int intersectionType;

            // The exact representation of the contact time and point.  To
            // convert to a floating-point type, use
            //   FloatType contactTime = field.Convert(result.contactTime);
            //   Vector3<FloatType> contactPoint
            //   {
            //       field.Convert(result.contactPoint[0]),
            //       field.Convert(result.contactPoint[1]),
            //       field.Convert(result.contactPoint[2])
            //   };
            QuadraticField<Real> field;
            QFElement contactTime;
            Vector3<QFElement> contactPoint;
        };

        template <typename Dummy = Real>
        typename std::enable_if<is_arbitrary_precision<Dummy>::value, ExactResult>::type
        operator()(Sphere3<Real> const& sphere, Vector3<Real> const& sphereVelocity,
            Triangle3<Real> const& triangle, Vector3<Real> const& triangleVelocity)
        {
            // The default constructors for the members of 'result' set their
            // own members to zero.
            ExactResult result;

            // Test for initial overlap or contact.
            DCPQuery<Real, Vector3<Real>, Triangle3<Real>> ptQuery;
            auto ptResult = ptQuery(sphere.center, triangle);
            Real rsqr = sphere.radius * sphere.radius;
            if (ptResult.sqrDistance <= rsqr)
            {
                // The values result.field, result.contactTime and
                // result.contactPoint[] are all zero, so we need only set
                // the result.contactPoint[].x values.
                result.intersectionType = (ptResult.sqrDistance < rsqr ? -1 : +1);
                for (int j = 0; j < 3; ++j)
                {
                    result.contactPoint[j].x = ptResult.closest[j];
                }
                return result;
            }

            // To reach here, the sphere and triangle are initially separated.
            // Compute the velocity of the sphere relative to the triangle.
            Vector3<Real> V = sphereVelocity - triangleVelocity;
            Real sqrLenV = Dot(V, V);
            if (sqrLenV == (Real)0)
            {
                // The sphere and triangle are separated and the sphere is not
                // moving relative to the triangle, so there is no contact.
                // The 'result' is already set to the correct state for this
                // case.
                return result;
            }

            // Compute the triangle edge directions E[], the vector U normal
            // to the plane of the triangle,  and compute the normals to the
            // edges in the plane of the triangle.  TODO: For a nondeforming
            // triangle (or mesh of triangles), these quantities can all be
            // precomputed to reduce the computational cost of the query.  Add
            // another operator()-query that accepts the precomputed values.
            // TODO: When the triangle is deformable, these quantities must be
            // computed, either by the caller or here.  Optimize the code to
            // compute the quantities on-demand (i.e. only when they are
            // needed, but cache them for later use).
            Vector3<Real> E[3] =
            {
                triangle.v[1] - triangle.v[0],
                triangle.v[2] - triangle.v[1],
                triangle.v[0] - triangle.v[2]
            };
            Real sqrLenE[3] = { Dot(E[0], E[0]), Dot(E[1], E[1]), Dot(E[2], E[2]) };
            // Use an unnormalized U for the plane of the triangle.  This
            // allows us to use quadratic fields for the comparisons of the
            // constraints.
            Vector3<Real> U = Cross(E[0], E[1]);
            Real sqrLenU = Dot(U, U);
            Vector3<Real> ExU[3] =
            {
                Cross(E[0], U),
                Cross(E[1], U),
                Cross(E[2], U)
            };

            // Compute the vectors from the triangle vertices to the sphere
            // center.
            Vector3<Real> Delta[3] =
            {
                sphere.center - triangle.v[0],
                sphere.center - triangle.v[1],
                sphere.center - triangle.v[2]
            };

            // Determine where the sphere center is located relative to the
            // planes of the triangle offset faces of the sphere-swept volume.
            QuadraticField<Real> ufield(sqrLenU);
            QFElement element(Dot(U, Delta[0]), -sphere.radius);
            if (ufield.GreaterThanOrEqualZero(element))
            {
                // The sphere is on the positive side of Dot(U,X-C) = r|U|.
                // If the sphere will contact the sphere-swept volume at a
                // triangular face, it can do so only on the face of the
                // aforementioned plane.
                Real dotUV = Dot(U, V);
                if (dotUV >= (Real)0)
                {
                    // The sphere is moving away from, or parallel to, the
                    // plane of the triangle.  The 'result' is already set
                    // to the correct state for this case.
                    return result;
                }

                bool foundContact = true;
                for (int i = 0; i < 3; ++i)
                {
                    Real phi = Dot(ExU[i], Delta[i]);
                    Real psi = Dot(ExU[i], V);
                    QFElement arg(psi * element.x - phi * dotUV, psi * element.y);
                    if (ufield.GreaterThanZero(arg))
                    {
                        foundContact = false;
                        break;
                    }
                }
                if (foundContact)
                {
                    result.intersectionType = 1;
                    result.field = ufield;
                    result.contactTime.x = -element.x / dotUV;
                    result.contactTime.y = -element.y / dotUV;
                    for (int j = 0; j < 3; ++j)
                    {
                        result.contactPoint[j].x = sphere.center[j] + result.contactTime.x * sphereVelocity[j];
                        result.contactPoint[j].y = result.contactTime.y * sphereVelocity[j];
                    }
                    return result;
                }
            }
            else
            {
                element.y = -element.y;
                if (ufield.LessThanOrEqualZero(element))
                {
                    // The sphere is on the positive side of Dot(-U,X-C) = r|U|.
                    // If the sphere will contact the sphere-swept volume at a
                    // triangular face, it can do so only on the face of the
                    // aforementioned plane.
                    Real dotUV = Dot(U, V);
                    if (dotUV <= (Real)0)
                    {
                        // The sphere is moving away from, or parallel to, the
                        // plane of the triangle.  The 'result' is already set
                        // to the correct state for this case.
                        return result;
                    }

                    bool foundContact = true;
                    for (int i = 0; i < 3; ++i)
                    {
                        Real phi = Dot(ExU[i], Delta[i]);
                        Real psi = Dot(ExU[i], V);
                        QFElement arg(phi * dotUV - psi * element.x, -psi * element.y);
                        if (ufield.GreaterThanZero(arg))
                        {
                            foundContact = false;
                            break;
                        }
                    }
                    if (foundContact)
                    {
                        result.intersectionType = 1;
                        result.field = ufield;
                        result.contactTime.x = -element.x / dotUV;
                        result.contactTime.y = -element.y / dotUV;
                        for (int j = 0; j < 3; ++j)
                        {
                            result.contactPoint[j].x = sphere.center[j] + result.contactTime.x * sphereVelocity[j];
                            result.contactPoint[j].y = result.contactTime.y * sphereVelocity[j];
                        }
                        return result;
                    }
                }
                // else: The ray-sphere-swept-volume contact point (if any)
                // cannot be on a triangular face of the sphere-swept-volume.
            }

            // The sphere is moving towards the slab between the two planes
            // of the sphere-swept volume triangular faces.  Determine whether
            // the ray intersects the half cylinders or sphere wedges of the
            // sphere-swept volume.

            // Test for contact with half cylinders of the sphere-swept
            // volume.  First, precompute some dot products required in the
            // computations.  TODO: Optimize the code to compute the quantities
            // on-demand (i.e. only when they are needed, but cache them for
            // later use).
            Real del[3], delp[3], nu[3];
            for (int im1 = 2, i = 0; i < 3; im1 = i++)
            {
                del[i] = Dot(E[i], Delta[i]);
                delp[im1] = Dot(E[im1], Delta[i]);
                nu[i] = Dot(E[i], V);
            }

            for (int i = 2, ip1 = 0; ip1 < 3; i = ip1++)
            {
                Vector3<Real> hatV = V - E[i] * nu[i] / sqrLenE[i];
                Real sqrLenHatV = Dot(hatV, hatV);
                if (sqrLenHatV > (Real)0)
                {
                    Vector3<Real> hatDelta = Delta[i] - E[i] * del[i] / sqrLenE[i];
                    Real alpha = -Dot(hatV, hatDelta);
                    if (alpha >= (Real)0)
                    {
                        Real sqrLenHatDelta = Dot(hatDelta, hatDelta);
                        Real beta = alpha * alpha - sqrLenHatV * (sqrLenHatDelta - rsqr);
                        if (beta >= (Real)0)
                        {
                            QuadraticField<Real> bfield(beta);
                            Real mu = Dot(ExU[i], Delta[i]);
                            Real omega = Dot(ExU[i], hatV);
                            QFElement arg0(mu * sqrLenHatV + omega * alpha, -omega);
                            if (bfield.GreaterThanOrEqualZero(arg0))
                            {
                                QFElement arg1(del[i] * sqrLenHatV + nu[i] * alpha, -nu[i]);
                                if (bfield.GreaterThanOrEqualZero(arg1))
                                {
                                    QFElement arg2(delp[i] * sqrLenHatV + nu[i] * alpha, -nu[i]);
                                    if (bfield.LessThanOrEqualZero(arg2))
                                    {
                                        result.intersectionType = 1;
                                        result.field = bfield;
                                        result.contactTime.x = alpha / sqrLenHatV;
                                        result.contactTime.y = (Real)-1 / sqrLenHatV;
                                        for (int j = 0; j < 3; ++j)
                                        {
                                            result.contactPoint[j].x = sphere.center[j] + result.contactTime.x * sphereVelocity[j];
                                            result.contactPoint[j].y = result.contactTime.y * sphereVelocity[j];
                                        }
                                        return result;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Test for contact with sphere wedges of the sphere-swept
            // volume.  We know that |V|^2 > 0 because of a previous
            // early-exit test.
            for (int im1 = 2, i = 0; i < 3; im1 = i++)
            {
                Real alpha = -Dot(V, Delta[i]);
                if (alpha >= (Real)0)
                {
                    Real sqrLenDelta = Dot(Delta[i], Delta[i]);
                    Real beta = alpha * alpha - sqrLenV * (sqrLenDelta - rsqr);
                    if (beta >= (Real)0)
                    {
                        QuadraticField<Real> bfield(beta);
                        QFElement arg0(delp[im1] * sqrLenV + nu[im1] * alpha, -nu[im1]);
                        if (bfield.GreaterThanOrEqualZero(arg0))
                        {
                            QFElement arg1(del[i] * sqrLenV + nu[i] * alpha, -nu[i]);
                            if (bfield.LessThanOrEqualZero(arg1))
                            {
                                result.intersectionType = 1;
                                result.field = bfield;
                                result.contactTime.x = alpha / sqrLenV;
                                result.contactTime.y = (Real)-1 / sqrLenV;
                                for (int j = 0; j < 3; ++j)
                                {
                                    result.contactPoint[j].x = sphere.center[j] + result.contactTime.x * sphereVelocity[j];
                                    result.contactPoint[j].y = result.contactTime.y * sphereVelocity[j];
                                }
                                return result;
                            }
                        }
                    }
                }
            }

            // The ray and sphere-swept volume do not intersect, so the sphere
            // and triangle do not come into contact.  The 'result' is already
            // set to the correct state for this case.
            return result;
        }
    };
}
