// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProfilingDebugging/StallDetector.h"

#if STALL_DETECTOR

#include "HAL/ExceptionHandling.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

// counters for sending information into trace system
TRACE_DECLARE_INT_COUNTER(StallCount, TEXT("StallDetector/Count"));
TRACE_DECLARE_FLOAT_COUNTER(StallTimeSeconds, TEXT("StallDetector/TimeSeconds"));

// force normal behavior in the face of debug configuration and debugger attached
#define STALL_DETECTOR_DEBUG 0

// use the heart beat clock to account for process suspend
#define STALL_DETECTOR_HEART_BEAT_CLOCK 1

#if STALL_DETECTOR_HEART_BEAT_CLOCK
 #include "HAL/ThreadHeartBeat.h"
#endif // STALL_DETECTOR_HEART_BEAT_CLOCK

DEFINE_LOG_CATEGORY(LogStall);

/**
* Globals
**/

// The reference count for the resources for this API
static int32 InitCount = 0;

// Sentinel value for invalid timestamp
static const double InvalidSeconds = -1.0;

/**
* Stall Detector Thread
**/

namespace UE
{
	class FStallDetectorRunnable : public FRunnable
	{
	public:
		FStallDetectorRunnable();

		// FRunnable implementation
		virtual uint32 Run() override;
		virtual void Stop() override
		{
			StopThread = true;
		}
		virtual void Exit() override
		{
			Stop();
		}

		bool GetStartedThread()
		{
			return StartedThread;
		}
		
#if STALL_DETECTOR_HEART_BEAT_CLOCK
		FThreadHeartBeatClock& GetClock()
		{
			return Clock;
		}
#endif

	private:
		bool StartedThread;
		bool StopThread;

#if STALL_DETECTOR_HEART_BEAT_CLOCK
		FThreadHeartBeatClock Clock;
#endif
	};

	static FStallDetectorRunnable* Runnable = nullptr;
	static FRunnableThread* Thread = nullptr;
}

UE::FStallDetectorRunnable::FStallDetectorRunnable()
	: StartedThread(false)
	, StopThread(false)
#if STALL_DETECTOR_HEART_BEAT_CLOCK
	, Clock(50.0/1000.0) // the clamped time interval that each tick of the clock can possibly advance
#endif
{
}

uint32 UE::FStallDetectorRunnable::Run()
{
	while (!StopThread)
	{
#if STALL_DETECTOR_HEART_BEAT_CLOCK
		Clock.Tick();
#endif

		// Clock has been ticked
		StartedThread = true;

		// Use this timestamp to try to avoid marginal triggering
		double Seconds = FStallDetector::Seconds();

		// Check the detectors
		if (Seconds != InvalidSeconds)
		{
			FScopeLock ScopeLock(&FStallDetector::GetInstancesSection());
			for (FStallDetector* Detector : FStallDetector::GetInstances())
			{
				Detector->Check(false, Seconds);
			}
		}

		// Sleep an interval, the resolution at which we want to detect an overage
		FPlatformProcess::SleepNoStats(0.005f);
	}

	return 0;
}

/**
* Stall Detector Stats
**/

FCriticalSection UE::FStallDetectorStats::InstancesSection;
TSet<UE::FStallDetectorStats*> UE::FStallDetectorStats::Instances;
FCountersTrace::TCounter<std::atomic<int64>, TraceCounterType_Int> UE::FStallDetectorStats::TotalTriggeredCount (TEXT("StallDetector/TotalTriggeredCount"), TraceCounterDisplayHint_None);
FCountersTrace::TCounter<std::atomic<int64>, TraceCounterType_Int> UE::FStallDetectorStats::TotalReportedCount (TEXT("StallDetector/TotalReportedCount"), TraceCounterDisplayHint_None);

