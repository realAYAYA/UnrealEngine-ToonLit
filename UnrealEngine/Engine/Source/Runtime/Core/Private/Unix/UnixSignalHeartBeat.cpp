// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixSignalHeartBeat.h"
#include "Unix/UnixPlatformRealTimeSignals.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformTime.h"

#include <signal.h>
#include <unistd.h>

DEFINE_LOG_CATEGORY_STATIC(LogUnixHeartBeat, Log, All);

FUnixSignalGameHitchHeartBeat* FUnixSignalGameHitchHeartBeat::Singleton = nullptr;

FUnixSignalGameHitchHeartBeat& FUnixSignalGameHitchHeartBeat::Get()
{
	struct FInitHelper
	{
		FUnixSignalGameHitchHeartBeat* Instance;

		FInitHelper()
		{
			check(!Singleton);
			Instance = new FUnixSignalGameHitchHeartBeat;
			Singleton = Instance;
		}

		~FInitHelper()
		{
			Singleton = nullptr;

			delete Instance;
			Instance = nullptr;
		}
	};

	// Use a function static helper to ensure creation
	// of the FUnixSignalGameHitchHeartBeat instance is thread safe.
	static FInitHelper Helper;
	return *Helper.Instance;
}

FUnixSignalGameHitchHeartBeat* FUnixSignalGameHitchHeartBeat::GetNoInit()
{
	return Singleton;
}

namespace
{
	// 1ms for the lowest amount allowed for hitch detection. Anything less we wont try to detect hitches
	double MinimalHitchThreashold = 0.001;

	void SignalHitchHandler(int Signal)
	{
#if USE_HITCH_DETECTION
		GHitchDetected = true;
#endif
	}
}

FUnixSignalGameHitchHeartBeat::FUnixSignalGameHitchHeartBeat()
{
	Init();
}

FUnixSignalGameHitchHeartBeat::~FUnixSignalGameHitchHeartBeat()
{
	if (TimerId)
	{
		timer_delete(TimerId);
		TimerId = nullptr;
	}
}

void FUnixSignalGameHitchHeartBeat::Init()
{
#if USE_HITCH_DETECTION
	// Setup the signal callback for when we hit a hitch
	struct sigaction SigAction;
	FMemory::Memzero(SigAction);
	SigAction.sa_flags = SA_SIGINFO;
	SigAction.sa_handler = SignalHitchHandler;

	sigaction(HEART_BEAT_SIGNAL, &SigAction, nullptr);

	struct sigevent SignalEvent;
	FMemory::Memzero(SignalEvent);
	SignalEvent.sigev_notify = SIGEV_SIGNAL;
	SignalEvent.sigev_signo = HEART_BEAT_SIGNAL;

	if (timer_create(CLOCK_REALTIME, &SignalEvent, &TimerId) == -1)
	{
		int Errno = errno;
		UE_LOG(LogUnixHeartBeat, Warning, TEXT("Failed to timer_create() errno=%d (%s)"), Errno, UTF8_TO_TCHAR(strerror(Errno)));

		TimerId = nullptr;
	}

	float CmdLine_HitchDurationS = 0.0;
	bHasCmdLine = FParse::Value(FCommandLine::Get(), TEXT("hitchdetection="), CmdLine_HitchDurationS);

	if (bHasCmdLine)
	{
		HitchThresholdS = CmdLine_HitchDurationS;
	}

	SuspendCount = 0;

	InitSettings();
#endif
}

