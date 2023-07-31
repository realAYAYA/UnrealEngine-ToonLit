// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>

// The interpolator is for uniformly spaced(x,y z)-values.  The input samples
// must be stored in lexicographical order to represent f(x,y,z); that is,
// F[c + xBound*(r + yBound*s)] corresponds to f(x,y,z), where c is the index
// corresponding to x, r is the index corresponding to y, and s is the index
// corresponding to z.

namespace gte
{

template <typename Real>
class IntpTrilinear3
{
public:
    // Construction.
    IntpTrilinear3(int xBound, int yBound, int zBound, Real xMin,
        Real xSpacing, Real yMin, Real ySpacing, Real zMin, Real zSpacing,
        Real const* F);

    // Member access.
    inline int GetXBound() const;
    inline int GetYBound() const;
    inline int GetZBound() const;
    inline int GetQuantity() const;
    inline Real const* GetF() const;
    inline Real GetXMin() const;
    inline Real GetXMax() const;
    inline Real GetXSpacing() const;
    inline Real GetYMin() const;
    inline Real GetYMax() const;
    inline Real GetYSpacing() const;
    inline Real GetZMin() const;
    inline Real GetZMax() const;
    inline Real GetZSpacing() const;

    // Evaluate the function and its derivatives.  The functions clamp the
    // inputs to xmin <= x <= xmax, ymin <= y <= ymax, and zmin <= z <= zmax.
    // The first operator is for function evaluation.  The second operator is
    // for function or derivative evaluations.  The xOrder argument is the
    // order of the x-derivative, the yOrder argument is the order of the
    // y-derivative, and the zOrder argument is the order of the z-derivative.
    // All orders are zero to get the function value itself.
    Real operator()(Real x, Real y, Real z) const;
    Real operator()(int xOrder, int yOrder, int zOrder, Real x, Real y,
        Real z) const;

private:
    int mXBound, mYBound, mZBound, mQuantity;
    Real mXMin, mXMax, mXSpacing, mInvXSpacing;
    Real mYMin, mYMax, mYSpacing, mInvYSpacing;
    Real mZMin, mZMax, mZSpacing, mInvZSpacing;
    Real const* mF;
    Real mBlend[2][2];
};


template <typename Real>
IntpTrilinear3<Real>::IntpTrilinear3(int xBound, int yBound, int zBound,
    Real xMin, Real xSpacing, Real yMin, Real ySpacing, Real zMin,
    Real zSpacing, Real const* F)
    :
    mXBound(xBound),
    mYBound(yBound),
    mZBound(zBound),
    mQuantity(xBound * yBound * zBound),
    mXMin(xMin),
    mXSpacing(xSpacing),
    mYMin(yMin),
    mYSpacing(ySpacing),
    mZMin(zMin),
    mZSpacing(zSpacing),
    mF(F)
{
    // At least a 2x2x2 block of data points are needed to construct the
    // trilinear interpolation.
    LogAssert(mXBound >= 2 && mYBound >= 2 && mZBound >= 2 && mF,
        "Invalid input.");
    LogAssert(mXSpacing > (Real)0 && mYSpacing > (Real)0 &&
        mZSpacing > (Real)0, "Invalid input.");

    mXMax = mXMin + mXSpacing * static_cast<Real>(mXBound - 1);
    mInvXSpacing = ((Real)1) / mXSpacing;
    mYMax = mYMin + mYSpacing * static_cast<Real>(mYBound - 1);
    mInvYSpacing = ((Real)1) / mYSpacing;
    mZMax = mZMin + mZSpacing * static_cast<Real>(mZBound - 1);
    mInvZSpacing = ((Real)1) / mZSpacing;

    mBlend[0][0] = (Real)1;
    mBlend[0][1] = (Real)-1;
    mBlend[1][0] = (Real)0;
    mBlend[1][1] = (Real)1;
}

template <typename Real> inline
int IntpTrilinear3<Real>::GetXBound() const
{
    return mXBound;
}

template <typename Real> inline
int IntpTrilinear3<Real>::GetYBound() const
{
    return mYBound;
}

template <typename Real> inline
int IntpTrilinear3<Real>::GetZBound() const
{
    return mZBound;
}

template <typename Real> inline
int IntpTrilinear3<Real>::GetQuantity() const
{
    return mQuantity;
}

template <typename Real> inline
Real const* IntpTrilinear3<Real>::GetF() const
{
    return mF;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetXMin() const
{
    return mXMin;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetXMax() const
{
    return mXMax;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetXSpacing() const
{
    return mXSpacing;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetYMin() const
{
    return mYMin;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetYMax() const
{
    return mYMax;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetYSpacing() const
{
    return mYSpacing;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetZMin() const
{
    return mZMin;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetZMax() const
{
    return mZMax;
}

template <typename Real> inline
Real IntpTrilinear3<Real>::GetZSpacing() const
{
    return mZSpacing;
}

template <typename Real>
Real IntpTrilinear3<Real>::operator()(Real x, Real y, Real z) const
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

    // Compute z-index and clamp to image.
    Real zIndex = (z - mZMin) * mInvZSpacing;
    int iz = static_cast<int>(zIndex);
    if (iz < 0)
    {
        iz = 0;
    }
    else if (iz >= mZBound)
    {
        iz = mZBound - 1;
    }

    Real U[2];
    U[0] = (Real)1;
    U[1] = xIndex - ix;

    Real V[2];
    V[0] = (Real)1;
    V[1] = yIndex - iy;

    Real W[2];
    W[0] = (Real)1;
    W[1] = zIndex - iz;

    // Compute P = M*U, Q = M*V, R = M*W.
    Real P[2], Q[2], R[2];
    for (int row = 0; row < 2; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        R[row] = (Real)0;
        for (int col = 0; col < 2; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
            R[row] += mBlend[row][col] * W[col];
        }
    }

    // compute the tensor product (M*U)(M*V)(M*W)*D where D is the 2x2x2
    // subimage containing (x,y,z)
    Real result = (Real)0;
    for (int slice = 0; slice < 2; ++slice)
    {
        int zClamp = iz + slice;
        if (zClamp >= mZBound)
        {
            zClamp = mZBound - 1;
        }

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

                result += P[col] * Q[row] * R[slice] *
                    mF[xClamp + mXBound * (yClamp + mYBound * zClamp)];
            }
        }
    }

    return result;
}

template <typename Real>
Real IntpTrilinear3<Real>::operator()(int xOrder, int yOrder, int zOrder,
    Real x, Real y, Real z) const
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

    // Compute z-index and clamp to image.
    Real zIndex = (z - mZMin) * mInvZSpacing;
    int iz = static_cast<int>(zIndex);
    if (iz < 0)
    {
        iz = 0;
    }
    else if (iz >= mZBound)
    {
        iz = mZBound - 1;
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

    Real W[2], dz, zMult;
    switch (zOrder)
    {
    case 0:
        dz = zIndex - iz;
        W[0] = (Real)1;
        W[1] = dz;
        zMult = (Real)1;
        break;
    case 1:
        dz = zIndex - iz;
        W[0] = (Real)0;
        W[1] = (Real)1;
        zMult = mInvZSpacing;
        break;
    default:
        return (Real)0;
    }

    // Compute P = M*U, Q = M*V, and R = M*W.
    Real P[2], Q[2], R[2];
    for (int row = 0; row < 2; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        R[row] = (Real)0;
        for (int col = 0; col < 2; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
            R[row] += mBlend[row][col] * W[col];
        }
    }

    // Compute the tensor product (M*U)(M*V)(M*W)*D where D is the 2x2x2
    // subimage containing (x,y,z).
    Real result = (Real)0;
    for (int slice = 0; slice < 2; ++slice)
    {
        int zClamp = iz + slice;
        if (zClamp >= mZBound)
        {
            zClamp = mZBound - 1;
        }

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

                result += P[col] * Q[row] * R[slice] *
                    mF[xClamp + mXBound * (yClamp + mYBound * zClamp)];
            }
        }
    }
    result *= xMult * yMult * zMult;

    return result;
}


}