UE::FStallDetectorStats::FStallDetectorStats(const TCHAR* InName, const double InBudgetSeconds, const EStallDetectorReportingMode InReportingMode)
	: Name(InName)
	, BudgetSeconds(InBudgetSeconds)
	, ReportingMode(InReportingMode)
	, bReported(false)
	, TriggerCount(
		(
			FCString::Strcat(TriggerCountCounterName, TEXT("StallDetector/")),
			FCString::Strcat(TriggerCountCounterName, InName),
			FCString::Strcat(TriggerCountCounterName, TEXT(" TriggerCount"))
		), TraceCounterDisplayHint_None)
	, OverageSeconds(
		(
			FCString::Strcat(OverageSecondsCounterName, TEXT("StallDetector/")),
			FCString::Strcat(OverageSecondsCounterName, InName),
			FCString::Strcat(OverageSecondsCounterName, TEXT(" OverageSeconds"))
		), TraceCounterDisplayHint_None)
{
	// Add at the end of construction
	FScopeLock ScopeLock(&InstancesSection);
	Instances.Add(this);
}

UE::FStallDetectorStats::~FStallDetectorStats()
{
	// Remove at the beginning of destruction
	FScopeLock ScopeLock(&InstancesSection);
	Instances.Remove(this);
}

void UE::FStallDetectorStats::OnStallCompleted(double InOverageSeconds)
{
	// we sync access around these for coherency reasons, can be polled from another thread (tabulation)
	FScopeLock StatsLock(&StatsSection);
	TriggerCount.Increment();
	OverageSeconds.Add(InOverageSeconds);
}

void UE::FStallDetectorStats::TabulateStats(TArray<TabulatedResult>& TabulatedResults)
{
	TabulatedResults.Empty();

	struct SortableStallStats
	{
		explicit SortableStallStats(const UE::FStallDetectorStats* InStats)
			: StallStats(InStats)
			, OverageRatio(0.0)
		{
			FScopeLock Lock(&InStats->StatsSection);
			if (InStats->TriggerCount.Get() && InStats->BudgetSeconds > 0.0)
			{
				OverageRatio = (InStats->OverageSeconds.Get() / (double)InStats->TriggerCount.Get()) / InStats->BudgetSeconds;
			}
		}

		bool operator<(const SortableStallStats& InRhs) const
		{
			// NOTE THIS IS REVERSED TO PUT THE MOST OVERAGE AT THE FRONT
			return OverageRatio > InRhs.OverageRatio;
		}

		const UE::FStallDetectorStats* StallStats;
		double OverageRatio;
	};

	TArray<SortableStallStats> StatsArray;
	FScopeLock InstancesLock(&UE::FStallDetectorStats::GetInstancesSection());
	for (const UE::FStallDetectorStats* StallStats : UE::FStallDetectorStats::GetInstances())
	{
		if (StallStats->TriggerCount.Get() && StallStats->ReportingMode != UE::EStallDetectorReportingMode::Disabled)
		{
			StatsArray.Emplace(StallStats);
		}
	}

	if (!StatsArray.IsEmpty())
	{
		StatsArray.Sort();

		for (const SortableStallStats& Stat : StatsArray)
		{
			TabulatedResults.Emplace(TabulatedResult());
			TabulatedResult& Result(TabulatedResults.Last());
			Result.Stats = Stat.StallStats;

			// we sync access around these for coherency reasons, can be polled from another thread (detector or scope)
			FScopeLock Lock(&Result.Stats->StatsSection);
			Result.TriggerCount = Result.Stats->TriggerCount.Get();
			Result.OverageSeconds = Result.Stats->OverageSeconds.Get();
		}
	}
}


/**
* Stall Timer
**/

UE::FStallTimer::FStallTimer()
: PauseCount(0)
, LastCheckSeconds(InvalidSeconds)
, RemainingSeconds(InvalidSeconds)
{
}

void UE::FStallTimer::Reset(const double InSeconds, const double InRemainingSeconds)
{
	FScopeLock ScopeLock(&Section);

	LastCheckSeconds = InSeconds;
	RemainingSeconds = InRemainingSeconds;
}

void UE::FStallTimer::Check(const double InSeconds, double& OutDeltaSeconds, double& OutOverageSeconds)
{
	OutDeltaSeconds = 0.0;
	OutOverageSeconds = 0.0;

	FScopeLock ScopeLock(&Section);

	// Check to see if we are paused, no need to check in this case
	if (LastCheckSeconds == InvalidSeconds)
	{
		return;
	}

	// The amount of time that has transpired since the last check
	OutDeltaSeconds = InSeconds - LastCheckSeconds;

	// Update the last check time
	LastCheckSeconds = InSeconds;

	// Deduct our delta from the remaining time
	RemainingSeconds -= OutDeltaSeconds;

	// The following calls use overage vs. negative remaining time
	OutOverageSeconds = -RemainingSeconds;
}

