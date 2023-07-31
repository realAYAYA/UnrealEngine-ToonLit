// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteArray2.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteParametricCurve.h>

namespace gte
{

template <int N, typename Real>
class BezierCurve : public ParametricCurve<N, Real>
{
public:
    // Construction and destruction.  The number of control points must be
    // degree + 1.  This object copies the input array.  The domain is t
    // in [0,1].  To validate construction, create an object as shown:
    //     BezierCurve<N, Real> curve(parameters);
    //     if (!curve) { <constructor failed, handle accordingly>; }
    virtual ~BezierCurve();
    BezierCurve(int degree, Vector<N, Real> const* controls);

    // Member access.
    inline int GetDegree() const;
    inline int GetNumControls() const;
    inline Vector<N, Real> const* GetControls() const;

    // Evaluation of the curve.  The function supports derivative calculation
    // through order 3; that is, maxOrder <= 3 is required.  If you want
    // only the position, pass in maxOrder of 0.  If you want the position and
    // first derivative, pass in maxOrder of 1, and so on.  The output
    // 'values' are ordered as: position, first derivative, second derivative,
    // third derivative.
    virtual void Evaluate(Real t, unsigned int maxOrder,
        Vector<N, Real> values[4]) const;

protected:
    // Support for Evaluate(...).
    Vector<N, Real> Compute(Real t, Real omt, int order) const;

    int mDegree, mNumControls;
    std::vector<Vector<N, Real>> mControls[4];
    Array2<Real> mChoose;
};


template <int N, typename Real>
BezierCurve<N, Real>::~BezierCurve()
{
}

template <int N, typename Real>
BezierCurve<N, Real>::BezierCurve(int degree, Vector<N, Real> const* controls)
    :
    ParametricCurve<N, Real>((Real)0, (Real)1),
    mDegree(degree),
    mNumControls(degree + 1),
    mChoose(mNumControls, mNumControls)
{
    if (degree < 2 || !controls)
    {
        LogError("Invalid input.");
        return;
    }

    // Copy the controls.
    mControls[0].resize(mNumControls);
    std::copy(controls, controls + mNumControls, mControls[0].begin());

    // Compute first-order differences.
    mControls[1].resize(mNumControls - 1);
    for (int i = 0; i < mNumControls - 1; ++i)
    {
        mControls[1][i] = mControls[0][i + 1] - mControls[0][i];
    }

    // Compute second-order differences.
    mControls[2].resize(mNumControls - 2);
    for (int i = 0; i < mNumControls - 2; ++i)
    {
        mControls[2][i] = mControls[1][i + 1] - mControls[1][i];
    }

    // Compute third-order differences.
    if (degree >= 3)
    {
        mControls[3].resize(mNumControls - 3);
        for (int i = 0; i < mNumControls - 3; ++i)
        {
            mControls[3][i] = mControls[2][i + 1] - mControls[2][i];
        }
    }

    // Compute combinatorial values Choose(n,k) and store in mChoose[n][k].
    // The values mChoose[r][c] are invalid for r < c; that is, we use only
    // the entries for r >= c.
    mChoose[0][0] = (Real)1;
    mChoose[1][0] = (Real)1;
    mChoose[1][1] = (Real)1;
    for (int i = 2; i <= mDegree; ++i)
    {
        mChoose[i][0] = (Real)1;
        mChoose[i][i] = (Real)1;
        for (int j = 1; j < i; ++j)
        {
            mChoose[i][j] = mChoose[i - 1][j - 1] + mChoose[i - 1][j];
        }
    }

    this->mConstructed = true;
}

template <int N, typename Real> inline
int BezierCurve<N, Real>::GetDegree() const
{
    return mDegree;
}

template <int N, typename Real> inline
int BezierCurve<N, Real>::GetNumControls() const
{
    return mNumControls;
}

template <int N, typename Real> inline
Vector<N, Real> const* BezierCurve<N, Real>::GetControls() const
{
    return &mControls[0][0];
}

template <int N, typename Real>
void BezierCurve<N, Real>::Evaluate(Real t, unsigned int maxOrder,
    Vector<N, Real> values[4]) const
{
    if (!this->mConstructed)
    {
        // Errors were already generated during construction.
        for (unsigned int order = 0; order < 4; ++order)
        {
            values[order].MakeZero();
        }
        return;
    }

    // Compute position.
    Real omt = (Real)1 - t;
    values[0] = Compute(t, omt, 0);
    if (maxOrder >= 1)
    {
        // Compute first derivative.
        values[1] = Compute(t, omt, 1);
        if (maxOrder >= 2)
        {
            // Compute second derivative.
            values[2] = Compute(t, omt, 2);
            if (maxOrder >= 3 && mDegree >= 3)
            {
                // Compute third derivative.
                values[3] = Compute(t, omt, 3);
            }
            else
            {
                values[3].MakeZero();
            }
        }
    }
}

template <int N, typename Real>
Vector<N, Real> BezierCurve<N, Real>::Compute(Real t, Real omt, int order)
const
{
    Vector<N, Real> result = omt * mControls[order][0];

    Real tpow = t;
    int isup = mDegree - order;
    for (int i = 1; i < isup; ++i)
    {
        Real c = mChoose[isup][i] * tpow;
        result = (result + c * mControls[order][i]) * omt;
        tpow *= t;
    }
    result = (result + tpow * mControls[order][isup]);

    int multiplier = 1;
    for (int i = 0; i < order; ++i)
    {
        multiplier *= mDegree - i;
    }
    result *= (Real)multiplier;

    return result;
}


}
