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
// corresponding to z.  Exact interpolation is achieved by setting catmullRom
// to 'true', giving you the Catmull-Rom blending matrix.  If a smooth
// interpolation is desired, set catmullRom to 'false' to obtain B-spline
// blending.

namespace gte
{

template <typename Real>
class IntpTricubic3
{
public:
    // Construction.
    IntpTricubic3(int xBound, int yBound, int zBound, Real xMin,
        Real xSpacing, Real yMin, Real ySpacing, Real zMin, Real zSpacing,
        Real const* F, bool catmullRom);

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
    Real mBlend[4][4];
};


template <typename Real>
IntpTricubic3<Real>::IntpTricubic3(int xBound, int yBound, int zBound,
    Real xMin, Real xSpacing, Real yMin, Real ySpacing, Real zMin,
    Real zSpacing, Real const* F, bool catmullRom)
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
    // At least a 4x4x4 block of data points are needed to construct the
    // tricubic interpolation.
    LogAssert(mXBound >= 4 && mYBound >= 4 && mZBound >= 4 && mF,
        "Invalid input.");
    LogAssert(mXSpacing > (Real)0 && mYSpacing > (Real)0 &&
        mZSpacing > (Real)0, "Invalid input.");

    mXMax = mXMin + mXSpacing * static_cast<Real>(mXBound - 1);
    mInvXSpacing = ((Real)1) / mXSpacing;
    mYMax = mYMin + mYSpacing * static_cast<Real>(mYBound - 1);
    mInvYSpacing = ((Real)1) / mYSpacing;
    mZMax = mZMin + mZSpacing * static_cast<Real>(mZBound - 1);
    mInvZSpacing = ((Real)1) / mZSpacing;

    if (catmullRom)
    {
        mBlend[0][0] = (Real)0;
        mBlend[0][1] = (Real)-0.5;
        mBlend[0][2] = (Real)1;
        mBlend[0][3] = (Real)-0.5;
        mBlend[1][0] = (Real)1;
        mBlend[1][1] = (Real)0;
        mBlend[1][2] = (Real)-2.5;
        mBlend[1][3] = (Real)1.5;
        mBlend[2][0] = (Real)0;
        mBlend[2][1] = (Real)0.5;
        mBlend[2][2] = (Real)2;
        mBlend[2][3] = (Real)-1.5;
        mBlend[3][0] = (Real)0;
        mBlend[3][1] = (Real)0;
        mBlend[3][2] = (Real)-0.5;
        mBlend[3][3] = (Real)0.5;
    }
    else
    {
        mBlend[0][0] = (Real)1 / (Real)6;
        mBlend[0][1] = (Real)-3 / (Real)6;
        mBlend[0][2] = (Real)3 / (Real)6;
        mBlend[0][3] = (Real)-1 / (Real)6;;
        mBlend[1][0] = (Real)4 / (Real)6;
        mBlend[1][1] = (Real)0 / (Real)6;
        mBlend[1][2] = (Real)-6 / (Real)6;
        mBlend[1][3] = (Real)3 / (Real)6;
        mBlend[2][0] = (Real)1 / (Real)6;
        mBlend[2][1] = (Real)3 / (Real)6;
        mBlend[2][2] = (Real)3 / (Real)6;
        mBlend[2][3] = (Real)-3 / (Real)6;
        mBlend[3][0] = (Real)0 / (Real)6;
        mBlend[3][1] = (Real)0 / (Real)6;
        mBlend[3][2] = (Real)0 / (Real)6;
        mBlend[3][3] = (Real)1 / (Real)6;
    }
}

template <typename Real> inline
int IntpTricubic3<Real>::GetXBound() const
{
    return mXBound;
}

template <typename Real> inline
int IntpTricubic3<Real>::GetYBound() const
{
    return mYBound;
}

template <typename Real> inline
int IntpTricubic3<Real>::GetZBound() const
{
    return mZBound;
}

template <typename Real> inline
int IntpTricubic3<Real>::GetQuantity() const
{
    return mQuantity;
}