void UE::FStallTimer::Pause(const double InSeconds)
{
	FScopeLock ScopeLock(&Section);

	if (++PauseCount == 1 && ensure(LastCheckSeconds != InvalidSeconds))
	{
		// Keep remaining time up to date with now
		RemainingSeconds -= InSeconds - LastCheckSeconds;

		// Forget when we start this interval, we don't need synchronization here as if Check() updates LastCheckSeconds between the above and here, it doesn't matter as RemainingSeconds will still be correct
		LastCheckSeconds = InvalidSeconds;
	}
}

void UE::FStallTimer::Resume(const double InSeconds)
{
	FScopeLock ScopeLock(&Section);

	if (--PauseCount == 0)
	{
		// We should be paused!
		ensure(LastCheckSeconds == InvalidSeconds);

		// Resume checking from this time, we don't need atomics here as this thread is the only one that should issue this write (we were paused which means the StallDetectorThread won't Check())
		LastCheckSeconds = InSeconds;
	}
}


/**
* Stall Detector
**/

// statics
FCriticalSection UE::FStallDetector::InstancesSection;
TSet<UE::FStallDetector*> UE::FStallDetector::Instances;

// globals
class ThreadLocalSlotAcquire
{
public:
	ThreadLocalSlotAcquire()
	{
		Slot = FPlatformTLS::AllocTlsSlot();
		FPlatformTLS::SetTlsValue(Slot, nullptr);
	}

	~ThreadLocalSlotAcquire()
	{
		FPlatformTLS::FreeTlsSlot(Slot);
		Slot = ~0;
	}

	uint32 GetSlot()
	{
		return Slot;
	}

private:
	uint32 Slot = ~0;
};
static ThreadLocalSlotAcquire ThreadLocalSlot;

UE::FStallDetector::FStallDetector(FStallDetectorStats& InStats)
	: Stats(InStats)
	, Parent(nullptr)
	, ThreadId(0)
	, bStarted (false)
	, bPersistent(false)
	, Triggered(false)
{
	// Capture this thread's current stall detector to our parent member
	Parent = static_cast<FStallDetector*>(FPlatformTLS::GetTlsValue(ThreadLocalSlot.GetSlot()));

	// Set the current thread's stall detector to this object
	FPlatformTLS::SetTlsValue(ThreadLocalSlot.GetSlot(), this);

	// This will be invalid if the stall detector thread isn't running
	double Seconds = FStallDetector::Seconds();
	if (Seconds != InvalidSeconds)
	{
		// Capture the calling thread's id, can't do it later because we could need it from the stall detector thread
		ThreadId = FPlatformTLS::GetCurrentThreadId();

		// Setting this will cause our dtor to do a check (unless we are put into persistent mode by CheckAndReset())
		Timer.Reset(Seconds, InStats.BudgetSeconds);

		// Flag this detector to be checked at destruction time
		bStarted = true;
	}

	// Add at the end of construction
	FScopeLock ScopeLock(&InstancesSection);
	Instances.Add(this);
}

UE::FStallDetector::~FStallDetector()
{
	// Remove at the beginning of destruction
	{
		FScopeLock ScopeLock(&InstancesSection);
		Instances.Remove(this);
	}

	// This will be invalid if the stall detector thread isn't running
	double Seconds = FStallDetector::Seconds();

	// If we have a timestamp, and captured a timestamp at contruction time, and we are not a persistent detector, do the final check
	if (Seconds != InvalidSeconds && bStarted && !bPersistent)
	{
		Check(true, Seconds);
	}

	// We are destructed, so set current thread's detector to our parent
	FPlatformTLS::SetTlsValue(ThreadLocalSlot.GetSlot(), Parent);
}

