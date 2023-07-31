// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstdint>

#if defined(__MSWINDOWS__) && _MSC_VER < 1900
// MSVS 2013 has an implementation of std::chrono::high_resolution_clock
// that does not use a 64-bit clock (QueryPerformanceCounter).  Instead,
// it appears to use a file-system clock that is 24 bits.  To obtain
// accurate measurements, we need the 64-bit clock.  However, MSVS 2015
// does use a 64-bit clock.
#define GTE_NEEDS_64_BIT_CLOCK
#endif

#if !defined(GTE_NEEDS_64_BIT_CLOCK)
#include <chrono>
#endif

namespace gte
{

class GTE_IMPEXP Timer
{
public:
    // Construction of a high-resolution timer (64-bit).
    Timer();

    // Get the current time relative to the initial time.
    int64_t GetNanoseconds() const;
    int64_t GetMicroseconds() const;
    int64_t GetMilliseconds() const;
    double GetSeconds() const;

    // Reset so that the current time is the initial time.
    void Reset();

private:
#if defined(GTE_NEEDS_64_BIT_CLOCK)
    // Internally use QueryPerformanceCounter.
    int64_t GetTicks() const;

    int64_t mFrequency, mInitialTicks;
    double mInvFrequency;
#else
    std::chrono::high_resolution_clock::time_point mInitialTime;
#endif
};

}
