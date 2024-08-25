// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Async/Fundamental/Scheduler.h"
#include "Async/Fundamental/Task.h"
#include "Async/TaskTrace.h"
#include "CoreMinimal.h"
#include "CoroLocalVariable.h"
#include "CoroutineHandle.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "HAL/Platform.h"
#include "Misc/MemStack.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Templates/Decay.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

#include <atomic>

#if WITH_CPP_COROUTINES

#define coroCheck(...) checkSlow(__VA_ARGS__)
#define coroVerify(...) verifySlow(__VA_ARGS__)
#define COROFORCEINLINE FORCEINLINE_DEBUGGABLE
#if DO_CHECK
	#define COROCHECK(...) __VA_ARGS__
#else
	#define COROCHECK(...)
#endif
#if CPUPROFILERTRACE_ENABLED
	#define COROPROFILERTRACE(...) __VA_ARGS__
#else
	#define COROPROFILERTRACE(...)
#endif

#if UE_TASK_TRACE_ENABLED
	#define COROTASKTRACE_ENABLED 1
	#define COROTASKTRACE(...) __VA_ARGS__
#else
	#define COROTASKTRACE_ENABLED 0
	#define COROTASKTRACE(...)
#endif

#if defined(_MSC_VER)
	#define CORO_ALIGNMENT 32
#else
	#define CORO_ALIGNMENT 8
#endif

#define CORO_FRAME(...) TCoroFrame<__VA_ARGS__>
#define CORO_TASK(...) TCoroTask<__VA_ARGS__>
#define LAUNCHED_TASK(...) TLaunchedCoroTask<__VA_ARGS__>
#define CO_AWAIT co_await
#define CO_RETURN co_return
#define CO_RETURN_TASK(...) co_return __VA_ARGS__

template<typename>
class TCoroTask;

template<typename>
class TLaunchedCoroTask;

class FLockedTask;

template<typename>
class TCoroFrame;

namespace CoroTask_Detail
{
	struct FCoroBlockAllocationTag : FDefaultBlockAllocationTag
	{
		static constexpr uint32 BlockSize = 2 * 1024 * 1024;
		static constexpr bool AllowOversizedBlocks = false;
		static constexpr bool RequiresAccurateSize = false;
		static constexpr bool InlineBlockAllocation = true;
		static constexpr const char* TagName = "CoroLinear";
	};

	class FPromise;

	template<typename>
	class TFramePromise;

	class IPromise
	{
		template<typename>
		friend class ::TCoroFrame;

		template<typename>
		friend class ::TCoroTask;

		template<typename>
		friend class ::TLaunchedCoroTask;

	protected:
		mutable FPromise* Prerequisite = nullptr;

	public:
		COROFORCEINLINE void Suspend(const FPromise* InPrerequisite) const
		{
			coroCheck(Prerequisite == nullptr && InPrerequisite != nullptr);
			Prerequisite = const_cast<FPromise*>(InPrerequisite);
		}
	};

	/*
	* Safe use of Prerequisite:
	* 
	*                    Task.Init in Launch |   Task.TryCancel in TryExpedite |   Task.TryRevive in TryExpedite |  (optional: FTaskExecutor()) | Task.Init/CAS in Reschedule or TryExpedite
	*                               |                               |                             |                               |                             |
	*                               V                               V                             V                               V                             V
	*  ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	* | Safe to access Prerequisite | Unsafe to access Prerequisite | Safe to access Prerequisite | Unsafe to access Prerequisite | Safe to access Prerequisite | Unsafe to access Prerequisite | ...
	*  ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	*/

	class FPromise : public IPromise
	{
		using ThisType = FPromise;

		template<typename>
		friend class ::TCoroFrame;

		template<typename>
		friend class ::TCoroTask;

		friend class ::FLockedTask;

		template<typename>
		friend class ::TLaunchedCoroTask;

	private:
		coroutine_handle_base CoroutineHandle;
		LowLevelTasks::FTask Task;
		std::atomic_int RefCount {1};
		ETaskTag TaskTag = ETaskTag::ENone;
		std::atomic<FPromise*> Subsequent { nullptr };
		COROPROFILERTRACE(uint64 CoroId = ~0ull);
		COROPROFILERTRACE(uint32 TimerScopeDepth = 0);
		COROTASKTRACE(TaskTrace::FId TraceId = ~0);
		FCoroLocalState ClsData;
		FMemStack MemStack;

	public:
		COROFORCEINLINE bool IsExpeditable() const
		{
			return Prerequisite != reinterpret_cast<FPromise*>(~uintptr_t(0));
		}

