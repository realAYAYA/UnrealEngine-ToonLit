// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.4.0 (2016/11/08)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteTIQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteAlignedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteCylinder3.h>
#include <ThirdParty/GTEngine/Mathematics/GteLCPSolver.h>
#include <ThirdParty/GTEngine/Mathematics/GteMatrix3x3.h>

// The query considers the cylinder and box to be solids.

namespace gte
{

template <typename Real>
class TIQuery<Real, AlignedBox3<Real>, Cylinder3<Real>>
{
public:
    struct Result
    {
        bool intersect;
    };

    Result operator()(AlignedBox3<Real> const& box, Cylinder3<Real> const& cylinder);

private:
};


template <typename Real>
typename TIQuery<Real, AlignedBox3<Real>, Cylinder3<Real>>::Result
TIQuery<Real, AlignedBox3<Real>, Cylinder3<Real>>::operator()(
    AlignedBox3<Real> const& box, Cylinder3<Real> const& cylinder)
{
    Result result;

    // Translate the box and cylinder so that the box is in the first octant
    // where all points in the box have nonnegative components.
    Vector3<Real> corner = box.max - box.min;
    Vector3<Real> origin = cylinder.axis.origin - box.min;
    Vector3<Real> direction = cylinder.axis.direction;

    // Compute quantities to initialize q and M in the LCP.
    Real halfHeight = cylinder.height * (Real)0.5;
    Matrix3x3<Real> P = (Matrix3x3<Real>::Identity() - OuterProduct(direction, direction));
    Vector3<Real> C = -(P * origin);
    Real originDotDirection = Dot(origin, direction);

    Matrix<5, 3, Real> A;
    A.SetRow(0, { (Real)-1, (Real) 0, (Real) 0 });
    A.SetRow(1, { (Real) 0, (Real)-1, (Real) 0 });
    A.SetRow(2, { (Real) 0, (Real) 0, (Real)-1 });
    A.SetRow(3, direction);
    A.SetRow(4, -direction);

    Vector<5, Real> B =
    {
        -corner[0],
        -corner[1],
        -corner[2],
        originDotDirection - halfHeight,
        -originDotDirection - halfHeight
    };

    std::array<std::array<Real, 8>, 8> M;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            M[r][c] = P(r, c);
        }

        for (int c = 3, i = 0; c < 8; ++c, ++i)
        {
            M[r][c] = -A(i, r);
        }
    }

    for (int r = 3, i = 0; r < 8; ++r, ++i)
    {
        for (int c = 0; c < 3; ++c)
        {
            M[r][c] = A(i, c);
        }

        for (int c = 3; c < 8; ++c)
        {
            M[r][c] = (Real)0;
        }
    }

    std::array<Real, 8> q;
    for (int r = 0; r < 3; ++r)
    {
        q[r] = C[r];
    }

    for (int r = 3, i = 0; r < 8; ++r, ++i)
    {
        q[r] = -B[i];
    }

    std::array<Real, 8> w, z;
    LCPSolver<Real, 8> LCP;
    if (LCP.Solve(q, M, w, z))
    {
        Vector3<Real> zSolution{ z[0], z[1], z[2] };
        Vector3<Real> diff = zSolution - origin;
        Real qform = Dot(diff, P * diff);
        result.intersect = (qform <= cylinder.radius * cylinder.radius);
    }
    else
    {
        result.intersect = false;
    }

    return result;
}

}
