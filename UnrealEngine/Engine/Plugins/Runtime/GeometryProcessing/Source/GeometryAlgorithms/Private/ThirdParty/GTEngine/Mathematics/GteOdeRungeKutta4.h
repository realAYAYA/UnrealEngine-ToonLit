// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteOdeSolver.h>

// The TVector template parameter allows you to create solvers with
// Vector<N,Real> when the dimension N is known at compile time or
// GVector<Real> when the dimension N is known at run time.  Both classes
// have 'int GetSize() const' that allow OdeSolver-derived classes to query
// for the dimension.

namespace gte
{

template <typename Real, typename TVector>
class OdeRungeKutta4 : public OdeSolver<Real,TVector>
{
public:
    // Construction and destruction.
    virtual ~OdeRungeKutta4();
    OdeRungeKutta4(Real tDelta,
        std::function<TVector(Real, TVector const&)> const& F);

    // Estimate x(t + tDelta) from x(t) using dx/dt = F(t,x).  You may allow
    // xIn and xOut to be the same object.
    virtual void Update(Real tIn, TVector const& xIn, Real& tOut,
        TVector& xOut);
};


template <typename Real, typename TVector>
OdeRungeKutta4<Real, TVector>::~OdeRungeKutta4()
{
}

template <typename Real, typename TVector>
OdeRungeKutta4<Real, TVector>::OdeRungeKutta4(Real tDelta,
    std::function<TVector(Real, TVector const&)> const& F)
    :
    OdeSolver<Real, TVector>(tDelta, F)
{
}

template <typename Real, typename TVector>
void OdeRungeKutta4<Real, TVector>::Update(Real tIn, TVector const& xIn,
    Real& tOut, TVector& xOut)
{
    // Compute the first step.
    Real halfTDelta = ((Real)0.5) * this->mTDelta;
    TVector fTemp1 = this->mFunction(tIn, xIn);
    TVector xTemp = xIn + halfTDelta * fTemp1;

    // Compute the second step.
    Real halfT = tIn + halfTDelta;
    TVector fTemp2 = this->mFunction(halfT, xTemp);
    xTemp = xIn + halfTDelta * fTemp2;

    // Compute the third step.
    TVector fTemp3 = this->mFunction(halfT, xTemp);
    xTemp = xIn + this->mTDelta * fTemp3;

    // Compute the fourth step.
    Real sixthTDelta = this->mTDelta / (Real)6;
    tOut = tIn + this->mTDelta;
    TVector fTemp4 = this->mFunction(tOut, xTemp);
    xOut = xIn + sixthTDelta * (
        fTemp1 + ((Real)2)*(fTemp2 + fTemp3) + fTemp4);
}


}
