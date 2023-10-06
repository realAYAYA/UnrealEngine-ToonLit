// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteGMatrix.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>
#include <array>

// Fit the data with a polynomial of the form
//     w = sum_{i=0}^{n-1} c[i]*x^{p[i]}
// where p[i] are distinct nonnegative powers provided by the caller.  A
// least-squares fitting algorithm is used, but the input data is first
// mapped to (x,w) in [-1,1]^2 for numerical robustness.

namespace gte
{

template <typename Real>
class ApprPolynomialSpecial2
    :
    public ApprQuery<Real, ApprPolynomialSpecial2<Real>, std::array<Real, 2>>
{
public:
    // Initialize the model parameters to zero.  The degrees must be
    // nonnegative and strictly increasing.
    ApprPolynomialSpecial2(std::vector<int> const& degrees);

    // The minimum number of observations required to fit the model.
    int GetMinimumRequired() const;

    // Estimate the model parameters for all observations specified by the
    // indices.  This function is called by the base-class Fit(...) functions.
    bool Fit(std::vector<std::array<Real, 2>> const& observations,
        std::vector<int> const& indices);

    // Compute the model error for the specified observation for the current
    // model parameters.  The returned value for observation (x0,w0) is
    // |w(x0) - w0|, where w(x) is the fitted polynomial.
    Real Error(std::array<Real, 2> const& observation) const;

    // Get the parameters of the model.
    std::vector<Real> const& GetParameters() const;

    // Evaluate the polynomial.  The domain interval is provided so you can
    // interpolate (x in domain) or extrapolate (x not in domain).
    std::array<Real, 2> const& GetXDomain() const;
    Real Evaluate(Real x) const;

private:
    // Transform the (x,w) values to (x',w') in [-1,1]^2.
    void Transform(std::vector<std::array<Real, 2>> const& observations,
        std::vector<int> const& indices,
        std::vector<std::array<Real, 2>>& transformed);

    // The least-squares fitting algorithm for the transformed data.
    bool DoLeastSquares(std::vector<std::array<Real, 2>>& transformed);

    std::vector<int> mDegrees;
    std::vector<Real> mParameters;

    // Support for evaluation.  The coefficients were generated for the
    // samples mapped to [-1,1]^2.  The Evaluate() function must transform
    // x to x' in [-1,1], compute w' in [-1,1], then transform w' to w.
    std::array<Real, 2> mXDomain, mWDomain;
    std::array<Real, 2> mScale;
    Real mInvTwoWScale;

    // This array is used by Evaluate() to avoid reallocation of the 'vector'
    // for each call.  The member is mutable because, to the user, the call
    // to Evaluate does not modify the polynomial.
    mutable std::vector<Real> mXPowers;
};


template <typename Real>
ApprPolynomialSpecial2<Real>::ApprPolynomialSpecial2(
    std::vector<int> const& degrees)
    :
    mDegrees(degrees),
    mParameters(degrees.size())
{
#if !defined(GTE_NO_LOGGER)
    LogAssert(mDegrees.size() > 0, "The input array must have elements.");
    int lastDegree = -1;
    for (auto degree : mDegrees)
    {
        LogAssert(degree > lastDegree, "Degrees must be increasing.");
        lastDegree = degree;
    }
#endif

    mXDomain[0] = std::numeric_limits<Real>::max();
    mXDomain[1] = -mXDomain[0];
    mWDomain[0] = std::numeric_limits<Real>::max();
    mWDomain[1] = -mWDomain[0];
    std::fill(mParameters.begin(), mParameters.end(), (Real)0);

    mScale[0] = (Real)0;
    mScale[1] = (Real)0;
    mInvTwoWScale = (Real)0;

    // Powers of x are computed up to twice the powers when constructing the
    // fitted polynomial.  Powers of x are computed up to the powers for the
    // evaluation of the fitted polynomial.
    mXPowers.resize(2 * mDegrees.back() + 1);
    mXPowers[0] = (Real)1;
}

template <typename Real>
int ApprPolynomialSpecial2<Real>::GetMinimumRequired() const
{
    return static_cast<int>(mParameters.size());
}

template <typename Real>
bool ApprPolynomialSpecial2<Real>::Fit(
    std::vector<std::array<Real, 2>> const& observations,
    std::vector<int> const& indices)
{
    if (indices.size() > 0)
    {
        // Transform the observations to [-1,1]^2 for numerical robustness.
        std::vector<std::array<Real, 2>> transformed;
        Transform(observations, indices, transformed);

        // Fit the transformed data using a least-squares algorithm.
        return DoLeastSquares(transformed);
    }

    std::fill(mParameters.begin(), mParameters.end(), (Real)0);
    return false;
}

template <typename Real>
Real ApprPolynomialSpecial2<Real>::Error(
    std::array<Real, 2> const& observation) const
{
    Real w = Evaluate(observation[0]);
    Real error = std::abs(w - observation[1]);
    return error;
}

template <typename Real>
std::vector<Real> const& ApprPolynomialSpecial2<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
std::array<Real, 2> const& ApprPolynomialSpecial2<Real>::GetXDomain() const
{
    return mXDomain;
}

template <typename Real>
Real ApprPolynomialSpecial2<Real>::Evaluate(Real x) const
{
    // Transform x to x' in [-1,1].
    x = (Real)-1 + ((Real)2) * mScale[0] * (x - mXDomain[0]);

    // Compute relevant powers of x.
    int jmax = mDegrees.back();
    for (int j = 1; j <= jmax; ++j)
    {
        mXPowers[j] = mXPowers[j - 1] * x;
    }

    Real w = (Real)0;
    int isup = static_cast<int>(mDegrees.size());
    for (int i = 0; i < isup; ++i)
    {
        Real xp = mXPowers[mDegrees[i]];
        w += mParameters[i] * xp;
    }

    // Transform w from [-1,1] back to the original space.
    w = (w + (Real)1) * mInvTwoWScale + mWDomain[0];
    return w;
}

template <typename Real>
void ApprPolynomialSpecial2<Real>::Transform(
    std::vector<std::array<Real, 2>> const& observations,
    std::vector<int> const& indices,
    std::vector<std::array<Real, 2>>& transformed)
{
    int numSamples = static_cast<int>(indices.size());
    transformed.resize(numSamples);

    std::array<Real, 2> omin = observations[indices[0]];
    std::array<Real, 2> omax = omin;
    std::array<Real, 2> obs;
    int s, i;
    for (s = 1; s < numSamples; ++s)
    {
        obs = observations[indices[s]];
        for (i = 0; i < 2; ++i)
        {
            if (obs[i] < omin[i])
            {
                omin[i] = obs[i];
            }
            else if (obs[i] > omax[i])
            {
                omax[i] = obs[i];
            }
        }
    }

    mXDomain[0] = omin[0];
    mXDomain[1] = omax[0];
    mWDomain[0] = omin[1];
    mWDomain[1] = omax[1];
    for (i = 0; i < 2; ++i)
    {
        mScale[i] = ((Real)1) / (omax[i] - omin[i]);
    }

    for (s = 0; s < numSamples; ++s)
    {
        obs = observations[indices[s]];
        for (i = 0; i < 2; ++i)
        {
            transformed[s][i] = (Real)-1 + ((Real)2) * mScale[i] *
                (obs[i] - omin[i]);
        }
    }
    mInvTwoWScale = ((Real)0.5) / mScale[1];
}

template <typename Real>
bool ApprPolynomialSpecial2<Real>::DoLeastSquares(
    std::vector<std::array<Real, 2>>& transformed)
{
    // Set up a linear system A*X = B, where X are the polynomial
    // coefficients.
    int size = static_cast<int>(mDegrees.size());
    GMatrix<Real> A(size, size);
    A.MakeZero();
    GVector<Real> B(size);
    B.MakeZero();

    int numSamples = static_cast<int>(transformed.size());
    int twoMaxXDegree = 2 * mDegrees.back();
    int row, col;
    for (int i = 0; i < numSamples; ++i)
    {
        // Compute relevant powers of x.
        Real x = transformed[i][0];
        Real w = transformed[i][1];
        for (int j = 0; j <= twoMaxXDegree; ++j)
        {
            mXPowers[j] = mXPowers[j - 1] * x;
        }

        for (row = 0; row < size; ++row)
        {
            // Update the upper-triangular portion of the symmetric matrix.
            for (col = row; col < size; ++col)
            {
                A(row, col) += mXPowers[mDegrees[row] + mDegrees[col]];
            }

            // Update the right-hand side of the system.
            B[row] += mXPowers[mDegrees[row]] * w;
        }
    }

    // Copy the upper-triangular portion of the symmetric matrix to the
    // lower-triangular portion.
    for (row = 0; row < size; ++row)
    {
        for (col = 0; col < row; ++col)
        {
            A(row, col) = A(col, row);
        }
    }

    // Precondition by normalizing the sums.
    Real invNumSamples = ((Real)1) / (Real)numSamples;
    A *= invNumSamples;
    B *= invNumSamples;

    // Solve for the polynomial coefficients.
    GVector<Real> coefficients = Inverse(A) * B;
    bool hasNonzero = false;
    for (int i = 0; i < size; ++i)
    {
        mParameters[i] = coefficients[i];
        if (coefficients[i] != (Real)0)
        {
            hasNonzero = true;
        }
    }
    return hasNonzero;
}


}
