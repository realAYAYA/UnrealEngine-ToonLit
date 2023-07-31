// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnixPlatfomTime.cpp Unix implementations of time functions
=============================================================================*/

#include "Unix/UnixPlatformTime.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include <sys/resource.h>

int FUnixTime::ClockSource = -1;
char FUnixTime::CalibrationLog[4096] = {0};

namespace FUnixTimeInternal
{
	constexpr double TimeValToMicroSec(timeval & tv)
	{
		return static_cast<double>(tv.tv_sec) * 1e6 + static_cast<double>(tv.tv_usec);
	}

	constexpr uint64 TimeSpecToNanoSec(timespec &ts)
	{
		return static_cast<uint64>(static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec));
	}

	constexpr double MicroSecondsToSeconds(double MicroSec)
	{
		return MicroSec / 1e6;
	}

	// last time we checked the timer
	static double PreviousUpdateTimeNanoSec = 0.0;

	// last user + system time
	static double PreviousSystemAndUserProcessTimeMicroSec = 0.0;

	// last CPU utilization
	static float CurrentCpuUtilization = 0.0f;
	// last CPU utilization (per core)
	static float CurrentCpuUtilizationNormalized = 0.0f;

	struct FThreadCPUStats
	{
		/** Per-Thread CPU Utilization */
		float ThreadCPUUtilization = 0.f;

		/** Per-Thread CPU Utilization (per Core) */
		float ThreadCPUUtilizationNormalized = 0.f;

		/** The per-thread CPU processing time (kernel + user) from the last update */
		uint64 LastIntervalThreadTimeNS = 0;
	};

	/** Per-Thread CPU Stats */
	thread_local FThreadCPUStats CurrentThreadCPUStats = {};


	/** Process lifetime Soft Page Fault count */
	static uint64 SoftPageFaultCount = 0;

	/** Process lifetime Hard Page Fault count */
	static uint64 HardPageFaultCount = 0;

	/** Process lifetime Blocking Input count */
	static uint64 BlockingInputCount = 0;

	/** Process lifetime Blocking Output count */
	static uint64 BlockingOutputCount = 0;

	/** Process lifetime Voluntary Context Switch count */
	static uint64 VoluntaryContextSwitchCount = 0;

	/** Process lifetime Involuntary Context Switch count */
	static uint64 InvoluntaryContextSwitchCount = 0;
}

double FUnixTime::InitTiming()
{
	if (ClockSource == -1)
	{
		// Only ever set this ClockSource once
		ClockSource = FUnixTime::CalibrateAndSelectClock();
	}
	SecondsPerCycle = 1e-6;
	SecondsPerCycle64 = 1e-7;

	return FPlatformTime::Seconds();
}

FCPUTime FUnixTime::GetCPUTime()
{
	// 250ms minimum delay between checks to minimize overhead (and also match Windows version)
	constexpr double MinDelayBetweenChecksNanoSec = 250 * 1e6;
	
	if (UNLIKELY(ClockSource < 0))
	{
		ClockSource = FUnixTime::CalibrateAndSelectClock();
	}

	struct timespec ts;
	if (0 == clock_gettime(ClockSource, &ts))
	{
		const double CurrentTimeNanoSec = static_cast<double>(FUnixTimeInternal::TimeSpecToNanoSec(ts));

		// see if we need to update the values
		double TimeSinceLastUpdateNanoSec = CurrentTimeNanoSec - FUnixTimeInternal::PreviousUpdateTimeNanoSec;
		if (TimeSinceLastUpdateNanoSec >= MinDelayBetweenChecksNanoSec)
		{
			const float DeltaTimeInMs = MinDelayBetweenChecksNanoSec / 1e6;
			UpdateCPUTime(DeltaTimeInMs);
			FUnixTimeInternal::PreviousUpdateTimeNanoSec = CurrentTimeNanoSec;
		}
	}

	return FCPUTime(FUnixTimeInternal::CurrentCpuUtilizationNormalized, FUnixTimeInternal::CurrentCpuUtilization);
}

FCPUTime FUnixTime::GetThreadCPUTime()
{
	return FCPUTime(FUnixTimeInternal::CurrentThreadCPUStats.ThreadCPUUtilizationNormalized,
					FUnixTimeInternal::CurrentThreadCPUStats.ThreadCPUUtilization);
}

