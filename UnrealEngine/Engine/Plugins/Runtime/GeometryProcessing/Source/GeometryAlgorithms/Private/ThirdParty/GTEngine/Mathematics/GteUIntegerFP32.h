// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/07/04)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>
#include <ThirdParty/GTEngine/Mathematics/GteUIntegerALU32.h>
#include <array>
#include <fstream>

// Class UIntegerFP32 is designed to support fixed precision arithmetic
// using BSNumber and BSRational.  It is not a general-purpose class for
// arithmetic of unsigned integers.  The template parameter N is the
// number of 32-bit words required to store the precision for the desired
// computations (maximum number of bits is 32*N).

// Uncomment this to trap when an attempt is made to create storage with
// more than N uint32_t items.
//
//#define GTE_ASSERT_ON_UINTEGERFP32_OUT_OF_RANGE

// Uncomment this to collect statistics on how large the UIntegerFP32 storage
// becomes when using it for the UIntegerType of BSNumber.  After a sequence
// of BSNumber operations,  look at UIntegerFP32::msMaxSize in the debugger
// watch window.  If the number is not too large, you might be safe in
// replacing the template parameter N by a smaller number.  See class
// BSPrecision for code that allows you to compute maximum N.
//
// NOTE:  Because UIntegerFP32::msMaxSize is a static member of a template
// class, if you expose this define, you must also declare in your code
// 'std::atomic<int32_t> gte::UIntegerFP32<N>::msMaxSize;'
//#define GTE_COLLECT_UINTEGERFP32_STATISTICS

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
#include <ThirdParty/GTEngine/LowLevel/GteAtomicMinMax.h>
#endif

namespace gte
{

template <int N>
class UIntegerFP32 : public UIntegerALU32<UIntegerFP32<N>>
{
public:
    // Construction.
    UIntegerFP32();
    UIntegerFP32(UIntegerFP32 const& number);
    UIntegerFP32(uint32_t number);
    UIntegerFP32(uint64_t number);
    UIntegerFP32(int numBits);

    // Assignment.  Only mSize elements are copied.
    UIntegerFP32& operator=(UIntegerFP32 const& number);

    // Support for std::move.  The interface is required by BSNumber, but the
    // std::move of std::array is a copy (no pointer stealing).  Moreover,
    // a std::array object in this class typically uses smaller than N
    // elements, the actual size stored in mSize, so we do not want to move
    // everything.  Therefore, the move operator only copies the bits BUT
    // BUT 'number' is modified as if you have stolen the data (mNumBits and
    // mSize set to zero).
    UIntegerFP32(UIntegerFP32&& number);
    UIntegerFP32& operator=(UIntegerFP32&& number);

    // Member access.
    void SetNumBits(uint32_t numBits);
    inline int32_t GetNumBits() const;
    inline std::array<uint32_t, N> const& GetBits() const;
    inline std::array<uint32_t, N>& GetBits();
    inline void SetBack(uint32_t value);
    inline uint32_t GetBack() const;
    inline int32_t GetSize() const;

    // Disk input/output.  The fstream objects should be created using
    // std::ios::binary.  The return value is 'true' iff the operation
    // was successful.
    bool Write(std::ofstream& output) const;
    bool Read(std::ifstream& input);

private:
    int32_t mNumBits, mSize;
    std::array<uint32_t, N> mBits;

