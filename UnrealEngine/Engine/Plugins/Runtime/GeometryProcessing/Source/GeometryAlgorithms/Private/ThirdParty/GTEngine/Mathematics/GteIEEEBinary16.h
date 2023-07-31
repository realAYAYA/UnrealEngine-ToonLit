// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteMath.h>
#include <ThirdParty/GTEngine/Mathematics/GteIEEEBinary.h>
#include "Misc/AssertionMacros.h"

namespace gte
{
    class IEEEBinary16 : public IEEEBinary<int16_t, uint16_t, 16, 11>
    {
    public:
        // Construction and destruction.  The base class destructor is hidden,
        // but this is safe because there are no side effects of the
        // destruction.
        ~IEEEBinary16();
        IEEEBinary16();  // uninitialized
        IEEEBinary16(IEEEBinary16 const& object);
        IEEEBinary16(float number);
        IEEEBinary16(double number);
        IEEEBinary16(uint16_t encoding);

        // Implicit conversions.
        operator float() const;
        operator double() const;

        // Assignment.
        IEEEBinary16& operator=(IEEEBinary16 const& object);

        // Comparison.
        bool operator==(IEEEBinary16 const& object) const;
        bool operator!=(IEEEBinary16 const& object) const;
        bool operator< (IEEEBinary16 const& object) const;
        bool operator<=(IEEEBinary16 const& object) const;
        bool operator> (IEEEBinary16 const& object) const;
        bool operator>=(IEEEBinary16 const& object) const;

    private:
        // Support for conversions between encodings.
        enum
        {
            F32_NUM_ENCODING_BITS = 32,
            F32_NUM_TRAILING_BITS = 23,
            F32_EXPONENT_BIAS = 127,
            F32_MAX_BIASED_EXPONENT = 255,
            F32_SIGN_MASK = 0x80000000,
            F32_NOT_SIGN_MASK = 0x7FFFFFFF,
            F32_BIASED_EXPONENT_MASK = 0x7F800000,
            F32_TRAILING_MASK = 0x007FFFFF,
            F16_AVR_MIN_SUBNORMAL_ZERO = 0x33000000,
            F16_MIN_SUBNORMAL = 0x33800000,
            F16_MIN_NORMAL = 0x38800000,
            F16_MAX_NORMAL = 0x477FE000,
            F16_AVR_MAX_NORMAL_INFINITY = 0x477FF000,
            DIFF_NUM_ENCODING_BITS = 16,
            DIFF_NUM_TRAILING_BITS = 13,
            DIFF_PAYLOAD_SHIFT = 13,
            INT_PART_MASK = 0x007FE000,
            FRC_PART_MASK = 0x00001FFF,
            FRC_HALF = 0x00001000
        };

        static uint16_t Convert32To16(uint32_t encoding);
        static uint32_t Convert16To32(uint16_t encoding);
    };

    // Arithmetic operations (high-precision).
    IEEEBinary16 operator-(IEEEBinary16 x);
    float operator+(IEEEBinary16 x, IEEEBinary16 y);
    float operator-(IEEEBinary16 x, IEEEBinary16 y);
    float operator*(IEEEBinary16 x, IEEEBinary16 y);
    float operator/(IEEEBinary16 x, IEEEBinary16 y);
    float operator+(IEEEBinary16 x, float y);
    float operator-(IEEEBinary16 x, float y);
    float operator*(IEEEBinary16 x, float y);
    float operator/(IEEEBinary16 x, float y);
    float operator+(float x, IEEEBinary16 y);
    float operator-(float x, IEEEBinary16 y);
    float operator*(float x, IEEEBinary16 y);
    float operator/(float x, IEEEBinary16 y);

    // Arithmetic updates.
    IEEEBinary16& operator+=(IEEEBinary16& x, IEEEBinary16 y);
    IEEEBinary16& operator-=(IEEEBinary16& x, IEEEBinary16 y);
    IEEEBinary16& operator*=(IEEEBinary16& x, IEEEBinary16 y);
    IEEEBinary16& operator/=(IEEEBinary16& x, IEEEBinary16 y);
    IEEEBinary16& operator+=(IEEEBinary16& x, float y);
    IEEEBinary16& operator-=(IEEEBinary16& x, float y);
    IEEEBinary16& operator*=(IEEEBinary16& x, float y);
    IEEEBinary16& operator/=(IEEEBinary16& x, float y);

}

namespace std
{
    inline gte::IEEEBinary16 abs(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::abs((float)x);
    }

    inline gte::IEEEBinary16 acos(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::acos((float)x);
    }

    inline gte::IEEEBinary16 acosh(gte::IEEEBinary16 x)
    {
		check(true);
#if defined(__ANDROID__)
		check(false);		// not supported on android
		return (gte::IEEEBinary16)0.0f;
#else
        return (gte::IEEEBinary16)std::acosh((float)x);
#endif
	}

    inline gte::IEEEBinary16 asin(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::asin((float)x);
    }

