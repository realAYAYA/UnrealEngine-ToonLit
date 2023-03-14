// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeStat.h"

#if PLATFORM_MAC
	#include <sys/time.h>
	#include <sys/times.h>
#endif

BEGIN_NAMESPACE_UE_AC

// Contructor (Get current process CPU time and real time)
void FTimeStat::ReStart()
{
	CpuTime = CpuTimeClock();
	RealTime = RealTimeClock();
}

// Cumulate time from the other
void FTimeStat::AddDiff(const FTimeStat& InOther)
{
	CpuTime += CpuTimeClock() - InOther.CpuTime;
	double RealSeconds = RealTimeClock() - InOther.RealTime;
	if (RealSeconds < 0) // Before and after midnight ?
	{
		RealSeconds += 24 * 60 * 60;
	}
	RealTime += RealSeconds;
}

// Print time differences
void FTimeStat::PrintDiff(const char* InStatLabel, const FTimeStat& InStart)
{
	double CpuSeconds = CpuTime - InStart.CpuTime;
	double RealSeconds = RealTime - InStart.RealTime;
	if (RealSeconds < 0) // Before and after midnight ?
	{
		RealSeconds += 24 * 60 * 60;
	}
	UE_AC_ReportF("Seconds for %s cpu=%.2lgs, real=%.2lgs\n", InStatLabel, CpuSeconds, RealSeconds);
}

// Tool get current real time clock
double FTimeStat::RealTimeClock()
{
#if PLATFORM_WINDOWS
	LARGE_INTEGER Time, Freq;
	if (QueryPerformanceFrequency(&Freq) && QueryPerformanceCounter(&Time))
	{
		return double(Time.QuadPart) / Freq.QuadPart;
	}
#else
	struct timeval Time;
	if (gettimeofday(&Time, NULL) == 0)
	{
		return Time.tv_sec + double(Time.tv_usec) * .000001;
	}
#endif
	return 0;
}

// Tool get process CPU real time clock
double FTimeStat::CpuTimeClock()
{
#if PLATFORM_WINDOWS
	FILETIME A, B, C, D;
	if (GetProcessTimes(GetCurrentProcess(), &A, &B, &C, &D) != 0)
	{
		return double(D.dwLowDateTime | ((unsigned long long)D.dwHighDateTime << 32)) * 0.0000001;
	}
	return 0;
#else
	static double spc = 1.0 / sysconf(_SC_CLK_TCK);
	struct tms	  AllTimes;
	times(&AllTimes);
	return (AllTimes.tms_utime + AllTimes.tms_stime + AllTimes.tms_cutime + AllTimes.tms_cstime) * spc;
#endif
}

END_NAMESPACE_UE_AC
