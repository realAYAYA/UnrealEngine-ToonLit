// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteGMatrix.h>
#include <ThirdParty/GTEngine/Mathematics/GteOdeSolver.h>

// The TVector template parameter allows you to create solvers with
// Vector<N,Real> when the dimension N is known at compile time or
// GVector<Real> when the dimension N is known at run time.  Both classes
// have 'int GetSize() const' that allow OdeSolver-derived classes to query
// for the dimension.  The TMatrix parameter must be either Matrix<N,N,Real>
// or GMatrix<Real> accordingly.
//
// The function F(t,x) has input t, a scalar, and input x, an N-vector.
// The first derivative matrix with respect to x is DF(t,x), an
// N-by-N matrix.  Entry DF(r,c) is the derivative of F[r] with
// respect to x[c].

namespace gte
{

template <typename Real, typename TVector, typename TMatrix>
class OdeImplicitEuler : public OdeSolver<Real,TVector>
{
public:
    // Construction and destruction.
    virtual ~OdeImplicitEuler();
    OdeImplicitEuler(Real tDelta,
        std::function<TVector(Real, TVector const&)> const& F,
        std::function<TMatrix(Real, TVector const&)> const& DF);

    // Estimate x(t + tDelta) from x(t) using dx/dt = F(t,x).  You may allow
    // xIn and xOut to be the same object.
    virtual void Update(Real tIn, TVector const& xIn, Real& tOut,
        TVector& xOut);

private:
    std::function<TMatrix(Real, TVector const&)> mDerivativeFunction;
};


template <typename Real, typename TVector, typename TMatrix>
OdeImplicitEuler<Real, TVector, TMatrix>::~OdeImplicitEuler()
{
}

template <typename Real, typename TVector, typename TMatrix>
OdeImplicitEuler<Real, TVector, TMatrix>::OdeImplicitEuler(Real tDelta,
    std::function<TVector(Real, TVector const&)> const& F,
    std::function<TMatrix(Real, TVector const&)> const& DF)
    :
    OdeSolver<Real, TVector>(tDelta, F),
    mDerivativeFunction(DF)
{
}

template <typename Real, typename TVector, typename TMatrix>
void OdeImplicitEuler<Real, TVector, TMatrix>::Update(Real tIn,
    TVector const& xIn, Real& tOut, TVector& xOut)
{
    TVector fVector = this->mFunction(tIn, xIn);
    TMatrix dfMatrix = mDerivativeFunction(tIn, xIn);
    TMatrix dgMatrix = TMatrix::Identity() - this->mTDelta * dfMatrix;
    TMatrix dgInverse = Inverse(dgMatrix);
    fVector = dgInverse * fVector;
    tOut = tIn + this->mTDelta;
    xOut = xIn + this->mTDelta * fVector;
}


}
