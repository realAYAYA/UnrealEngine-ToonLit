// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/Mathematics/GteBSPrecision.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <algorithm>
using namespace gte;


BSPrecision::BSPrecision(bool isFloat, bool forBSNumber)
    :
    mForBSNumber(forBSNumber)
{
    // NOTE:  It is not clear what the rationale is for the C++ Standard
    // Library to set numeric_limits<float>::min_exponent to -125
    // instead of -126; same issue with numeric_limits<double>::min_exponent
    // set to -1021 instead of -1022.  Similarly, it is not clear why
    // numeric_limits<float>::max_exponent is set to 128 when the maximum
    // finite 'float' has exponent 127.  The exponent 128 is used in the
    // 'float' infinity, but treating this as 2^{128} does not seem to be
    // consistent with the IEEE 754-2008 standard.  Same issue with
    // numeric_limits<double>::max_exponent set to 1024 rather than 1023.

    if (isFloat)
    {
        mNumBits = std::numeric_limits<float>::digits;  // 24
        mMinBiasedExponent =
            std::numeric_limits<float>::min_exponent - mNumBits;  // -149
        mMaxExponent = std::numeric_limits<float>::max_exponent - 1;  // 127
    }
    else
    {
        mNumBits = std::numeric_limits<double>::digits;  // 53
        mMinBiasedExponent =
            std::numeric_limits<double>::min_exponent - mNumBits;  // -1074
        mMaxExponent = std::numeric_limits<double>::max_exponent - 1;  // 1023
    }
}

BSPrecision::BSPrecision(bool isFloat, int32_t maxExponent, bool forBSNumber)
    :
    mForBSNumber(forBSNumber)
{
    if (isFloat)
    {
        mNumBits = std::numeric_limits<float>::digits;  // 24
        mMinBiasedExponent =
            std::numeric_limits<float>::min_exponent - mNumBits;  // -149
        mMaxExponent = maxExponent;
    }
    else
    {
        mNumBits = std::numeric_limits<double>::digits;  // 53
        mMinBiasedExponent =
            std::numeric_limits<double>::min_exponent - mNumBits;  // -1074
        mMaxExponent = maxExponent;
    }
}

BSPrecision::BSPrecision(int32_t numBits, int32_t minBiasedExponent,
    int32_t maxExponent, bool forBSNumber)
    :
    mNumBits(numBits),
    mMinBiasedExponent(minBiasedExponent),
    mMaxExponent(maxExponent),
    mForBSNumber(forBSNumber)
{
}

int32_t BSPrecision::GetNumWords() const
{
    return mNumBits / 32 + ((mNumBits % 32) > 0 ? 1 : 0);
}

int32_t BSPrecision::GetNumBits() const
{
    return mNumBits;
}

int32_t BSPrecision::GetMinBiasedExponent() const
{
    return mMinBiasedExponent;
}

int32_t BSPrecision::GetMinExponent() const
{
    return mMinBiasedExponent + mNumBits - 1;
}

int32_t BSPrecision::GetMaxExponent() const
{
    return mMaxExponent;
}

BSPrecision BSPrecision::operator*(BSPrecision const& precision)
{
    // BSRational operator* involves a product of numerators and a product
    // of denominators.  In worst case, the precision requirements are the
    // same as a BSNumber operator*, so testing of mForBSNumber is not
    // needed.
    int32_t numBits = mNumBits + precision.mNumBits;
    int32_t maxExponent = mMaxExponent + precision.mMaxExponent + 1;
    int32_t minBiasedExponent =
        mMinBiasedExponent + precision.mMinBiasedExponent;
    return BSPrecision(numBits, minBiasedExponent, maxExponent, mForBSNumber);
}

BSPrecision BSPrecision::operator+(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        int32_t minBiasedExponent =
            std::min(mMinBiasedExponent, precision.mMinBiasedExponent);
        int32_t maxExponent =
            std::max(mMaxExponent, precision.mMaxExponent) + 1;
        int32_t numBits = maxExponent - minBiasedExponent;
        return BSPrecision(numBits, minBiasedExponent, maxExponent,
            mForBSNumber);
    }
    else
    {
        // n0/d0 + n1/d1 = (n0*d1 + n1*d0) / (d0*d1)
        BSPrecision product = operator*(precision);
        product.mForBSNumber = true;
        BSPrecision sum = product + product;
        sum.mForBSNumber = false;
        BSPrecision division = sum / product;
        division.mForBSNumber = false;
        return division;
    }
}

BSPrecision BSPrecision::operator-(BSPrecision const& precision)
{
    return operator+(precision);
}

BSPrecision BSPrecision::operator/(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Division leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator==(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator!=(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator< (BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator<=(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator> (BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

BSPrecision BSPrecision::operator>=(BSPrecision const& precision)
{
    if (mForBSNumber)
    {
        return precision;
    }
    else
    {
        // Comparison leads to multiplication of numerators and denominators.
        return operator*(precision);
    }
}