void UE::FStallDetector::Check(bool bFinalCheck, double InCheckSeconds)
{
	// all callers need to sort out checking the clock
	check(InCheckSeconds != InvalidSeconds);

	// Both the potentially stalling thread and the stall detector thread will call into here.
	// FStallTimer will take a section and calculate the time remaining and update the last checked time stamp.
	// We don't hammer this too too badly (5ms at the time of this writing), so the lock shouldn't be too costly.
	// There shouldn't be enough contention to warrant trying to implement these semantics via atomics.
	// The critical section fastpath should be in effect almost all the time.
	double DeltaSeconds;
	double OverageSeconds;
	Timer.Check(InCheckSeconds, DeltaSeconds, OverageSeconds);

	if (Triggered)
	{
		if (bFinalCheck)
		{
			// this callback allows for tracking the total overrun, once the stalling period is finally done
			Stats.OnStallCompleted(OverageSeconds);

#if STALL_DETECTOR_DEBUG
			FString OverageString = FString::Printf(TEXT("[FStallDetector] [%s] Overage of %f\n"), Stats.Name, OverageSeconds);
			FPlatformMisc::LocalPrint(OverageString.GetCharArray().GetData());
#endif
			if (Stats.ReportingMode != EStallDetectorReportingMode::Disabled)
			{
				UE_LOG(LogStall, Log, TEXT("Stall detector '%s' complete in %fs (%fs overbudget)"), Stats.Name, DeltaSeconds, OverageSeconds);
			}
		}
	}
	else
	{
		if (OverageSeconds > 0.0)
		{
			// can be called from multiple threads, but we only want the first caller to call OnStallDetected
			bool PreviousTriggered = false;
			if (Triggered.compare_exchange_strong(PreviousTriggered, true, std::memory_order_acquire, std::memory_order_relaxed))
			{
#if STALL_DETECTOR_DEBUG
				FString OverageString = FString::Printf(TEXT("[FStallDetector] [%s] Triggered at %f\n"), Stats.Name, InCheckSeconds);
				FPlatformMisc::LocalPrint(OverageString.GetCharArray().GetData());
#endif
				OnStallDetected(ThreadId, DeltaSeconds);
			}
		}
	}
}

void UE::FStallDetector::CheckAndReset()
{
	double CheckSeconds = FStallDetector::Seconds();
	if (CheckSeconds == InvalidSeconds)
	{
		return;
	}

	// If this is the first call to CheckAndReset, flag that we are now in persistent mode (vs. scope mode)
	if (!bPersistent)
	{
		bPersistent = true;
	}
	else
	{
		// only perform the check on the second call
		Check(true, CheckSeconds);
	}

	Timer.Reset(CheckSeconds, Stats.BudgetSeconds);
	Triggered = false;
}

void UE::FStallDetector::Pause(const double InSeconds)
{
	// All callers need to sort out checking the clock
	check(InSeconds != InvalidSeconds);

	Timer.Pause(InSeconds);
}

void UE::FStallDetector::Resume(const double InSeconds)
{
	// All callers need to sort out checking the clock
	check(InSeconds != InvalidSeconds);

	Timer.Resume(InSeconds);
}

void UE::FStallDetector::OnStallDetected(uint32 InThreadId, const double InElapsedSeconds)
{
	Stats.TotalTriggeredCount.Increment();

	//
	// Determine if we want to undermine the specified reporting mode
	//

	EStallDetectorReportingMode ReportingMode = Stats.ReportingMode;

	bool bDisableReporting = false;

#if UE_BUILD_DEBUG
	bDisableReporting |= true; // Do not generate a report in debug configurations due to performance characteristics
#endif

#if !STALL_DETECTOR_DEBUG
	bDisableReporting |= FPlatformMisc::IsDebuggerPresent(); // Do not generate a report if we detect the debugger mucking with things
#endif

	if (bDisableReporting)
	{
#if !STALL_DETECTOR_DEBUG
		ReportingMode = EStallDetectorReportingMode::Disabled;
#endif
	}

	//
	// Resolve reporting mode to whether we should send a report for this call
	//

	bool bSendReport = false;
	switch (ReportingMode)
	{
	case EStallDetectorReportingMode::First:
		bSendReport = !Stats.bReported;
		break;

	case EStallDetectorReportingMode::Always:
		bSendReport = true;
		break;

	default:
		break;
	}

	//
	// Send the report
	//

	if (bSendReport)
	{
		Stats.bReported = true;
		Stats.TotalReportedCount.Increment();
		const int NumStackFramesToIgnore = FPlatformTLS::GetCurrentThreadId() == InThreadId ? 2 : 0;
		UE_LOG(LogStall, Log, TEXT("Stall detector '%s' exceeded budget of %fs, reporting..."), Stats.Name, Stats.BudgetSeconds);
		double ReportSeconds = FStallDetector::Seconds();
		ReportStall(Stats.Name, InThreadId);
		ReportSeconds = FStallDetector::Seconds() - ReportSeconds;
		UE_LOG(LogStall, Log, TEXT("Stall detector '%s' report submitted, and took %fs"), Stats.Name, ReportSeconds);
	}
	else
	{
		if (ReportingMode != EStallDetectorReportingMode::Disabled)
		{
			UE_LOG(LogStall, Log, TEXT("Stall detector '%s' exceeded budget of %fs"), Stats.Name, Stats.BudgetSeconds);
		}
	}
}