    inline gte::IEEEBinary16 asinh(gte::IEEEBinary16 x)
    {
#if defined(__ANDROID__)
		check(false);		// not supported on android
		return (gte::IEEEBinary16)0.0f;
#else
		return (gte::IEEEBinary16)std::asinh((float)x);
#endif
    }

    inline gte::IEEEBinary16 atan(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::atan((float)x);
    }

    inline gte::IEEEBinary16 atanh(gte::IEEEBinary16 x)
    {
#if defined(__ANDROID__)
		check(false);		// not supported on android
		return (gte::IEEEBinary16)0.0f;
#else
        return (gte::IEEEBinary16)std::atanh((float)x);
#endif
	}

    inline gte::IEEEBinary16 atan2(gte::IEEEBinary16 y, gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::atan2((float)y, (float)x);
    }

    inline gte::IEEEBinary16 ceil(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::ceil((float)x);
    }

    inline gte::IEEEBinary16 cos(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::cos((float)x);
    }

    inline gte::IEEEBinary16 cosh(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::cosh((float)x);
    }

    inline gte::IEEEBinary16 exp(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::exp((float)x);
    }

    inline gte::IEEEBinary16 exp2(gte::IEEEBinary16 x)
    {
#if defined(__ANDROID__)
		check(false);		// not supported on android
		return (gte::IEEEBinary16)0.0f;
#else
        return (gte::IEEEBinary16)std::exp2((float)x);
#endif
	}

    inline gte::IEEEBinary16 floor(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::floor((float)x);
    }

    inline gte::IEEEBinary16 fmod(gte::IEEEBinary16 x, gte::IEEEBinary16 y)
    {
        return (gte::IEEEBinary16)std::fmod((float)x, (float)y);
    }

    inline gte::IEEEBinary16 frexp(gte::IEEEBinary16 x, int* exponent)
    {
        return (gte::IEEEBinary16)std::frexp((float)x, exponent);
    }

    inline gte::IEEEBinary16 ldexp(gte::IEEEBinary16 x, int exponent)
    {
        return (gte::IEEEBinary16)std::ldexp((float)x, exponent);
    }

    inline gte::IEEEBinary16 log(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::log((float)x);
    }

    inline gte::IEEEBinary16 log2(gte::IEEEBinary16 x)
    {
#if defined(__ANDROID__)
		check(false);		// not supported on android
		return (gte::IEEEBinary16)0.0f;
#else
		return (gte::IEEEBinary16)std::log2((float)x);
#endif
    }

    inline gte::IEEEBinary16 log10(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::log10((float)x);
    }

    inline gte::IEEEBinary16 pow(gte::IEEEBinary16 x, gte::IEEEBinary16 y)
    {
        return (gte::IEEEBinary16)std::pow((float)x, (float)y);
    }

    inline gte::IEEEBinary16 sin(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::sin((float)x);
    }

    inline gte::IEEEBinary16 sinh(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::sinh((float)x);
    }

    inline gte::IEEEBinary16 sqrt(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::sqrt((float)x);
    }

    inline gte::IEEEBinary16 tan(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::tan((float)x);
    }

    inline gte::IEEEBinary16 tanh(gte::IEEEBinary16 x)
    {
        return (gte::IEEEBinary16)std::tanh((float)x);
    }
}

namespace gte
{
    inline IEEEBinary16 atandivpi(IEEEBinary16 x)
    {
        return (IEEEBinary16)atandivpi((float)x);
    }

    inline IEEEBinary16 atan2divpi(IEEEBinary16 y, IEEEBinary16 x)
    {
        return (IEEEBinary16)atan2divpi((float)y, (float)x);
    }

    inline IEEEBinary16 clamp(IEEEBinary16 x, IEEEBinary16 xmin, IEEEBinary16 xmax)
    {
        return (IEEEBinary16)clamp((float)x, (float)xmin, (float)xmax);
    }

    inline IEEEBinary16 cospi(IEEEBinary16 x)
    {
        return (IEEEBinary16)cospi((float)x);
    }

    inline IEEEBinary16 exp10(IEEEBinary16 x)
    {
        return (IEEEBinary16)exp10((float)x);
    }

    inline IEEEBinary16 invsqrt(IEEEBinary16 x)
    {
        return (IEEEBinary16)invsqrt((float)x);
    }

    inline int isign(IEEEBinary16 x)
    {
        return isign((float)x);
    }

    inline IEEEBinary16 saturate(IEEEBinary16 x)
    {
        return (IEEEBinary16)saturate((float)x);
    }

    inline IEEEBinary16 sign(IEEEBinary16 x)
    {
        return (IEEEBinary16)sign((float)x);
    }

    inline IEEEBinary16 sinpi(IEEEBinary16 x)
    {
        return (IEEEBinary16)sinpi((float)x);
    }

    inline IEEEBinary16 sqr(IEEEBinary16 x)
    {
        return (IEEEBinary16)sqr((float)x);
    }
}