	private:
		COROFORCEINLINE void Lock() const
		{
			coroCheck(Prerequisite == nullptr);
			Prerequisite = reinterpret_cast<FPromise*>(~uintptr_t(0));
			coroCheck(!IsExpeditable());
		}
	
		COROFORCEINLINE void Execute()
		{
			coroCheck(!CoroutineHandle.done());
			coroCheck(Prerequisite == nullptr);

			FCoroLocalState* PrevCls = FCoroLocalState::SetCoroLocalState(&ClsData);
			TaskTag = FTaskTagScope::SwapTag(TaskTag);
			FMemStack* PrevMemStack = FMemStack::Get().Inject(&MemStack);
			COROTASKTRACE(TaskTrace::Started(TraceId));
			COROPROFILERTRACE(FCpuProfilerTrace::OutputResumeEvent(CoroId, TimerScopeDepth));
			CoroutineHandle();
			COROPROFILERTRACE(FCpuProfilerTrace::OutputSuspendEvent());
			COROTASKTRACE(CompleteTaskTraceContext());
			TaskTag = FTaskTagScope::SwapTag(TaskTag);	
			coroVerify(FCoroLocalState::SetCoroLocalState(PrevCls) == &ClsData);
			coroVerify(FMemStack::Get().Inject(PrevMemStack) == &MemStack);	
			coroCheck(IsExpeditable());
		}

		class FTaskExecutor
		{
			FPromise* Promise;
			mutable bool bDeferCleanup = false;

		public:
			COROFORCEINLINE FTaskExecutor(FPromise* InPromise) : Promise(InPromise)
			{
			}

			FTaskExecutor(const FTaskExecutor&) = delete;
			COROFORCEINLINE FTaskExecutor(FTaskExecutor&& Other) : Promise(Other.Promise), bDeferCleanup(Other.bDeferCleanup)
			{
				Other.Promise = nullptr;
			}

			COROFORCEINLINE void operator()() const noexcept
			{
				coroCheck(Promise->IsExpeditable());
				if(Promise->Prerequisite == nullptr || Promise->Prerequisite->IsCompleted())
				{
					Promise->Prerequisite = nullptr;
					Promise->Execute();
				}
				bDeferCleanup = !Promise->CoroutineHandle.done();
			}

			COROFORCEINLINE ~FTaskExecutor() 
			{
				if(Promise != nullptr)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(CoroExecutorDestructor);
					if (bDeferCleanup)
					{
						LowLevelTasks::FTask::FInitData InitData = Promise->Task.GetInitData();
						Promise->Reschedule(InitData.DebugName, InitData.Priority, InitData.Flags);
						return;
					}
					Promise->DecrementRefCount();
				}
			}
		};

		inline void Reschedule(const TCHAR* DebugName, LowLevelTasks::ETaskPriority Priority, LowLevelTasks::ETaskFlags Flags)
		{
			COROTASKTRACE(TaskTrace::Started(TraceId));
			TRACE_CPUPROFILER_EVENT_SCOPE(CoroReschedule);

			coroCheck(IsExpeditable());
			coroCheck(!CoroutineHandle.done());

			Flags = (Prerequisite && !Prerequisite->IsExpeditable()) ? (Flags & ~LowLevelTasks::ETaskFlags::AllowCancellation) : (Flags | LowLevelTasks::ETaskFlags::AllowCancellation);
			FPromise* NullPromise = nullptr;

			//the coroutine that has a prerequesite try to add itself as a subsequent to it's prerequesite, which might already be completed at this point
			if (Prerequisite && Prerequisite->Subsequent.compare_exchange_strong(NullPromise, this, std::memory_order_release, std::memory_order_relaxed))
			{
				COROTASKTRACE(CompleteTaskTraceContext(false));
				Task.Init(DebugName, Priority, FTaskExecutor(this), Flags); //as soon as Inti is called it is not safe to read the Prerequisite anymore
				return;
			}

			//otherwise if there was no prerequesite for the prerequesite already completed we just reschedule for execution
			coroCheck(NullPromise == nullptr || NullPromise == reinterpret_cast<FPromise*>(~0ull));
			LowLevelTasks::EQueuePreference QueuePreference = Prerequisite ? LowLevelTasks::EQueuePreference::LocalQueuePreference : LowLevelTasks::EQueuePreference::GlobalQueuePreference;
			Prerequisite = nullptr;
			COROTASKTRACE(CompleteTaskTraceContext());
			Task.Init(DebugName, Priority, FTaskExecutor(this), Flags); //as soon as Inti is called it is not safe to read the Prerequisite anymore

			coroVerify(LowLevelTasks::TryLaunch(Task, QueuePreference));
		}

