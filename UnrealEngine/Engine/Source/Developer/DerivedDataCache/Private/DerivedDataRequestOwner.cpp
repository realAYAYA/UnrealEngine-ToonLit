// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataRequestOwner.h"

#include "Containers/Array.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestTypes.h"
#include "Experimental/Async/LazyEvent.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include <atomic>

namespace UE::DerivedData::Private
{

/** A one-time-use event to work around the lack of condition variables. */
struct FRequestBarrierEvent : public FRefCountBase
{
	FLazyEvent Event{EEventMode::ManualReset};
};

class FRequestOwnerShared final : public IRequestOwner, public FRequestBase
{
public:
	explicit FRequestOwnerShared(EPriority Priority);
	~FRequestOwnerShared() final = default;

	void Begin(IRequest* Request) final;
	TRefCountPtr<IRequest> End(IRequest* Request) final;

	void BeginBarrier(ERequestBarrierFlags Flags) final;
	void EndBarrier(ERequestBarrierFlags Flags) final;

	inline EPriority GetPriority() const final { return Priority; }
	inline bool IsCanceled() const final { return bIsCanceled; }

	inline void SetPriority(EPriority Priority) final;
	inline void Cancel() final;
	inline void Wait() final;
	inline bool Poll() const;

	inline void KeepAlive();
	inline void Destroy();

private:
	mutable FRWLock Lock;
	TArray<TRefCountPtr<IRequest>, TInlineAllocator<1>> Requests;
	TRefCountPtr<FRequestBarrierEvent> BarrierEvent;
	uint32 BarrierCount{0};
	uint32 PriorityBarrierCount{0};
	EPriority Priority{EPriority::Normal};
	uint8 bPriorityChangedInBarrier : 1;
	uint8 bBeginExecuted : 1;
	bool bIsCanceled{false};
	bool bKeepAlive{false};
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
		NewPriority = Priority;
	}
	// Loop until priority is stable. Another thread may be changing the priority concurrently.
	for (EPriority CheckPriority; ; NewPriority = CheckPriority)
	{
		Request->SetPriority(NewPriority);
		
		{
			FReadScopeLock ScopeLock(Lock);
			CheckPriority = Priority;
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
	Requests.RemoveAtSwap(UE_PTRDIFF_TO_INT32(RequestPtr - Requests.GetData()), 1, /*bAllowShrinking*/ false);
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
	TRefCountPtr<FRequestBarrierEvent> LocalBarrierEvent;
	{
		FWriteScopeLock WriteLock(Lock);
		if (EnumHasAnyFlags(Flags, ERequestBarrierFlags::Priority) && --PriorityBarrierCount == 0)
		{
			bPriorityChangedInBarrier = false;
		}
		if (--BarrierCount == 0)
		{
			LocalBarrierEvent = MoveTemp(BarrierEvent);
			BarrierEvent = nullptr;
		}
	}
	if (LocalBarrierEvent)
	{
		LocalBarrierEvent->Event.Trigger();
	}
}

void FRequestOwnerShared::SetPriority(EPriority NewPriority)
{
	TArray<TRefCountPtr<IRequest>, TInlineAllocator<16>> LocalRequests;
	if (FWriteScopeLock WriteLock(Lock); Priority == NewPriority)
	{
		return;
	}
	else
	{
		Priority = NewPriority;
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
		TRefCountPtr<FRequestBarrierEvent> LocalBarrierEvent;

		{
			FWriteScopeLock WriteLock(Lock);
			bIsCanceled = true;

			if (!Requests.IsEmpty())
			{
				LocalRequest = Requests.Last();
			}
			else if (BarrierCount > 0)
			{
				LocalBarrierEvent = BarrierEvent;
				if (!LocalBarrierEvent)
				{
					LocalBarrierEvent = BarrierEvent = new FRequestBarrierEvent;
				}
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
			LocalBarrierEvent->Event.Wait();
		}
	}
}

void FRequestOwnerShared::Wait()
{
	for (;;)
	{
		TRefCountPtr<IRequest> LocalRequest;
		TRefCountPtr<FRequestBarrierEvent> LocalBarrierEvent;

		{
			FWriteScopeLock WriteLock(Lock);

			if (!Requests.IsEmpty())
			{
				LocalRequest = Requests.Last();
			}
			else if (BarrierCount > 0)
			{
				LocalBarrierEvent = BarrierEvent;
				if (!LocalBarrierEvent)
				{
					LocalBarrierEvent = BarrierEvent = new FRequestBarrierEvent;
				}
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
			LocalBarrierEvent->Event.Wait();
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
	FWriteScopeLock WriteLock(Lock);
	bKeepAlive = true;
}

void FRequestOwnerShared::Destroy()
{
	bool bLocalKeepAlive;
	{
		FWriteScopeLock ScopeLock(Lock);
		bLocalKeepAlive = bKeepAlive;
	}
	if (!bLocalKeepAlive)
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
		LLM(const UE::LLMPrivate::FTagData* MemTag = nullptr);
	};

	Tasks::FTaskEvent TaskEvent(TEXT("LaunchTaskRequest"));
	FTaskRequest* Request = new FTaskRequest;
	LLM_IF_ENABLED(Request->MemTag = FLowLevelMemTracker::Get().GetActiveTagData(ELLMTracker::Default));
	Request->Task = Launch(
		DebugName,
		[this, Request, TaskBody = MoveTemp(TaskBody)]
		{
			LLM_SCOPE(Request->MemTag);
			End(Request, TaskBody);
		},
		TaskEvent,
		GetPriority() <= EPriority::Normal ? ETaskPriority::BackgroundNormal : ETaskPriority::BackgroundHigh);
	Begin(Request);
	TaskEvent.Trigger();
}

} // UE::DerivedData
