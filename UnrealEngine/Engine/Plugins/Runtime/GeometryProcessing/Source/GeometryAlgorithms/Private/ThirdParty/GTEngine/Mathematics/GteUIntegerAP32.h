// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteUIntegerALU32.h>
#include <fstream>
#include <vector>

// Class UIntegerAP32 is designed to support arbitrary precision arithmetic
// using BSNumber and BSRational.  It is not a general-purpose class for
// arithmetic of unsigned integers.

// Uncomment this to collect statistics on how large the UIntegerAP32 storage
// becomes when using it for the UIntegerType of BSNumber.  After a sequence
// of BSNumber operations,  look at UIntegerAP32::msMaxSize in the debugger
// watch window.  If the number is not too large, you might be safe in
// replacing UIntegerAP32 by UIntegerFP32<N>, where N is the value of
// UIntegerAP32::msMaxSize.  This leads to much faster code because you no
// you no longer have dynamic memory allocations and deallocations that occur
// regularly with std::vector<uint32_t> during BSNumber operations.  A safer
// choice is to argue mathematically that the maximum size is bounded by N.
// This requires an analysis of how many bits of precision you need for the
// types of computation you perform.  See class BSPrecision for code that
// allows you to compute maximum N.
//
//#define GTE_COLLECT_UINTEGERAP32_STATISTICS

#if defined(GTE_COLLECT_UINTEGERAP32_STATISTICS)
#include <ThirdParty/GTEngine/LowLevel/GteAtomicMinMax.h>
#endif

namespace gte
{

class UIntegerAP32 : public UIntegerALU32<UIntegerAP32>
{
public:
    // Construction.
    UIntegerAP32();
    UIntegerAP32(UIntegerAP32 const& number);
    UIntegerAP32(uint32_t number);
    UIntegerAP32(uint64_t number);
    UIntegerAP32(int numBits);

    // Assignment.
    UIntegerAP32& operator=(UIntegerAP32 const& number);

    // Support for std::move.
    UIntegerAP32(UIntegerAP32&& number);
    UIntegerAP32& operator=(UIntegerAP32&& number);

    // Member access.
    void SetNumBits(uint32_t numBits);
    inline int32_t GetNumBits() const;
    inline std::vector<uint32_t> const& GetBits() const;
    inline std::vector<uint32_t>& GetBits();
    inline void SetBack(uint32_t value);
    inline uint32_t GetBack() const;
    inline int32_t GetSize() const;

    // Disk input/output.  The fstream objects should be created using
    // std::ios::binary.  The return value is 'true' iff the operation
    // was successful.
    bool Write(std::ofstream& output) const;
    bool Read(std::ifstream& input);

private:
    int32_t mNumBits;
    std::vector<uint32_t> mBits;

    friend class UnitTestBSNumber;

#if defined(GTE_COLLECT_UINTEGERAP32_STATISTICS)
    static std::atomic<size_t> msMaxSize;
public:
    static void SetMaxSizeToZero() { msMaxSize = 0; }
    static size_t GetMaxSize() { return msMaxSize; }
#endif
};


inline int32_t UIntegerAP32::GetNumBits() const
{
    return mNumBits;
}

inline std::vector<uint32_t> const& UIntegerAP32::GetBits() const
{
    return mBits;
}

inline std::vector<uint32_t>& UIntegerAP32::GetBits()
{
    return mBits;
}

inline void UIntegerAP32::SetBack(uint32_t value)
{
    mBits.back() = value;
}

inline uint32_t UIntegerAP32::GetBack() const
{
    return mBits.back();
}

inline int32_t UIntegerAP32::GetSize() const
{
    return static_cast<int32_t>(mBits.size());
}


}