	protected:
		COROFORCEINLINE FPromise(coroutine_handle_base InCoroutineHandle) : CoroutineHandle(InCoroutineHandle), ClsData(*this)
		{
			COROPROFILERTRACE(CoroId = FCoroLocalState::GenerateCoroId());
			COROTASKTRACE(TraceId = TaskTrace::GenerateTaskId());
		}

		COROFORCEINLINE void Complete()
		{
			if (FPromise* LocalSubsequent = Subsequent.exchange(reinterpret_cast<FPromise*>(~0ull), std::memory_order_acq_rel))
			{
				COROTASKTRACE(TaskTrace::Scheduled(LocalSubsequent->TraceId));
				while(!LowLevelTasks::TryLaunch(LocalSubsequent->Task, LowLevelTasks::EQueuePreference::LocalQueuePreference))
				{
					FPlatformProcess::Yield();
				}
			}
		}

	public:
		COROFORCEINLINE bool IsCompleted() const
		{
			return Subsequent.load(std::memory_order_acquire) == reinterpret_cast<FPromise*>(~0ull);
		}

		COROFORCEINLINE bool HasSubsequent() const
		{
			FPromise* LocalSubsequent = Subsequent.load(std::memory_order_acquire);
			coroCheck(LocalSubsequent != reinterpret_cast<FPromise*>(~0ull));
			return LocalSubsequent != nullptr;
		}

		COROFORCEINLINE const TCHAR* GetDebugName() const
		{
			return Task.GetDebugName();
		}

	private:
#if COROTASKTRACE_ENABLED
		inline void CompleteTaskTraceContext(bool AlwaysSchedule = true)
		{
			TaskTrace::FId OldTraceId = TraceId;
			if(!CoroutineHandle.done())
			{
				TraceId = TaskTrace::GenerateTaskId();
				TaskTrace::Launched(TraceId, GetDebugName(), true, ENamedThreads::Type::AnyThread);
				TaskTrace::SubsequentAdded(TraceId, OldTraceId);
				if(Prerequisite != nullptr)
				{
					TaskTrace::SubsequentAdded(Prerequisite->TraceId, TraceId);
				}
				if(AlwaysSchedule)
				{
					TaskTrace::Scheduled(TraceId);
				}
			}
			TaskTrace::Finished(OldTraceId);
			TaskTrace::Completed(OldTraceId);
		}
#endif

		inline void Launch(const TCHAR* DebugName, LowLevelTasks::ETaskPriority Priority, LowLevelTasks::EQueuePreference QueuePreference, LowLevelTasks::ETaskFlags Flags)
		{
			coroCheck(Prerequisite == nullptr);
			IncrementRefCount();
			COROTASKTRACE(TaskTrace::Launched(TraceId, DebugName, true, ENamedThreads::Type::AnyThread));
			Task.Init(DebugName, Priority, FTaskExecutor(this), Flags);
			COROTASKTRACE(TaskTrace::Scheduled(TraceId));
			coroVerify(LowLevelTasks::TryLaunch(Task, QueuePreference));
		}

		bool TryExpedite(bool bAllowNested)
		{
			ensureMsgf(bAllowNested || !FCoroLocalState::IsCoroLaunchedTask(), TEXT("Unconnected Expedition within a CoroTask, connect the Callstacks for better Performance."));

			coroCheck(IsExpeditable());
			if (!Task.TryCancel(LowLevelTasks::ECancellationFlags::PrelaunchCancellation)) 
			{
				return CoroutineHandle.done();
			}
			coroCheck(!CoroutineHandle.done());

			//if cancellation succeeded it is safe to read the Prerequisite
			for(;;)
			{
				//try to execute the task if there is no Prerequisite
				if(Prerequisite == nullptr || Prerequisite->IsCompleted())
				{
					Prerequisite = nullptr;
					Execute();
				}

				//try to execute the Prerequisite if it has one
				if(Prerequisite && Prerequisite->TryExpedite(bAllowNested))
				{
					Prerequisite = nullptr;
					continue;
				}

				//if the Coroutine is done we succeeded expediting
				if(CoroutineHandle.done())
				{
					coroCheck(Prerequisite == nullptr);
					return true;
				}

				//try to revive the task (revert the cancellation) it is now not safe anymore to read the Prerequisite
				if(Task.TryRevive())
				{
					return false;
				}

				//when revivial failed the task is already executing on a workerthread and we need to wait until its completed before reusing its memory
				coroCheck(Task.WasCanceled());
				while(!Task.IsCompleted()) 
				{
					FPlatformProcess::Yield();
				}

				//when the task is completed but the Coroutine is not Finished we need to Reschedule another Tasks to finish it
				IncrementRefCount();
				LowLevelTasks::FTask::FInitData InitData = Task.GetInitData();
				Reschedule(InitData.DebugName, InitData.Priority, InitData.Flags);
				return false;
			}
		}

