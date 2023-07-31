// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteLog2Estimate.h>

// Minimax polynomial approximations to log2(x).  The polynomial p(x) of
// degree D minimizes the quantity maximum{|log2(x) - p(x)| : x in [1,2]}
// over all polynomials of degree D.  The natural logarithm is computed
// using log(x) = log2(x)/log2(e) = log2(x)*log(2).

namespace gte
{

template <typename Real>
class LogEstimate
{
public:
    // The input constraint is x in [1,2].  For example,
    //   float x; // in [1,2]
    //   float result = LogEstimate<float>::Degree<3>(x);
    template <int D>
    inline static Real Degree(Real x);

    // The input constraint is x > 0.  Range reduction is used to generate a
    // value y in (0,1], call Degree(y), and add the exponent for the power
    // of two in the binary scientific representation of x.  For example,
    //   float x;  // x > 0
    //   float result = LogEstimate<float>::DegreeRR<3>(x);
    template <int D>
    inline static Real DegreeRR(Real x);
};


template <typename Real>
template <int D>
inline Real LogEstimate<Real>::Degree(Real x)
{
    return Log2Estimate<Real>::Degree<D>(x) * (Real)GTE_C_INV_LN_2;
}

template <typename Real>
template <int D>
inline Real LogEstimate<Real>::DegreeRR(Real x)
{
    return Log2Estimate<Real>::DegreeRR<D>(x) * (Real)GTE_C_INV_LN_2;
}


}
