// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/02/17)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteGMatrix.h>
#include <ThirdParty/GTEngine/Mathematics/GteParametricCurve.h>

namespace gte
{

template <int N, typename Real>
class NaturalSplineCurve : public ParametricCurve<N, Real>
{
public:
    // Construction and destruction.  The object copies the input arrays.  The
    // number of points M must be at least 2.  The first constructor is for a
    // spline with second derivatives zero at the endpoints (isFree = true)
    // or a spline that is closed (isFree = false).  The second constructor is
    // for clamped splines, where you specify the first derivatives at the
    // endpoints.  Usually, derivative0 = points[1] - points[0] at the first
    // point and derivative1 = points[M-1] - points[M-2].  To validate
    // construction, create an object as shown:
    //     NaturalSplineCurve<N, Real> curve(parameters);
    //     if (!curve) { <constructor failed, handle accordingly>; }
    virtual ~NaturalSplineCurve();
    NaturalSplineCurve(bool isFree, int numPoints,
        Vector<N, Real> const* points, Real const* times);
    NaturalSplineCurve(int numPoints, Vector<N, Real> const* points,
        Real const* times, Vector<N, Real> const& derivative0,
        Vector<N, Real> const& derivative1);

    // Member access.
    inline int GetNumPoints() const;
    inline Vector<N, Real> const* GetPoints() const;

    // Evaluation of the curve.  The function supports derivative calculation
    // through order 3; that is, maxOrder <= 3 is required.  If you want
    // only the position, pass in maxOrder of 0.  If you want the position and
    // first derivative, pass in maxOrder of 1, and so on.  The output
    // 'values' are ordered as: position, first derivative, second derivative,
    // third derivative.
    virtual void Evaluate(Real t, unsigned int maxOrder,
        Vector<N, Real> values[4]) const;

protected:
    // Support for construction.
    void CreateFree();
    void CreateClosed();
    void CreateClamped(Vector<N, Real> const& derivative0,
        Vector<N, Real> const& derivative1);

    // Determine the index i for which times[i] <= t < times[i+1].
    void GetKeyInfo(Real t, int& key, Real& dt) const;