		COROFORCEINLINE void Unlock()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPromise_Unlock);
			coroCheck(Task.IsCompleted());
			coroCheck(!IsExpeditable());
			Prerequisite = nullptr;
			CoroutineHandle();
			coroCheck(Prerequisite == nullptr);
			coroCheck(CoroutineHandle.done());
		}

		COROFORCEINLINE bool InvokeOnceAndLaunch(const TCHAR* DebugName = nullptr, LowLevelTasks::ETaskPriority Priority = LowLevelTasks::ETaskPriority::Inherit, LowLevelTasks::ETaskFlags Flags = LowLevelTasks::ETaskFlags::DefaultFlags)
		{
			COROTASKTRACE(TaskTrace::Launched(TraceId, DebugName, true, ENamedThreads::Type::AnyThread));
			COROTASKTRACE(TaskTrace::Scheduled(TraceId));
			Execute();
			if(!CoroutineHandle.done())
			{
				IncrementRefCount();
				Reschedule(DebugName, Priority, Flags);
				return false;
			}
			return true;
		}

		COROFORCEINLINE void IncrementRefCount()
		{
			coroVerify(RefCount.fetch_add(1, std::memory_order_acquire) <= 2);
		}

		COROFORCEINLINE void DecrementRefCount()
		{
			if (RefCount.fetch_sub(1, std::memory_order_release) == 1)
			{
				COROPROFILERTRACE(coroCheck(TimerScopeDepth == 0));
				coroCheck(Prerequisite == nullptr || !IsExpeditable());
				coroCheck(Task.GetPriority() == LowLevelTasks::ETaskPriority::Count || CoroutineHandle.done());
				coroCheck(TaskTag == ETaskTag::ENone);
				coroCheck(Task.IsCompleted());
				CoroutineHandle.destroy();
			}
		}

	public:
	#if !PLATFORM_EXCEPTIONS_DISABLED || 1
		COROFORCEINLINE void unhandled_exception() {}
	#endif

		COROFORCEINLINE auto initial_suspend() noexcept { return suspend_always(); }
		COROFORCEINLINE auto final_suspend() noexcept { return suspend_always(); }
	};


	template<typename TReturnType>
	class alignas((CORO_ALIGNMENT > alignof(TReturnType)) ? CORO_ALIGNMENT : alignof(TReturnType)) TPromise final : public FPromise
	{
		using BaseType = FPromise;
		using ThisType = TPromise<TReturnType>;
		TReturnType ReturnValue;

	public:
		using ReturnType = TReturnType;

		FORCEINLINE_DEBUGGABLE void* operator new(size_t Size)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::template Malloc<alignof(TPromise<ReturnType>)>(Size);
		}

		FORCEINLINE_DEBUGGABLE void operator delete(void* Ptr)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::Free(Ptr);
		}

		COROFORCEINLINE TPromise() : BaseType(coroutine_handle<ThisType>::from_promise(*this))
		{
		}

		COROFORCEINLINE ReturnType GetResult()
		{
			coroCheck(this->IsCompleted());
			return MoveTemp(ReturnValue);
		}

		template<typename LocalReturnValue>
		COROFORCEINLINE void return_value(LocalReturnValue&& Value)
		{
			ReturnValue = Forward<LocalReturnValue>(Value);
			this->Complete();
		}

		COROFORCEINLINE ThisType* get_return_object() noexcept
		{
			return this;
		}
	};

	template<>
	class alignas(CORO_ALIGNMENT) TPromise<void> final : public FPromise
	{
		using BaseType = FPromise;
		using ThisType = TPromise<void>;

	public:
		using ReturnType = void;

		FORCEINLINE_DEBUGGABLE void* operator new(size_t Size)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::template Malloc<alignof(TPromise<void>)>(Size);
		}

		FORCEINLINE_DEBUGGABLE void operator delete(void* Ptr)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::Free(Ptr);
		}

		COROFORCEINLINE TPromise() : BaseType(coroutine_handle<ThisType>::from_promise(*this))
		{
		}

		COROFORCEINLINE void GetResult()
		{
			coroCheck(this->IsCompleted());
		}

		COROFORCEINLINE void return_void()
		{
			this->Complete();
		}

		COROFORCEINLINE ThisType* get_return_object() noexcept
		{
			return this;
		}
	};


	template<typename ReturnType>
	class alignas((CORO_ALIGNMENT > alignof(ReturnType)) ? CORO_ALIGNMENT : alignof(ReturnType)) TFramePromise final : public IPromise
	{
		ReturnType ReturnValue;

	public:
		FORCEINLINE_DEBUGGABLE void* operator new(size_t Size)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::template Malloc<alignof(TFramePromise<ReturnType>)>(Size);
		}

		FORCEINLINE_DEBUGGABLE void operator delete(void* Ptr)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::Free(Ptr);
		}

		COROFORCEINLINE ReturnType GetResult()
		{
			return MoveTemp(ReturnValue);
		}

		template<typename LocalReturnValue>
		COROFORCEINLINE void return_value(LocalReturnValue&& Value)
		{
			ReturnValue = Forward<LocalReturnValue>(Value);
		}

		COROFORCEINLINE coroutine_handle<TFramePromise<ReturnType>> get_return_object() noexcept
		{
			return coroutine_handle<TFramePromise<ReturnType>>::from_promise(*this);
		}

