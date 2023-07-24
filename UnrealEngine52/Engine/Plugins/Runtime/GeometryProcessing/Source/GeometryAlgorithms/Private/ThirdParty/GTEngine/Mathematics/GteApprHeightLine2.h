// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMatrix2x2.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>

// Least-squares fit of a line to height data (x,f(x)).  The line is of the
// form: (y - yAvr) = a*(x - xAvr), where (xAvr,yAvr) is the average of the
// sample points.  The return value of Fit is 'true' iff the fit is successful
// (the input points are not degenerate to a single point).  The mParameters
// values are ((xAvr,yAvr),(a,-1)) on success and ((0,0),(0,0)) on failure.
// The error for (x0,y0) is [a*(x0-xAvr)-(y0-yAvr)]^2.

namespace gte
{

template <typename Real>
class ApprHeightLine2
    :
    public ApprQuery<Real, ApprHeightLine2<Real>, Vector2<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprHeightLine2();

    // Basic fitting algorithm.
    bool Fit(int numPoints, Vector2<Real> const* points);
    std::pair<Vector2<Real>, Vector2<Real>> const& GetParameters() const;

    // Functions called by ApprQuery::RANSAC.  See GteApprQuery.h for a
    // detailed description.
    int GetMinimumRequired() const;
    Real Error(Vector2<Real> const& observation) const;
    bool Fit(std::vector<Vector2<Real>> const& observations,
        std::vector<int> const& indices);

private:
    std::pair<Vector2<Real>, Vector2<Real>> mParameters;
};


template <typename Real>
ApprHeightLine2<Real>::ApprHeightLine2()
{
    mParameters.first = Vector2<Real>::Zero();
    mParameters.second = Vector2<Real>::Zero();
}

template <typename Real>
bool ApprHeightLine2<Real>::Fit(int numPoints, Vector2<Real> const* points)
{
    if (numPoints >= GetMinimumRequired() && points)
    {
        // Compute the mean of the points.
        Vector2<Real> mean = Vector2<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            mean += points[i];
        }
        mean /= (Real)numPoints;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0;
        for (int i = 0; i < numPoints; ++i)
        {
            Vector2<Real> diff = points[i] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
        }

        // Decompose the covariance matrix.
        if (covar00 >(Real)0)
        {
            mParameters.first = mean;
            mParameters.second[0] = covar01 / covar00;
            mParameters.second[1] = (Real)-1;
            return true;
        }
    }

    mParameters.first = Vector2<Real>::Zero();
    mParameters.second = Vector2<Real>::Zero();
    return false;
}

template <typename Real>
std::pair<Vector2<Real>, Vector2<Real>> const&
ApprHeightLine2<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprHeightLine2<Real>::GetMinimumRequired() const
{
    return 2;
}

template <typename Real>
Real ApprHeightLine2<Real>::Error(Vector2<Real> const& observation) const
{
    Real d = Dot(observation - mParameters.first, mParameters.second);
    Real error = d*d;
    return error;
}

template <typename Real>
bool ApprHeightLine2<Real>::Fit(
    std::vector<Vector2<Real>> const& observations,
    std::vector<int> const& indices)
{
    if (static_cast<int>(indices.size()) >= GetMinimumRequired())
    {
        // Compute the mean of the points.
        Vector2<Real> mean = Vector2<Real>::Zero();
        for (auto index : indices)
        {
            mean += observations[index];
        }
        mean /= (Real)indices.size();

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0;
        for (auto index : indices)
        {
            Vector2<Real> diff = observations[index] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
        }

        // Decompose the covariance matrix.
        if (covar00 > (Real)0)
        {
            mParameters.first = mean;
            mParameters.second[0] = covar01 / covar00;
            mParameters.second[1] = (Real)-1;
            return true;
        }
    }

    mParameters.first = Vector2<Real>::Zero();
    mParameters.second = Vector2<Real>::Zero();
    return false;
}


}
