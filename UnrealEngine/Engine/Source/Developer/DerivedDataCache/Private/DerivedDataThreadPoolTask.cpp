// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataThreadPoolTask.h"

#include "Async/InheritedContext.h"
#include "Async/ManualResetEvent.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataRequestTypes.h"
#include "Misc/IQueuedWork.h"
#include "Misc/QueuedThreadPool.h"
#include "Stats/Stats.h"

namespace UE::DerivedData
{

class FThreadPoolTaskRequest final : public FRequestBase, private FInheritedContextBase, private IQueuedWork
{
public:
	FThreadPoolTaskRequest(
		uint64 MemoryEstimate,
		const TCHAR * DebugName,
		IRequestOwner& Owner,
		FQueuedThreadPool& ThreadPool,
		TUniqueFunction<void ()>&& TaskBody);
	~FThreadPoolTaskRequest();

private:
	TRefCountPtr<IRequest> TryEnd();
	void Execute();

	void SetPriority(EPriority Priority) final;
	void Cancel() final { Wait(); }
	void Wait() final;

	void DoThreadedWork() final { Execute(); }
	void Abandon() final { Execute(); }
	EQueuedWorkFlags GetQueuedWorkFlags() const final
	{
		return EQueuedWorkFlags::DoNotRunInsideBusyWait;
	}
	
	int64 GetRequiredMemory() const final
	{
		return int64(MemoryEstimate);
	}
	
	const TCHAR * GetDebugName() const final
	{
		return DebugName;
	}

	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FThreadPoolTaskRequest, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	uint64 MemoryEstimate;
	const TCHAR * DebugName = nullptr;
	IRequestOwner& Owner;
	FQueuedThreadPool& ThreadPool;
	TUniqueFunction<void ()> TaskBody;
	FManualResetEvent Event;
	std::atomic<bool> bClaimed = false;
};

FThreadPoolTaskRequest::FThreadPoolTaskRequest(
	const uint64 InMemoryEstimate,
	const TCHAR * InDebugName,
	IRequestOwner& InOwner,
	FQueuedThreadPool& InThreadPool,
	TUniqueFunction<void ()>&& InTaskBody)
	: MemoryEstimate(InMemoryEstimate)
	, DebugName(InDebugName)
	, Owner(InOwner)
	, ThreadPool(InThreadPool)
	, TaskBody(MoveTemp(InTaskBody))
{
	check(MemoryEstimate <= MAX_int64);
	CaptureInheritedContext();
	AddRef(); // Released in Execute() or Cancel()
	Owner.Begin(this);
	ThreadPool.AddQueuedWork(this, ConvertToQueuedWorkPriority(Owner.GetPriority()));
}

FThreadPoolTaskRequest::~FThreadPoolTaskRequest()
{
	check(bClaimed.load(std::memory_order_relaxed));
}

TRefCountPtr<IRequest> FThreadPoolTaskRequest::TryEnd()
{
	return bClaimed.exchange(true) ? nullptr : Owner.End(this, [this]
	{
		FInheritedContextScope InheritedContextScope = RestoreInheritedContext();
		FScopeCycleCounter Scope(GetStatId(), /*bAlways*/ true);
		TaskBody();
		Event.Notify();
	});
}

void FThreadPoolTaskRequest::Execute()
{
	TryEnd();
	Release();
	// DO NOT ACCESS ANY MEMBERS PAST THIS POINT!
}

void FThreadPoolTaskRequest::SetPriority(EPriority Priority)
{
	if (ThreadPool.RetractQueuedWork(this))
	{
		ThreadPool.AddQueuedWork(this, ConvertToQueuedWorkPriority(Priority));
	}
}

void FThreadPoolTaskRequest::Wait()
{
	if (TRefCountPtr<IRequest> Self = TryEnd())
	{
		if (ThreadPool.RetractQueuedWork(this))
		{
			Release();
		}
	}
	else
	{
		FScopeCycleCounter Scope(GetStatId());
		Event.Wait();
	}
}

void LaunchTaskInThreadPool(
	uint64 MemoryEstimate,
	const TCHAR * DebugName,
	IRequestOwner& Owner,
	FQueuedThreadPool* ThreadPool,
	TUniqueFunction<void ()>&& TaskBody)
{
	if (ThreadPool)
	{
		// The request is reference-counted and will be deleted when complete.
		new FThreadPoolTaskRequest(MemoryEstimate, DebugName, Owner, *ThreadPool, MoveTemp(TaskBody));
	}
	else
	{
		TaskBody();
	}
}

void LaunchTaskInThreadPool(
	uint64 MemoryEstimate,
	IRequestOwner& Owner,
	FQueuedThreadPool* ThreadPool,
	TUniqueFunction<void ()>&& TaskBody)
{
	LaunchTaskInThreadPool(MemoryEstimate,TEXT("LaunchTaskInThreadPool"), Owner, ThreadPool, MoveTemp(TaskBody));
}

void LaunchTaskInThreadPool(
	IRequestOwner& Owner,
	FQueuedThreadPool* ThreadPool,
	TUniqueFunction<void ()>&& TaskBody)
{
	LaunchTaskInThreadPool(0,TEXT("LaunchTaskInThreadPool"), Owner, ThreadPool, MoveTemp(TaskBody));
}

} // UE::DerivedData
