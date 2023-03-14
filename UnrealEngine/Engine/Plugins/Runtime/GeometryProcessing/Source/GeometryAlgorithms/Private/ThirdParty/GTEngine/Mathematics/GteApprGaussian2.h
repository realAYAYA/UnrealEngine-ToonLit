// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector2.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteSymmetricEigensolver2x2.h>

// Fit points with a Gaussian distribution.  The center is the mean of the
// points, the axes are the eigenvectors of the covariance matrix, and the
// extents are the eigenvalues of the covariance matrix and are returned in
// increasing order.  An oriented box is used to store the mean, axes, and
// extents.

namespace gte
{

template <typename Real>
class ApprGaussian2
    :
    public ApprQuery<Real, ApprGaussian2<Real>, Vector2<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprGaussian2();

    // Basic fitting algorithm.
    bool Fit(int numPoints, Vector2<Real> const* points);
    OrientedBox2<Real> const& GetParameters() const;

    // Functions called by ApprQuery::RANSAC.  See GteApprQuery.h for a
    // detailed description.
    int GetMinimumRequired() const;
    Real Error(Vector2<Real> const& observation) const;
    bool Fit(std::vector<Vector2<Real>> const& observations,
        std::vector<int> const& indices);

private:
    OrientedBox2<Real> mParameters;
};


template <typename Real>
ApprGaussian2<Real>::ApprGaussian2()
{
    mParameters.center = Vector2<Real>::Zero();
    mParameters.axis[0] = Vector2<Real>::Zero();
    mParameters.axis[1] = Vector2<Real>::Zero();
    mParameters.extent = Vector2<Real>::Zero();
}

template <typename Real>
bool ApprGaussian2<Real>::Fit(int numPoints, Vector2<Real> const* points)
{
    if (numPoints >= GetMinimumRequired() && points)
    {
        // Compute the mean of the points.
        Vector2<Real> mean = Vector2<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            mean += points[i];
        }
        Real invSize = ((Real)1) / (Real)numPoints;
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar11 = (Real)0;
        for (int i = 0; i < numPoints; ++i)
        {
            Vector2<Real> diff = points[i] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar11 += diff[1] * diff[1];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar11 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver2x2<Real> es;
        std::array<Real, 2> eval;
        std::array<std::array<Real, 2>, 2> evec;
        es(covar00, covar01, covar11, +1, eval, evec);
        mParameters.center = mean;
        mParameters.axis[0] = evec[0];
        mParameters.axis[1] = evec[1];
        mParameters.extent = eval;
        return true;
    }

    mParameters.center = Vector2<Real>::Zero();
    mParameters.axis[0] = Vector2<Real>::Zero();
    mParameters.axis[1] = Vector2<Real>::Zero();
    mParameters.extent = Vector2<Real>::Zero();
    return false;
}

template <typename Real>
OrientedBox2<Real> const& ApprGaussian2<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprGaussian2<Real>::GetMinimumRequired() const
{
    return 2;
}

template <typename Real>
bool ApprGaussian2<Real>::Fit(
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
        Real invSize = ((Real)1) / (Real)indices.size();
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar11 = (Real)0;
        for (auto index : indices)
        {
            Vector2<Real> diff = observations[index] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar11 += diff[1] * diff[1];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar11 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver2x2<Real> es;
        std::array<Real, 2> eval;
        std::array<std::array<Real, 2>, 2> evec;
        es(covar00, covar01, covar11, +1, eval, evec);
        mParameters.center = mean;
        mParameters.axis[0] = evec[0];
        mParameters.axis[1] = evec[1];
        mParameters.extent = eval;
    }

    mParameters.center = Vector2<Real>::Zero();
    mParameters.axis[0] = Vector2<Real>::Zero();
    mParameters.axis[1] = Vector2<Real>::Zero();
    mParameters.extent = Vector2<Real>::Zero();
    return false;
}

template <typename Real>
Real ApprGaussian2<Real>::Error(Vector2<Real> const& observation) const
{
    Vector2<Real> diff = observation - mParameters.center;
    Real error = (Real)0;
    for (int i = 0; i < 2; ++i)
    {
        if (mParameters.extent[i] >(Real)0)
        {
            Real ratio = Dot(diff, mParameters.axis[i]) /
                mParameters.extent[i];
            error += ratio * ratio;
        }
    }
    return error;
}


}
