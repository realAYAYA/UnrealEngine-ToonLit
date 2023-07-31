// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

#include <ThirdParty/GTEngine/Mathematics/GteIEEEBinary16.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <ThirdParty/GTEngine/Mathematics/GteBitHacks.h>

namespace gte
{

IEEEBinary16::~IEEEBinary16()
{
}

IEEEBinary16::IEEEBinary16()
    :
    IEEEBinary<int16_t, uint16_t, 16, 11>()
{
    // uninitialized
}

IEEEBinary16::IEEEBinary16(IEEEBinary16 const& object)
    :
    IEEEBinary<int16_t, uint16_t, 16, 11>(object)
{
}

IEEEBinary16::IEEEBinary16(float number)
    :
    IEEEBinary<int16_t, uint16_t, 16, 11>()
{
    union { float n; uint32_t e; } temp = { number };
    encoding = Convert32To16(temp.e);
}

IEEEBinary16::IEEEBinary16(double number)
    :
    IEEEBinary<int16_t, uint16_t, 16, 11>()
{
    union { float n; uint32_t e; } temp;
    temp.n = (float)number;
    encoding = Convert32To16(temp.e);
}

IEEEBinary16::IEEEBinary16(uint16_t encoding)
    :
    IEEEBinary<int16_t, uint16_t, 16, 11>(encoding)
{
}

IEEEBinary16::operator float() const
{
    union { uint32_t e; float n; } temp = { Convert16To32(encoding) };
    return temp.n;
}

IEEEBinary16::operator double() const
{
    union { uint32_t e; float n; } temp = { Convert16To32(encoding) };
    return (double)temp.n;
}

IEEEBinary16& IEEEBinary16::operator= (IEEEBinary16 const& object)
{
    IEEEBinary<int16_t, uint16_t, 16, 11>::operator=(object);
    return *this;
}

bool IEEEBinary16::operator==(IEEEBinary16 const& object) const
{
    return (float)*this == (float)object;
}

bool IEEEBinary16::operator!=(IEEEBinary16 const& object) const
{
    return (float)*this != (float)object;
}

bool IEEEBinary16::operator< (IEEEBinary16 const& object) const
{
    return (float)*this < (float)object;
}

bool IEEEBinary16::operator<=(IEEEBinary16 const& object) const
{
    return (float)*this <= (float)object;
}

bool IEEEBinary16::operator> (IEEEBinary16 const& object) const
{
    return (float)*this > (float)object;
}

bool IEEEBinary16::operator>=(IEEEBinary16 const& object) const
{
    return (float)*this >= (float)object;
}

uint16_t IEEEBinary16::Convert32To16(uint32_t encoding)
{
    // Extract the channels for the binary32 number.
    uint32_t sign32 = (encoding & F32_SIGN_MASK);
    uint32_t biased32 =
        ((encoding & F32_BIASED_EXPONENT_MASK) >> F32_NUM_TRAILING_BITS);
    uint32_t trailing32 = (encoding & F32_TRAILING_MASK);
    uint32_t nonneg32 = (encoding & F32_NOT_SIGN_MASK);

    // Generate the channels for the IEEEBinary16 number.
    uint16_t sign16 = static_cast<uint16_t>(sign32 >> DIFF_NUM_ENCODING_BITS);
    uint16_t biased16, trailing16;
    uint32_t frcpart;

    if (biased32 == 0)
    {
        // nonneg32 is 32-zero or 32-subnormal, nearest is 16-zero.
        return sign16;
    }

    if (biased32 < F32_MAX_BIASED_EXPONENT)
    {
        // nonneg32 is 32-normal.
        if (nonneg32 <= F16_AVR_MIN_SUBNORMAL_ZERO)
        {
            // nonneg32 <= 2^{-25}, nearest is 16-zero.
            return sign16;
        }

        if (nonneg32 <= F16_MIN_SUBNORMAL)
        {
            // 2^{-25} < nonneg32 <= 2^{-24}, nearest is 16-min-subnormal.
            return sign16 | IEEEBinary16::MIN_SUBNORMAL;
        }

        if (nonneg32 < F16_MIN_NORMAL)
        {
            // 2^{-24} < nonneg32 < 2^{-14}, round to nearest
            // 16-subnormal with ties to even.  Note that biased16 is zero.
            trailing16 = static_cast<uint16_t>(((trailing32 & INT_PART_MASK) >> DIFF_NUM_TRAILING_BITS));
            frcpart = (trailing32 & FRC_PART_MASK);
            if (frcpart > FRC_HALF || (frcpart == FRC_HALF && (trailing16 & 1)))
            {
                // If there is a carry into the exponent, the nearest is
                // actually 16-min-normal 1.0*2^{-14}, so the high-order bit
                // of trailing16 makes biased16 equal to 1 and the result is
                // correct.
                ++trailing16;
            }
            return sign16 | trailing16;
        }

        if (nonneg32 <= F16_MAX_NORMAL)
        {
            // 2^{-14} <= nonneg32 <= 1.1111111111*2^{15}, round to nearest
            // 16-normal with ties to even.
            biased16 = static_cast<uint16_t>((biased32 - F32_EXPONENT_BIAS +
                IEEEBinary16::EXPONENT_BIAS)
                << IEEEBinary16::NUM_TRAILING_BITS);
            trailing16 = static_cast<uint16_t>(((trailing32 & INT_PART_MASK) >> DIFF_NUM_TRAILING_BITS));
            frcpart = (trailing32 & FRC_PART_MASK);
            if (frcpart > FRC_HALF || (frcpart == FRC_HALF && (trailing16 & 1)))
            {
                // If there is a carry into the exponent, the addition of
                // trailing16 to biased16 (rather than or-ing) produces the
                // correct result.
                ++trailing16;
            }
            return sign16 | (biased16 + trailing16);
        }

        if (nonneg32 < F16_AVR_MAX_NORMAL_INFINITY)
        {
            // 1.1111111111*2^{15} < nonneg32 < (MAX_NORMAL+INFINITY)/2, so
            // the number is closest to 16-max-normal.
            return sign16 | IEEEBinary16::MAX_NORMAL;
        }

        // nonneg32 >= (MAX_NORMAL+INFINITY)/2, so convert to 16-infinite.
        return sign16 | IEEEBinary16::POS_INFINITY;
    }

    if (trailing32 == 0)
    {
        // The number is 32-infinite.  Convert to 16-infinite.
        return sign16 | IEEEBinary16::POS_INFINITY;
    }

    // The number is 32-NaN.  Convert to 16-NaN with 16-payload the high-order
    // 9 bits of the 32-payload.  The code also grabs the 32-quietNaN mask
    // bit.
    uint16_t maskPayload = static_cast<uint16_t>(
        (trailing32 & 0x007FE000u) >> 13);
    return sign16 | IEEEBinary16::EXPONENT_MASK | maskPayload;
}

uint32_t IEEEBinary16::Convert16To32(uint16_t encoding)
{
    // Extract the channels for the IEEEBinary16 number.
    uint16_t sign16 = (encoding & IEEEBinary16::SIGN_MASK);
    uint16_t biased16 = ((encoding & IEEEBinary16::EXPONENT_MASK)
        >> IEEEBinary16::NUM_TRAILING_BITS);
    uint16_t trailing16 = (encoding & IEEEBinary16::TRAILING_MASK);

    // Generate the channels for the binary32 number.
    uint32_t sign32 = static_cast<uint32_t>(sign16 << DIFF_NUM_ENCODING_BITS);
    uint32_t biased32, trailing32;

    if (biased16 == 0)
    {
        if (trailing16 == 0)
        {
            // The number is 16-zero.  Convert to 32-zero.
            return sign32;
        }
        else
        {
            // The number is 16-subnormal.  Convert to 32-normal.
            trailing32 = static_cast<uint32_t>(trailing16);
            int32_t leading = GetLeadingBit(trailing32);
            int32_t shift = 23 - leading;
            biased32 = static_cast<uint32_t>(F32_EXPONENT_BIAS - 1 - shift);
            trailing32 = (trailing32 << shift) & F32_TRAILING_MASK;
            return sign32 | (biased32 << F32_NUM_TRAILING_BITS) | trailing32;
        }
    }

    if (biased16 < IEEEBinary16::MAX_BIASED_EXPONENT)
    {
        // The number is 16-normal.  Convert to 32-normal.
        biased32 = static_cast<uint32_t>(biased16 - IEEEBinary16::EXPONENT_BIAS +
            F32_EXPONENT_BIAS);
        trailing32 = (static_cast<uint32_t>(
            trailing16) << DIFF_NUM_TRAILING_BITS);
        return sign32 | (biased32 << F32_NUM_TRAILING_BITS) | trailing32;
    }

    if (trailing16 == 0)
    {
        // The number is 16-infinite.  Convert to 32-infinite.
        return sign32 | F32_BIASED_EXPONENT_MASK;
    }

    // The number is 16-NaN.  Convert to 32-NaN with 16-payload embedded in
    // the high-order 9 bits of the 32-payload.  The code also copies the
    // 16-quietNaN mask bit.
    uint32_t maskPayload =
        ((trailing16 & IEEEBinary16::TRAILING_MASK) << DIFF_PAYLOAD_SHIFT);
    return sign32 | F32_BIASED_EXPONENT_MASK | maskPayload;
}

IEEEBinary16 operator- (IEEEBinary16 x)
{
    uint16_t result = static_cast<uint16_t>(x) ^ IEEEBinary16::SIGN_MASK;
    return result;
}

float operator+ (IEEEBinary16 x, IEEEBinary16 y)
{
    return static_cast<float>(x)+static_cast<float>(y);
}

float operator- (IEEEBinary16 x, IEEEBinary16 y)
{
    return static_cast<float>(x)-static_cast<float>(y);
}

float operator* (IEEEBinary16 x, IEEEBinary16 y)
{
    return static_cast<float>(x)* static_cast<float>(y);
}

float operator/ (IEEEBinary16 x, IEEEBinary16 y)
{
    return static_cast<float>(x) / static_cast<float>(y);
}

float operator+ (IEEEBinary16 x, float y)
{
    return static_cast<float>(x)+y;
}

float operator- (IEEEBinary16 x, float y)
{
    return static_cast<float>(x)-y;
}

float operator* (IEEEBinary16 x, float y)
{
    return static_cast<float>(x)* y;
}

float operator/ (IEEEBinary16 x, float y)
{
    return static_cast<float>(x) / y;
}

float operator+ (float x, IEEEBinary16 y)
{
    return x + static_cast<float>(y);
}

float operator- (float x, IEEEBinary16 y)
{
    return x - static_cast<float>(y);
}

float operator* (float x, IEEEBinary16 y)
{
    return x * static_cast<float>(y);
}

float operator/ (float x, IEEEBinary16 y)
{
    return x / static_cast<float>(y);
}

IEEEBinary16& operator+= (IEEEBinary16& x, IEEEBinary16 y)
{
    x = static_cast<float>(x)+static_cast<float>(y);
    return x;
}

IEEEBinary16& operator-= (IEEEBinary16& x, IEEEBinary16 y)
{
    x = static_cast<float>(x)-static_cast<float>(y);
    return x;
}

IEEEBinary16& operator*= (IEEEBinary16& x, IEEEBinary16 y)
{
    x = static_cast<float>(x)* static_cast<float>(y);
    return x;
}

IEEEBinary16& operator/= (IEEEBinary16& x, IEEEBinary16 y)
{
    x = static_cast<float>(x) / static_cast<float>(y);
    return x;
}

IEEEBinary16& operator+= (IEEEBinary16& x, float y)
{
    x = static_cast<float>(x)+y;
    return x;
}

IEEEBinary16& operator-= (IEEEBinary16& x, float y)
{
    x = static_cast<float>(x)-y;
    return x;
}

IEEEBinary16& operator*= (IEEEBinary16& x, float y)
{
    x = static_cast<float>(x)* y;
    return x;
}

IEEEBinary16& operator/= (IEEEBinary16& x, float y)
{
    x = static_cast<float>(x) / y;
    return x;
}

}