#if !PLATFORM_EXCEPTIONS_DISABLED || 1
		COROFORCEINLINE void unhandled_exception() {}
#endif
		COROFORCEINLINE auto initial_suspend() noexcept { return suspend_never(); }
		COROFORCEINLINE auto final_suspend() noexcept { return suspend_always(); }
	};

	template<>
	class alignas(CORO_ALIGNMENT) TFramePromise<void> final : public IPromise
	{
	public:
		FORCEINLINE_DEBUGGABLE void* operator new(size_t Size)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::template Malloc<alignof(TFramePromise<void>)>(Size);
		}

		FORCEINLINE_DEBUGGABLE void operator delete(void* Ptr)
		{
			return TConcurrentLinearAllocator<FCoroBlockAllocationTag>::Free(Ptr);
		}

		COROFORCEINLINE void GetResult()
		{
		}

		COROFORCEINLINE void return_void()
		{
		}

		COROFORCEINLINE coroutine_handle<TFramePromise<void>> get_return_object() noexcept
		{
			return coroutine_handle<TFramePromise<void>>::from_promise(*this);
		}

#if !PLATFORM_EXCEPTIONS_DISABLED || 1
		COROFORCEINLINE void unhandled_exception() {}
#endif
		COROFORCEINLINE auto initial_suspend() noexcept { return suspend_never(); }
		COROFORCEINLINE auto final_suspend() noexcept { return suspend_always(); }
	};

	/*
	* FCoroPromise just returns the Promise of the current Coroutine
	*/
	class FCoroPromise
	{
		const IPromise* Promise = nullptr;

	public:
		template<typename PromiseType>
		COROFORCEINLINE bool await_suspend(coroutine_handle<PromiseType> Continuation) noexcept
		{
			Promise = &Continuation.promise();
			return false;
		}

		COROFORCEINLINE bool await_ready() const noexcept
		{
			return false;
		}

		COROFORCEINLINE const IPromise* await_resume() noexcept
		{
			return Promise;
		}
	};
}

/*
* TCoroTask serves as the ReturnType for Coroutine Tasks
*/
template<typename ReturnType>
class [[nodiscard]] TCoroTask
{
	friend class CoroTask_Detail::IPromise;
	friend class CoroTask_Detail::FPromise;
	
	template<typename>
	friend class TCoroFrame;

	friend class FLockedTask;

public:
	using promise_type = CoroTask_Detail::TPromise<ReturnType>;

private:
	using ThisClass = TCoroTask<ReturnType>;

protected:
	promise_type* Promise = nullptr;

public:
	TCoroTask() = default;

	TCoroTask(promise_type* InPromise) : Promise(InPromise)
	{
	}

	~TCoroTask()
	{
		Reset();
	}

	TCoroTask(ThisClass&& Other)
	{
		Promise = MoveTemp(Other.Promise);
		Other.Promise = nullptr;
	}

	ThisClass& operator= (ThisClass&& Other)
	{
		if(this != &Other)
		{
			Reset();
			Promise = MoveTemp(Other.Promise);
			Other.Promise = nullptr;
		}
		return *this;
	}

