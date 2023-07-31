// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/LowLevel/GteTimer.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
using namespace gte;

#if defined(GTE_NEEDS_64_BIT_CLOCK)

#include <cmath>
#include <windows.h>

Timer::Timer()
    :
    mFrequency(0),
    mInitialTicks(0),
    mInvFrequency(0.0)
{
    LARGE_INTEGER frequency = { 1 }, counter = { 0 };
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    mFrequency = static_cast<int64_t>(frequency.QuadPart);
    mInitialTicks = static_cast<int64_t>(counter.QuadPart);
    mInvFrequency = 1.0 / static_cast<double>(mFrequency);
}

int64_t Timer::GetNanoseconds() const
{
    int64_t ticks = GetTicks();
    double seconds = mInvFrequency * static_cast<double>(ticks);
    int64_t nanoseconds = static_cast<int64_t>(std::ceil(1000000000.0 * seconds));
    return nanoseconds;
}

int64_t Timer::GetMicroseconds() const
{
    int64_t ticks = GetTicks();
    double seconds = mInvFrequency * static_cast<double>(ticks);
    int64_t microseconds = static_cast<int64_t>(std::ceil(1000000.0 * seconds));
    return microseconds;
}

int64_t Timer::GetMilliseconds() const
{
    int64_t ticks = GetTicks();
    double seconds = mInvFrequency * static_cast<double>(ticks);
    int64_t milliseconds = static_cast<int64_t>(std::ceil(1000.0 * seconds));
    return milliseconds;
}

double Timer::GetSeconds() const
{
    int64_t ticks = GetTicks();
    double seconds = mInvFrequency * static_cast<double>(ticks);
    return seconds;
}

void Timer::Reset()
{
    LARGE_INTEGER counter = { 0 };
    QueryPerformanceCounter(&counter);
    mInitialTicks = static_cast<int64_t>(counter.QuadPart);
}

int64_t Timer::GetTicks() const
{
    LARGE_INTEGER counter = { 0 };
    QueryPerformanceCounter(&counter);
    return static_cast<int64_t>(counter.QuadPart) - mInitialTicks;
}

#else

Timer::Timer()
{
    Reset();
}

int64_t Timer::GetNanoseconds() const
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        currentTime - mInitialTime).count();
}

int64_t Timer::GetMicroseconds() const
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        currentTime - mInitialTime).count();
}

int64_t Timer::GetMilliseconds() const
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - mInitialTime).count();
}

double Timer::GetSeconds() const
{
    int64_t msecs = GetMilliseconds();
    return static_cast<double>(msecs) / 1000.0;
}

void Timer::Reset()
{
    mInitialTime = std::chrono::high_resolution_clock::now();
}

#endif
