// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace gte
{

template <typename Real>
class IntpAkima1
{
protected:
    // Construction (abstract base class).
    IntpAkima1(int quantity, Real const* F);
public:
    // Abstract base class.
    virtual ~IntpAkima1();

    // Member access.
    inline int GetQuantity() const;
    inline Real const* GetF() const;
    virtual Real GetXMin() const = 0;
    virtual Real GetXMax() const = 0;

    // Evaluate the function and its derivatives.  The functions clamp the
    // inputs to xmin <= x <= xmax.  The first operator is for function
    // evaluation.  The second operator is for function or derivative
    // evaluations.  The 'order' argument is the order of the derivative or
    // zero for the function itself.
    Real operator()(Real x) const;
    Real operator()(int order, Real x) const;

protected:
    class Polynomial
    {
    public:
        // P(x) = c[0] + c[1]*x + c[2]*x^2 + c[3]*x^3
        inline Real& operator[](int i);
        Real operator()(Real x) const;
        Real operator()(int order, Real x) const;

    private:
        Real mCoeff[4];
    };

    Real ComputeDerivative(Real* slope) const;
    virtual void Lookup(Real x, int& index, Real& dx) const = 0;

    int mQuantity;
    Real const* mF;
    std::vector<Polynomial> mPoly;
};


template <typename Real>
IntpAkima1<Real>::IntpAkima1(int quantity, Real const* F)
    :
    mQuantity(quantity),
    mF(F)
{
    // At least three data points are needed to construct the estimates of
    // the boundary derivatives.
    LogAssert(mQuantity >= 3 && F, "Invalid input.");

    mPoly.resize(mQuantity - 1);
}

template <typename Real>
IntpAkima1<Real>::~IntpAkima1()
{
}

template <typename Real> inline
int IntpAkima1<Real>::GetQuantity() const
{
    return mQuantity;
}

template <typename Real> inline
Real const* IntpAkima1<Real>::GetF() const
{
    return mF;
}

template <typename Real>
Real IntpAkima1<Real>::operator()(Real x) const
{
    x = std::min(std::max(x, GetXMin()), GetXMax());
    int index;
    Real dx;
    Lookup(x, index, dx);
    return mPoly[index](dx);
}

template <typename Real>
Real IntpAkima1<Real>::operator()(int order, Real x) const
{
    x = std::min(std::max(x, GetXMin()), GetXMax());
    int index;
    Real dx;
    Lookup(x, index, dx);
    return mPoly[index](order, dx);
}

template <typename Real>
Real IntpAkima1<Real>::ComputeDerivative(Real* slope) const
{
    if (slope[1] != slope[2])
    {
        if (slope[0] != slope[1])
        {
            if (slope[2] != slope[3])
            {
                Real ad0 = std::abs(slope[3] - slope[2]);
                Real ad1 = std::abs(slope[0] - slope[1]);
                return (ad0 * slope[1] + ad1 * slope[2]) / (ad0 + ad1);
            }
            else
            {
                return slope[2];
            }
        }
        else
        {
            if (slope[2] != slope[3])
            {
                return slope[1];
            }
            else
            {
                return ((Real)0.5) * (slope[1] + slope[2]);
            }
        }
    }
    else
    {
        return slope[1];
    }
}

template <typename Real> inline
Real& IntpAkima1<Real>::Polynomial::operator[](int i)
{
    return mCoeff[i];
}

template <typename Real>
Real IntpAkima1<Real>::Polynomial::operator()(Real x) const
{
    return mCoeff[0] + x * (mCoeff[1] + x * (mCoeff[2] + x * mCoeff[3]));
}

template <typename Real>
Real IntpAkima1<Real>::Polynomial::operator()(int order, Real x) const
{
    switch (order)
    {
    case 0:
        return mCoeff[0] + x * (mCoeff[1] + x * (mCoeff[2] + x * mCoeff[3]));
    case 1:
        return mCoeff[1] + x * (((Real)2)*mCoeff[2] + x * ((Real)3) * mCoeff[3]);
    case 2:
        return ((Real)2) * mCoeff[2] + x * ((Real)6) * mCoeff[3];
    case 3:
        return ((Real)6) * mCoeff[3];
    }

    return (Real)0;
}


}
