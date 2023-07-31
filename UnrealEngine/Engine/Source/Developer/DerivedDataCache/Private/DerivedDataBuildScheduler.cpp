// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildScheduler.h"
#include "Containers/RingBuffer.h"
#include "DerivedDataBuildFunction.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "DerivedDataBuildFunctionRegistry.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "Experimental/Async/LazyEvent.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"

namespace UE::DerivedData::Private
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void ScheduleAsyncStep(IBuildJob& Job, IRequestOwner& Owner, const TCHAR* DebugName)
{
	Owner.LaunchTask(DebugName, [&Job] { Job.StepExecution(); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Limits simultaneous build jobs to reduce peak memory usage */
class FMemoryScheduler
{
public:
	FMemoryScheduler();
	~FMemoryScheduler();

	void RegisterRunningJob(uint64 MemoryEstimate);
	void StepAsyncOrQueue(uint64 MemoryEstimate, IBuildJob& Job, IRequestOwner& Owner, const TCHAR* DebugName);
	void RegisterEndedJob(uint64 MemoryEstimate);

private:
	/** Handles waiting and cancellation while a job is queued up */
	class FRequest final : public FRequestBase
	{
	public:
		FRequest(FMemoryScheduler& InScheduler, IBuildJob& InJob, IRequestOwner& InOwner, uint64 InMemoryEstimate);
		~FRequest() { ensure(!TryClaimEnd()); }

		void SetPriority(EPriority Priority) final {}
		void Wait() final { Event.Wait(); }
		void Cancel() final;

		uint64 GetMemoryEstimate() const { return MemoryEstimate; }
		bool TryClaimEnd() { return !bClaimed.test_and_set(); }
		void End(const TCHAR* DebugName);

	private:
		FMemoryScheduler& Scheduler;
		IBuildJob& Job;
		IRequestOwner& Owner;
		const uint64 MemoryEstimate;
		UE::FLazyEvent Event{EEventMode::ManualReset};
		std::atomic_flag bClaimed = ATOMIC_FLAG_INIT;
	};

	const uint64 TotalPhysical;
	const uint64 AvailablePhysicalAtStartup;
	const uint64 MaxMemoryUsage;

	FCriticalSection CriticalSection;
	TRingBuffer<TRefCountPtr<FRequest>> Queue;
	uint64 TotalScheduledMemory = 0;
	uint64 TotalScheduledWatermark = 0;

	// @pre CriticalSection locked
	bool CanRunNow(uint64 MemoryEstimate) const
	{
		return TotalScheduledMemory == 0 || TotalScheduledMemory + MemoryEstimate < MaxMemoryUsage;
	}
};

FMemoryScheduler::FRequest::FRequest(
	FMemoryScheduler& InScheduler,
	IBuildJob& InJob,
	IRequestOwner& InOwner,
	uint64 InMemoryEstimate)
	: Scheduler(InScheduler)
	, Job(InJob)
	, Owner(InOwner)
	, MemoryEstimate(InMemoryEstimate)
{
	Owner.Begin(this);
}

void FMemoryScheduler::FRequest::End(const TCHAR* DebugName)
{
	Owner.End(this, [this, DebugName]
	{ 
		ScheduleAsyncStep(Job, Owner, DebugName);
		Event.Trigger();
	});
}

void FMemoryScheduler::FRequest::Cancel()
{
	if (TryClaimEnd())
	{
		// Add estimated memory to simplify implementation, even though memory won't be allocated.
		// FBuildJobSchedule::EndJob() will restore the scheduler's available memory.
		// Might require optimization if lots of queued jobs are cancelled at the same time.
		Scheduler.RegisterRunningJob(MemoryEstimate);
		End(TEXT("MemoryQueueCancel"));
	}
}

FMemoryScheduler::FMemoryScheduler()
	: TotalPhysical(FPlatformMemory::GetStats().TotalPhysical)
	, AvailablePhysicalAtStartup(FPlatformMemory::GetStats().AvailablePhysical)
	, MaxMemoryUsage(TotalPhysical / 8 + AvailablePhysicalAtStartup / 2)
{
	Queue.Reserve(128);
}

FMemoryScheduler::~FMemoryScheduler()
{
	ensure(Queue.IsEmpty());
	ensure(TotalScheduledMemory == 0);
}

void FMemoryScheduler::RegisterRunningJob(uint64 MemoryEstimate)
{
	check(MemoryEstimate);

	FScopeLock Lock(&CriticalSection);
	TotalScheduledMemory += MemoryEstimate;
	TotalScheduledWatermark = FMath::Max(TotalScheduledWatermark, TotalScheduledMemory);
}

void FMemoryScheduler::StepAsyncOrQueue(
	uint64 MemoryEstimate,
	IBuildJob& Job,
	IRequestOwner& Owner,
	const TCHAR* DebugName)
{
	check(MemoryEstimate);
	{
		FScopeLock Lock(&CriticalSection);

		if (!CanRunNow(MemoryEstimate))
		{
			Queue.Emplace(new FRequest(*this, Job, Owner, MemoryEstimate));
			return;
		}

		TotalScheduledMemory += MemoryEstimate;
		TotalScheduledWatermark = FMath::Max(TotalScheduledWatermark, TotalScheduledMemory);
	}

	ScheduleAsyncStep(Job, Owner, DebugName);
}

void FMemoryScheduler::RegisterEndedJob(uint64 DoneEstimate)
{
	if (DoneEstimate)
	{
		TArray<TRefCountPtr<FRequest>, TInlineAllocator<16>> Continuations;

		{
			FScopeLock Lock(&CriticalSection);

			TotalScheduledMemory -= DoneEstimate;

			if (Queue.IsEmpty())
			{
				return;
			}

			while (Queue.Num() && CanRunNow(Queue.First()->GetMemoryEstimate()))
			{
				if (Queue.First()->TryClaimEnd())
				{
					TotalScheduledMemory += Queue.First()->GetMemoryEstimate();
					Continuations.Add(Queue.First());
				}
				Queue.PopFront();
			}

			TotalScheduledWatermark = FMath::Max(TotalScheduledWatermark, TotalScheduledMemory);
		}

		for (const TRefCountPtr<FRequest>& Request : Continuations)
		{
			Request->End(TEXT("MemoryQueueContinue"));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildJobSchedule final : public IBuildJobSchedule
{
public:
	FBuildJobSchedule(IBuildJob& InJob, IRequestOwner& InOwner, FMemoryScheduler& InMemoryLimiter)
		: Job(InJob)
		, Owner(InOwner)
		, MemoryLimiter(InMemoryLimiter)
	{
	}

	FBuildSchedulerParams& EditParameters() final { return Params; }

	void ScheduleCacheQuery() final       { StepSync(); }
	void ScheduleCacheStore() final       { StepSync(); }
	void ScheduleResolveKey() final       { StepAsync(TEXT("ResolveKey")); }
	void ScheduleResolveInputMeta() final { StepAsync(TEXT("ResolveInputMeta")); }
	void ScheduleResolveInputData() final { StepAsyncOrQueue(TEXT("ResolveInputData")); }
	void ScheduleExecuteRemote() final    { StepAsyncOrQueue(TEXT("ExecuteRemote")); }
	void ScheduleExecuteLocal() final     { StepAsyncOrQueue(TEXT("ExecuteLocal")); }

	void EndJob() final { MemoryLimiter.RegisterEndedJob(ScheduledMemoryEstimate); }

private:
	void StepSync()
	{
		Job.StepExecution();
	}

	void StepAsync(const TCHAR* DebugName)
	{
		if (Owner.GetPriority() == EPriority::Blocking)
		{
			StepSync();
		}
		else
		{
			ScheduleAsyncStep(Job, Owner, DebugName);
		}
	}

	void StepAsyncOrQueue(const TCHAR* DebugName)
	{
		check(Params.TotalRequiredMemory >= Params.ResolvedInputsSize);
		const uint64 CurrentMemoryEstimate = Params.TotalRequiredMemory - Params.ResolvedInputsSize;

		// Only queue for memory once
		if (ScheduledMemoryEstimate || CurrentMemoryEstimate == 0)
		{
			StepAsync(DebugName);
		}
		else
		{
			ScheduledMemoryEstimate = CurrentMemoryEstimate;

			if (Owner.GetPriority() == EPriority::Blocking)
			{
				MemoryLimiter.RegisterRunningJob(ScheduledMemoryEstimate);
				StepSync();
			}
			else
			{
				MemoryLimiter.StepAsyncOrQueue(ScheduledMemoryEstimate, Job, Owner, DebugName);
			}
		}
	}

private:
	IBuildJob& Job;
	IRequestOwner& Owner;
	FBuildSchedulerParams Params;
	FMemoryScheduler& MemoryLimiter;
	uint64 ScheduledMemoryEstimate = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildScheduler final : public IBuildScheduler
{
	TUniquePtr<IBuildJobSchedule> BeginJob(IBuildJob& Job, IRequestOwner& Owner) final
	{
		return MakeUnique<FBuildJobSchedule>(Job, Owner, MemoryLimiter);
	}

	FMemoryScheduler MemoryLimiter;
};

IBuildScheduler* CreateBuildScheduler()
{
	return new FBuildScheduler();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // UE::DerivedData::Private
