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
//     w = sum_{i=0}^{n-1} c[i]*x^{p[i]}*y^{q[i]}
// where <p[i],q[i]> are distinct pairs of nonnegative powers provided by the
// caller.  A least-squares fitting algorithm is used, but the input data is
// first mapped to (x,y,w) in [-1,1]^3 for numerical robustness.

namespace gte
{

template <typename Real>
class ApprPolynomialSpecial3
    :
    public ApprQuery<Real, ApprPolynomialSpecial3<Real>, std::array<Real, 3>>
{
public:
    // Initialize the model parameters to zero.  The degrees must be
    // nonnegative and strictly increasing.
    ApprPolynomialSpecial3(std::vector<int> const& xDegrees,
        std::vector<int> const& yDegrees);

    // The minimum number of observations required to fit the model.
    int GetMinimumRequired() const;

    // Estimate the model parameters for all observations specified by the
    // indices.  This function is called by the base-class Fit(...) functions.
    bool Fit(std::vector<std::array<Real, 3>> const& observations,
        std::vector<int> const& indices);

    // Compute the model error for the specified observation for the current
    // model parameters.  The returned value for observation (x0,w0) is
    // |w(x0) - w0|, where w(x) is the fitted polynomial.
    Real Error(std::array<Real, 3> const& observation) const;

    // Get the parameters of the model.
    std::vector<Real> const& GetParameters() const;

    // Evaluate the polynomial.  The domain interval is provided so you can
    // interpolate ((x,y) in domain) or extrapolate ((x,y) not in domain).
    std::array<Real, 2> const& GetXDomain() const;
    std::array<Real, 2> const& GetYDomain() const;
    Real Evaluate(Real x, Real y) const;

private:
    // Transform the (x,y,w) values to (x',y',w') in [-1,1]^3.
    void Transform(std::vector<std::array<Real, 3>> const& observations,
        std::vector<int> const& indices,
        std::vector<std::array<Real, 3>>& transformed);

    // The least-squares fitting algorithm for the transformed data.
    bool DoLeastSquares(std::vector<std::array<Real, 3>>& transformed);

    std::vector<int> mXDegrees, mYDegrees;
    std::vector<Real> mParameters;

    // Support for evaluation.  The coefficients were generated for the
    // samples mapped to [-1,1]^3.  The Evaluate() function must transform
    // (x,y) to (x',y') in [-1,1]^2, compute w' in [-1,1], then transform w'
    // to w.
    std::array<Real, 2> mXDomain, mYDomain, mWDomain;
    std::array<Real, 3> mScale;
    Real mInvTwoWScale;

    // This array is used by Evaluate() to avoid reallocation of the 'vector's
    // for each call.  The members are mutable because, to the user, the call
    // to Evaluate does not modify the polynomial.
    mutable std::vector<Real> mXPowers, mYPowers;
};


template <typename Real>
ApprPolynomialSpecial3<Real>::ApprPolynomialSpecial3(
    std::vector<int> const& xDegrees, std::vector<int> const& yDegrees)
    :
    mXDegrees(xDegrees),
    mYDegrees(yDegrees),
    mParameters(mXDegrees.size() * mYDegrees.size())
{
#if !defined(GTE_NO_LOGGER)
    LogAssert(mXDegrees.size() == mYDegrees.size(),
        "The input arrays must have the same size.");

    LogAssert(mXDegrees.size() > 0, "The input array must have elements.");
    int lastDegree = -1;
    for (auto degree : mXDegrees)
    {
        LogAssert(degree > lastDegree, "Degrees must be increasing.");
        lastDegree = degree;
    }

    LogAssert(mYDegrees.size() > 0, "The input array must have elements.");
    lastDegree = -1;
    for (auto degree : mYDegrees)
    {
        LogAssert(degree > lastDegree, "Degrees must be increasing.");
        lastDegree = degree;
    }
#endif

    mXDomain[0] = std::numeric_limits<Real>::max();
    mXDomain[1] = -mXDomain[0];
    mYDomain[0] = std::numeric_limits<Real>::max();
    mYDomain[1] = -mYDomain[0];
    mWDomain[0] = std::numeric_limits<Real>::max();
    mWDomain[1] = -mWDomain[0];
    std::fill(mParameters.begin(), mParameters.end(), (Real)0);

    mScale[0] = (Real)0;
    mScale[1] = (Real)0;
    mScale[2] = (Real)0;
    mInvTwoWScale = (Real)0;

    // Powers of x and y are computed up to twice the powers when constructing
    // the fitted polynomial.  Powers of x and y are computed up to the powers
    // for the evaluation of the fitted polynomial.
    mXPowers.resize(2 * mXDegrees.back() + 1);
    mXPowers[0] = (Real)1;
    mYPowers.resize(2 * mYDegrees.back() + 1);
    mYPowers[0] = (Real)1;
}

template <typename Real>
int ApprPolynomialSpecial3<Real>::GetMinimumRequired() const
{
    return static_cast<int>(mParameters.size());
}

template <typename Real>
bool ApprPolynomialSpecial3<Real>::Fit(
    std::vector<std::array<Real, 3>> const& observations,
    std::vector<int> const& indices)
{
    if (indices.size() > 0)
    {
        // Transform the observations to [-1,1]^3 for numerical robustness.
        std::vector<std::array<Real, 3>> transformed;
        Transform(observations, indices, transformed);

        // Fit the transformed data using a least-squares algorithm.
        return DoLeastSquares(transformed);
    }

    std::fill(mParameters.begin(), mParameters.end(), (Real)0);
    return false;
}

template <typename Real>
Real ApprPolynomialSpecial3<Real>::Error(
    std::array<Real, 3> const& observation) const
{
    Real w = Evaluate(observation[0], observation[1]);
    Real error = std::abs(w - observation[2]);
    return error;
}

template <typename Real>
std::vector<Real> const& ApprPolynomialSpecial3<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
std::array<Real, 2> const& ApprPolynomialSpecial3<Real>::GetXDomain() const
{
    return mXDomain;
}

template <typename Real>
std::array<Real, 2> const& ApprPolynomialSpecial3<Real>::GetYDomain() const
{
    return mYDomain;
}

template <typename Real>
Real ApprPolynomialSpecial3<Real>::Evaluate(Real x, Real y) const
{
    // Transform (x,y) to (x',y') in [-1,1]^2.
    x = (Real)-1 + ((Real)2) * mScale[0] * (x - mXDomain[0]);
    y = (Real)-1 + ((Real)2) * mScale[1] * (y - mYDomain[0]);

    // Compute relevant powers of x and y.
    int jmax = mXDegrees.back();
    for (int j = 1; j <= jmax; ++j)
    {
        mXPowers[j] = mXPowers[j - 1] * x;
    }

    jmax = mYDegrees.back();
    for (int j = 1; j <= jmax; ++j)
    {
        mYPowers[j] = mYPowers[j - 1] * y;
    }

    Real w = (Real)0;
    int isup = static_cast<int>(mXDegrees.size());
    for (int i = 0; i < isup; ++i)
    {
        Real xp = mXPowers[mXDegrees[i]];
        Real yp = mYPowers[mYDegrees[i]];
        w += mParameters[i] * xp * yp;
    }

    // Transform w from [-1,1] back to the original space.
    w = (w + (Real)1) * mInvTwoWScale + mWDomain[0];
    return w;
}

template <typename Real>
void ApprPolynomialSpecial3<Real>::Transform(
    std::vector<std::array<Real, 3>> const& observations,
    std::vector<int> const& indices,
    std::vector<std::array<Real, 3>>& transformed)
{
    int numSamples = static_cast<int>(indices.size());
    transformed.resize(numSamples);

    std::array<Real, 3> omin = observations[indices[0]];
    std::array<Real, 3> omax = omin;
    std::array<Real, 3> obs;
    int s, i;
    for (s = 1; s < numSamples; ++s)
    {
        obs = observations[indices[s]];
        for (i = 0; i < 3; ++i)
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
    mYDomain[0] = omin[1];
    mYDomain[1] = omax[1];
    mWDomain[0] = omin[2];
    mWDomain[1] = omax[2];
    for (i = 0; i < 3; ++i)
    {
        mScale[i] = ((Real)1) / (omax[i] - omin[i]);
    }

    for (s = 0; s < numSamples; ++s)
    {
        obs = observations[indices[s]];
        for (i = 0; i < 3; ++i)
        {
            transformed[s][i] = (Real)-1 + ((Real)2) * mScale[i] *
                (obs[i] - omin[i]);
        }
    }
    mInvTwoWScale = ((Real)0.5) / mScale[2];
}

template <typename Real>
bool ApprPolynomialSpecial3<Real>::DoLeastSquares(
    std::vector<std::array<Real, 3>>& transformed)
{
    // Set up a linear system A*X = B, where X are the polynomial
    // coefficients.
    int size = static_cast<int>(mXDegrees.size());
    GMatrix<Real> A(size, size);
    A.MakeZero();
    GVector<Real> B(size);
    B.MakeZero();

    int numSamples = static_cast<int>(transformed.size());
    int twoMaxXDegree = 2 * mXDegrees.back();
    int twoMaxYDegree = 2 * mYDegrees.back();
    int row, col;
    for (int i = 0; i < numSamples; ++i)
    {
        // Compute relevant powers of x and y.
        Real x = transformed[i][0];
        Real y = transformed[i][1];
        Real w = transformed[i][2];
        for (int j = 1; j <= 2 * twoMaxXDegree; ++j)
        {
            mXPowers[j] = mXPowers[j - 1] * x;
        }
        for (int j = 1; j <= 2 * twoMaxYDegree; ++j)
        {
            mYPowers[j] = mYPowers[j - 1] * y;
        }

        for (row = 0; row < size; ++row)
        {
            // Update the upper-triangular portion of the symmetric matrix.
            Real xp, yp;
            for (col = row; col < size; ++col)
            {
                xp = mXPowers[mXDegrees[row] + mXDegrees[col]];
                yp = mYPowers[mYDegrees[row] + mYDegrees[col]];
                A(row, col) += xp * yp;
            }

            // Update the right-hand side of the system.
            xp = mXPowers[mXDegrees[row]];
            yp = mYPowers[mYDegrees[row]];
            B[row] += xp * yp * w;
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