	TLaunchedCoroTask<ReturnType> Launch(
		const TCHAR* DebugName = nullptr, 
		LowLevelTasks::ETaskPriority Priority = LowLevelTasks::ETaskPriority::Inherit, 
		LowLevelTasks::EQueuePreference QueuePreference = LowLevelTasks::EQueuePreference::DefaultPreference,
		LowLevelTasks::ETaskFlags Flags = LowLevelTasks::ETaskFlags::DefaultFlags) &&;

	//Abandon the Task and freeing its memory if this is the last reference to it.
	inline void Reset()
	{
		if(Promise)
		{
			Promise->DecrementRefCount();
		}
		Promise = nullptr;
	}

	//Does the TaskHandle hold a valid or Dummy Task?
	inline bool IsValid() const
	{
		return Promise != nullptr;
	}

	const CoroTask_Detail::FPromise* GetPromise() const
	{
		return Promise;
	}

	inline bool await_ready() const noexcept
	{
		coroCheck(IsValid());
		return Promise->InvokeOnceAndLaunch();
	}

	template<typename PromiseType>
	inline void await_suspend(coroutine_handle<PromiseType> Continuation) noexcept
	{
		coroCheck(IsValid());
		Continuation.promise().Suspend(Promise);
	}

	inline ReturnType await_resume() noexcept
	{
		ON_SCOPE_EXIT
		{
			Reset();
		};

		coroCheck(IsValid());
		return Promise->GetResult();
	}
};

/*
* FLockedTask is a special use case Task that cannot be expedited
*/
class [[nodiscard]] FLockedTask : private TCoroTask<void>
{
	FLockedTask(promise_type* InPromise) : TCoroTask<void>(InPromise)
	{
		this->Promise->Lock();
	}

public:
	using promise_type = TCoroTask<void>::promise_type;

	FLockedTask() = default;

	FLockedTask(FLockedTask&& Other) : TCoroTask<void>(MoveTemp(Other))
	{
	}

	FLockedTask& operator= (FLockedTask&& Other)
	{
		if(this != &Other)
		{
			this->Reset();
			this->Promise = MoveTemp(Other.Promise);
			Other.Promise = nullptr;
		}
		return *this;
	}

	using TCoroTask<void>::IsValid;
	using TCoroTask<void>::Reset;
	using TCoroTask<void>::GetPromise;

	inline bool HasSubsequent() const
	{
		if(this->Promise)
		{
			return this->Promise->HasSubsequent();
		}
		return false;
	}

	inline void Unlock()
	{
		coroCheck(Promise);
		Promise->Unlock();
		Reset();
	}

	static FLockedTask Create()
	{
		co_return;
	}
};

/*
* TLaunchedCoroTask is the moved result of a Launched TCoroTask this way uniqueness is guranteed
*/
template<typename ReturnType>
class [[nodiscard]] TLaunchedCoroTask : protected TCoroTask<ReturnType>
{
	friend class CoroTask_Detail::IPromise;
	friend class CoroTask_Detail::FPromise;

	friend class TCoroTask<ReturnType>;

	using ThisClass = TLaunchedCoroTask<ReturnType>;

	TLaunchedCoroTask(TCoroTask<ReturnType>&& self) : TCoroTask<ReturnType>(MoveTemp(self))
	{}

public:
	TLaunchedCoroTask() = default;

	TLaunchedCoroTask(ThisClass&& Other) : TCoroTask<ReturnType>(MoveTemp(Other))
	{
	}

	ThisClass& operator= (ThisClass&& Other)
	{
		if(this != &Other)
		{
			this->Reset();
			this->Promise = MoveTemp(Other.Promise);
			Other.Promise = nullptr;
		}
		return *this;
	}

	using TCoroTask<ReturnType>::IsValid;
	using TCoroTask<ReturnType>::Reset;
	using TCoroTask<ReturnType>::GetPromise;

	inline const TCHAR* GetDebugName() const
	{
		if(this->Promise)
		{
			return this->Promise->GetDebugName();
		}
		return TEXT("");
	}

	inline bool await_ready() const noexcept
	{
		coroCheck(this->IsValid());
		return this->Promise->IsCompleted();
	}

	template<typename PromiseType>
	inline void await_suspend(coroutine_handle<PromiseType> Continuation) noexcept
	{
		coroCheck(this->IsValid());
		Continuation.promise().Suspend(this->Promise);
	}

