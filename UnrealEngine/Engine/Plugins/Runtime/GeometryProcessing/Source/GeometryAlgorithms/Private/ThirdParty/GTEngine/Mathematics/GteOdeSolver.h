// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteGVector.h>
#include <functional>

// The differential equation is dx/dt = F(t,x).  The TVector template
// parameter allows you to create solvers with Vector<N,Real> when the
// dimension N is known at compile time or GVector<Real> when the dimension
// N is known at run time.  Both classes have 'int GetSize() const' that
// allow OdeSolver-derived classes to query for the dimension.

namespace gte
{

template <typename Real, typename TVector>
class OdeSolver
{
public:
    // Abstract base class.
public:
    virtual ~OdeSolver();
protected:
    OdeSolver(Real tDelta,
        std::function<TVector(Real, TVector const&)> const& F);

public:
    // Member access.
    inline void SetTDelta(Real tDelta);
    inline Real GetTDelta() const;

    // Estimate x(t + tDelta) from x(t) using dx/dt = F(t,x).  The
    // derived classes implement this so that it is possible for xIn and
    // xOut to be the same object.
    virtual void Update(Real tIn, TVector const& xIn, Real& tOut,
        TVector& xOut) = 0;

protected:
    Real mTDelta;
    std::function<TVector(Real, TVector const&)> mFunction;
};


template <typename Real, typename TVector>
OdeSolver<Real, TVector>::~OdeSolver()
{
}

template <typename Real, typename TVector>
OdeSolver<Real, TVector>::OdeSolver(Real tDelta,
    std::function<TVector(Real, TVector const&)> const& F)
    :
    mTDelta(tDelta),
    mFunction(F)
{
}

template <typename Real, typename TVector> inline
void OdeSolver<Real, TVector>::SetTDelta(Real tDelta)
{
    mTDelta = tDelta;
}

template <typename Real, typename TVector> inline
Real OdeSolver<Real, TVector>::GetTDelta() const
{
    return mTDelta;
}


}
