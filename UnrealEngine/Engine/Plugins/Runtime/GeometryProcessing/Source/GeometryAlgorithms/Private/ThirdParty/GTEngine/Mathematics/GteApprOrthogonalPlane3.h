// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector3.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>
#include <ThirdParty/GTEngine/Mathematics/GteSymmetricEigensolver3x3.h>

// Least-squares fit of a plane to (x,y,z) data by using distance measurements
// orthogonal to the proposed plane.  The return value is 'true' iff the fit
// is unique (always successful, 'true' when a minimum eigenvalue is unique).
// The mParameters value is (P,N) = (origin,normal).  The error for
// S = (x0,y0,z0) is (S-P)^T*(I - N*N^T)*(S-P).

namespace gte
{

template <typename Real>
class ApprOrthogonalPlane3
    :
    public ApprQuery<Real, ApprOrthogonalPlane3<Real>, Vector3<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprOrthogonalPlane3();

    // Basic fitting algorithm.
    bool Fit(int numPoints, Vector3<Real> const* points);
    std::pair<Vector3<Real>, Vector3<Real>> const& GetParameters() const;

    // Functions called by ApprQuery::RANSAC.  See GteApprQuery.h for a
    // detailed description.
    int GetMinimumRequired() const;
    Real Error(Vector3<Real> const& observation) const;
    bool Fit(std::vector<Vector3<Real>> const& observations,
        std::vector<int> const& indices);

private:
    std::pair<Vector3<Real>, Vector3<Real>> mParameters;
};


template <typename Real>
ApprOrthogonalPlane3<Real>::ApprOrthogonalPlane3()
{
    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
}

template <typename Real>
bool ApprOrthogonalPlane3<Real>::Fit(int numPoints,
    Vector3<Real> const* points)
{
    if (numPoints >= GetMinimumRequired() && points)
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            mean += points[i];
        }
        mean /= (Real)numPoints;

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

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);

        // The plane normal is the eigenvector in the direction of smallest
        // variance of the points.
        mParameters.first = mean;
        mParameters.second = evec[0];

        // The fitted plane is unique when the minimum eigenvalue has
        // multiplicity 1.
        return eval[0] < eval[1];
    }

    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
    return false;
}

template <typename Real>
std::pair<Vector3<Real>, Vector3<Real>> const&
ApprOrthogonalPlane3<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprOrthogonalPlane3<Real>::GetMinimumRequired() const
{
    return 3;
}

template <typename Real>
Real ApprOrthogonalPlane3<Real>::Error(Vector3<Real> const& observation)
const
{
    Vector3<Real> diff = observation - mParameters.first;
    Real sqrlen = Dot(diff, diff);
    Real dot = Dot(diff, mParameters.second);
    Real error = std::abs(sqrlen - dot*dot);
    return error;
}

template <typename Real>
bool ApprOrthogonalPlane3<Real>::Fit(
    std::vector<Vector3<Real>> const& observations,
    std::vector<int> const& indices)
{
    if (static_cast<int>(indices.size()) >= GetMinimumRequired())
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (auto index : indices)
        {
            mean += observations[index];
        }
        mean /= (Real)indices.size();

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

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);

        // The plane normal is the eigenvector in the direction of smallest
        // variance of the points.
        mParameters.first = mean;
        mParameters.second = evec[0];

        // The fitted plane is unique when the minimum eigenvalue has
        // multiplicity 1.
        return eval[0] < eval[1];
    }

    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
    return false;
}


}
