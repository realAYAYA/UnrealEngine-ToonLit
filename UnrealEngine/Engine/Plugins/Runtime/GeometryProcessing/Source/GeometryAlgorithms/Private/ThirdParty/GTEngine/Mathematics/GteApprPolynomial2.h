// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteArray2.h>
#include <ThirdParty/GTEngine/Mathematics/GteGMatrix.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>
#include <array>

// The samples are (x[i],w[i]) for 0 <= i < S.  Think of w as a function of
// x, say w = f(x).  The function fits the samples with a polynomial of
// degree d, say w = sum_{i=0}^d c[i]*x^i.  The method is a least-squares
// fitting algorithm.  The mParameters stores the coefficients c[i] for
// 0 <= i <= d.  The observation type is std::array<Real,2>, which represents
// a pair (x,w).
//
// WARNING.  The fitting algorithm for polynomial terms
//   (1,x,x^2,...,x^d)
// is known to be nonrobust for large degrees and for large magnitude data.
// One alternative is to use orthogonal polynomials
//   (f[0](x),...,f[d](x))
// and apply the least-squares algorithm to these.  Another alternative is to
// transform
//   (x',w') = ((x-xcen)/rng, w/rng)
// where xmin = min(x[i]), xmax = max(x[i]), xcen = (xmin+xmax)/2, and
// rng = xmax-xmin.  Fit the (x',w') points,
//   w' = sum_{i=0}^d c'[i]*(x')^i.
// The original polynomial is evaluated as
//   w = rng*sum_{i=0}^d c'[i]*((x-xcen)/rng)^i

namespace gte
{

template <typename Real>
class ApprPolynomial2
    :
    public ApprQuery<Real, ApprPolynomial2<Real>, std::array<Real, 2>>
{
public:
    // Initialize the model parameters to zero.
    ApprPolynomial2(int degree);

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
    int mDegree, mSize;
    std::array<Real, 2> mXDomain;
    std::vector<Real> mParameters;
};


template <typename Real>
ApprPolynomial2<Real>::ApprPolynomial2(int degree)
    :
    mDegree(degree),
    mSize(degree + 1),
    mParameters(mSize)
{
    mXDomain[0] = std::numeric_limits<Real>::max();
    mXDomain[1] = -mXDomain[0];
    std::fill(mParameters.begin(), mParameters.end(), (Real)0);
}

template <typename Real>
int ApprPolynomial2<Real>::GetMinimumRequired() const
{
    return mSize;
}

template <typename Real>
bool ApprPolynomial2<Real>::Fit(
    std::vector<std::array<Real, 2>> const& observations,
    std::vector<int> const& indices)
{
    if (indices.size() > 0)
    {
        int s, i0, i1;

        // Compute the powers of x.
        int numSamples = static_cast<int>(indices.size());
        int twoDegree = 2 * mDegree;
        Array2<Real> xPower(twoDegree + 1, numSamples);
        for (s = 0; s < numSamples; ++s)
        {
            Real x = observations[indices[s]][0];
            mXDomain[0] = std::min(x, mXDomain[0]);
            mXDomain[1] = std::max(x, mXDomain[1]);

            xPower[s][0] = (Real)1;
            for (i0 = 1; i0 <= twoDegree; ++i0)
            {
                xPower[s][i0] = x * xPower[s][i0 - 1];
            }
        }

        // Matrix A is the Vandermonde matrix and vector B is the right-hand
        // side of the linear system A*X = B.
        GMatrix<Real> A(mSize, mSize);
        GVector<Real> B(mSize);
        for (i0 = 0; i0 <= mDegree; ++i0)
        {
            Real sum = (Real)0;
            for (s = 0; s < numSamples; ++s)
            {
                Real w = observations[indices[s]][1];
                sum += w * xPower[s][i0];
            }

            B[i0] = sum;

            for (i1 = 0; i1 <= mDegree; ++i1)
            {
                sum = (Real)0;
                for (s = 0; s < numSamples; ++s)
                {
                    sum += xPower[s][i0 + i1];
                }

                A(i0, i1) = sum;
            }
        }

        // Solve for the polynomial coefficients.
        GVector<Real> coefficients = Inverse(A) * B;
        bool hasNonzero = false;
        for (int i = 0; i < mSize; ++i)
        {
            mParameters[i] = coefficients[i];
            if (coefficients[i] != (Real)0)
            {
                hasNonzero = true;
            }
        }
        return hasNonzero;

    }

    std::fill(mParameters.begin(), mParameters.end(), (Real)0);
    return false;
}

template <typename Real>
Real ApprPolynomial2<Real>::Error(std::array<Real, 2> const& observation)
const
{
    Real w = Evaluate(observation[0]);
    Real error = std::abs(w - observation[1]);
    return error;
}

template <typename Real>
std::vector<Real> const& ApprPolynomial2<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
std::array<Real, 2> const& ApprPolynomial2<Real>::GetXDomain() const
{
    return mXDomain;
}

template <typename Real>
Real ApprPolynomial2<Real>::Evaluate(Real x) const
{
    int i = mDegree;
    Real w = mParameters[i];
    while (--i >= 0)
    {
        w = mParameters[i] + w * x;
    }
    return w;
}


}
