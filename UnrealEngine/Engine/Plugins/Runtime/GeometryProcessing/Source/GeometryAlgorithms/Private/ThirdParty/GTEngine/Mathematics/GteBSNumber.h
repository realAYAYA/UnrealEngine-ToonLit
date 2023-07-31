// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2019/01/17)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteBitHacks.h>
#include <ThirdParty/GTEngine/Mathematics/GteMath.h>
#include <ThirdParty/GTEngine/Mathematics/GteIEEEBinary.h>
#include <algorithm>
#include <fstream>

// The class BSNumber (binary scientific number) is designed to provide exact
// arithmetic for robust algorithms, typically those for which we need to know
// the exact sign of determinants.  The template parameter UIntegerType must
// have support for at least the following public interface.  The fstream
// objects for Write/Read must be created using std::ios::binary.  The return
// value of Write/Read is 'true' iff the operation was successful.
//
//      class UIntegerType
//      {
//      public:
//          UIntegerType();
//          UIntegerType(UIntegerType const& number);
//          UIntegerType(uint32_t number);
//          UIntegerType(uint64_t number);
//          UIntegerType(int numBits);
//          UIntegerType& operator=(UIntegerType const& number);
//          UIntegerType(UIntegerType&& number);
//          UIntegerType& operator=(UIntegerType&& number);
//          int32_t GetNumBits() const;
//          bool operator==(UIntegerType const& number) const;
//          bool operator< (UIntegerType const& number) const;
//          void Add(UIntegerType const& n0, UIntegerType const& n1);
//          void Sub(UIntegerType const& n0, UIntegerType const& n1);
//          void Mul(UIntegerType const& n0, UIntegerType const& n1);
//          void ShiftLeft(UIntegerType const& number, int shift);
//          int32_t ShiftRightToOdd(UIntegerType const& number);
//          uint64_t GetPrefix(int numRequested) const;
//          bool Write(std::ofstream& output) const;
//          bool Read(std::ifstream& input);
//      };
//
// GTEngine currently has 32-bits-per-word storage for UIntegerType.  See the
// classes UIntegerAP32 (arbitrary precision), UIntegerFP32<N> (fixed
// precision), and UIntegerALU32 (arithmetic logic unit shared by the previous
// two classes).  The document at the following link describes the design,
// implementation, and use of BSNumber and BSRational.
//   http://www.geometrictools.com/Documentation/ArbitraryPrecision.pdf
//
// Support for debugging algorithms that use exact rational arithmetic.  Each
// BSNumber and BSRational has a double-precision member that is exposed when
// the conditional define is enabled.  Be aware that this can be very slow
// because of the conversion to double-precision whenever new objects are
// created by arithmetic operations.  As a faster alternative, you can add
// temporary code in your algorithms that explicitly convert specific rational
// numbers to double precision.
//
//#define GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE

namespace gte
{
    template <typename UIntegerType> class BSRational;

    template <typename UIntegerType>
    class BSNumber
    {
    public:
        // Construction.  The default constructor generates the zero BSNumber.
        BSNumber()
            :
            mSign(0),
            mBiasedExponent(0)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(BSNumber const& number)
        {
            *this = number;
        }

