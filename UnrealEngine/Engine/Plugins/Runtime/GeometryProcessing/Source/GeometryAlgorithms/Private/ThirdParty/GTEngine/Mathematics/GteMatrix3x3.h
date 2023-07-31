// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMatrix.h>
#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>

namespace gte
{

// Template alias for convenience.
template <typename Real>
using Matrix3x3 = Matrix<3, 3, Real>;

// Geometric operations.
template <typename Real>
Matrix3x3<Real> Inverse(Matrix3x3<Real> const& M,
    bool* reportInvertibility = nullptr);

template <typename Real>
Matrix3x3<Real> Adjoint(Matrix3x3<Real> const& M);

template <typename Real>
Real Determinant(Matrix3x3<Real> const& M);

template <typename Real>
Real Trace(Matrix3x3<Real> const& M);


template <typename Real>
Matrix3x3<Real> Inverse(Matrix3x3<Real> const& M, bool* reportInvertibility)
{
    Matrix3x3<Real> inverse;
    bool invertible;
    Real c00 = M(1, 1)*M(2, 2) - M(1, 2)*M(2, 1);
    Real c10 = M(1, 2)*M(2, 0) - M(1, 0)*M(2, 2);
    Real c20 = M(1, 0)*M(2, 1) - M(1, 1)*M(2, 0);
    Real det = M(0, 0)*c00 + M(0, 1)*c10 + M(0, 2)*c20;
    if (det != (Real)0)
    {
        Real invDet = ((Real)1) / det;
        inverse = Matrix3x3<Real>
        {
            c00*invDet,
                (M(0, 2)*M(2, 1) - M(0, 1)*M(2, 2))*invDet,
                (M(0, 1)*M(1, 2) - M(0, 2)*M(1, 1))*invDet,
                c10*invDet,
                (M(0, 0)*M(2, 2) - M(0, 2)*M(2, 0))*invDet,
                (M(0, 2)*M(1, 0) - M(0, 0)*M(1, 2))*invDet,
                c20*invDet,
                (M(0, 1)*M(2, 0) - M(0, 0)*M(2, 1))*invDet,
                (M(0, 0)*M(1, 1) - M(0, 1)*M(1, 0))*invDet
        };
        invertible = true;
    }
    else
    {
        inverse.MakeZero();
        invertible = false;
    }

    if (reportInvertibility)
    {
        *reportInvertibility = invertible;
    }
    return inverse;
}

template <typename Real>
Matrix3x3<Real> Adjoint(Matrix3x3<Real> const& M)
{
    return Matrix3x3<Real>
    {
        M(1, 1)*M(2, 2) - M(1, 2)*M(2, 1),
            M(0, 2)*M(2, 1) - M(0, 1)*M(2, 2),
            M(0, 1)*M(1, 2) - M(0, 2)*M(1, 1),
            M(1, 2)*M(2, 0) - M(1, 0)*M(2, 2),
            M(0, 0)*M(2, 2) - M(0, 2)*M(2, 0),
            M(0, 2)*M(1, 0) - M(0, 0)*M(1, 2),
            M(1, 0)*M(2, 1) - M(1, 1)*M(2, 0),
            M(0, 1)*M(2, 0) - M(0, 0)*M(2, 1),
            M(0, 0)*M(1, 1) - M(0, 1)*M(1, 0)
    };
}

template <typename Real>
Real Determinant(Matrix3x3<Real> const& M)
{
    Real c00 = M(1, 1)*M(2, 2) - M(1, 2)*M(2, 1);
    Real c10 = M(1, 2)*M(2, 0) - M(1, 0)*M(2, 2);
    Real c20 = M(1, 0)*M(2, 1) - M(1, 1)*M(2, 0);
    Real det = M(0, 0)*c00 + M(0, 1)*c10 + M(0, 2)*c20;
    return det;
}

template <typename Real>
Real Trace(Matrix3x3<Real> const& M)
{
    Real trace = M(0, 0) + M(1, 1) + M(2, 2);
    return trace;
}


}
