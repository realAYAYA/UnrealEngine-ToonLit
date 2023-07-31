// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteWrapper.h>
#include <ThirdParty/GTEngine/Mathematics/GteGVector.h>
#include <ThirdParty/GTEngine/Mathematics/GteMinimize1.h>
#include <cstring>

// The Cartesian-product domain provided to GetMinimum(*) has minimum values
// stored in t0[0..d-1] and maximum values stored in t1[0..d-1], where d is
// 'dimensions'.  The domain is searched along lines through the current
// estimate of the minimum location.  Each such line is searched for a minimum
// using a Minimize1<Real> object.  This is called "Powell's Direction Set
// Method".  The parameters 'maxLevel' and 'maxBracket' are used by
// Minimize1<Real>, so read the documentation for that class (in its header
// file) to understand what these mean.  The input 'maxIterations' is the
// number of iterations for the direction-set method.

namespace gte
{

template <typename Real>
class MinimizeN
{
public:
    // Construction.
    MinimizeN(int dimensions, std::function<Real(Real const*)> const& F,
        int maxLevel, int maxBracket, int maxIterations);

    // Find the minimum on the Cartesian-product domain whose minimum values
    // are stored in t0[0..d-1] and whose maximum values are stored in
    // t1[0..d-1], where d is 'dimensions'.  An initial guess is specified in
    // tInitial[0..d-1].  The location of the minimum is tMin[0..d-1] and
    // the value of the minimum is 'fMin'.
    void GetMinimum(Real const* t0, Real const* t1, Real const* tInitial,
        Real* tMin, Real& fMin);

private:
    // The current estimate of the minimum location is mTCurr[0..d-1].  The
    // direction of the current line to search is mDCurr[0..d-1].  This line
    // must be clipped against the Cartesian-product domain, a process
    // implemented in this function.  If the line is mTCurr+s*mDCurr, the
    // clip result is the s-interval [ell0,ell1].
    void ComputeDomain(Real const* t0, Real const* t1, Real& ell0,
        Real& ell1);

    int mDimensions;
    std::function<Real(Real const*)> mFunction;
    int mMaxIterations;
    std::vector<GVector<Real>> mDirections;
    int mDConjIndex;
    int mDCurrIndex;
    GVector<Real> mTCurr;
    GVector<Real> mTSave;
    Real mFCurr;
    Minimize1<Real> mMinimizer;
};


template <typename Real>
MinimizeN<Real>::MinimizeN(int dimensions,
    std::function<Real(Real const*)> const& F, int maxLevel, int maxBracket,
    int maxIterations)
    :
    mDimensions(dimensions),
    mFunction(F),
    mMaxIterations(maxIterations),
    mDirections(dimensions + 1),
    mDConjIndex(dimensions),
    mDCurrIndex(0),
    mTCurr(dimensions),
    mTSave(dimensions),
    mMinimizer(
    [this](Real t)
{
    return mFunction(&(mTCurr + t*mDirections[mDCurrIndex])[0]);
},
maxLevel, maxBracket)
{
    for (auto& direction : mDirections)
    {
        direction.SetSize(dimensions);
    }
}

template <typename Real>
void MinimizeN<Real>::GetMinimum(Real const* t0, Real const* t1,
    Real const* tInitial, Real* tMin, Real& fMin)
{
    // The initial guess.
    size_t numBytes = mDimensions*sizeof(Real);
    mFCurr = mFunction(tInitial);
    Memcpy(&mTSave[0], tInitial, numBytes);
    Memcpy(&mTCurr[0], tInitial, numBytes);

    // Initialize the direction set to the standard Euclidean basis.
    for (int i = 0; i < mDimensions; ++i)
    {
        mDirections[i].MakeUnit(i);
    }

    Real ell0, ell1, ellMin;
    for (int iter = 0; iter < mMaxIterations; ++iter)
    {
        // Find minimum in each direction and update current location.
        for (int i = 0; i < mDimensions; ++i)
        {
            mDCurrIndex = i;
            ComputeDomain(t0, t1, ell0, ell1);
            mMinimizer.GetMinimum(ell0, ell1, (Real)0, ellMin, mFCurr);
            mTCurr += ellMin*mDirections[i];
        }

        // Estimate a unit-length conjugate direction.  TODO: Expose
        // epsilon to the caller.
        mDirections[mDConjIndex] = mTCurr - mTSave;
        Real length = Length(mDirections[mDConjIndex]);
        Real const epsilon = (Real)1e-06;
        if (length < epsilon)
        {
            // New position did not change significantly from old one.
            // Should there be a better convergence criterion here?
            break;
        }

        mDirections[mDConjIndex] /= length;

        // Minimize in conjugate direction.
        mDCurrIndex = mDConjIndex;
        ComputeDomain(t0, t1, ell0, ell1);
        mMinimizer.GetMinimum(ell0, ell1, (Real)0, ellMin, mFCurr);
        mTCurr += ellMin*mDirections[mDCurrIndex];

        // Cycle the directions and add conjugate direction to set.
        mDConjIndex = 0;
        for (int i = 0; i < mDimensions; ++i)
        {
            mDirections[i] = mDirections[i + 1];
        }

        // Set parameters for next pass.
        mTSave = mTCurr;
    }

    Memcpy(tMin, &mTCurr[0], numBytes);
    fMin = mFCurr;
}

template <typename Real>
void MinimizeN<Real>::ComputeDomain(Real const* t0, Real const* t1,
    Real& ell0, Real& ell1)
{
    ell0 = -std::numeric_limits<Real>::max();
    ell1 = +std::numeric_limits<Real>::max();

    for (int i = 0; i < mDimensions; ++i)
    {
        Real value = mDirections[mDCurrIndex][i];
        if (value != (Real)0)
        {
            Real b0 = t0[i] - mTCurr[i];
            Real b1 = t1[i] - mTCurr[i];
            Real inv = ((Real)1) / value;
            if (value >(Real)0)
            {
                // The valid t-interval is [b0,b1].
                b0 *= inv;
                if (b0 > ell0)
                {
                    ell0 = b0;
                }
                b1 *= inv;
                if (b1 < ell1)
                {
                    ell1 = b1;
                }
            }
            else
            {
                // The valid t-interval is [b1,b0].
                b0 *= inv;
                if (b0 < ell1)
                {
                    ell1 = b0;
                }
                b1 *= inv;
                if (b1 > ell0)
                {
                    ell0 = b1;
                }
            }
        }
    }

    // Correction if numerical errors lead to values nearly zero.
    if (ell0 > (Real)0)
    {
        ell0 = (Real)0;
    }
    if (ell1 < (Real)0)
    {
        ell1 = (Real)0;
    }
}


}