double UE::FStallDetector::Seconds()
{
	double Result = InvalidSeconds;

	if (FStallDetector::IsRunning())
	{
#if STALL_DETECTOR_HEART_BEAT_CLOCK
		Result = Runnable->GetClock().Seconds();
#else
		Result = FPlatformTime::Seconds();
#endif

#if STALL_DETECTOR_DEBUG
		static double ClockStartSeconds = Result;
		static double PlatformStartSeconds = FPlatformTime::Seconds();
		double ClockDelta = Result - ClockStartSeconds;
		double PlatformDelta = FPlatformTime::Seconds() - PlatformStartSeconds;
		double Drift = PlatformDelta - ClockDelta;
		static double LastDrift = Drift;
		double DriftDelta = Drift - LastDrift;
		if (DriftDelta > 0.001)
		{
			FString ResultString = FString::Printf(TEXT("[FStallDetector] Thread %5d / Platform: %f / Clock: %f / Drift: %f (%f)\n"), FPlatformTLS::GetCurrentThreadId(), PlatformDelta, ClockDelta, Drift, DriftDelta);
			FPlatformMisc::LocalPrint(ResultString.GetCharArray().GetData());
			LastDrift = Drift;
		}
#endif
	}

	return Result;
}

void UE::FStallDetector::Startup()
{
	check(InitCount >= 0);
	if (++InitCount == 1)
	{
		UE_LOG(LogStall, Log, TEXT("Startup..."));

		check(FPlatformTime::GetSecondsPerCycle() > 0.0);

		// Cannot be a global due to clock member
		Runnable = new FStallDetectorRunnable();

		if (FPlatformProcess::SupportsMultithreading())
		{
			if (Thread == nullptr)
			{
				Thread = FRunnableThread::Create(Runnable, TEXT("StallDetectorThread"));
				check(Thread);

				// Poll until we have ticked the clock
				while (!Runnable->GetStartedThread())
				{
					FPlatformProcess::YieldThread();
				}
			}
		}

		UE_LOG(LogStall, Log, TEXT("Startup complete."));
	}
}

void UE::FStallDetector::Shutdown()
{
	if (--InitCount == 0)
	{
		UE_LOG(LogStall, Log, TEXT("Shutdown..."));

		delete Thread;
		Thread = nullptr;

		delete Runnable;
		Runnable = nullptr;

		UE_LOG(LogStall, Log, TEXT("Shutdown complete."));
	}
	check(InitCount >= 0);
}

bool UE::FStallDetector::IsRunning()
{
	return InitCount > 0;
}

UE::FStallDetectorPause::FStallDetectorPause()
	: bPaused(false)
{
	// this will be invalid if the thread isn't running
	double Seconds = FStallDetector::Seconds();
	if (Seconds == InvalidSeconds)
	{
		return;
	}

	bPaused = true;

	for (FStallDetector* Current = static_cast<FStallDetector*>(FPlatformTLS::GetTlsValue(ThreadLocalSlot.GetSlot())); Current; Current = Current->GetParent())
	{
		Current->Pause(Seconds);
	}
}

UE::FStallDetectorPause::~FStallDetectorPause()
{
	// this will be invalid if the thread isn't running
	double Seconds = FStallDetector::Seconds();
	if (Seconds == InvalidSeconds)
	{
		return;
	}

	if (bPaused)
	{
		for (FStallDetector* Current = static_cast<FStallDetector*>(FPlatformTLS::GetTlsValue(ThreadLocalSlot.GetSlot())); Current; Current = Current->GetParent())
		{
			Current->Resume(Seconds);
		}
	}
}

#endif // STALL_DETECTOR