    friend class UnitTestBSNumber;

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
    static std::atomic<int32_t> msMaxSize;
public:
    static void SetMaxSizeToZero() { msMaxSize = 0; }
    static int32_t GetMaxSize() { return msMaxSize; }
#endif
};


template <int N>
UIntegerFP32<N>::UIntegerFP32()
    :
    mNumBits(0),
    mSize(0)
{
    static_assert(N >= 1, "Invalid size N.");
}

template <int N>
UIntegerFP32<N>::UIntegerFP32(UIntegerFP32 const& number)
{
    static_assert(N >= 1, "Invalid size N.");

    *this = number;
}

template <int N>
UIntegerFP32<N>::UIntegerFP32(uint32_t number)
{
    static_assert(N >= 1, "Invalid size N.");

    if (number > 0)
    {
        int32_t first = GetLeadingBit(number);
        int32_t last = GetTrailingBit(number);
        mNumBits = first - last + 1;
        mSize = 1;
        mBits[0] = (number >> last);
    }
    else
    {
        mNumBits = 0;
        mSize = 0;
    }

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
    AtomicMax(msMaxSize, mSize);
#endif
}

template <int N>
UIntegerFP32<N>::UIntegerFP32(uint64_t number)
{
    static_assert(N >= 2, "N not large enough to store 64-bit integers.");

    if (number > 0)
    {
        int32_t first = GetLeadingBit(number);
        int32_t last = GetTrailingBit(number);
        number >>= last;
        mNumBits = first - last + 1;
        mSize = 1 + (mNumBits - 1) / 32;
        mBits[0] = (uint32_t)(number & 0x00000000FFFFFFFFull);
        if (mSize > 1)
        {
            mBits[1] = (uint32_t)((number >> 32) & 0x00000000FFFFFFFFull);
        }
    }
    else
    {
        mNumBits = 0;
        mSize = 0;
    }

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
    AtomicMax(msMaxSize, mSize);
#endif
}

template <int N>
UIntegerFP32<N>::UIntegerFP32(int numBits)
    :
    mNumBits(numBits),
    mSize(1 + (numBits - 1) / 32)
{
    static_assert(N >= 1, "Invalid size N.");

#if defined(GTE_ASSERT_ON_UINTEGERFP32_OUT_OF_RANGE)
    LogAssert(mSize <= N, "N not large enough to store number of bits.");
#endif

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
    AtomicMax(msMaxSize, mSize);
#endif
}

template <int N>
UIntegerFP32<N>& UIntegerFP32<N>::operator=(UIntegerFP32 const& number)
{
    static_assert(N >= 1, "Invalid size N.");

    mNumBits = number.mNumBits;
    mSize = number.mSize;
    std::copy(number.mBits.begin(), number.mBits.begin() + mSize,
        mBits.begin());
    return *this;
}

template <int N>
UIntegerFP32<N>::UIntegerFP32(UIntegerFP32&& number)
{
    *this = std::move(number);
}

template <int N>
UIntegerFP32<N>& UIntegerFP32<N>::operator=(UIntegerFP32&& number)
{
    mNumBits = number.mNumBits;
    mSize = number.mSize;
    std::copy(number.mBits.begin(), number.mBits.begin() + mSize,
        mBits.begin());
    number.mNumBits = 0;
    number.mSize = 0;
    return *this;
}

template <int N>
void UIntegerFP32<N>::SetNumBits(uint32_t numBits)
{
    mNumBits = numBits;
    mSize = 1 + (numBits - 1) / 32;

#if defined(GTE_ASSERT_ON_UINTEGERFP32_OUT_OF_RANGE)
    LogAssert(mSize <= N, "N not large enough to store number of bits.");
#endif

#if defined(GTE_COLLECT_UINTEGERFP32_STATISTICS)
    AtomicMax(msMaxSize, mSize);
#endif
}

template <int N> inline
int32_t UIntegerFP32<N>::GetNumBits() const
{
    return mNumBits;
}

template <int N> inline
std::array<uint32_t, N> const& UIntegerFP32<N>::GetBits() const
{
    return mBits;
}

template <int N> inline
std::array<uint32_t, N>& UIntegerFP32<N>::GetBits()
{
    return mBits;
}

template <int N> inline
void UIntegerFP32<N>::SetBack(uint32_t value)
{
    mBits[mSize - 1] = value;
}

template <int N> inline
uint32_t UIntegerFP32<N>::GetBack() const
{
    return mBits[mSize - 1];
}

template <int N> inline
int32_t UIntegerFP32<N>::GetSize() const
{
    return mSize;
}

template <int N>
bool UIntegerFP32<N>::Write(std::ofstream& output) const
{
    if (output.write((char const*)&mNumBits, sizeof(mNumBits)).bad())
    {
        return false;
    }

    if (output.write((char const*)&mSize, sizeof(mSize)).bad())
    {
        return false;
    }

    return output.write((char const*)&mBits[0],
        mSize*sizeof(mBits[0])).good();
}

template <int N>
bool UIntegerFP32<N>::Read(std::ifstream& input)
{
    if (input.read((char*)&mNumBits, sizeof(mNumBits)).bad())
    {
        return false;
    }

    if (input.read((char*)&mSize, sizeof(mSize)).bad())
    {
        return false;
    }

    return input.read((char*)&mBits[0], mSize*sizeof(mBits[0])).good();
}


}
