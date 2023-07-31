// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>

// The interpolator is for uniformly spaced (x,y)-values.  The input samples
// F must be stored in row-major order to represent f(x,y); that is,
// F[c + xBound*r] corresponds to f(x,y), where c is the index corresponding
// to x and r is the index corresponding to y.

namespace gte
{

template <typename Real>
class IntpBilinear2
{
public:
    // Construction.
    IntpBilinear2(int xBound, int yBound, Real xMin, Real xSpacing,
        Real yMin, Real ySpacing, Real const* F);

    // Member access.
    inline int GetXBound() const;
    inline int GetYBound() const;
    inline int GetQuantity() const;
    inline Real const* GetF() const;
    inline Real GetXMin() const;
    inline Real GetXMax() const;
    inline Real GetXSpacing() const;
    inline Real GetYMin() const;
    inline Real GetYMax() const;
    inline Real GetYSpacing() const;

    // Evaluate the function and its derivatives.  The functions clamp the
    // inputs to xmin <= x <= xmax and ymin <= y <= ymax.  The first operator
    // is for function evaluation.  The second operator is for function or
    // derivative evaluations.  The xOrder argument is the order of the
    // x-derivative and the yOrder argument is the order of the y-derivative.
    // Both orders are zero to get the function value itself.
    Real operator()(Real x, Real y) const;
    Real operator()(int xOrder, int yOrder, Real x, Real y) const;

private:
    int mXBound, mYBound, mQuantity;
    Real mXMin, mXMax, mXSpacing, mInvXSpacing;
    Real mYMin, mYMax, mYSpacing, mInvYSpacing;
    Real const* mF;
    Real mBlend[2][2];
};


template <typename Real>
IntpBilinear2<Real>::IntpBilinear2(int xBound, int yBound, Real xMin,
    Real xSpacing, Real yMin, Real ySpacing, Real const* F)
    :
    mXBound(xBound),
    mYBound(yBound),
    mQuantity(xBound * yBound),
    mXMin(xMin),
    mXSpacing(xSpacing),
    mYMin(yMin),
    mYSpacing(ySpacing),
    mF(F)
{
    // At least a 2x2 block of data points are needed.
    LogAssert(mXBound >= 2 && mYBound >= 2 && mF, "Invalid input.");
    LogAssert(mXSpacing > (Real)0 && mYSpacing > (Real)0, "Invalid input.");

    mXMax = mXMin + mXSpacing * static_cast<Real>(mXBound - 1);
    mInvXSpacing = ((Real)1) / mXSpacing;
    mYMax = mYMin + mYSpacing * static_cast<Real>(mYBound - 1);
    mInvYSpacing = ((Real)1) / mYSpacing;

    mBlend[0][0] = (Real)1;
    mBlend[0][1] = (Real)-1;
    mBlend[1][0] = (Real)0;
    mBlend[1][1] = (Real)1;
}

template <typename Real> inline
int IntpBilinear2<Real>::GetXBound() const
{
    return mXBound;
}

template <typename Real> inline
int IntpBilinear2<Real>::GetYBound() const
{
    return mYBound;
}

template <typename Real> inline
int IntpBilinear2<Real>::GetQuantity() const
{
    return mQuantity;
}

template <typename Real> inline
Real const* IntpBilinear2<Real>::GetF() const
{
    return mF;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetXMin() const
{
    return mXMin;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetXMax() const
{
    return mXMax;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetXSpacing() const
{
    return mXSpacing;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetYMin() const
{
    return mYMin;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetYMax() const
{
    return mYMax;
}

template <typename Real> inline
Real IntpBilinear2<Real>::GetYSpacing() const
{
    return mYSpacing;
}

template <typename Real>
Real IntpBilinear2<Real>::operator()(Real x, Real y) const
{
    // Compute x-index and clamp to image.
    Real xIndex = (x - mXMin) * mInvXSpacing;
    int ix = static_cast<int>(xIndex);
    if (ix < 0)
    {
        ix = 0;
    }
    else if (ix >= mXBound)
    {
        ix = mXBound - 1;
    }

    // Compute y-index and clamp to image.
    Real yIndex = (y - mYMin) * mInvYSpacing;
    int iy = static_cast<int>(yIndex);
    if (iy < 0)
    {
        iy = 0;
    }
    else if (iy >= mYBound)
    {
        iy = mYBound - 1;
    }

    Real U[2];
    U[0] = (Real)1;
    U[1] = xIndex - ix;

    Real V[2];
    V[0] = (Real)1.0;
    V[1] = yIndex - iy;

    // Compute P = M*U and Q = M*V.
    Real P[2], Q[2];
    for (int row = 0; row < 2; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        for (int col = 0; col < 2; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
        }
    }

    // Compute (M*U)^t D (M*V) where D is the 2x2 subimage containing (x,y).
    Real result = (Real)0;
    for (int row = 0; row < 2; ++row)
    {
        int yClamp = iy + row;
        if (yClamp >= mYBound)
        {
            yClamp = mYBound - 1;
        }

        for (int col = 0; col < 2; ++col)
        {
            int xClamp = ix + col;
            if (xClamp >= mXBound)
            {
                xClamp = mXBound - 1;
            }

            result += P[col] * Q[row] * mF[xClamp + mXBound * yClamp];
        }
    }

    return result;
}

template <typename Real>
Real IntpBilinear2<Real>::operator()(int xOrder, int yOrder, Real x, Real y)
const
{
    // Compute x-index and clamp to image.
    Real xIndex = (x - mXMin) * mInvXSpacing;
    int ix = static_cast<int>(xIndex);
    if (ix < 0)
    {
        ix = 0;
    }
    else if (ix >= mXBound)
    {
        ix = mXBound - 1;
    }

    // Compute y-index and clamp to image.
    Real yIndex = (y - mYMin) * mInvYSpacing;
    int iy = static_cast<int>(yIndex);
    if (iy < 0)
    {
        iy = 0;
    }
    else if (iy >= mYBound)
    {
        iy = mYBound - 1;
    }

    Real U[2], dx, xMult;
    switch (xOrder)
    {
    case 0:
        dx = xIndex - ix;
        U[0] = (Real)1;
        U[1] = dx;
        xMult = (Real)1;
        break;
    case 1:
        dx = xIndex - ix;
        U[0] = (Real)0;
        U[1] = (Real)1;
        xMult = mInvXSpacing;
        break;
    default:
        return (Real)0;
    }

    Real V[2], dy, yMult;
    switch (yOrder)
    {
    case 0:
        dy = yIndex - iy;
        V[0] = (Real)1;
        V[1] = dy;
        yMult = (Real)1;
        break;
    case 1:
        dy = yIndex - iy;
        V[0] = (Real)0;
        V[1] = (Real)1;
        yMult = mInvYSpacing;
        break;
    default:
        return (Real)0;
    }

    // Compute P = M*U and Q = M*V.
    Real P[2], Q[2];
    for (int row = 0; row < 2; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        for (int col = 0; col < 2; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
        }
    }

    // Compute (M*U)^t D (M*V) where D is the 2x2 subimage containing (x,y).
    Real result = (Real)0;
    for (int row = 0; row < 2; ++row)
    {
        int yClamp = iy + row;
        if (yClamp >= mYBound)
        {
            yClamp = mYBound - 1;
        }

        for (int col = 0; col < 2; ++col)
        {
            int xClamp = ix + col;
            if (xClamp >= mXBound)
            {
                xClamp = mXBound - 1;
            }

            result += P[col] * Q[row] * mF[xClamp + mXBound * yClamp];
        }
    }
    result *= xMult * yMult;

    return result;
}


}
