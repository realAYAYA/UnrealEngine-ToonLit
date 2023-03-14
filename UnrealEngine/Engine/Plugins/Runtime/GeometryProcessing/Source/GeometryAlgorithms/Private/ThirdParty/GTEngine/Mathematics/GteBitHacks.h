// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/07/04)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstdint>

namespace gte
{

GTE_IMPEXP bool IsPowerOfTwo(uint32_t value);
GTE_IMPEXP bool IsPowerOfTwo(int32_t value);

GTE_IMPEXP uint32_t Log2OfPowerOfTwo(uint32_t powerOfTwo);
GTE_IMPEXP int32_t Log2OfPowerOfTwo(int32_t powerOfTwo);

// Call these only for nonzero values.  If value is zero, then GetLeadingBit
// and GetTrailingBit return zero.
GTE_IMPEXP int32_t GetLeadingBit(uint32_t value);
GTE_IMPEXP int32_t GetLeadingBit(int32_t value);
GTE_IMPEXP int32_t GetLeadingBit(uint64_t value);
GTE_IMPEXP int32_t GetLeadingBit(int64_t value);
GTE_IMPEXP int32_t GetTrailingBit(uint32_t value);
GTE_IMPEXP int32_t GetTrailingBit(int32_t value);
GTE_IMPEXP int32_t GetTrailingBit(uint64_t value);
GTE_IMPEXP int32_t GetTrailingBit(int64_t value);

// Round up to a power of two.  If input is zero, the return is 1.  If input
// is larger than 2^{31}, the return is 2^{32}.
GTE_IMPEXP uint64_t RoundUpToPowerOfTwo(uint32_t value);

// Round down to a power of two.  If input is zero, the return is 0.
GTE_IMPEXP uint32_t RoundDownToPowerOfTwo(uint32_t value);

}