bool FUnixTime::UpdateCPUTime(float InDeltaTimeInMs)
{
	rusage Usage;

	double DeltaTimeInMs = InDeltaTimeInMs;

	if (getrusage(RUSAGE_SELF, &Usage) == 0)
	{
		// Get delta between last two calls if the passed DeltaTime is zero
		if (DeltaTimeInMs <= 0.0)
		{
			timespec ts;

			if (clock_gettime(ClockSource, &ts) == 0)
			{
				const double CurrentTimeNanoSec = static_cast<double>(FUnixTimeInternal::TimeSpecToNanoSec(ts));

				DeltaTimeInMs = (CurrentTimeNanoSec - FUnixTimeInternal::PreviousUpdateTimeNanoSec) / 1e6;
				FUnixTimeInternal::PreviousUpdateTimeNanoSec = CurrentTimeNanoSec;
			}
		}

		const double DeltaTimeInMicroSec = DeltaTimeInMs * 1e3;
		const double CurrentSystemAndUserProcessTimeMicroSec = FUnixTimeInternal::TimeValToMicroSec(Usage.ru_utime) + FUnixTimeInternal::TimeValToMicroSec(Usage.ru_stime); // holds all usages on all cores
		const double CpuTimeDuringPeriodMicroSec = CurrentSystemAndUserProcessTimeMicroSec - FUnixTimeInternal::PreviousSystemAndUserProcessTimeMicroSec;

		double CurrentCpuUtilizationHighPrec = (CpuTimeDuringPeriodMicroSec / DeltaTimeInMicroSec) * 100.0;

		// recalculate the values
		FUnixTimeInternal::CurrentCpuUtilizationNormalized = static_cast<float>(CurrentCpuUtilizationHighPrec / static_cast<double>(FPlatformMisc::NumberOfCoresIncludingHyperthreads()));
		FUnixTimeInternal::CurrentCpuUtilization = static_cast<float>(CurrentCpuUtilizationHighPrec);

		// update previous
		FUnixTimeInternal::PreviousSystemAndUserProcessTimeMicroSec = CurrentSystemAndUserProcessTimeMicroSec;
		
		LastIntervalCPUTimeInSeconds = FUnixTimeInternal::MicroSecondsToSeconds(CpuTimeDuringPeriodMicroSec);


		// Free performance stats
		FUnixTimeInternal::SoftPageFaultCount = Usage.ru_minflt;
		FUnixTimeInternal::HardPageFaultCount = Usage.ru_majflt;
		FUnixTimeInternal::BlockingInputCount = Usage.ru_inblock;
		FUnixTimeInternal::BlockingOutputCount = Usage.ru_oublock;
		FUnixTimeInternal::VoluntaryContextSwitchCount = Usage.ru_nvcsw;
		FUnixTimeInternal::InvoluntaryContextSwitchCount = Usage.ru_nivcsw;
	}

	return true;
}

bool FUnixTime::UpdateThreadCPUTime(float/*= 0.0*/)
{
	bool bReturnVal = false;

#ifdef PLATFORM_HAS_BSD_THREAD_CPUTIME
	timespec SystemTime;
	timespec ThreadTime;

	if (clock_gettime(ClockSource, &SystemTime) == 0 && clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ThreadTime) == 0)
	{
		struct FThreadCPUTime
		{
			uint64 LastThreadCPUTimeUpdateNS = 0;
			uint64 LastThreadTimeNS = 0;
		};

		thread_local FThreadCPUTime ThreadTimeInfo = {};

		const uint64 SystemTimeNS = FUnixTimeInternal::TimeSpecToNanoSec(SystemTime);
		const uint64 ThreadTimeNS = FUnixTimeInternal::TimeSpecToNanoSec(ThreadTime);
		const uint64 DeltaTimeNS = SystemTimeNS - ThreadTimeInfo.LastThreadCPUTimeUpdateNS;

		ThreadTimeInfo.LastThreadCPUTimeUpdateNS = SystemTimeNS;

		const uint64 ElapsedThreadCPUTimeNS = ThreadTimeNS - ThreadTimeInfo.LastThreadTimeNS;
		const double ThreadCPUUtilizationHighPrec = (static_cast<double>(ElapsedThreadCPUTimeNS) / static_cast<double>(DeltaTimeNS)) * 100.0;

		FUnixTimeInternal::CurrentThreadCPUStats.ThreadCPUUtilization = static_cast<float>(ThreadCPUUtilizationHighPrec);
		FUnixTimeInternal::CurrentThreadCPUStats.ThreadCPUUtilizationNormalized =
																static_cast<float>(ThreadCPUUtilizationHighPrec / FPlatformMisc::NumberOfCoresIncludingHyperthreads());

		ThreadTimeInfo.LastThreadTimeNS = ThreadTimeNS;
		FUnixTimeInternal::CurrentThreadCPUStats.LastIntervalThreadTimeNS = ElapsedThreadCPUTimeNS;

		bReturnVal = true;
	}