	inline ReturnType await_resume() noexcept
	{
		ON_SCOPE_EXIT
		{
			this->Reset();
		};

		coroCheck(this->IsValid());
		return this->Promise->GetResult();
	}

	inline bool TryExpedite()
	{
		coroCheck(this->IsValid());
		return this->Promise->TryExpedite(true);
	}

	inline ReturnType SpinWait()
	{
		while(!this->Promise->TryExpedite(false))
		{
			FPlatformProcess::Yield();
		}
		return await_resume();
	}
};

template<typename ReturnType>
inline TLaunchedCoroTask<ReturnType> TCoroTask<ReturnType>::Launch(const TCHAR* DebugName, LowLevelTasks::ETaskPriority Priority, LowLevelTasks::EQueuePreference QueuePreference, LowLevelTasks::ETaskFlags Flags) &&
{
	if(Promise)
	{
		Promise->Launch(DebugName, Priority, QueuePreference, Flags);
	}
	return TLaunchedCoroTask<ReturnType>(MoveTemp(*this));
}

/*
* TCoroFrame just serves as storage for a Stackframe and therefore allowing the stackless Coroutines to have a Stack
*/
template<typename ReturnType>
class [[nodiscard]] TCoroFrame
{
public:
	using promise_type = CoroTask_Detail::TFramePromise<ReturnType>;
	coroutine_handle<promise_type> CoroutineHandle;

	// Non-copyable
	TCoroFrame(const TCoroFrame&) = delete;
	TCoroFrame& operator=(const TCoroFrame&) = delete;

	COROFORCEINLINE TCoroFrame(coroutine_handle<promise_type> InCoroutineHandle)
		: CoroutineHandle(InCoroutineHandle)
	{
	}

	/**
	 * Move constructor
	 */
	COROFORCEINLINE TCoroFrame(TCoroFrame&& Other)
		: CoroutineHandle(MoveTemp(Other.CoroutineHandle))
	{
		// Make sure we explicitly set the moved coroutine to null as we might
		// rely on different implementations which might not garantee this behavior.
		Other.CoroutineHandle = nullptr;
	}

	COROFORCEINLINE ~TCoroFrame()
	{
		// Do not try to destroy a coroframe that has been moved.
		if (CoroutineHandle)
		{
			coroCheck(CoroutineHandle.done());
			CoroutineHandle.destroy();
		}
	}

	COROFORCEINLINE auto GetResult()
	{
		promise_type& Promise = CoroutineHandle.promise();
		return Promise.GetResult();
	}

	COROFORCEINLINE bool AsyncIsDone(const CoroTask_Detail::IPromise* ParentPromise)
	{
		coroCheck(ParentPromise != nullptr);
		promise_type& Promise = CoroutineHandle.promise();
		if (Promise.Prerequisite)
		{
			coroCheck(!CoroutineHandle.done());
			ParentPromise->Suspend(Promise.Prerequisite);
			Promise.Prerequisite = nullptr;
		}
		return CoroutineHandle.done();
	}

	COROFORCEINLINE bool SyncIsDone()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CoroFrame_SyncIsDone);
		CoroTask_Detail::IPromise& Promise = CoroutineHandle.promise();
		CoroTask_Detail::FPromise* Prerequisite = Promise.Prerequisite;
		checkf(Prerequisite == nullptr || Prerequisite->IsExpeditable(), TEXT("The Prerequisite(%s) is not synchronously invokable because its execution was delayed by a FCoroEvent"), Prerequisite->GetDebugName())
		while(Prerequisite && !Prerequisite->TryExpedite(false))
		{
			FPlatformProcess::Yield();
		}
		Promise.Prerequisite = nullptr;
		return CoroutineHandle.done();
	}
};

/*
* These Macros are used with TCoroFrame to synchronously call the Coroutine (use with Caution because they are spinning) 
*/
#define SYNC_INVOKE(...)												\
{																		\
	auto coro = __VA_ARGS__;											\
	while(!coro.SyncIsDone())											\
	{																	\
		coro.CoroutineHandle();											\
	}																	\
}

#define SYNC_INVOKE_ASSIGN(value, ...)									\
{																		\
	auto coro = __VA_ARGS__;											\
	while(!coro.SyncIsDone())											\
	{																	\
		coro.CoroutineHandle();											\
	}																	\
	value = coro.GetResult();											\
}

#define SYNC_INVOKE_RETURN(...)											\
{																		\
	auto coro = __VA_ARGS__;											\
	while(!coro.SyncIsDone())											\
	{																	\
		coro.CoroutineHandle();											\
	}																	\
	return coro.GetResult();											\
}

