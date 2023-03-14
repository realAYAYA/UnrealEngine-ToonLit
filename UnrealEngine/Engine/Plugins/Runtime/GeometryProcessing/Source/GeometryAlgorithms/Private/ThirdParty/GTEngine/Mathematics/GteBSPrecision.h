// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstdint>
#include <limits>

// Support for determining the number of bits of precision required to compute
// an expression.  See the document
//   http://www.geometrictools.com/Documentation/ArbitraryPrecision.pdf
// for an example of how to use this class.

namespace gte
{

class BSPrecision
{
public:
    // This constructor is used for 'float' or 'double'.  The floating-point
    // inputs for the expressions have no restrictions; that is, the inputs
    // can be any finite floating-point numbers (normal or subnormal).
    BSPrecision(bool isFloat, bool forBSNumber);

    // If you know that your inputs are limited in magnitude, use this
    // constructor.  For example, if you know that your inputs x satisfy
    // |x| <= 8, you can specify maxExponent of 3.  The minimum power will
    // still be that for the smallest positive subnormal.
    BSPrecision(bool isFloat, int32_t maxExponent, bool forBSNumber);

    // You must use this constructor carefully based on knowledge of your
    // expressions.  For example, if you know that your inputs are 'float'
    // and in the interval [1,2), you would choose 24 for the number of bits
    // of precision, a minimum biased exponent of -23 because the largest
    // 'float' smaller than 2 is 1.1^{23}*2^0 = 1^{24}*2^{-23}, and a maximum
    // exponent of 0.  These numbers work to determine bits of precision to
    // compute x*y+z*w.  However, if you then compute an expression such as
    // x-y for x and y in [1,2) and multiply by powers of 1/2, the bit
    // counting will not be correct because the results can be subnormals
    // where the minimum biased exponent is -149, not -23.
    BSPrecision(int32_t numBits, int32_t minBiasedExponent,
        int32_t maxExponent, bool forBSNumber);

    // Member access.
    int32_t GetNumWords() const;
    int32_t GetNumBits() const;
    int32_t GetMinBiasedExponent() const;
    int32_t GetMinExponent() const;
    int32_t GetMaxExponent() const;

    // Support for determining the number of bits of precision required to
    // compute an expression using BSNumber or BSRational.
    BSPrecision operator*(BSPrecision const& precision);
    BSPrecision operator+(BSPrecision const& precision);
    BSPrecision operator-(BSPrecision const& precision);

    // Support for determining the number of bits of precision required to
    // compute a BSRational expression (operations not relevant for BSNumber).
    BSPrecision operator/(BSPrecision const& precision);
    BSPrecision operator==(BSPrecision const& precision);
    BSPrecision operator!=(BSPrecision const& precision);
    BSPrecision operator< (BSPrecision const& precision);
    BSPrecision operator<=(BSPrecision const& precision);
    BSPrecision operator> (BSPrecision const& precision);
    BSPrecision operator>=(BSPrecision const& precision);

private:
    int32_t mNumBits, mMinBiasedExponent, mMaxExponent;
    bool mForBSNumber;
};

}
