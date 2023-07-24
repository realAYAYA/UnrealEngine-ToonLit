// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2019/01/17)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteBSNumber.h>

// See the comments in GteBSNumber.h about the UIntegerType requirements.  The
// denominator of a BSRational is chosen to be positive, which allows some
// simplification of comparisons.  Also see the comments about exposing the
// GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE conditional define.

namespace gte
{
    template <typename UIntegerType>
    class BSRational
    {
    public:
        // Construction.  The default constructor generates the zero BSRational.
        // The constructors that take only numerators set the denominators to one.
        BSRational()
            :
            mNumerator(0),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(BSRational const& r)
        {
            *this = r;
        }

        BSRational(float numerator)
            :
            mNumerator(numerator),
            mDenominator(1.0f)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(double numerator)
            :
            mNumerator(numerator),
            mDenominator(1.0)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(int32_t numerator)
            :
            mNumerator(numerator),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(uint32_t numerator)
            :
            mNumerator(numerator),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(int64_t numerator)
            :
            mNumerator(numerator),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(uint64_t numerator)
            :
            mNumerator(numerator),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(BSNumber<UIntegerType> const& numerator)
            :
            mNumerator(numerator),
            mDenominator(1)
        {
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(float numerator, float denominator)
            :
            mNumerator(numerator),
            mDenominator(denominator)
        {
            LogAssert(mDenominator.mSign != 0, "Division by zero not allowed.");
            if (mDenominator.mSign < 0)
            {
                mNumerator.mSign = -mNumerator.mSign;
                mDenominator.mSign = 1;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(double numerator, double denominator)
            :
            mNumerator(numerator),
            mDenominator(denominator)
        {
            LogAssert(mDenominator.mSign != 0, "Division by zero not allowed.");
            if (mDenominator.mSign < 0)
            {
                mNumerator.mSign = -mNumerator.mSign;
                mDenominator.mSign = 1;
            }
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        BSRational(BSNumber<UIntegerType> const& numerator, BSNumber<UIntegerType> const& denominator)
            :
            mNumerator(numerator),
            mDenominator(denominator)
        {
            LogAssert(mDenominator.mSign != 0, "Division by zero not allowed.");
            if (mDenominator.mSign < 0)
            {
                mNumerator.mSign = -mNumerator.mSign;
                mDenominator.mSign = 1;
            }

            // Set the exponent of the denominator to zero, but you can do so only
            // by modifying the biased exponent.  Adjust the numerator accordingly.
            // This prevents large growth of the exponents in both numerator and
            // denominator simultaneously.
            mNumerator.mBiasedExponent -= mDenominator.GetExponent();
            mDenominator.mBiasedExponent =
                -(mDenominator.GetUInteger().GetNumBits() - 1);

#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
        }

        // Implicit conversions.
        operator float() const
        {
            return Convert<uint32_t, float>();
        }

        operator double() const
        {
            return Convert<uint64_t, double>();
        }

        // Assignment.
        BSRational& operator=(BSRational const& r)
        {
            mNumerator = r.mNumerator;
            mDenominator = r.mDenominator;
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
            return *this;
        }

        // Support for std::move.
        BSRational(BSRational&& r)
        {
            *this = std::move(r);
        }

        BSRational& operator=(BSRational&& r)
        {
            mNumerator = std::move(r.mNumerator);
            mDenominator = std::move(r.mDenominator);
#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
            mValue = (double)*this;
#endif
            return *this;
        }

        // Member access.
        inline int GetSign() const
        {
            return mNumerator.GetSign() * mDenominator.GetSign();
        }

        inline BSNumber<UIntegerType> const& GetNumerator() const
        {
            return mNumerator;
        }

        inline BSNumber<UIntegerType> const& GetDenominator() const
        {
            return mDenominator;
        }

        // Comparisons.
        bool operator==(BSRational const& r) const
        {
            // Do inexpensive sign tests first for optimum performance.
            if (mNumerator.mSign != r.mNumerator.mSign)
            {
                return false;
            }
            if (mNumerator.mSign == 0)
            {
                // The numbers are both zero.
                return true;
            }

            return mNumerator * r.mDenominator == mDenominator * r.mNumerator;
        }

        bool operator!=(BSRational const& r) const
        {
            return !operator==(r);
        }

        bool operator< (BSRational const& r) const
        {
            // Do inexpensive sign tests first for optimum performance.
            if (mNumerator.mSign > 0)
            {
                if (r.mNumerator.mSign <= 0)
                {
                    return false;
                }
            }
            else if (mNumerator.mSign == 0)
            {
                return r.mNumerator.mSign > 0;
            }
            else if (mNumerator.mSign < 0)
            {
                if (r.mNumerator.mSign >= 0)
                {
                    return true;
                }
            }

            return mNumerator * r.mDenominator < mDenominator * r.mNumerator;
        }

        bool operator<=(BSRational const& r) const
        {
            return !r.operator<(*this);
        }

        bool operator> (BSRational const& r) const
        {
            return r.operator<(*this);
        }

        bool operator>=(BSRational const& r) const
        {
            return !operator<(r);
        }

        // Unary operations.
        BSRational operator+() const
        {
            return *this;
        }

        BSRational operator-() const
        {
            return BSRational(-mNumerator, mDenominator);
        }

        // Arithmetic.
        BSRational operator+(BSRational const& r) const
        {
            BSNumber<UIntegerType> product0 = mNumerator * r.mDenominator;
            BSNumber<UIntegerType> product1 = mDenominator * r.mNumerator;
            BSNumber<UIntegerType> numerator = product0 + product1;

            // Complex expressions can lead to 0/denom, where denom is not 1.
            if (numerator.mSign != 0)
            {
                BSNumber<UIntegerType> denominator = mDenominator * r.mDenominator;
                return BSRational(numerator, denominator);
            }
            else
            {
                return BSRational(0);
            }
        }

        BSRational operator-(BSRational const& r) const
        {
            BSNumber<UIntegerType> product0 = mNumerator * r.mDenominator;
            BSNumber<UIntegerType> product1 = mDenominator * r.mNumerator;
            BSNumber<UIntegerType> numerator = product0 - product1;

            // Complex expressions can lead to 0/denom, where denom is not 1.
            if (numerator.mSign != 0)
            {
                BSNumber<UIntegerType> denominator = mDenominator * r.mDenominator;
                return BSRational(numerator, denominator);
            }
            else
            {
                return BSRational(0);
            }
        }

        BSRational operator*(BSRational const& r) const
        {
            BSNumber<UIntegerType> numerator = mNumerator * r.mNumerator;

            // Complex expressions can lead to 0/denom, where denom is not 1.
            if (numerator.mSign != 0)
            {
                BSNumber<UIntegerType> denominator = mDenominator * r.mDenominator;
                return BSRational(numerator, denominator);
            }
            else
            {
                return BSRational(0);
            }
        }

        BSRational operator/(BSRational const& r) const
        {
            LogAssert(r.mNumerator.mSign != 0, "Division by zero not allowed.");

            BSNumber<UIntegerType> numerator = mNumerator * r.mDenominator;

            // Complex expressions can lead to 0/denom, where denom is not 1.
            if (numerator.mSign != 0)
            {
                BSNumber<UIntegerType> denominator = mDenominator * r.mNumerator;
                if (denominator.mSign < 0)
                {
                    numerator.mSign = -numerator.mSign;
                    denominator.mSign = 1;
                }
                return BSRational(numerator, denominator);
            }
            else
            {
                return BSRational(0);
            }
        }

        BSRational& operator+=(BSRational const& r)
        {
            *this = operator+(r);
            return *this;
        }

        BSRational& operator-=(BSRational const& r)
        {
            *this = operator-(r);
            return *this;
        }

        BSRational& operator*=(BSRational const& r)
        {
            *this = operator*(r);
            return *this;
        }

        BSRational& operator/=(BSRational const& r)
        {
            *this = operator/(r);
            return *this;
        }

        // Disk input/output.  The fstream objects should be created using
        // std::ios::binary.  The return value is 'true' iff the operation
        // was successful.
        bool Write(std::ofstream& output) const
        {
            return mNumerator.Write(output) && mDenominator.Write(output);
        }

        bool Read(std::ifstream& input)
        {
            return mNumerator.Read(input) && mDenominator.Read(input);
        }

    private:
        // Generic conversion code that converts to the correctly rounded
        // result using round-to-nearest-ties-to-even.
        template <typename UIntType, typename RealType>
        RealType Convert() const
        {
            if (mNumerator.mSign == 0)
            {
                return (RealType)0;
            }

            // The ratio is abstractly of the form (1.u*2^p)/(1.v*2^q).
            // Convert to the form (1.u/1.v)*2^{p-q}, if 1.u >= 1.v, or to the
            // form (2*(1.u)/1.v)*2*{p-q-1}) if 1.u < 1.v.  The final form
            // n/d must be in the interval [1,2).
            BSNumber<UIntegerType> n = mNumerator, d = mDenominator;
            int32_t sign = n.mSign * d.mSign;
            n.mSign = 1;
            d.mSign = 1;
            int32_t pmq = n.GetExponent() - d.GetExponent();
            n.mBiasedExponent = 1 - n.GetUInteger().GetNumBits();
            d.mBiasedExponent = 1 - d.GetUInteger().GetNumBits();
            if (BSNumber<UIntegerType>::LessThanIgnoreSign(n, d))
            {
                ++n.mBiasedExponent;
                --pmq;
            }

            // At this time, n/d = 1.c in [1,2).  Define the sequence of bits
            // w = 1c = w_{imax} w_{imax-1} ... w_0 w_{-1} w_{-2} ... where
            // imax = precision(RealType)-1 and w_{imax} = 1.

            // Compute 'precision' bits for w, the leading bit guaranteed to
            // be 1 and occurring at index (1 << (precision-1)).
            BSNumber<UIntegerType> one(1), two(2);
            int const imax = std::numeric_limits<RealType>::digits - 1;
            UIntType w = 0;
            UIntType mask = ((UIntType)1 << imax);
            for (int i = imax; i >= 0; --i, mask >>= 1)
            {
                if (BSNumber<UIntegerType>::LessThanIgnoreSign(n, d))
                {
                    n = two * n;
                }
                else
                {
                    n = two * (n - d);
                    w |= mask;
                }
            }

            // Apply the mode round-to-nearest-ties-to-even to decide whether
            // to round down or up.  We computed w = w_{imax} ... w_0.  The
            // remainder is n/d = w_{imax+1}.w_{imax+2}... in [0,2).  Compute
            // n'/d = (n-d)/d in [-1,1).  Round-to-nearest-ties-to-even mode
            // is the following, where we need only test the sign of n'.  A
            // remainder of "half" is the case n' = 0.
            //   Round down when n' < 0 or (n' = 0 and w_0 = 0):  use w
            //   Round up when n' > 0 or (n' = 0 and w_0 == 1):  use w+1
            n = n - d;
            if (n.mSign > 0 || (n.mSign == 0 && (w & 1) == 1))
            {
                ++w;
            }

            if (w > 0)
            {
                // Ensure that the low-order bit of w is 1, which is required
                // for the BSNumber integer part.
                int32_t trailing = GetTrailingBit(w);
                w >>= trailing;
                pmq += trailing;

                // Compute a BSNumber with integer part w and the appropriate
                // number of bits and exponents.
                BSNumber<UIntegerType> result(w);
                result.mBiasedExponent = pmq - imax;
                RealType converted = (RealType)result;
                if (sign < 0)
                {
                    converted = -converted;
                }
                return converted;
            }
            else
            {
                return (RealType)0;
            }
            }

#if defined(GTE_BINARY_SCIENTIFIC_SHOW_DOUBLE)
    public:
        // List this first so that it shows up first in the debugger watch
        // window.
        double mValue;
    private:
#endif

        BSNumber<UIntegerType> mNumerator, mDenominator;

        friend class UnitTestBSRational;
    };
}

namespace std
{
    // TODO: Allow for implementations of the math functions in which a
    // specified precision is used when computing the result.

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> abs(gte::BSRational<UIntegerType> const& x)
    {
        return (x.GetSign() >= 0 ? x : -x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> acos(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::acos((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> acosh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::acosh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> asin(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::asin((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> asinh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::asinh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> atan(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::atan((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> atanh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::atanh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> atan2(gte::BSRational<UIntegerType> const& y, gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::atan2((double)y, (double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> ceil(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::ceil((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> cos(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::cos((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> cosh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::cosh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> exp(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::exp((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> exp2(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::exp2((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> floor(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::floor((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> fmod(gte::BSRational<UIntegerType> const& x, gte::BSRational<UIntegerType> const& y)
    {
        return (gte::BSRational<UIntegerType>)std::fmod((double)x, (double)y);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> frexp(gte::BSRational<UIntegerType> const& x, int* exponent)
    {
        return (gte::BSRational<UIntegerType>)std::frexp((double)x, exponent);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> ldexp(gte::BSRational<UIntegerType> const& x, int exponent)
    {
        return (gte::BSRational<UIntegerType>)std::ldexp((double)x, exponent);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> log(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::log((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> log2(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::log2((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> log10(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::log10((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> pow(gte::BSRational<UIntegerType> const& x, gte::BSRational<UIntegerType> const& y)
    {
        return (gte::BSRational<UIntegerType>)std::pow((double)x, (double)y);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> sin(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::sin((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> sinh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::sinh((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> sqrt(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::sqrt((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> tan(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::tan((double)x);
    }

    template <typename UIntegerType>
    inline gte::BSRational<UIntegerType> tanh(gte::BSRational<UIntegerType> const& x)
    {
        return (gte::BSRational<UIntegerType>)std::tanh((double)x);
    }
}

namespace gte
{
    template <typename UIntegerType>
    inline BSRational<UIntegerType> atandivpi(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)atandivpi((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> atan2divpi(BSRational<UIntegerType> const& y, BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)atan2divpi((double)y, (double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> clamp(BSRational<UIntegerType> const& x, BSRational<UIntegerType> const& xmin, BSRational<UIntegerType> const& xmax)
    {
        return (BSRational<UIntegerType>)clamp((double)x, (double)xmin, (double)xmax);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> cospi(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)cospi((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> exp10(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)exp10((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> invsqrt(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)invsqrt((double)x);
    }

    template <typename UIntegerType>
    inline int isign(BSRational<UIntegerType> const& x)
    {
        return isign((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> saturate(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)saturate((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> sign(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)sign((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> sinpi(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)sinpi((double)x);
    }

    template <typename UIntegerType>
    inline BSRational<UIntegerType> sqr(BSRational<UIntegerType> const& x)
    {
        return (BSRational<UIntegerType>)sqr((double)x);
    }

    // See the comments in GteMath.h about traits is_arbitrary_precision
    // and has_division_operator.
    template <typename UIntegerType>
    struct is_arbitrary_precision_internal<BSRational<UIntegerType>> : std::true_type {};

    template <typename UIntegerType>
    struct has_division_operator_internal<BSRational<UIntegerType>> : std::true_type {};
}
