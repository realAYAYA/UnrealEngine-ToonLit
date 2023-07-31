// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMatrix3x3.h>
#include <ThirdParty/GTEngine/Mathematics/GteApprQuery.h>

// Least-squares fit of a plane to height data (x,y,f(x,y)).  The plane is of
// the form (z - zAvr) = a*(x - xAvr) + b*(y - yAvr), where (xAvr,yAvr,zAvr)
// is the average of the sample points.  The return value is 'true' iff the
// fit is successful (the input points are noncollinear).  The mParameters
// values are ((xAvr,yAvr,zAvr),(a,b,-1)) on success and ((0,0,0),(0,0,0)) on
// failure.  The error for (x0,y0,z0) is [a*(x0-xAvr)+b*(y0-yAvr)-(z0-zAvr)]^2.

namespace gte
{

template <typename Real>
class ApprHeightPlane3
    :
    public ApprQuery<Real, ApprHeightPlane3<Real>, Vector3<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprHeightPlane3();

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
ApprHeightPlane3<Real>::ApprHeightPlane3()
{
    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
}

template <typename Real>
bool ApprHeightPlane3<Real>::Fit(int numPoints, Vector3<Real> const* points)
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
        Real covar11 = (Real)0, covar12 = (Real)0;
        for (int i = 0; i < numPoints; ++i)
        {
            Vector3<Real> diff = points[i] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
        }

        // Decompose the covariance matrix.
        Real det = covar00*covar11 - covar01*covar01;
        if (det != (Real)0)
        {
            Real invDet = ((Real)1) / det;
            mParameters.first = mean;
            mParameters.second[0] =
                (covar11*covar02 - covar01*covar12)*invDet;
            mParameters.second[1] =
                (covar00*covar12 - covar01*covar02)*invDet;
            mParameters.second[2] = (Real)-1;
            return true;
        }
    }

    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
    return false;
}

template <typename Real>
std::pair<Vector3<Real>, Vector3<Real>> const&
ApprHeightPlane3<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprHeightPlane3<Real>::GetMinimumRequired() const
{
    return 3;
}

template <typename Real>
Real ApprHeightPlane3<Real>::Error(Vector3<Real> const& observation) const
{
    Real d = Dot(observation - mParameters.first, mParameters.second);
    Real error = d*d;
    return error;
}

template <typename Real>
bool ApprHeightPlane3<Real>::Fit(
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
        Real covar11 = (Real)0, covar12 = (Real)0;
        for (auto index : indices)
        {
            Vector3<Real> diff = observations[index] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
        }

        // Decompose the covariance matrix.
        Real det = covar00*covar11 - covar01*covar01;
        if (det != (Real)0)
        {
            Real invDet = ((Real)1) / det;
            mParameters.first = mean;
            mParameters.second[0] =
                (covar11*covar02 - covar01*covar12)*invDet;
            mParameters.second[1] =
                (covar00*covar12 - covar01*covar02)*invDet;
            mParameters.second[2] = (Real)-1;
            return true;
        }
    }

    mParameters.first = Vector3<Real>::Zero();
    mParameters.second = Vector3<Real>::Zero();
    return false;
}


}