#endif

	return bReturnVal;
}

void FUnixTime::AutoUpdateGameThreadCPUTime(double UpdateInterval)
{
	static bool bEnabledGameThreadTiming = false;

	if (!bEnabledGameThreadTiming)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FPlatformTime::UpdateThreadCPUTime), (float)UpdateInterval);

		bEnabledGameThreadTiming = true;
	}
}

double FUnixTime::GetLastIntervalThreadCPUTimeInSeconds()
{
	return static_cast<double>(FUnixTimeInternal::CurrentThreadCPUStats.LastIntervalThreadTimeNS) / 1e9;
}

uint64 FUnixTime::CallsPerSecondBenchmark(clockid_t BenchClockId, const char * BenchClockIdName)
{
	char Buffer[256];
	const uint64 kBenchmarkPeriodMicroSec = 1000000000ULL / 10;	// 0.1s

	// clock_getres() can fail when running on Windows Subsystem for Linux (but the clock can still be supported).
	struct timespec ts;
	if (clock_gettime(BenchClockId, &ts) == -1)
	{
		FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), "Clock_id %d (%s) is not supported on this system, clock_gettime() fails.\n", BenchClockId, BenchClockIdName);
		FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));
		return 0;	// unsupported clock id either
	}

	// from now on we'll assume that clock_gettime cannot fail
	uint64 StartTimestamp = FUnixTimeInternal::TimeSpecToNanoSec(ts);
	uint64 EndTimeStamp = StartTimestamp;

	uint64 NumCalls = 1;	// account for starting timestamp
	uint64 NumZeroDeltas = 0;
	const uint64 kHardLimitOnZeroDeltas = (1 << 26);	// arbitrary, but high enough so we don't hit it on fast coarse clocks
	do
	{
		clock_gettime(BenchClockId, &ts);

		uint64 NewEndTimeStamp = FUnixTimeInternal::TimeSpecToNanoSec(ts);
		++NumCalls;

		if (NewEndTimeStamp < EndTimeStamp)
		{
			FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), "Clock_id %d (%s) is unusable, can go backwards.\n", BenchClockId, BenchClockIdName);
			FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));
			return 0;
		}
		else if (NewEndTimeStamp == EndTimeStamp)
		{
			++NumZeroDeltas;

			// do not lock up if the clock is broken (e.g. stays in place)
			if (NumZeroDeltas > kHardLimitOnZeroDeltas)
			{
				FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), "Clock_id %d (%s) is unusable, too many (%llu) zero deltas.\n", BenchClockId, BenchClockIdName, NumZeroDeltas);
				FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));
				return 0;
			}
		}

		EndTimeStamp = NewEndTimeStamp;
	}
	while (EndTimeStamp - StartTimestamp < kBenchmarkPeriodMicroSec);

	double TimesPerSecond = 1e9 / static_cast<double>(EndTimeStamp - StartTimestamp);

	uint64 RealNumCalls = static_cast<uint64> (TimesPerSecond * static_cast<double>(NumCalls));

	char ZeroDeltasBuf[128];
	if (NumZeroDeltas)
	{
		FCStringAnsi::Snprintf(ZeroDeltasBuf, sizeof(ZeroDeltasBuf), "with %f%% zero deltas", 100.0 * static_cast<double>(NumZeroDeltas) / static_cast<double>(NumCalls));
	}

	FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), (" - %s (id=%d) can sustain %llu (%lluK, %lluM) calls per second %s.\n"), BenchClockIdName, BenchClockId,
		RealNumCalls, (RealNumCalls + 500) / 1000, (RealNumCalls + 500000) / 1000000,
		NumZeroDeltas ? ZeroDeltasBuf : "without zero deltas");
	FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));

	// if clock had zero deltas, we don't want it
	if (NumZeroDeltas)
	{
		return 0;
	}

	return RealNumCalls;
}

