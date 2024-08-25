// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataRequestOwner.h"

#include "Async/EventCount.h"
#include "Containers/Array.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestTypes.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include <atomic>

namespace UE::DerivedData::Private
{

class FRequestOwnerShared final : public IRequestOwner, public FRequestBase
{
public:
	explicit FRequestOwnerShared(EPriority Priority);
	~FRequestOwnerShared() final = default;

	void Begin(IRequest* Request) final;
	TRefCountPtr<IRequest> End(IRequest* Request) final;

	void BeginBarrier(ERequestBarrierFlags Flags) final;
	void EndBarrier(ERequestBarrierFlags Flags) final;

	inline EPriority GetPriority() const final { return Priority.load(std::memory_order_relaxed); }
	inline bool IsCanceled() const final { return bIsCanceled.load(std::memory_order_relaxed); }

	inline void SetPriority(EPriority Priority) final;
	inline void Cancel() final;
	inline void Wait() final;
	inline bool Poll() const;

	inline void KeepAlive();
	inline void Destroy();

private:
	mutable FRWLock Lock;
	TArray<TRefCountPtr<IRequest>, TInlineAllocator<1>> Requests;
	FEventCount BarrierEvent;
	uint32 BarrierCount{0};
	uint32 PriorityBarrierCount{0};
	std::atomic<EPriority> Priority{EPriority::Normal};
	std::atomic<bool> bIsCanceled{false};
	std::atomic<bool> bKeepAlive{false};
	uint8 bPriorityChangedInBarrier : 1;
	uint8 bBeginExecuted : 1;
};

FRequestOwnerShared::FRequestOwnerShared(EPriority NewPriority)
	: Priority(NewPriority)
	, bPriorityChangedInBarrier(false)
	, bBeginExecuted(false)
{
	AddRef(); // Release is called by Destroy.
}

void FRequestOwnerShared::Begin(IRequest* Request)
{
	AddRef();
	EPriority NewPriority;
	{
		FWriteScopeLock WriteLock(Lock);
		checkf(BarrierCount > 0 || !bBeginExecuted,
			TEXT("At least one FRequestBarrier must be in scope when beginning a request after the first request. "
			     "The overload of End that invokes a callback handles this automatically for most use cases."));
		check(Request);
		Requests.Add(Request);
		bBeginExecuted = true;
		if (!bPriorityChangedInBarrier)
		{
			return;
		}
		NewPriority = GetPriority();
	}
	// Loop until priority is stable. Another thread may be changing the priority concurrently.
	for (EPriority CheckPriority; ; NewPriority = CheckPriority)
	{
		Request->SetPriority(NewPriority);
		
		{
			FReadScopeLock ScopeLock(Lock);
			CheckPriority = GetPriority();
		}

		if (CheckPriority == NewPriority)
		{
			break;
		}
	}
}

TRefCountPtr<IRequest> FRequestOwnerShared::End(IRequest* Request)
{
	ON_SCOPE_EXIT { Release(); };
	FWriteScopeLock WriteLock(Lock);
	check(Request);
	TRefCountPtr<IRequest>* RequestPtr = Requests.FindByKey(Request);
	check(RequestPtr);
	TRefCountPtr<IRequest> RequestRef = MoveTemp(*RequestPtr);
	Requests.RemoveAtSwap(UE_PTRDIFF_TO_INT32(RequestPtr - Requests.GetData()), 1, EAllowShrinking::No);
	return RequestRef;
}

void FRequestOwnerShared::BeginBarrier(ERequestBarrierFlags Flags)
{
	AddRef();
	FWriteScopeLock WriteLock(Lock);
	++BarrierCount;
	if (EnumHasAnyFlags(Flags, ERequestBarrierFlags::Priority))
	{
		++PriorityBarrierCount;
	}
}

void FRequestOwnerShared::EndBarrier(ERequestBarrierFlags Flags)
{
	ON_SCOPE_EXIT { Release(); };
	bool bNotifyBarrier = false;
	{
		FWriteScopeLock WriteLock(Lock);
		if (EnumHasAnyFlags(Flags, ERequestBarrierFlags::Priority) && --PriorityBarrierCount == 0)
		{
			bPriorityChangedInBarrier = false;
		}
		if (--BarrierCount == 0)
		{
			bNotifyBarrier = true;
		}
	}
	if (bNotifyBarrier)
	{
		BarrierEvent.Notify();
	}
}

void FRequestOwnerShared::SetPriority(EPriority NewPriority)
{
	TArray<TRefCountPtr<IRequest>, TInlineAllocator<16>> LocalRequests;
	if (FWriteScopeLock WriteLock(Lock); GetPriority() == NewPriority)
	{
		return;
	}
	else
	{
		Priority.store(NewPriority, std::memory_order_relaxed);
		LocalRequests = Requests;
		bPriorityChangedInBarrier = (PriorityBarrierCount > 0);
	}

	for (IRequest* Request : LocalRequests)
	{
		Request->SetPriority(NewPriority);
	}
}

void FRequestOwnerShared::Cancel()
{
	for (;;)
	{
		TRefCountPtr<IRequest> LocalRequest;
		FEventCountToken BarrierToken;

		{
			FWriteScopeLock WriteLock(Lock);
			bIsCanceled.store(true, std::memory_order_relaxed);

			if (!Requests.IsEmpty())
			{
				LocalRequest = Requests.Last();
			}
			else if (BarrierCount > 0)
			{
				BarrierToken = BarrierEvent.PrepareWait();
			}
			else
			{
				return;
			}
		}

		if (LocalRequest)
		{
			LocalRequest->Cancel();
		}
		else
		{
			BarrierEvent.Wait(BarrierToken);
		}
	}
}

void FRequestOwnerShared::Wait()
{
	for (;;)
	{
		TRefCountPtr<IRequest> LocalRequest;
		FEventCountToken BarrierToken;

		{
			FWriteScopeLock WriteLock(Lock);

			if (!Requests.IsEmpty())
			{
				LocalRequest = Requests.Last();
			}
			else if (BarrierCount > 0)
			{
				BarrierToken = BarrierEvent.PrepareWait();
			}
			else
			{
				return;
			}
		}

		if (LocalRequest)
		{
			LocalRequest->Wait();
		}
		else
		{
			BarrierEvent.Wait(BarrierToken);
		}
	}
}

bool FRequestOwnerShared::Poll() const
{
	FReadScopeLock ReadLock(Lock);
	return Requests.IsEmpty() && BarrierCount == 0;
}

void FRequestOwnerShared::KeepAlive()
{
	bKeepAlive = true;
}

void FRequestOwnerShared::Destroy()
{
	if (!bKeepAlive)
	{
		Cancel();
	}
	Release();
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

FRequestOwner::FRequestOwner(EPriority Priority) : Owner(new Private::FRequestOwnerShared(Priority)) {}
static Private::FRequestOwnerShared* ToSharedOwner(IRequestOwner* Owner) { return static_cast<Private::FRequestOwnerShared*>(Owner); }
void FRequestOwner::KeepAlive()                         { return ToSharedOwner(Owner.Get())->KeepAlive(); }
EPriority FRequestOwner::GetPriority() const            { return ToSharedOwner(Owner.Get())->GetPriority(); }
void FRequestOwner::SetPriority(EPriority Priority)     { return ToSharedOwner(Owner.Get())->SetPriority(Priority); }
void FRequestOwner::Cancel()                            { return ToSharedOwner(Owner.Get())->Cancel(); }
void FRequestOwner::Wait()                              { return ToSharedOwner(Owner.Get())->Wait(); }
bool FRequestOwner::Poll() const                        { return ToSharedOwner(Owner.Get())->Poll(); }
FRequestOwner::operator IRequest*()                     { return ToSharedOwner(Owner.Get()); }
void FRequestOwner::Destroy(IRequestOwner& SharedOwner) { return ToSharedOwner(&SharedOwner)->Destroy(); }

void IRequestOwner::LaunchTask(const TCHAR* DebugName, TUniqueFunction<void ()>&& TaskBody)
{
	using namespace Tasks;

	struct FTaskRequest final : public FRequestBase
	{
		void SetPriority(EPriority Priority) final {}
		void Cancel() final { Task.Wait(); }
		void Wait() final { Task.Wait(); }
		FTask Task;
	};

	Tasks::FTaskEvent TaskEvent(TEXT("LaunchTaskRequest"));
	FTaskRequest* Request = new FTaskRequest;
	ETaskPriority TaskPriority;
	switch (GetPriority())
	{
	case EPriority::Highest:
	case EPriority::Blocking:
		TaskPriority = ETaskPriority::High;
		break;
	case EPriority::High:
		TaskPriority = ETaskPriority::BackgroundHigh;
		break;
	default:
		TaskPriority = ETaskPriority::BackgroundNormal;
		break;
	}
	Request->Task = Launch(
		DebugName,
		[this, Request, TaskBody = MoveTemp(TaskBody)]
		{
			End(Request, TaskBody);
		},
		TaskEvent,
		TaskPriority);
	Begin(Request);
	TaskEvent.Trigger();
}

} // UE::DerivedData
