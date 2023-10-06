// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UnixPlatformTime.h: Unix platform Time functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformTime.h"

#include <time.h> // IWYU pragma: export

/**
 * Unix implementation of the Time OS functions
 */
struct FUnixTime : public FGenericPlatformTime
{
	static CORE_API double InitTiming();

	static FORCEINLINE double Seconds()
	{
		if (UNLIKELY(ClockSource < 0))
		{
			ClockSource = CalibrateAndSelectClock();
		}

		struct timespec ts;
		clock_gettime(ClockSource, &ts);
		return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
	}

	static FORCEINLINE uint32 Cycles()
	{
		if (UNLIKELY(ClockSource < 0))
		{
			ClockSource = CalibrateAndSelectClock();
		}

		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return static_cast<uint32>(static_cast<uint64>(ts.tv_sec) * (uint64)1e6 + static_cast<uint64>(ts.tv_nsec) / 1000ULL);
	}

	static FORCEINLINE uint64 Cycles64()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return static_cast<uint64>(static_cast<uint64>(ts.tv_sec) * (uint64)1e7 + static_cast<uint64>(ts.tv_nsec) / 100ULL);
	}

	static CORE_API bool UpdateCPUTime(float DeltaSeconds);
	static CORE_API bool UpdateThreadCPUTime(float = 0.0);
	static CORE_API void AutoUpdateGameThreadCPUTime(double UpdateInterval);

	static CORE_API FCPUTime GetCPUTime();
	static CORE_API FCPUTime GetThreadCPUTime();
	static CORE_API double GetLastIntervalThreadCPUTimeInSeconds();

	/**
	 * Calibration log to be printed at later time
	 */
	static CORE_API void PrintCalibrationLog();

private:

	/** Clock source to use */
	static CORE_API int ClockSource;

	/** Log information about calibrating the clock. */
	static CORE_API char CalibrationLog[4096];

	/**
	 * Benchmarks clock_gettime(), possibly switches to something else is too slow.
	 * Unix-specific.
	 */
	static CORE_API int CalibrateAndSelectClock();

	/**
	 * Returns number of time we can call the clock per second.
	 */
	static CORE_API uint64 CallsPerSecondBenchmark(clockid_t BenchClockId, const char * BenchClockIdName);
};

typedef FUnixTime FPlatformTime;