/*
* These Macros are used with TCoroFrame to allow suspension and resumption of the Stack
*/
#define CORO_INVOKE(...)																	\
{																							\
	auto coro = __VA_ARGS__;																\
	const CoroTask_Detail::IPromise* Promise = co_await CoroTask_Detail::FCoroPromise();	\
	while(!coro.AsyncIsDone(Promise))														\
	{																						\
		co_await suspend_always();															\
		coro.CoroutineHandle();																\
	}																						\
}

#define CORO_INVOKE_ASSIGN(value, ...)														\
{																							\
	auto coro = __VA_ARGS__;																\
	const CoroTask_Detail::IPromise* Promise = co_await CoroTask_Detail::FCoroPromise();	\
	while(!coro.AsyncIsDone(Promise))														\
	{																						\
		co_await suspend_always();															\
		coro.CoroutineHandle();																\
	}																						\
	value = coro.GetResult();																\
}

#define CORO_INVOKE_RETURN(...)																\
{																							\
	auto coro = __VA_ARGS__;																\
	const CoroTask_Detail::IPromise* Promise = co_await CoroTask_Detail::FCoroPromise();	\
	while(!coro.AsyncIsDone(Promise))														\
	{																						\
		co_await suspend_always();															\
		coro.CoroutineHandle();																\
	}																						\
	co_return coro.GetResult();																\
}

#else

namespace CoroTask_Detail
{
	class FDummyType
	{
	public:
		bool IsValid() const
		{
			return true;
		}

		void Reset()
		{
		}

		inline const TCHAR* GetDebugName() const
		{
			return TEXT("");
		}

		inline bool TryExpedite()
		{
			return true;
		}

		inline bool Expedite()
		{
			return true;
		}
	};

	template<typename T>
	class TDummyType : public FDummyType
	{
		T Value;
	public:
		inline TDummyType(T&& InValue) : Value(MoveTemp(InValue))
		{
		}

		inline TDummyType(const T& InValue) : Value(InValue)
		{
		}

		inline operator T()
		{
			return MoveTemp(Value);
		}

		TDummyType<T> Launch(
			const TCHAR* DebugName = nullptr,
			LowLevelTasks::ETaskPriority Priority = LowLevelTasks::ETaskPriority::Inherit,
			LowLevelTasks::EQueuePreference QueuePreference = LowLevelTasks::EQueuePreference::DefaultPreference,
			LowLevelTasks::ETaskFlags Flags = LowLevelTasks::ETaskFlags::DefaultFlags) &&
		{
			return TDummyType<T>(MoveTemp(Value));
		}
	};

	template<>
	class TDummyType<void> : public FDummyType
	{
	public:
		TDummyType<void> Launch(
			const TCHAR* DebugName = nullptr,
			LowLevelTasks::ETaskPriority Priority = LowLevelTasks::ETaskPriority::Inherit,
			LowLevelTasks::EQueuePreference QueuePreference = LowLevelTasks::EQueuePreference::DefaultPreference,
			LowLevelTasks::ETaskFlags Flags = LowLevelTasks::ETaskFlags::DefaultFlags) &&
		{
			return TDummyType<void>();
		}
	};

	inline TDummyType<void> MakeDummyType()
	{
		return TDummyType<void>();
	}

	template<typename T>
	inline auto MakeDummyType(T&& Value) -> TDummyType<typename TDecay<T>::Type>
	{
		return TDummyType<typename TDecay<T>::Type>(Forward<T>(Value));
	}
}

#define CORO_FRAME(...) __VA_ARGS__
#define CORO_TASK(...) CoroTask_Detail::TDummyType<__VA_ARGS__>
#define LAUNCHED_TASK(...) CoroTask_Detail::TDummyType<__VA_ARGS__>
#define CO_AWAIT
#define CO_RETURN return
#define CO_RETURN_TASK(...) return CoroTask_Detail::MakeDummyType(__VA_ARGS__)

#define SYNC_INVOKE(...) __VA_ARGS__
#define SYNC_INVOKE_ASSIGN(value, ...) value = __VA_ARGS__
#define SYNC_INVOKE_RETURN(...) return __VA_ARGS__

#define CORO_INVOKE(...) __VA_ARGS__
#define CORO_INVOKE_ASSIGN(value, ...) value = __VA_ARGS__
#define CORO_INVOKE_RETURN(...) return __VA_ARGS__

#endif