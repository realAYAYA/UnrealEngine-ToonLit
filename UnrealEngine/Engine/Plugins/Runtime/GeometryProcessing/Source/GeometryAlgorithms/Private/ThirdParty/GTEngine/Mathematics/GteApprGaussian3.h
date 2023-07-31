// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteOrientedBox.h>
#include <ThirdParty/GTEngine/Mathematics/GteSymmetricEigensolver3x3.h>

// Fit points with a Gaussian distribution.  The center is the mean of the
// points, the axes are the eigenvectors of the covariance matrix, and the
// extents are the eigenvalues of the covariance matrix and are returned in
// increasing order.  An oriented box is used to store the mean, axes, and
// extents.

namespace gte
{

template <typename Real>
class ApprGaussian3
    :
    public ApprQuery<Real, ApprGaussian3<Real>, Vector3<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprGaussian3();

    // Basic fitting algorithm.
    bool Fit(int numPoints, Vector3<Real> const* points);
    OrientedBox3<Real> const& GetParameters() const;

    // Functions called by ApprQuery::RANSAC.  See GteApprQuery.h for a
    // detailed description.
    int GetMinimumRequired() const;
    Real Error(Vector3<Real> const& observation) const;
    bool Fit(std::vector<Vector3<Real>> const& observations,
        std::vector<int> const& indices);

private:
    OrientedBox3<Real> mParameters;
};


template <typename Real>
ApprGaussian3<Real>::ApprGaussian3()
{
    mParameters.center = Vector3<Real>::Zero();
    mParameters.axis[0] = Vector3<Real>::Zero();
    mParameters.axis[1] = Vector3<Real>::Zero();
    mParameters.axis[2] = Vector3<Real>::Zero();
    mParameters.extent = Vector3<Real>::Zero();
}

template <typename Real>
bool ApprGaussian3<Real>::Fit(int numPoints, Vector3<Real> const* points)
{
    if (numPoints >= GetMinimumRequired() && points)
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            mean += points[i];
        }
        Real invSize = ((Real)1) / (Real)numPoints;
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar02 = (Real)0;
        Real covar11 = (Real)0, covar12 = (Real)0, covar22 = (Real)0;
        for (int i = 0; i < numPoints; ++i)
        {
            Vector3<Real> diff = points[i] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
            covar22 += diff[2] * diff[2];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar02 *= invSize;
        covar11 *= invSize;
        covar12 *= invSize;
        covar22 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);
        mParameters.center = mean;
        mParameters.axis[0] = evec[0];
        mParameters.axis[1] = evec[1];
        mParameters.axis[2] = evec[2];
        mParameters.extent = eval;
        return true;
    }

    mParameters.center = Vector3<Real>::Zero();
    mParameters.axis[0] = Vector3<Real>::Zero();
    mParameters.axis[1] = Vector3<Real>::Zero();
    mParameters.axis[2] = Vector3<Real>::Zero();
    mParameters.extent = Vector3<Real>::Zero();
    return false;
}

template <typename Real>
OrientedBox3<Real> const& ApprGaussian3<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprGaussian3<Real>::GetMinimumRequired() const
{
    return 2;
}

template <typename Real>
Real ApprGaussian3<Real>::Error(Vector3<Real> const& observation) const
{
    Vector3<Real> diff = observation - mParameters.center;
    Real error = (Real)0;
    for (int i = 0; i < 3; ++i)
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

template <typename Real>
bool ApprGaussian3<Real>::Fit(
    std::vector<Vector3<Real>> const& observations,
    std::vector<int> const& indices)
{
    if (static_cast<int>(indices.size()) >= GetMinimumRequired())
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (auto index : indices)
        {
            mParameters.center += observations[index];
        }
        Real invSize = ((Real)1) / (Real)indices.size();
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar02 = (Real)0;
        Real covar11 = (Real)0, covar12 = (Real)0, covar22 = (Real)0;
        for (auto index : indices)
        {
            Vector3<Real> diff = observations[index] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
            covar22 += diff[2] * diff[2];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar02 *= invSize;
        covar11 *= invSize;
        covar12 *= invSize;
        covar22 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);
        mParameters.center = mean;
        mParameters.axis[0] = evec[0];
        mParameters.axis[1] = evec[1];
        mParameters.axis[2] = evec[2];
        mParameters.extent = eval;
    }

    mParameters.center = Vector3<Real>::Zero();
    mParameters.axis[0] = Vector3<Real>::Zero();
    mParameters.axis[1] = Vector3<Real>::Zero();
    mParameters.axis[2] = Vector3<Real>::Zero();
    mParameters.extent = Vector3<Real>::Zero();
    return false;
}


}