void FUnixSignalGameHitchHeartBeat::InitSettings()
{
	static bool bFirst = true;

	// Command line takes priority over config, so only check the ini if we didnt already set our selfs from the cmd line
	if (!bHasCmdLine)
	{
		float Config_HitchDurationS = 0.0;
		bool bReadFromConfig = GConfig->GetFloat(TEXT("Core.System"), TEXT("GameThreadHeartBeatHitchDuration"), Config_HitchDurationS, GEngineIni);

		if (bReadFromConfig)
		{
			HitchThresholdS = Config_HitchDurationS;
		}
	}

	if (bFirst)
	{
		bFirst = false;

		bool bStartSuspended = false;
		GConfig->GetBool(TEXT("Core.System"), TEXT("GameThreadHeartBeatStartSuspended"), bStartSuspended, GEngineIni);

		if (FParse::Param(FCommandLine::Get(), TEXT("hitchdetectionstartsuspended")))
		{
			bStartSuspended = true;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("hitchdetectionstartrunning")))
		{
			bStartSuspended = false;
		}

		if (bStartSuspended)
		{
			SuspendCount = 1;
		}
	}
}

void FUnixSignalGameHitchHeartBeat::FrameStart(bool bSkipThisFrame)
{
#if USE_HITCH_DETECTION
	check(IsInGameThread());

	if (!bDisabled && SuspendCount == 0 && TimerId)
	{
		if (HitchThresholdS > MinimalHitchThreashold)
		{
			struct itimerspec HeartBeatTime;
			FMemory::Memzero(HeartBeatTime);

			long FullSeconds = static_cast<long>(HitchThresholdS);
			long RemainderInNanoSeconds = static_cast<long>(FMath::Fmod(HitchThresholdS, 1.0) * 1000000000.0);
			HeartBeatTime.it_value.tv_sec = FullSeconds;
			HeartBeatTime.it_value.tv_nsec = RemainderInNanoSeconds;

			if (GHitchDetected)
			{
				UE_LOG(LogCore, Error, TEXT("Hitch detected on previous gamethread frame (%8.2fms since last frame)"), float(FPlatformTime::Seconds() - StartTime) * 1000.0f);
			}

			StartTime = FPlatformTime::Seconds();

			if (timer_settime(TimerId, 0, &HeartBeatTime, nullptr) == -1)
			{
				int Errno = errno;
				UE_LOG(LogUnixHeartBeat, Warning, TEXT("Failed to timer_settime() errno=%d (%s)"), Errno, UTF8_TO_TCHAR(strerror(Errno)));
			}
		}
	}

	GHitchDetected = false;
#endif
}

// If the process is suspended we will hit a hitch for how ever long it was suspended for
double FUnixSignalGameHitchHeartBeat::GetFrameStartTime()
{
	return StartTime;
}

double FUnixSignalGameHitchHeartBeat::GetCurrentTime()
{
	return FPlatformTime::Seconds();
}

void FUnixSignalGameHitchHeartBeat::SuspendHeartBeat()
{
#if USE_HITCH_DETECTION
	if (!IsInGameThread())
	{
		return;
	}

	SuspendCount++;

	if (TimerId)
	{
		struct itimerspec DisarmTime;
		FMemory::Memzero(DisarmTime);

		if (timer_settime(TimerId, 0, &DisarmTime, nullptr) == -1)
		{
			int Errno = errno;
			UE_LOG(LogUnixHeartBeat, Warning, TEXT("Failed to timer_settime() errno=%d (%s)"), Errno, UTF8_TO_TCHAR(strerror(Errno)));
		}
	}
#endif
}

void FUnixSignalGameHitchHeartBeat::ResumeHeartBeat()
{
#if USE_HITCH_DETECTION
	if (!IsInGameThread())
	{
		return;
	}

	if( SuspendCount > 0)
	{
		SuspendCount--;

		FrameStart(true);
	}
#endif
}

void FUnixSignalGameHitchHeartBeat::Restart()
{
	bDisabled = false;

	// If we still have a valid handle on the timer_t clean it up
	if (TimerId)
	{
		timer_delete(TimerId);
		TimerId = nullptr;
	}

	Init();
}

void FUnixSignalGameHitchHeartBeat::Stop()
{
	SuspendHeartBeat();
	bDisabled = true;
}
