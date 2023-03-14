// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstdint>

namespace gte
{

template <typename Float, typename UInt, int NumBits, int Precision>
class IEEEBinary
{
public:
    // For generic access of the template types.
    typedef Float FloatType;
    typedef UInt UIntType;

    // Construction from an encoding.  Copy constructor, destructor, and
    // assignment operator are implicitly generated.  For the 3-parameter
    // constructor, see the comments for SetEncoding(...).
    ~IEEEBinary ();
    IEEEBinary (); // The encoding is uninitialized.
    IEEEBinary (IEEEBinary const& object);
    IEEEBinary (UInt inEncoding);
    IEEEBinary (UInt inSign, UInt inBiased, UInt inTrailing);
    IEEEBinary (Float inNumber);

    // Implicit conversion to floating-point type.
    operator UInt () const;
    operator Float () const;

    // Assignment.
    IEEEBinary& operator= (IEEEBinary const& object);

    // Special constants.
    static int const NUM_ENCODING_BITS    = NumBits;
    static int const NUM_EXPONENT_BITS    = NumBits - Precision;
    static int const NUM_SIGNIFICAND_BITS = Precision;
    static int const NUM_TRAILING_BITS    = Precision - 1;
    static int const EXPONENT_BIAS        = (1 << (NUM_EXPONENT_BITS - 1)) - 1;
    static int const MAX_BIASED_EXPONENT  = (1 << NUM_EXPONENT_BITS) - 1;
    static int const MIN_SUB_EXPONENT     = 1 - EXPONENT_BIAS;
    static int const MIN_EXPONENT         = MIN_SUB_EXPONENT - NUM_TRAILING_BITS;
    static int const SIGN_SHIFT           = NumBits - 1;

    static UInt const SIGN_MASK           = (UInt(1) << (NumBits - 1));   
    static UInt const NOT_SIGN_MASK       = UInt(~SIGN_MASK);
    static UInt const TRAILING_MASK       = (UInt(1) << NUM_TRAILING_BITS) - 1;
    static UInt const EXPONENT_MASK       = NOT_SIGN_MASK & ~TRAILING_MASK;
    static UInt const NAN_QUIET_MASK      = (UInt(1) << (NUM_TRAILING_BITS - 1));
    static UInt const NAN_PAYLOAD_MASK    = (TRAILING_MASK >> 1);
    static UInt const MAX_TRAILING        = TRAILING_MASK;
    static UInt const SUP_TRAILING        = (UInt(1) << NUM_TRAILING_BITS);
    static UInt const POS_ZERO            = UInt(0);
    static UInt const NEG_ZERO            = SIGN_MASK;
    static UInt const MIN_SUBNORMAL       = UInt(1);
    static UInt const MAX_SUBNORMAL       = TRAILING_MASK;
    static UInt const MIN_NORMAL          = SUP_TRAILING;
    static UInt const MAX_NORMAL          = NOT_SIGN_MASK & ~SUP_TRAILING;
    static UInt const POS_INFINITY        = EXPONENT_MASK;
    static UInt const NEG_INFINITY        = SIGN_MASK | EXPONENT_MASK;

    // The types of numbers.
    enum Classification
    {
        CLASS_NEG_INFINITY,
        CLASS_NEG_SUBNORMAL,
        CLASS_NEG_NORMAL,
        CLASS_NEG_ZERO,
        CLASS_POS_ZERO,
        CLASS_POS_SUBNORMAL,
        CLASS_POS_NORMAL,
        CLASS_POS_INFINITY,
        CLASS_QUIET_NAN,
        CLASS_SIGNALING_NAN
    };

    Classification GetClassification () const;
    bool IsZero () const;
    bool IsSignMinus () const;
    bool IsSubnormal () const;
    bool IsNormal () const;
    bool IsFinite () const;
    bool IsInfinite () const;
    bool IsNaN () const;
    bool IsSignalingNaN () const;

    // Get neighboring numbers.
    UInt GetNextUp () const;
    UInt GetNextDown () const;

    // Encode and decode the binary representation.  The sign is 0 (number
    // is nonnegative) or 1 (number is negative).  The biased exponent is in
    // the range [0,MAX_BIASED_EXPONENT].  The trailing significand is in the
    // range [0,MAX_TRAILING].
    UInt GetSign () const;
    UInt GetBiased () const;
    UInt GetTrailing () const;
    void SetEncoding (UInt sign, UInt biased, UInt trailing);
    void GetEncoding (UInt& sign, UInt& biased, UInt& trailing) const;