        BSNumber(float number)
        {
            ConvertFrom<IEEEBinary32>(number);
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(double number)
        {
            ConvertFrom<IEEEBinary64>(number);
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(int32_t number)
        {
            if (number == 0)
            {
                mSign = 0;
                mBiasedExponent = 0;
            }
            else
            {
                if (number < 0)
                {
                    mSign = -1;
                    number = -number;
                }
                else
                {
                    mSign = 1;
                }

                mBiasedExponent = GetTrailingBit(number);
                mUInteger = (uint32_t)number;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(uint32_t number)
        {
            if (number == 0)
            {
                mSign = 0;
                mBiasedExponent = 0;
            }
            else
            {
                mSign = 1;
                mBiasedExponent = GetTrailingBit(number);
                mUInteger = number;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(int64_t number)
        {
            if (number == 0)
            {
                mSign = 0;
                mBiasedExponent = 0;
            }
            else
            {
                if (number < 0)
                {
                    mSign = -1;
                    number = -number;
                }
                else
                {
                    mSign = 1;
                }

                mBiasedExponent = GetTrailingBit(number);
                mUInteger = (uint64_t)number;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSNumber(uint64_t number)
        {
            if (number == 0)
            {
                mSign = 0;
                mBiasedExponent = 0;
            }
            else
            {
                mSign = 1;
                mBiasedExponent = GetTrailingBit(number);
                mUInteger = number;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        // Implicit conversions.
        inline operator float() const
        {
            return ConvertTo<IEEEBinary32>();
        }

        inline operator double() const
        {
            return ConvertTo<IEEEBinary64>();
        }

        // Assignment.
        BSNumber& operator=(BSNumber const& number)
        {
            mSign = number.mSign;
            mBiasedExponent = number.mBiasedExponent;
            mUInteger = number.mUInteger;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = number.mValue;
#endif
            return *this;
        }

        // Support for std::move.
        BSNumber(BSNumber&& number)
        {
            *this = std::move(number);
        }

        BSNumber& operator=(BSNumber&& number)
        {
            mSign = number.mSign;
            mBiasedExponent = number.mBiasedExponent;
            mUInteger = std::move(number.mUInteger);
            number.mSign = 0;
            number.mBiasedExponent = 0;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = number.mValue;
#endif
            return *this;
        }

        // Member access.
        inline int32_t GetSign() const
        {
            return mSign;
        }

        inline int32_t GetBiasedExponent() const
        {
            return mBiasedExponent;
        }

        inline int32_t GetExponent() const
        {
            return mBiasedExponent + mUInteger.GetNumBits() - 1;
        }

        inline UIntegerType const& GetUInteger() const
        {
            return mUInteger;
        }

        // Comparisons.
        bool operator==(BSNumber const& number) const
        {
            return (mSign == number.mSign ? EqualIgnoreSign(*this, number) : false);
        }

        bool operator!=(BSNumber const& number) const
        {
            return !operator==(number);
        }

        bool operator< (BSNumber const& number) const
        {
            if (mSign > 0)
            {
                if (number.mSign <= 0)
                {
                    return false;
                }

                // Both numbers are positive.
                return LessThanIgnoreSign(*this, number);
            }
            else if (mSign < 0)
            {
                if (number.mSign >= 0)
                {
                    return true;
                }

                // Both numbers are negative.
                return LessThanIgnoreSign(number, *this);
            }
            else
            {
                return number.mSign > 0;
            }
        }

        bool operator<=(BSNumber const& number) const
        {
            return !number.operator<(*this);
        }

        bool operator> (BSNumber const& number) const
        {
            return number.operator<(*this);
        }

        bool operator>=(BSNumber const& number) const
        {
            return !operator<(number);
        }

        // Unary operations.
        BSNumber operator+() const
        {
            return *this;
        }

        BSNumber operator-() const
        {
            BSNumber result = *this;
            result.mSign = -result.mSign;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            result.mValue = (double)result;
#endif
            return result;
        }

        // Arithmetic.
        BSNumber operator+(BSNumber const& n1) const
        {
            BSNumber const& n0 = *this;

            if (n0.mSign == 0)
            {
                return n1;
            }

            if (n1.mSign == 0)
            {
                return n0;
            }

            if (n0.mSign > 0)
            {
                if (n1.mSign > 0)
                {
                    // n0 + n1 = |n0| + |n1|
                    return AddIgnoreSign(n0, n1, +1);
                }
                else // n1.mSign < 0
                {
                    if (!EqualIgnoreSign(n0, n1))
                    {
                        if (LessThanIgnoreSign(n1, n0))
                        {
                            // n0 + n1 = |n0| - |n1| > 0
                            return SubIgnoreSign(n0, n1, +1);
                        }
                        else
                        {
                            // n0 + n1 = -(|n1| - |n0|) < 0
                            return SubIgnoreSign(n1, n0, -1);
                        }
                    }
                    // else n0 + n1 = 0
                }
            }
            else // n0.mSign < 0
            {
                if (n1.mSign < 0)
                {
                    // n0 + n1 = -(|n0| + |n1|)
                    return AddIgnoreSign(n0, n1, -1);
                }
                else // n1.mSign > 0
                {
                    if (!EqualIgnoreSign(n0, n1))
                    {
                        if (LessThanIgnoreSign(n1, n0))
                        {
                            // n0 + n1 = -(|n0| - |n1|) < 0
                            return SubIgnoreSign(n0, n1, -1);
                        }
                        else
                        {
                            // n0 + n1 = |n1| - |n0| > 0
                            return SubIgnoreSign(n1, n0, +1);
                        }
                    }
                    // else n0 + n1 = 0
                }
            }

            return BSNumber();  // = 0
        }

        BSNumber operator-(BSNumber const& n1) const
        {
            BSNumber const& n0 = *this;

            if (n0.mSign == 0)
            {
                return -n1;
            }

            if (n1.mSign == 0)
            {
                return n0;
            }

            if (n0.mSign > 0)
            {
                if (n1.mSign < 0)
                {
                    // n0 - n1 = |n0| + |n1|
                    return AddIgnoreSign(n0, n1, +1);
                }
                else // n1.mSign > 0
                {
                    if (!EqualIgnoreSign(n0, n1))
                    {
                        if (LessThanIgnoreSign(n1, n0))
                        {
                            // n0 - n1 = |n0| - |n1| > 0
                            return SubIgnoreSign(n0, n1, +1);
                        }
                        else
                        {
                            // n0 - n1 = -(|n1| - |n0|) < 0
                            return SubIgnoreSign(n1, n0, -1);
                        }
                    }
                    // else n0 - n1 = 0
                }
            }
            else // n0.mSign < 0
            {
                if (n1.mSign > 0)
                {
                    // n0 - n1 = -(|n0| + |n1|)
                    return AddIgnoreSign(n0, n1, -1);
                }
                else // n1.mSign < 0
                {
                    if (!EqualIgnoreSign(n0, n1))
                    {
                        if (LessThanIgnoreSign(n1, n0))
                        {
                            // n0 - n1 = -(|n0| - |n1|) < 0
                            return SubIgnoreSign(n0, n1, -1);
                        }
                        else
                        {
                            // n0 - n1 = |n1| - |n0| > 0
                            return SubIgnoreSign(n1, n0, +1);
                        }
                    }
                    // else n0 - n1 = 0
                }
            }

            return BSNumber();  // = 0
        }

        BSNumber operator*(BSNumber const& number) const
        {
            BSNumber result;  // = 0
            int sign = mSign * number.mSign;
            if (sign != 0)
            {
                result.mSign = sign;
                result.mBiasedExponent = mBiasedExponent + number.mBiasedExponent;
                result.mUInteger.Mul(mUInteger, number.mUInteger);
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            result.mValue = (double)result;
#endif
            return result;
        }

        BSNumber& operator+=(BSNumber const& number)
        {
            *this = operator+(number);
            return *this;
        }

        BSNumber& operator-=(BSNumber const& number)
        {
            *this = operator-(number);
            return *this;
        }

        BSNumber& operator*=(BSNumber const& number)
        {
            *this = operator*(number);
            return *this;
        }

        // Disk input/output.  The fstream objects should be created using
        // std::ios::binary.  The return value is 'true' iff the operation
        // was successful.
        bool Write(std::ofstream& output) const
        {
            if (output.write((char const*)&mSign, sizeof(mSign)).bad())
            {
                return false;
            }

            if (output.write((char const*)&mBiasedExponent,
                sizeof(mBiasedExponent)).bad())
            {
                return false;
            }

            return mUInteger.Write(output);
        }

        bool Read(std::ifstream& input)
        {
            if (input.read((char*)&mSign, sizeof(mSign)).bad())
            {
                return false;
            }

            if (input.read((char*)&mBiasedExponent, sizeof(mBiasedExponent)).bad())
            {
                return false;
            }

            return mUInteger.Read(input);
        }

    private:
        // Helpers for operator==, operator<, operator+, operator-.
        static bool EqualIgnoreSign(BSNumber const& n0, BSNumber const& n1)
        {
            return n0.mBiasedExponent == n1.mBiasedExponent && n0.mUInteger == n1.mUInteger;
        }

        static bool LessThanIgnoreSign(BSNumber const& n0, BSNumber const& n1)
        {
            int32_t e0 = n0.GetExponent(), e1 = n1.GetExponent();
            if (e0 < e1)
            {
                return true;
            }
            if (e0 > e1)
            {
                return false;
            }
            return n0.mUInteger < n1.mUInteger;
        }

        // Add two positive numbers.
        static BSNumber AddIgnoreSign(BSNumber const& n0, BSNumber const& n1, int32_t resultSign)
        {
            BSNumber result, temp;

            int32_t diff = n0.mBiasedExponent - n1.mBiasedExponent;
            if (diff > 0)
            {
                temp.mUInteger.ShiftLeft(n0.mUInteger, diff);
                result.mUInteger.Add(temp.mUInteger, n1.mUInteger);
                result.mBiasedExponent = n1.mBiasedExponent;
            }
            else if (diff < 0)
            {
                temp.mUInteger.ShiftLeft(n1.mUInteger, -diff);
                result.mUInteger.Add(n0.mUInteger, temp.mUInteger);
                result.mBiasedExponent = n0.mBiasedExponent;
            }
            else
            {
                temp.mUInteger.Add(n0.mUInteger, n1.mUInteger);
                int32_t shift = result.mUInteger.ShiftRightToOdd(temp.mUInteger);
                result.mBiasedExponent = n0.mBiasedExponent + shift;
            }

            result.mSign = resultSign;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            result.mValue = (double)result;
#endif
            return result;
        }

        // Subtract two positive numbers where n0 > n1.
        static BSNumber SubIgnoreSign(BSNumber const& n0, BSNumber const& n1, int32_t resultSign)
        {
            BSNumber result, temp;

            int32_t diff = n0.mBiasedExponent - n1.mBiasedExponent;
            if (diff > 0)
            {
                temp.mUInteger.ShiftLeft(n0.mUInteger, diff);
                result.mUInteger.Sub(temp.mUInteger, n1.mUInteger);
                result.mBiasedExponent = n1.mBiasedExponent;
            }
            else if (diff < 0)
            {
                temp.mUInteger.ShiftLeft(n1.mUInteger, -diff);
                result.mUInteger.Sub(n0.mUInteger, temp.mUInteger);
                result.mBiasedExponent = n0.mBiasedExponent;
            }
            else
            {
                temp.mUInteger.Sub(n0.mUInteger, n1.mUInteger);
                int32_t shift = result.mUInteger.ShiftRightToOdd(temp.mUInteger);
                result.mBiasedExponent = n0.mBiasedExponent + shift;
            }

            result.mSign = resultSign;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            result.mValue = (double)result;
#endif
            return result;
        }

        // Support for conversions from floating-point numbers to BSNumber.
        template <typename IEEE>
        void ConvertFrom(typename IEEE::FloatType number)
        {
            IEEE x(number);

            // Extract sign s, biased exponent e, and trailing significand t.
            typename IEEE::UIntType s = x.GetSign();
            typename IEEE::UIntType e = x.GetBiased();
            typename IEEE::UIntType t = x.GetTrailing();

            if (e == 0)
            {
                if (t == 0)  // zeros
                {
                    // x = (-1)^s * 0
                    mSign = 0;
                    mBiasedExponent = 0;
                }
                else  // subnormal numbers
                {
                    // x = (-1)^s * 0.t * 2^{1-EXPONENT_BIAS}
                    int32_t last = GetTrailingBit(t);
                    int32_t diff = IEEE::NUM_TRAILING_BITS - last;
                    mSign = (s > 0 ? -1 : 1);
                    mBiasedExponent = IEEE::MIN_SUB_EXPONENT - diff;
                    mUInteger = (t >> last);
                }
            }
            else if (e < IEEE::MAX_BIASED_EXPONENT)  // normal numbers
            {
                // x = (-1)^s * 1.t * 2^{e-EXPONENT_BIAS}
                if (t > 0)
                {
                    int32_t last = GetTrailingBit(t);
                    int32_t diff = IEEE::NUM_TRAILING_BITS - last;
                    mSign = (s > 0 ? -1 : 1);
                    mBiasedExponent =
                        static_cast<int32_t>(e) - IEEE::EXPONENT_BIAS - diff;
                    mUInteger = ((t | IEEE::SUP_TRAILING) >> last);
                }
                else
                {
                    mSign = (s > 0 ? -1 : 1);
                    mBiasedExponent = static_cast<int32_t>(e) - IEEE::EXPONENT_BIAS;
                    mUInteger = (typename IEEE::UIntType)1;
                }
            }
            else  // e == MAX_BIASED_EXPONENT, special numbers
            {
                if (t == 0)  // infinities
                {
                    // x = (-1)^s * infinity
                    LogWarning("Input is " + std::string(s > 0 ? "-" : "+") +
                        "infinity.");

                    // Return (-1)^s * 2^{1+EXPONENT_BIAS} for a graceful exit.
                    mSign = (s > 0 ? -1 : 1);
                    mBiasedExponent = 1 + IEEE::EXPONENT_BIAS;
                    mUInteger = (typename IEEE::UIntType)1;
                }
                else  // not-a-number (NaN)
                {
                    LogError("Input is a " +
                        std::string(t & IEEE::NAN_QUIET_MASK ?
                            "quiet" : "signaling") + " NaN with payload [redacted]" 
						// disabling this because std::to_string is not available on some platforms and mscver ifdef causes a weird error
						//+ std::to_string(t & IEEE::NAN_PAYLOAD_MASK) + "."
					);

                    // Return 0 for a graceful exit.
                    mSign = 0;
                    mBiasedExponent = 0;
                }
            }
        }

        // Support for conversions from BSNumber to floating-point numbers.
        template <typename IEEE>
        typename IEEE::FloatType ConvertTo() const
        {
            typename IEEE::UIntType s = (mSign < 0 ? 1 : 0);
            typename IEEE::UIntType e, t;

            if (mSign != 0)
            {
                // The conversions use round-to-nearest-ties-to-even semantics.
                int32_t exponent = GetExponent();
                if (exponent < IEEE::MIN_EXPONENT)
                {
                    if (exponent < IEEE::MIN_EXPONENT - 1
                        || mUInteger.GetNumBits() == 1)  // x = 1.0*2^{MIN_EXPONENT-1}
                    {
                        // Round to zero.
                        e = 0;
                        t = 0;
                    }
                    else
                    {
                        // Round to min subnormal.
                        e = 0;
                        t = 1;
                    }
                }
                else if (exponent < IEEE::MIN_SUB_EXPONENT)
                {
                    // The second input is in {0, ..., NUM_TRAILING_BITS-1}.
                    t = GetTrailing<IEEE>(0, IEEE::MIN_SUB_EXPONENT - exponent - 1);
                    if (t & IEEE::SUP_TRAILING)
                    {
                        // Leading NUM_SIGNIFICAND_BITS bits were all 1, so round to
                        // min normal.
                        e = 1;
                        t = 0;
                    }
                    else
                    {
                        e = 0;
                    }
                }
                else if (exponent <= IEEE::EXPONENT_BIAS)
                {
                    e = static_cast<uint32_t>(exponent + IEEE::EXPONENT_BIAS);
                    t = GetTrailing<IEEE>(1, 0);
                    if (t & (IEEE::SUP_TRAILING << 1))
                    {
                        // Carry-out occurred, so increase exponent by 1 and
                        // shift right to compensate.
                        ++e;
                        t >>= 1;
                    }
                    // Eliminate the leading 1 (implied for normals).
                    t &= ~IEEE::SUP_TRAILING;
                }
                else
                {
                    // Set to infinity.
                    e = IEEE::MAX_BIASED_EXPONENT;
                    t = 0;
                }
            }
            else
            {
                // The input is zero.
                e = 0;
                t = 0;
            }

            IEEE x(s, e, t);
            return x.number;
        }

        template <typename IEEE>
        typename IEEE::UIntType GetTrailing(int32_t normal, int32_t sigma) const
        {
            int32_t const numRequested = IEEE::NUM_SIGNIFICAND_BITS + normal;

            // We need numRequested bits to determine rounding direction.  These are
            // stored in the high-order bits of 'prefix'.
            uint64_t prefix = mUInteger.GetPrefix(numRequested);

            // The first bit index after the implied binary point for rounding.
            int32_t diff = numRequested - sigma;
            int32_t roundBitIndex = 64 - diff;

            // Determine rounding value based on round-to-nearest-ties-to-even.
            uint64_t mask = (1ull << roundBitIndex);
            uint64_t round;
            if (prefix & mask)
            {
                // The first bit of the remainder is 1.
                if (mUInteger.GetNumBits() == diff)
                {
                    // The first bit of the remainder is the lowest-order bit of
                    // mBits[0].  Apply the ties-to-even rule.
                    if (prefix & (mask << 1))
                    {
                        // The last bit of the trailing significand is odd, so
                        // round up.
                        round = 1;
                    }
                    else
                    {
                        // The last bit of the trailing significand is even, so
                        // round down.
                        round = 0;
                    }
                }
                else
                {
                    // The first bit of the remainder is not the lowest-order bit of
                    // mBits[0].  The remainder as a fraction is larger than 1/2, so
                    // round up.
                    round = 1;
                }
            }
            else
            {
                // The first bit of the remainder is 0, so round down.
                round = 0;
            }

            // Get the unrounded trailing significand.
            uint64_t trailing = prefix >> (roundBitIndex + 1);

            // Apply the rounding.
            trailing += round;
            return static_cast<typename IEEE::UIntType>(trailing);
        }

#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
    public:
        // List this first so that it shows up first in the debugger watch window.
        double mValue;
    private:
#endif

        // The number 0 is represented by: mSign = 0, mBiasedExponent = 0, and
        // mUInteger = 0.  For nonzero numbers, mSign != 0 and mUInteger > 0.
        int32_t mSign;
        int32_t mBiasedExponent;
        UIntegerType mUInteger;

        // Access to members to avoid exposing them publically when they are
        // needed only internally.
        friend class BSRational<UIntegerType>;
        friend class UnitTestBSNumber;
    };
}

namespace std
{
    // TODO: Allow for implementations of the math functions in which a
    // specified precision is used when computing the result.

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> abs(gte::BSNumber<UIntegerType> const& x)
    {
        return (x.GetSign() >= 0 ? x : -x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> acos(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::acos((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> acosh(gte::BSNumber<UIntegerType> const& x)
    {
#if defined(__ANDROID__)
		checkf(false, TEXT("not supported on Android"));
		return (gte::BSNumber<UIntegerType>)0;
#else
		return (gte::BSNumber<UIntegerType>)std::acosh((double)x);
#endif
	}

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> asin(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::asin((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> asinh(gte::BSNumber<UIntegerType> const& x)
    {
#if defined(__ANDROID__)
		checkf(false, TEXT("not supported on Android"));
		return (gte::BSNumber<UIntegerType>)0;
#else
        return (gte::BSNumber<UIntegerType>)std::asinh((double)x);
#endif
	}

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> atan(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::atan((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> atanh(gte::BSNumber<UIntegerType> const& x)
    {
#if defined(__ANDROID__)
		checkf(false, TEXT("not supported on Android"));
		return (gte::BSNumber<UIntegerType>)0;
#else
        return (gte::BSNumber<UIntegerType>)std::atanh((double)x);
#endif
	}

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> atan2(gte::BSNumber<UIntegerType> const& y, gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::atan2((double)y, (double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> ceil(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::ceil((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> cos(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::cos((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> cosh(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::cosh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> exp(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::exp((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> exp2(gte::BSNumber<UIntegerType> const& x)
    {
#if defined(__ANDROID__)
		checkf(false, TEXT("not supported on Android"));
		return (gte::BSNumber<UIntegerType>)0;
#else
        return (gte::BSNumber<UIntegerType>)std::exp2((double)x);
#endif
	}

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> floor(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::floor((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> fmod(gte::BSNumber<UIntegerType> const& x, gte::BSNumber<UIntegerType> const& y)
    {
        return (gte::BSNumber<UIntegerType>)std::fmod((double)x, (double)y);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> frexp(gte::BSNumber<UIntegerType> const& x, int* exponent)
    {
        return (gte::BSNumber<UIntegerType>)std::frexp((double)x, exponent);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> ldexp(gte::BSNumber<UIntegerType> const& x, int exponent)
    {
        return (gte::BSNumber<UIntegerType>)std::ldexp((double)x, exponent);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> log(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::log((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> log2(gte::BSNumber<UIntegerType> const& x)
    {
#if defined(__ANDROID__)
		checkf(false, TEXT("not supported on Android"));
		return (gte::BSNumber<UIntegerType>)0;
#else
        return (gte::BSNumber<UIntegerType>)std::log2((double)x);
#endif
	}

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> log10(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::log10((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> pow(gte::BSNumber<UIntegerType> const& x, gte::BSNumber<UIntegerType> const& y)
    {
        return (gte::BSNumber<UIntegerType>)std::pow((double)x, (double)y);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> sin(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::sin((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> sinh(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::sinh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> sqrt(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::sqrt((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> tan(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::tan((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSNumber<UIntegerType> tanh(gte::BSNumber<UIntegerType> const& x)
    {
        return (gte::BSNumber<UIntegerType>)std::tanh((double)x);
    }
}

namespace gte
{
    template <typename UIntegerType>
    inline BSNumber<UIntegerType> atandivpi(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)atandivpi((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> atan2divpi(BSNumber<UIntegerType> const& y, BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)atan2divpi((double)y, (double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> clamp(BSNumber<UIntegerType> const& x, BSNumber<UIntegerType> const& xmin, BSNumber<UIntegerType> const& xmax)
    {
        return (BSNumber<UIntegerType>)clamp((double)x, (double)xmin, (double)xmax);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> cospi(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)cospi((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> exp10(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)exp10((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> invsqrt(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)invsqrt((double)x);
    }

    template <typename UIntegerType>
    inline int isign(BSNumber<UIntegerType> const& x)
    {
        return isign((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> saturate(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)saturate((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> sign(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)sign((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> sinpi(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)sinpi((double)x);
    }

    template <typename UIntegerType>
    inline BSNumber<UIntegerType> sqr(BSNumber<UIntegerType> const& x)
    {
        return (BSNumber<UIntegerType>)sqr((double)x);
    }

    // See the comments in GteMath.h about trait is_arbitrary_precision.
    template <typename UIntegerType>
    struct is_arbitrary_precision_internal<BSNumber<UIntegerType>> : std::true_type {};
}