int FUnixTime::CalibrateAndSelectClock()
{
	// do not calibrate in case of programs, so e.g. ShaderCompileWorker speed is not impacted
	if (IS_PROGRAM)
	{
		struct timespec ts;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		{
			FCStringAnsi::Snprintf(CalibrationLog, sizeof(CalibrationLog),
				"Skipped benchmarking clocks because the engine is running in a standalone program mode: CLOCK_MONOTONIC is unavailable, CLOCK_REALTIME will be used.\n");
			return CLOCK_REALTIME;
		}
		else
		{
			FCStringAnsi::Snprintf(CalibrationLog, sizeof(CalibrationLog),
				"Skipped benchmarking clocks because the engine is running in a standalone program mode - CLOCK_MONOTONIC will be used.\n");
			return CLOCK_MONOTONIC;
		}
	}
	else
	{
		char Buffer[256];

		// init calibration log
		FCStringAnsi::Snprintf(CalibrationLog, sizeof(CalibrationLog), "Benchmarking clocks:\n");

		struct ClockDesc
		{
			int Id;
			const char *Desc;
			uint64 Rate;
		}
		Clocks[] =
		{
			{ CLOCK_REALTIME, "CLOCK_REALTIME", 0 },
			{ CLOCK_MONOTONIC, "CLOCK_MONOTONIC", 0 },
			{ CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW", 0 },
			{ CLOCK_MONOTONIC_COARSE, "CLOCK_MONOTONIC_COARSE", 0 }
		};

		int ChosenClock = 0;	
		// Skip measuring CLOCK_REALTIME, so it's never picked up if any other is available.
		// CLOCK_REALTIME should be always supported - as a last resort.
		for (int Idx = 1; Idx < UE_ARRAY_COUNT(Clocks); ++Idx)
		{
			Clocks[Idx].Rate = CallsPerSecondBenchmark(Clocks[Idx].Id, Clocks[Idx].Desc);
			if (Clocks[Idx].Rate > Clocks[ChosenClock].Rate)
			{
				ChosenClock = Idx;
			}
		}

		FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), "Selected clock_id %d (%s) since it is the fastest support clock without zero deltas.\n",
			Clocks[ChosenClock].Id, Clocks[ChosenClock].Desc);
		FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));

		// Warn if our current clock source cannot be called at least 1M times a second (<30k a frame) as this may affect tight loops
		if (ChosenClock != 0 && Clocks[ChosenClock].Rate < 1000000)
		{
			FCStringAnsi::Snprintf(Buffer, sizeof(Buffer), "The clock source is too slow on this machine, performance may be affected.\n");
			FCStringAnsi::Strncat(CalibrationLog, Buffer, sizeof(CalibrationLog));
		}

		return Clocks[ChosenClock].Id;
	}
}

void FUnixTime::PrintCalibrationLog()
{
	// clock selection happens too early to be printed to log, print it now
	FString Buffer(ANSI_TO_TCHAR(CalibrationLog));

	TArray<FString> Lines;
	Buffer.ParseIntoArrayLines(Lines);

	for(const FString& Line : Lines)
	{
		UE_LOG(LogCore, Log, TEXT("%s"), *Line);
	}
}

bool FUnixPlatformMisc::GetPageFaultStats(FPageFaultStats& OutStats, EPageFaultFlags Flags/*=EPageFaultFlags::All*/)
{
	// Ignore flags since all stats are free
	OutStats.SoftPageFaults = FUnixTimeInternal::SoftPageFaultCount;
	OutStats.HardPageFaults = FUnixTimeInternal::HardPageFaultCount;
	OutStats.TotalPageFaults = FUnixTimeInternal::SoftPageFaultCount + FUnixTimeInternal::HardPageFaultCount;

	return true;
}

bool FUnixPlatformMisc::GetBlockingIOStats(FProcessIOStats& OutStats, EInputOutputFlags Flags/*=EInputOutputFlags::All*/)
{
	bool bSuccess = false;

	if (EnumHasAnyFlags(Flags, EInputOutputFlags::BlockingInput | EInputOutputFlags::BlockingOutput))
	{
		OutStats.BlockingInput = FUnixTimeInternal::BlockingInputCount;
		OutStats.BlockingOutput = FUnixTimeInternal::BlockingOutputCount;

		bSuccess = true;
	}

	return bSuccess;
}

bool FUnixPlatformMisc::GetContextSwitchStats(FContextSwitchStats& OutStats, EContextSwitchFlags Flags/*=EContextSwitchFlags::All*/)
{
	// Ignore flags since all stats are free
	OutStats.VoluntaryContextSwitches = FUnixTimeInternal::VoluntaryContextSwitchCount;
	OutStats.InvoluntaryContextSwitches = FUnixTimeInternal::InvoluntaryContextSwitchCount;
	OutStats.TotalContextSwitches = FUnixTimeInternal::VoluntaryContextSwitchCount + FUnixTimeInternal::InvoluntaryContextSwitchCount;

	return true;
}

