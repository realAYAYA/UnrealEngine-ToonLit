// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
HoloLensTime.cpp: HoloLens implementations of time functions
=============================================================================*/

//#include "CorePrivatePCH.h"
#include "HoloLensPlatformTime.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Ticker.h"
#include "Microsoft/WindowsHWrapper.h"
#include "Stats/Stats.h"

float FHoloLensTime::CPUTimePctRelative = 0.0f;

double FHoloLensTime::InitTiming()
{
	LARGE_INTEGER Frequency;
	verify(QueryPerformanceFrequency(&Frequency));
	SecondsPerCycle = 1.0 / Frequency.QuadPart;
	SecondsPerCycle64 = 1.0 / Frequency.QuadPart;

	// Due to some limitation of the OS, we limit the polling frequency to 4 times per second, 
	// but it should be enough for longterm CPU usage monitoring.
	static const float PollingInterval = 1.0f / 4.0f;

	// Register a ticker delegate for updating the CPU utilization data.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FPlatformTime::UpdateCPUTime), PollingInterval);

	return FPlatformTime::Seconds();
}

void FHoloLensTime::SystemTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME st;
	::GetLocalTime(&st);

	Year = st.wYear;
	Month = st.wMonth;
	DayOfWeek = st.wDayOfWeek;
	Day = st.wDay;
	Hour = st.wHour;
	Min = st.wMinute;
	Sec = st.wSecond;
	MSec = st.wMilliseconds;
}

void FHoloLensTime::UtcTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME st;
	GetSystemTime(&st);

	Year = st.wYear;
	Month = st.wMonth;
	DayOfWeek = st.wDayOfWeek;
	Day = st.wDay;
	Hour = st.wHour;
	Min = st.wMinute;
	Sec = st.wSecond;
	MSec = st.wMilliseconds;
}

/** Holds Windows filetime misc functions. */
struct FFiletimeMisc
{
	/**
	 * @return number of ticks based on the specified Filetime.
	 */
	static FORCEINLINE uint64 TicksFromFileTime(const FILETIME& Filetime)
	{
		const uint64 NumTicks = (uint64(Filetime.dwHighDateTime) << 32) + Filetime.dwLowDateTime;
		return NumTicks;
	}

	/**
	 * @return number of seconds based on the specified Filetime.
	 */
	static FORCEINLINE double ToSeconds(const FILETIME& Filetime)
	{
		return double(TicksFromFileTime(Filetime)) / double(ETimespan::TicksPerSecond);
	}
};


bool FHoloLensTime::UpdateCPUTime(float /*DeltaTime*/)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHoloLensPlatformTime_UpdateCPUTime);

	static double LastTotalProcessTime = 0.0f;
	static double LastTotalUserAndKernelTime = 0.0f;

	FILETIME CreationTime = { 0 };
	FILETIME ExitTime = { 0 };
	FILETIME KernelTime = { 0 };
	FILETIME UserTime = { 0 };
	FILETIME CurrentTime = { 0 };

	::GetProcessTimes(::GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);
	::GetSystemTimeAsFileTime(&CurrentTime);

	const double CurrentTotalUserAndKernelTime = FFiletimeMisc::ToSeconds(KernelTime) + FFiletimeMisc::ToSeconds(UserTime);
	const double CurrentTotalProcessTime = FFiletimeMisc::ToSeconds(CurrentTime) - FFiletimeMisc::ToSeconds(CreationTime);

	const double IntervalProcessTime = CurrentTotalProcessTime - LastTotalProcessTime;
	const double IntervalUserAndKernelTime = CurrentTotalUserAndKernelTime - LastTotalUserAndKernelTime;

	// IntervalUserAndKernelTime == 0.0f means that the OS hasn't updated the data yet, 
	// so don't update to avoid oscillating between 0 and calculated value.
	if (IntervalUserAndKernelTime > 0.0f)
	{
		CPUTimePctRelative = IntervalUserAndKernelTime / IntervalProcessTime * 100.0f;

		LastTotalProcessTime = CurrentTotalProcessTime;
		LastTotalUserAndKernelTime = CurrentTotalUserAndKernelTime;
	}

	return true;
}


FCPUTime FHoloLensTime::GetCPUTime()
{
	return FCPUTime(CPUTimePctRelative / (float)FPlatformMisc::NumberOfCoresIncludingHyperthreads(), CPUTimePctRelative);
}