template <typename Real> inline
Real const* IntpTricubic3<Real>::GetF() const
{
    return mF;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetXMin() const
{
    return mXMin;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetXMax() const
{
    return mXMax;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetXSpacing() const
{
    return mXSpacing;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetYMin() const
{
    return mYMin;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetYMax() const
{
    return mYMax;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetYSpacing() const
{
    return mYSpacing;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetZMin() const
{
    return mZMin;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetZMax() const
{
    return mZMax;
}

template <typename Real> inline
Real IntpTricubic3<Real>::GetZSpacing() const
{
    return mZSpacing;
}

template <typename Real>
Real IntpTricubic3<Real>::operator()(Real x, Real y, Real z) const
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

    Real U[4];
    U[0] = (Real)1;
    U[1] = xIndex - ix;
    U[2] = U[1] * U[1];
    U[3] = U[1] * U[2];

    Real V[4];
    V[0] = (Real)1;
    V[1] = yIndex - iy;
    V[2] = V[1] * V[1];
    V[3] = V[1] * V[2];

    Real W[4];
    W[0] = (Real)1;
    W[1] = zIndex - iz;
    W[2] = W[1] * W[1];
    W[3] = W[1] * W[2];

    // Compute P = M*U, Q = M*V, R = M*W.
    Real P[4], Q[4], R[4];
    for (int row = 0; row < 4; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        R[row] = (Real)0;
        for (int col = 0; col < 4; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
            R[row] += mBlend[row][col] * W[col];
        }
    }

    // Compute the tensor product (M*U)(M*V)(M*W)*D where D is the 4x4x4
    // subimage containing (x,y,z).
    --ix;
    --iy;
    --iz;
    Real result = (Real)0;
    for (int slice = 0; slice < 4; ++slice)
    {
        int zClamp = iz + slice;
        if (zClamp < 0)
        {
            zClamp = 0;
        }
        else if (zClamp > mZBound - 1)
        {
            zClamp = mZBound - 1;
        }

        for (int row = 0; row < 4; ++row)
        {
            int yClamp = iy + row;
            if (yClamp < 0)
            {
                yClamp = 0;
            }
            else if (yClamp > mYBound - 1)
            {
                yClamp = mYBound - 1;
            }

            for (int col = 0; col < 4; ++col)
            {
                int xClamp = ix + col;
                if (xClamp < 0)
                {
                    xClamp = 0;
                }
                else if (xClamp > mXBound - 1)
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
Real IntpTricubic3<Real>::operator()(int xOrder, int yOrder, int zOrder,
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

    Real U[4], dx, xMult;
    switch (xOrder)
    {
    case 0:
        dx = xIndex - ix;
        U[0] = (Real)1;
        U[1] = dx;
        U[2] = dx * U[1];
        U[3] = dx * U[2];
        xMult = (Real)1;
        break;
    case 1:
        dx = xIndex - ix;
        U[0] = (Real)0;
        U[1] = (Real)1;
        U[2] = ((Real)2) * dx;
        U[3] = ((Real)3) * dx * dx;
        xMult = mInvXSpacing;
        break;
    case 2:
        dx = xIndex - ix;
        U[0] = (Real)0;
        U[1] = (Real)0;
        U[2] = (Real)2;
        U[3] = ((Real)6) * dx;
        xMult = mInvXSpacing * mInvXSpacing;
        break;
    case 3:
        U[0] = (Real)0;
        U[1] = (Real)0;
        U[2] = (Real)0;
        U[3] = (Real)6;
        xMult = mInvXSpacing * mInvXSpacing * mInvXSpacing;
        break;
    default:
        return (Real)0;
    }

    Real V[4], dy, yMult;
    switch (yOrder)
    {
    case 0:
        dy = yIndex - iy;
        V[0] = (Real)1;
        V[1] = dy;
        V[2] = dy * V[1];
        V[3] = dy * V[2];
        yMult = (Real)1;
        break;
    case 1:
        dy = yIndex - iy;
        V[0] = (Real)0;
        V[1] = (Real)1;
        V[2] = ((Real)2) * dy;
        V[3] = ((Real)3) * dy * dy;
        yMult = mInvYSpacing;
        break;
    case 2:
        dy = yIndex - iy;
        V[0] = (Real)0;
        V[1] = (Real)0;
        V[2] = (Real)2;
        V[3] = ((Real)6) * dy;
        yMult = mInvYSpacing * mInvYSpacing;
        break;
    case 3:
        V[0] = (Real)0;
        V[1] = (Real)0;
        V[2] = (Real)0;
        V[3] = (Real)6;
        yMult = mInvYSpacing * mInvYSpacing * mInvYSpacing;
        break;
    default:
        return (Real)0;
    }

    Real W[4], dz, zMult;
    switch (zOrder)
    {
    case 0:
        dz = zIndex - iz;
        W[0] = (Real)1;
        W[1] = dz;
        W[2] = dz * W[1];
        W[3] = dz * W[2];
        zMult = (Real)1;
        break;
    case 1:
        dz = zIndex - iz;
        W[0] = (Real)0;
        W[1] = (Real)1;
        W[2] = ((Real)2) * dz;
        W[3] = ((Real)3) * dz * dz;
        zMult = mInvZSpacing;
        break;
    case 2:
        dz = zIndex - iz;
        W[0] = (Real)0;
        W[1] = (Real)0;
        W[2] = (Real)2;
        W[3] = ((Real)6) * dz;
        zMult = mInvZSpacing * mInvZSpacing;
        break;
    case 3:
        W[0] = (Real)0;
        W[1] = (Real)0;
        W[2] = (Real)0;
        W[3] = (Real)6;
        zMult = mInvZSpacing * mInvZSpacing * mInvZSpacing;
        break;
    default:
        return (Real)0;
    }

    // Compute P = M*U, Q = M*V, and R = M*W.
    Real P[4], Q[4], R[4];
    for (int row = 0; row < 4; ++row)
    {
        P[row] = (Real)0;
        Q[row] = (Real)0;
        R[row] = (Real)0;
        for (int col = 0; col < 4; ++col)
        {
            P[row] += mBlend[row][col] * U[col];
            Q[row] += mBlend[row][col] * V[col];
            R[row] += mBlend[row][col] * W[col];
        }
    }

    // Compute the tensor product (M*U)(M*V)(M*W)*D where D is the 4x4x4
    // subimage containing (x,y,z).
    --ix;
    --iy;
    --iz;
    Real result = (Real)0;
    for (int slice = 0; slice < 4; ++slice)
    {
        int zClamp = iz + slice;
        if (zClamp < 0)
        {
            zClamp = 0;
        }
        else if (zClamp > mZBound - 1)
        {
            zClamp = mZBound - 1;
        }

        for (int row = 0; row < 4; ++row)
        {
            int yClamp = iy + row;
            if (yClamp < 0)
            {
                yClamp = 0;
            }
            else if (yClamp > mYBound - 1)
            {
                yClamp = mYBound - 1;
            }

            for (int col = 0; col < 4; ++col)
            {
                int xClamp = ix + col;
                if (xClamp < 0)
                {
                    xClamp = 0;
                }
                else if (xClamp > mXBound - 1)
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