    // Polynomial coefficients.  mA are the points (constant coefficients of
    // polynomials.  mB are the degree 1 coefficients, mC are the degree 2
    // coefficients, and mD are the degree 3 coefficients.
    std::vector<Vector<N, Real>> mA, mB, mC, mD;
};


template <int N, typename Real>
NaturalSplineCurve<N, Real>::~NaturalSplineCurve()
{
}

template <int N, typename Real>
NaturalSplineCurve<N, Real>::NaturalSplineCurve(bool isFree, int numPoints,
    Vector<N, Real> const* points, Real const* times)
    :
    ParametricCurve<N, Real>(numPoints - 1, times)
{
    if (numPoints < 2 || !points)
    {
        LogError("Invalid input.");
        return;
    }

    mA.resize(numPoints);
    std::copy(points, points + numPoints, mA.begin());

    if (isFree)
    {
        CreateFree();
    }
    else
    {
        CreateClosed();
    }

    this->mConstructed = true;
}

template <int N, typename Real>
NaturalSplineCurve<N, Real>::NaturalSplineCurve(int numPoints,
    Vector<N, Real> const* points, Real const* times,
    Vector<N, Real> const& derivative0, Vector<N, Real> const& derivative1)
    :
    ParametricCurve<N, Real>(numPoints - 1, times)
{
    if (numPoints < 2 || !points)
    {
        LogError("Invalid input.");
        return;
    }

    mA.resize(numPoints);
    std::copy(points, points + numPoints, mA.begin());

    CreateClamped(derivative0, derivative1);
    this->mConstructed = true;
}

template <int N, typename Real> inline
int NaturalSplineCurve<N, Real>::GetNumPoints() const
{
    return static_cast<int>(mA.size());
}

template <int N, typename Real> inline
Vector<N, Real> const* NaturalSplineCurve<N, Real>::GetPoints() const
{
    return &mA[0];
}

template <int N, typename Real>
void NaturalSplineCurve<N, Real>::Evaluate(Real t, unsigned int maxOrder,
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

    int key = 0;
    Real dt = (Real)0;
    GetKeyInfo(t, key, dt);

    // Compute position.
    values[0] = mA[key] + dt * (mB[key] + dt * (mC[key] + dt * mD[key]));
    if (maxOrder >= 1)
    {
        // Compute first derivative.
        values[1] = mB[key] + dt * (((Real)2) * mC[key] +
            ((Real)3) * dt * mD[key]);
        if (maxOrder >= 2)
        {
            // Compute second derivative.
            values[2] = ((Real)2) * mC[key] + ((Real)6) * dt * mD[key];
            if (maxOrder == 3)
            {
                values[3] = ((Real)6) * mD[key];
            }
            else
            {
                values[3].MakeZero();
            }
        }
    }
}

template <int N, typename Real>
void NaturalSplineCurve<N, Real>::CreateFree()
{
    int numSegments = GetNumPoints() - 1;
    std::vector<Real> dt(numSegments);
    for (int i = 0; i < numSegments; ++i)
    {
        dt[i] = this->mTime[i + 1] - this->mTime[i];
    }

    std::vector<Real> d2t(numSegments);
    for (int i = 1; i < numSegments; ++i)
    {
        d2t[i] = this->mTime[i + 1] - this->mTime[i - 1];
    }

    std::vector<Vector<N, Real>> alpha(numSegments);
    for (int i = 1; i < numSegments; ++i)
    {
        Vector<N, Real> numer = ((Real)3)*(dt[i - 1] * mA[i + 1] -
            d2t[i] * mA[i] + dt[i] * mA[i - 1]);
        Real invDenom = ((Real)1) / (dt[i - 1] * dt[i]);
        alpha[i] = invDenom * numer;
    }

    std::vector<Real> ell(numSegments + 1);
    std::vector<Real> mu(numSegments);
    std::vector<Vector<N, Real>> z(numSegments + 1);
    Real inv;

    ell[0] = (Real)1;
    mu[0] = (Real)0;
    z[0].MakeZero();
    for (int i = 1; i < numSegments; ++i)
    {
        ell[i] = ((Real)2) * d2t[i] - dt[i - 1] * mu[i - 1];
        inv = ((Real)1) / ell[i];
        mu[i] = inv * dt[i];
        z[i] = inv * (alpha[i] - dt[i - 1] * z[i - 1]);
    }
    ell[numSegments] = (Real)1;
    z[numSegments].MakeZero();

    mB.resize(numSegments);
    mC.resize(numSegments + 1);
    mD.resize(numSegments);

    Real oneThird = ((Real)1) / (Real)3;
    mC[numSegments].MakeZero();
    for (int i = numSegments - 1; i >= 0; --i)
    {
        mC[i] = z[i] - mu[i] * mC[i + 1];
        inv = ((Real)1) / dt[i];
        mB[i] = inv * (mA[i + 1] - mA[i]) - oneThird * dt[i] * (mC[i + 1] +
            ((Real)2) * mC[i]);
        mD[i] = oneThird * inv * (mC[i + 1] - mC[i]);
    }
}

template <int N, typename Real>
void NaturalSplineCurve<N, Real>::CreateClosed()
{
    // TODO:  A general linear system solver is used here.  The matrix
    // corresponding to this case is actually "cyclic banded", so a faster
    // linear solver can be used.  The current linear system code does not
    // have such a solver.

    int numSegments = GetNumPoints() - 1;
    std::vector<Real> dt(numSegments);
    for (int i = 0; i < numSegments; ++i)
    {
        dt[i] = this->mTime[i + 1] - this->mTime[i];
    }

    // Construct matrix of system.
    GMatrix<Real> mat(numSegments + 1, numSegments + 1);
    mat(0, 0) = (Real)1;
    mat(0, numSegments) = (Real)-1;
    for (int i = 1; i <= numSegments - 1; ++i)
    {
        mat(i, i - 1) = dt[i - 1];
        mat(i, i) = ((Real)2) * (dt[i - 1] + dt[i]);
        mat(i, i + 1) = dt[i];
    }
    mat(numSegments, numSegments - 1) = dt[numSegments - 1];
    mat(numSegments, 0) = ((Real)2) * (dt[numSegments - 1] + dt[0]);
    mat(numSegments, 1) = dt[0];

    // Construct right-hand side of system.
    mC.resize(numSegments + 1);
    mC[0].MakeZero();
    Real inv0, inv1;
    for (int i = 1; i <= numSegments - 1; ++i)
    {
        inv0 = ((Real)1) / dt[i];
        inv1 = ((Real)1) / dt[i - 1];
        mC[i] = ((Real)3) * (inv0 * (mA[i + 1] - mA[i]) -
            inv1*(mA[i] - mA[i - 1]));
    }
    inv0 = ((Real)1) / dt[0];
    inv1 = ((Real)1) / dt[numSegments - 1];
    mC[numSegments] = ((Real)3) * (inv0 * (mA[1] - mA[0]) -
        inv1 * (mA[0] - mA[numSegments - 1]));

    // Solve the linear systems.
    GMatrix<Real> invMat = Inverse(mat);
    GVector<Real> input(numSegments + 1);
    GVector<Real> output(numSegments + 1);
    for (int j = 0; j < N; ++j)
    {
        for (int i = 0; i <= numSegments; ++i)
        {
            input[i] = mC[i][j];
        }
        output = invMat * input;
        for (int i = 0; i <= numSegments; ++i)
        {
            mC[i][j] = output[i];
        }
    }

    Real oneThird = ((Real)1) / (Real)3;
    mB.resize(numSegments);
    mD.resize(numSegments);
    for (int i = 0; i < numSegments; ++i)
    {
        inv0 = ((Real)1) / dt[i];
        mB[i] = inv0 * (mA[i + 1] - mA[i]) - oneThird * (mC[i + 1] +
            ((Real)2) * mC[i]) * dt[i];
        mD[i] = oneThird * inv0 * (mC[i + 1] - mC[i]);
    }
}

template <int N, typename Real>
void NaturalSplineCurve<N, Real>::CreateClamped(
    Vector<N, Real> const& derivative0, Vector<N, Real> const& derivative1)
{
    int numSegments = GetNumPoints() - 1;
    std::vector<Real> dt(numSegments);
    for (int i = 0; i < numSegments; ++i)
    {
        dt[i] = this->mTime[i + 1] - this->mTime[i];
    }

    std::vector<Real> d2t(numSegments);
    for (int i = 1; i < numSegments; ++i)
    {
        d2t[i] = this->mTime[i + 1] - this->mTime[i - 1];
    }

    std::vector<Vector<N, Real>> alpha(numSegments + 1);
    Real inv = ((Real)1) / dt[0];
    alpha[0] = ((Real)3) * (inv * (mA[1] - mA[0]) - derivative0);
    inv = ((Real)1) / dt[numSegments - 1];
    alpha[numSegments] = ((Real)3)*(derivative1 -
        inv * (mA[numSegments] - mA[numSegments - 1]));
    for (int i = 1; i < numSegments; ++i)
    {
        Vector<N, Real> numer = ((Real)3)*(dt[i - 1] * mA[i + 1] -
            d2t[i] * mA[i] + dt[i] * mA[i - 1]);
        Real invDenom = ((Real)1) / (dt[i - 1] * dt[i]);
        alpha[i] = invDenom*numer;
    }

    std::vector<Real> ell(numSegments + 1);
    std::vector<Real> mu(numSegments);
    std::vector<Vector<N, Real>> z(numSegments + 1);

    ell[0] = ((Real)2) * dt[0];
    mu[0] = (Real)0.5;
    inv = ((Real)1) / ell[0];
    z[0] = inv * alpha[0];

    for (int i = 1; i < numSegments; ++i)
    {
        ell[i] = ((Real)2) * d2t[i] - dt[i - 1] * mu[i - 1];
        inv = ((Real)1) / ell[i];
        mu[i] = inv * dt[i];
        z[i] = inv * (alpha[i] - dt[i - 1] * z[i - 1]);
    }
    ell[numSegments] = dt[numSegments - 1] *
        (((Real)2) - mu[numSegments - 1]);
    inv = ((Real)1) / ell[numSegments];
    z[numSegments] = inv * (alpha[numSegments] - dt[numSegments - 1] *
        z[numSegments - 1]);

    mB.resize(numSegments);
    mC.resize(numSegments + 1);
    mD.resize(numSegments);

    Real oneThird = ((Real)1) / (Real)3;
    mC[numSegments] = z[numSegments];
    for (int i = numSegments - 1; i >= 0; --i)
    {
        mC[i] = z[i] - mu[i] * mC[i + 1];
        inv = ((Real)1) / dt[i];
        mB[i] = inv * (mA[i + 1] - mA[i]) - oneThird*dt[i] * (mC[i + 1] +
            ((Real)2) * mC[i]);
        mD[i] = oneThird * inv * (mC[i + 1] - mC[i]);
    }
}

template <int N, typename Real>
void NaturalSplineCurve<N, Real>::GetKeyInfo(Real t, int& key, Real& dt) const
{
    int numSegments = GetNumPoints() - 1;
    if (t <= this->mTime[0])
    {
        key = 0;
        dt = (Real)0;
    }
    else if (t >= this->mTime[numSegments])
    {
        key = numSegments - 1;
        dt = this->mTime[numSegments] - this->mTime[numSegments - 1];
    }
    else
    {
        for (int i = 0; i < numSegments; ++i)
        {
            if (t < this->mTime[i + 1])
            {
                key = i;
                dt = t - this->mTime[i];
                break;
            }
        }
    }
}


}
