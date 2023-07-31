// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

// Linear interpolation of a network of triangles whose vertices are of the
// form (x,y,f(x,y)).  The function samples are F[i] and represent
// f(x[i],y[i]), where i is the index of the input vertex (x[i],y[i]) to
// Delaunay2.
//
// The TriangleMesh interface must support the following:
//   bool GetIndices(int, std::array<int, 3>&) const;
//   bool GetBarycentrics(int, Vector2<Real> const&,
//       std::array<Real, 3>&) const;
//   int GetContainingTriangle(Vector2<Real> const&) const;

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>

namespace gte
{

template <typename Real, typename TriangleMesh>
class IntpLinearNonuniform2
{
public:
    // Construction.
    IntpLinearNonuniform2(TriangleMesh const& mesh, Real const* F);

    // Linear interpolation.  The return value is 'true' if and only if the
    // input point P is in the convex hull of the input vertices, in which
    // case the interpolation is valid.
    bool operator()(Vector2<Real> const& P, Real& F) const;

private:
    TriangleMesh const* mMesh;
    Real const* mF;
};


template <typename Real, typename TriangleMesh>
IntpLinearNonuniform2<Real, TriangleMesh>::IntpLinearNonuniform2(
    TriangleMesh const& mesh, Real const* F)
    :
    mMesh(&mesh),
    mF(F)
{
    LogAssert(mF != nullptr, "Invalid input.");
}

template <typename Real, typename TriangleMesh>
bool IntpLinearNonuniform2<Real, TriangleMesh>::operator()(
    Vector2<Real> const& P, Real& F) const
{
    int t = mMesh->GetContainingTriangle(P);
    if (t == -1)
    {
        // The point is outside the triangulation.
        return false;
    }

    // Get the barycentric coordinates of P with respect to the triangle,
    // P = b0*V0 + b1*V1 + b2*V2, where b0 + b1 + b2 = 1.
    std::array<Real, 3> bary;
    if (!mMesh->GetBarycentrics(t, P, bary))
    {
        LogWarning("P is in a needle-like or degenerate triangle.");
        return false;
    }

    // The result is a barycentric combination of function values.
    std::array<int, 3> indices;
    mMesh->GetIndices(t, indices);
    F = bary[0] * mF[indices[0]] + bary[1] * mF[indices[1]] +
        bary[2] * mF[indices[2]];
    return true;
}


}