    // Access for direct manipulation of the object.
    union
    {
        UInt encoding;
        Float number;
    };
};

typedef IEEEBinary<float, uint32_t, 32, 24> IEEEBinary32;
typedef IEEEBinary<double, uint64_t, 64, 53> IEEEBinary64;


template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::~IEEEBinary()
{
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::IEEEBinary()
{
    // The member mEncoding is uninitialized.
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::IEEEBinary(
    IEEEBinary const& object)
    :
    encoding(object.encoding)
{
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::IEEEBinary(UInt inEncoding)
    :
    encoding(inEncoding)
{
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::IEEEBinary(UInt inSign,
    UInt inBiased, UInt inTrailing)
{
    SetEncoding(inSign, inBiased, inTrailing);
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::IEEEBinary(Float inNumber)
    :
    number(inNumber)
{
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::operator UInt () const
{
    return encoding;
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>::operator Float () const
{
    return number;
}

template <typename Float, typename UInt, int NumBits, int Precision>
IEEEBinary<Float, UInt, NumBits, Precision>&
IEEEBinary<Float, UInt, NumBits, Precision>::operator= (IEEEBinary const& object)
{
    encoding = object.encoding;
    return *this;
}

template <typename Float, typename UInt, int NumBits, int Precision>
typename IEEEBinary<Float, UInt, NumBits, Precision>::Classification
IEEEBinary<Float, UInt, NumBits, Precision>::GetClassification() const
{
    UInt sign, biased, trailing;
    GetEncoding(sign, biased, trailing);

    if (biased == 0)
    {
        if (trailing == 0)
        {
            return (sign != 0 ? CLASS_NEG_ZERO : CLASS_POS_ZERO);
        }
        else
        {
            return (sign != 0 ? CLASS_NEG_SUBNORMAL : CLASS_POS_SUBNORMAL);
        }
    }
    else if (biased < MAX_BIASED_EXPONENT)
    {
        return (sign != 0 ? CLASS_NEG_NORMAL : CLASS_POS_NORMAL);
    }
    else if (trailing == 0)
    {
        return (sign != 0 ? CLASS_NEG_INFINITY : CLASS_POS_INFINITY);
    }
    else if (trailing & NAN_QUIET_MASK)
    {
        return CLASS_QUIET_NAN;
    }
    else
    {
        return CLASS_SIGNALING_NAN;
    }
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsZero() const
{
    return encoding == POS_ZERO || encoding == NEG_ZERO;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsSignMinus() const
{
    return (encoding & SIGN_MASK) != 0;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsSubnormal() const
{
    return GetBiased() == 0 && GetTrailing() > 0;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsNormal() const
{
    UInt biased = GetBiased();
    return 0 < biased && biased < MAX_BIASED_EXPONENT;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsFinite() const
{
    return GetBiased() < MAX_BIASED_EXPONENT;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsInfinite() const
{
    return GetBiased() == MAX_BIASED_EXPONENT && GetTrailing() == 0;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsNaN() const
{
    return GetBiased() == MAX_BIASED_EXPONENT && GetTrailing() != 0;
}

template <typename Float, typename UInt, int NumBits, int Precision>
bool IEEEBinary<Float, UInt, NumBits, Precision>::IsSignalingNaN() const
{
    UInt trailing = GetTrailing();
    return GetBiased() == MAX_BIASED_EXPONENT
        && (trailing & NAN_QUIET_MASK) == 0
        && (trailing & NAN_PAYLOAD_MASK) != 0;
}

template <typename Float, typename UInt, int NumBits, int Precision>
UInt IEEEBinary<Float, UInt, NumBits, Precision>::GetNextUp() const
{
    UInt sign, biased, trailing;
    GetEncoding(sign, biased, trailing);

    if (biased == 0)
    {
        if (trailing == 0)
        {
            // The next-up for both -0 and +0 is MIN_SUBNORMAL.
            return MIN_SUBNORMAL;
        }
        else
        {
            if (sign != 0)
            {
                // When trailing is 1, 'this' is -MIN_SUBNORMAL and next-up
                // is -0.
                --trailing;
                return SIGN_MASK | trailing;
            }
            else
            {
                // When trailing is MAX_TRAILING, 'this' is MAX_SUBNORMAL
                // and next-up is MIN_NORMAL.
                ++trailing;
                return trailing;
            }
        }
    }
    else if (biased < MAX_BIASED_EXPONENT)
    {
        UInt nonnegative = (encoding & NOT_SIGN_MASK);
        if (sign != 0)
        {
            --nonnegative;
            return SIGN_MASK | nonnegative;
        }
        else
        {
            ++nonnegative;
            return nonnegative;
        }
    }
    else if (trailing == 0)
    {
        if (sign != 0)
        {
            // The next-up of -INFINITY is -MAX_NORMAL.
            return SIGN_MASK | MAX_NORMAL;
        }
        else
        {
            // The next-up of +INFINITY is +INFINITY.
            return POS_INFINITY;
        }
    }
    else if (trailing & NAN_QUIET_MASK)
    {
        // TODO.  The IEEE standard is not clear what to do here.  Figure
        // out what it means.
        return 0;
    }
    else
    {
        // TODO.  The IEEE standard is not clear what to do here.  Figure
        // out what it means.
        return 0;
    }
}

template <typename Float, typename UInt, int NumBits, int Precision>
UInt IEEEBinary<Float, UInt, NumBits, Precision>::GetNextDown() const
{
    UInt sign, biased, trailing;
    GetEncoding(sign, biased, trailing);

    if (biased == 0)
    {
        if (trailing == 0)
        {
            // The next-down for both -0 and +0 is -MIN_SUBNORMAL.
            return SIGN_MASK | MIN_SUBNORMAL;
        }
        else
        {
            if (sign == 0)
            {
                // When trailing is 1, 'this' is MIN_SUBNORMAL and next-down
                // is +0.
                --trailing;
                return trailing;
            }
            else
            {
                // When trailing is MAX_TRAILING, 'this' is -MAX_SUBNORMAL
                // and next-down is -MIN_NORMAL.
                ++trailing;
                return SIGN_MASK | trailing;
            }
        }
    }
    else if (biased < MAX_BIASED_EXPONENT)
    {
        UInt nonnegative = (encoding & NOT_SIGN_MASK);
        if (sign == 0)
        {
            --nonnegative;
            return nonnegative;
        }
        else
        {
            ++nonnegative;
            return SIGN_MASK | nonnegative;
        }
    }
    else if (trailing == 0)
    {
        if (sign == 0)
        {
            // The next-down of +INFINITY is +MAX_NORMAL.
            return MAX_NORMAL;
        }
        else
        {
            // The next-down of -INFINITY is -INFINITY.
            return NEG_INFINITY;
        }
    }
    else if (trailing & NAN_QUIET_MASK)
    {
        // TODO.  The IEEE standard is not clear what to do here.  Figure
        // out what it means.
        return 0;
    }
    else
    {
        // TODO.  The IEEE standard is not clear what to do here.  Figure
        // out what it means.
        return 0;
    }
}

template <typename Float, typename UInt, int NumBits, int Precision>
UInt IEEEBinary<Float, UInt, NumBits, Precision>::GetSign() const
{
    return (encoding & SIGN_MASK) >> SIGN_SHIFT;
}

template <typename Float, typename UInt, int NumBits, int Precision>
UInt IEEEBinary<Float, UInt, NumBits, Precision>::GetBiased() const
{
    return (encoding & EXPONENT_MASK) >> NUM_TRAILING_BITS;
}

template <typename Float, typename UInt, int NumBits, int Precision>
UInt IEEEBinary<Float, UInt, NumBits, Precision>::GetTrailing() const
{
    return encoding & TRAILING_MASK;
}

template <typename Float, typename UInt, int NumBits, int Precision>
void IEEEBinary<Float, UInt, NumBits, Precision>::SetEncoding(UInt sign,
    UInt biased, UInt trailing)
{
    encoding =
        (sign << SIGN_SHIFT) | (biased << NUM_TRAILING_BITS) | trailing;
}

template <typename Float, typename UInt, int NumBits, int Precision>
void IEEEBinary<Float, UInt, NumBits, Precision>::GetEncoding(UInt& sign,
    UInt& biased, UInt& trailing) const
{
    sign = GetSign();
    biased = GetBiased();
    trailing = GetTrailing();
}


}
