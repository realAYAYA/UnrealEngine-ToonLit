// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/TaskPrivate.h"
#include "Tasks/Pipe.h"

#include "Async/TaskGraphInterfaces.h"

namespace UE::Tasks
{
	namespace Private
	{
		FExecutableTaskAllocator SmallTaskAllocator;
		FTaskEventBaseAllocator TaskEventBaseAllocator;

		void FTaskBase::Schedule(bool& bWakeUpWorker)
		{
			TaskTrace::Scheduled(GetTraceId());

#if TASKGRAPH_NEW_FRONTEND
			if (IsNamedThreadTask())
			{
				ENamedThreads::Type ConversionMap[] =
				{
					ENamedThreads::GameThread,
					(ENamedThreads::Type)(ENamedThreads::GameThread | ENamedThreads::HighTaskPriority),
					(ENamedThreads::Type)(ENamedThreads::GameThread | ENamedThreads::LocalQueue),
					(ENamedThreads::Type)(ENamedThreads::GameThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue),

					ENamedThreads::GetRenderThread(),
					(ENamedThreads::Type)(ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority),
					(ENamedThreads::Type)(ENamedThreads::GetRenderThread() | ENamedThreads::LocalQueue),
					(ENamedThreads::Type)(ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue),

					ENamedThreads::RHIThread,
					(ENamedThreads::Type)(ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority),
					(ENamedThreads::Type)(ENamedThreads::RHIThread | ENamedThreads::LocalQueue),
					(ENamedThreads::Type)(ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue)
				};

				FTaskGraphInterface::Get().QueueTask(static_cast<FBaseGraphTask*>(this), true, ConversionMap[(int32)ExtendedPriority - (int32)EExtendedTaskPriority::GameThreadNormalPri]);
				return;
			}
#endif

			bWakeUpWorker |= LowLevelTasks::FSchedulerTls::IsBusyWaiting();
			bWakeUpWorker |= LowLevelTasks::FScheduler::Get().TryLaunch(LowLevelTask, bWakeUpWorker ? LowLevelTasks::EQueuePreference::GlobalQueuePreference : LowLevelTasks::EQueuePreference::LocalQueuePreference, bWakeUpWorker);
		}

		thread_local uint32 TaskRetractionRecursion = 0;

		bool IsThreadRetractingTask()
		{
			return TaskRetractionRecursion != 0;
		}

		struct FThreadLocalRetractionScope
		{
			FThreadLocalRetractionScope()
			{
				checkSlow(TaskRetractionRecursion != TNumericLimits<decltype(TaskRetractionRecursion)>::Max() - 1);
				++TaskRetractionRecursion;
			}

			~FThreadLocalRetractionScope()
			{
				checkSlow(TaskRetractionRecursion != 0);
				--TaskRetractionRecursion;
			}
		};

		bool FTaskBase::TryRetractAndExecute(FTimeout Timeout, uint32 RecursionDepth/* = 0*/)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FTaskBase::TryRetractAndExecute);

			if (!IsAwaitable())
			{
				UE_LOG(LogTemp, Fatal, TEXT("Deadlock detected! A task can't be waited here, e.g. because it's being executed by the current thread"));
				return false;
			}

			if (IsCompleted() || Timeout)
			{
				return IsCompleted();
			}

#if TASKGRAPH_NEW_FRONTEND
			// task retraction is not supported for named thread tasks
			if (IsNamedThreadTask())
			{
				return false;
			}
#endif

			// avoid stack overflow. is not expected in a real-life cases but happens in stress tests
			if (RecursionDepth == 200)
			{
				return false;
			}
			++RecursionDepth;

			// returns false if the task has passed "pre-scheduling" state: all (if any) prerequisites are completed
			auto IsLockedByPrerequisites = [this]
			{
				uint32 LocalNumLocks = NumLocks.load(std::memory_order_relaxed); // the order doesn't matter as this "happens before" task execution
				return LocalNumLocks != 0 && LocalNumLocks < ExecutionFlag;
			};

			if (IsLockedByPrerequisites())
			{
				// try to unlock the task. even if (some or all) prerequisites retraction fails we still proceed to try helping with other prerequisites or this task execution

				// prerequisites are "consumed" here even if their retraction fails. this means that once prerequisite retraction failed, it won't be performed again. 
				// this can be potentially improved by using a different container for prerequisites
				for (FTaskBase* Prerequisite : Prerequisites.PopAll())
				{
					// ignore if retraction failed, as this thread still can try to help with other prerequisites instead of being blocked in waiting
					Prerequisite->TryRetractAndExecute(Timeout, RecursionDepth);
					Prerequisite->Release();
				}
			}

			// If we don't have any more prerequisites, let TryUnlock
			// execute these to avoid any race condition where we could clear
			// the last reference before TryUnlock finishes and cause a use-after-free.
			// These are super fast to process anyway so we can just consider them done
			// for retraction purpose.
			if (ExtendedPriority == EExtendedTaskPriority::TaskEvent ||
				ExtendedPriority == EExtendedTaskPriority::Inline)
			{
				return true;
			}

			if (Timeout)
			{
				return IsCompleted();
			}

			{
				FThreadLocalRetractionScope ThreadLocalRetractionScope;

				// next we try to execute the task, despite we haven't verified that the task is unlocked. trying to obtain execution permission will fail in this case

				if (!TryExecuteTask())
				{
					return false; // still locked by prerequisites, or another thread managed to set execution flag first, or we're inside this task execution
					// we could try to help with nested tasks execution (the task execution could already spawned a couple of nested tasks sitting in the queue). 
					// it's unclear how important this is, but this would definitely lead to more complicated impl. we can revisit this once we see such instances in profiler captures
				}
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(SuccessfulTaskRetraction);

			// the task was launched so the scheduler will handle the internal reference held by low-level task

			// retract nested tasks, if any
			{
				// keep trying retracting all nested tasks even if some of them fail, so the current worker can contribute instead of being blocked
				bool bSucceeded = true;
				// prerequisites are "consumed" here even if their retraction fails. this means that once prerequisite retraction failed, it won't be performed again. 
				// this can be potentially improved by using a different container for prerequisites
				for (FTaskBase* Prerequisite : Prerequisites.PopAll())
				{
					if (!Prerequisite->TryRetractAndExecute(Timeout, RecursionDepth))
					{
						bSucceeded = false;
					}
					Prerequisite->Release();
				}

				if (!bSucceeded)
				{
					return false;
				}
			}

			// at this point the task is executed and has no pending nested tasks, but still can be "not completed" (nested tasks can be 
			// in the process of completing it (setting the flag) concurrently), so the caller still has to wait for completion
			return true;
		}

		bool FTaskBase::Wait(FTimeout Timeout)
		{
			if (IsCompleted() || Timeout)
			{
				return IsCompleted();
			}

			TaskTrace::FWaitingScope WaitingScope(GetTraceId());
			TRACE_CPUPROFILER_EVENT_SCOPE(Tasks::Wait);

			return WaitImpl(Timeout);
		}

		void FTaskBase::WaitWithNamedThreadsSupport()
		{
			if (IsCompleted())
			{
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(FTaskBase::WaitWithNamedThreadsSupport);
			TaskTrace::FWaitingScope WaitingScope(GetTraceId());

			if (!TryWaitOnNamedThread(*this))
			{
				WaitImpl(FTimeout::Never());
			}
		}

		bool FTaskBase::WaitImpl(FTimeout Timeout)
		{
			// ignore the result as we still have to make sure the task is completed upon returning from this function call
			TryRetractAndExecute(Timeout);

			// spin for a while with hope the task is getting completed right now, to avoid getting blocked by a pricy syscall
			const uint32 MaxSpinCount = 40;
			for (uint32 SpinCount = 0; SpinCount != MaxSpinCount && !IsCompleted() && !Timeout; ++SpinCount)
			{
				FPlatformProcess::Yield(); // YieldThread() was much slower on some platforms with low core count and contention for CPU
			}

			if (IsCompleted() || Timeout)
			{
				return IsCompleted();
			}

			// the event must be alive for the task and this function lifetime, we don't know which one will be finished first as waiting can 
			// time out before the waiting task is completed
			FSharedEventRef CompletionEvent;
			auto WaitingTaskBody = [CompletionEvent] { CompletionEvent->Trigger(); };
			using FWaitingTask = TExecutableTask<decltype(WaitingTaskBody)>;

			TRefCountPtr<FWaitingTask> WaitingTask{ FWaitingTask::Create(TEXT("Waiting Task"), MoveTemp(WaitingTaskBody), ETaskPriority::Default /* doesn't matter*/, EExtendedTaskPriority::Inline, ETaskFlags::None), /*bAddRef=*/ false };
			WaitingTask->AddPrerequisites(*this);

			if (WaitingTask->TryLaunch(sizeof(WaitingTask)))
			{	// was executed inline
				check(WaitingTask->IsCompleted());
				return true;
			}

			return CompletionEvent->Wait(Timeout.GetRemainingRoundedUpMilliseconds());
		}

		FTaskBase* FTaskBase::TryPushIntoPipe()
		{
			return GetPipe()->PushIntoPipe(*this);
		}

		void FTaskBase::StartPipeExecution()
		{
			GetPipe()->ExecutionStarted();
		}

		void FTaskBase::FinishPipeExecution()
		{
			GetPipe()->ExecutionFinished();
		}

		void FTaskBase::ClearPipe()
		{
			GetPipe()->TryClearTask(*this);
		}

		static thread_local FTaskBase* CurrentTask = nullptr;

		FTaskBase* GetCurrentTask()
		{
			return CurrentTask;
		}

		FTaskBase* ExchangeCurrentTask(FTaskBase* Task)
		{
			FTaskBase* PrevTask = CurrentTask;
			CurrentTask = Task;
			return PrevTask;
		}

		bool TryWaitOnNamedThread(FTaskBase& Task)
		{
#if TASKGRAPH_NEW_FRONTEND
			// handle waiting only on a named thread and if not called from inside a task
			FTaskGraphInterface& TaskGraph = FTaskGraphInterface::Get();
			ENamedThreads::Type CurrentThread = TaskGraph.GetCurrentThreadIfKnown();
			if (CurrentThread < ENamedThreads::ActualRenderingThread /* is a named thread? */ && !TaskGraph.IsThreadProcessingTasks(CurrentThread))
			{
				// execute other tasks of this named thread while waiting
				ETaskPriority Dummy;
				EExtendedTaskPriority ExtendedPriority;
				TranslatePriority(CurrentThread, Dummy, ExtendedPriority);

				auto TaskBody = [CurrentThread, &TaskGraph] { TaskGraph.RequestReturn(CurrentThread); };
				using FReturnFromNamedThreadTask = TExecutableTask<decltype(TaskBody)>;
				FReturnFromNamedThreadTask ReturnTask { TEXT("ReturnFromNamedThreadTask"), MoveTemp(TaskBody), ETaskPriority::High, ExtendedPriority, ETaskFlags::None };
				ReturnTask.AddPrerequisites(Task);
				ReturnTask.TryLaunch(sizeof(ReturnTask)); // the result doesn't matter

				TaskGraph.ProcessThreadUntilRequestReturn(CurrentThread);
				check(Task.IsCompleted());
				return true;
			}
#endif

			return false;
		}
	}

	const TCHAR* ToString(EExtendedTaskPriority ExtendedPriority)
	{
		if (ExtendedPriority < EExtendedTaskPriority::None || ExtendedPriority >= EExtendedTaskPriority::Count)
		{
			return nullptr;
		}

		const TCHAR* ExtendedTaskPriorityToStr[] =
		{
			TEXT("None"),
			TEXT("Inline"),
			TEXT("TaskEvent"),

#if TASKGRAPH_NEW_FRONTEND
			TEXT("GameThreadNormalPri"),
			TEXT("GameThreadHiPri"),
			TEXT("GameThreadNormalPriLocalQueue"),
			TEXT("GameThreadHiPriLocalQueue"),

			TEXT("RenderThreadNormalPri"),
			TEXT("RenderThreadHiPri"),
			TEXT("RenderThreadNormalPriLocalQueue"),
			TEXT("RenderThreadHiPriLocalQueue"),

			TEXT("RHIThreadNormalPri"),
			TEXT("RHIThreadHiPri"),
			TEXT("RHIThreadNormalPriLocalQueue"),
			TEXT("RHIThreadHiPriLocalQueue")
#endif
		};
		return ExtendedTaskPriorityToStr[(int32)ExtendedPriority];
	}

	bool ToExtendedTaskPriority(const TCHAR* ExtendedPriorityStr, EExtendedTaskPriority& OutExtendedPriority)
	{
#define CONVERT_EXTENDED_TASK_PRIORITY(ExtendedTaskPriority)\
		if (FCString::Stricmp(ExtendedPriorityStr, ToString(EExtendedTaskPriority::ExtendedTaskPriority)) == 0)\
		{\
			OutExtendedPriority = EExtendedTaskPriority::ExtendedTaskPriority;\
			return true;\
		}

		CONVERT_EXTENDED_TASK_PRIORITY(None);
		CONVERT_EXTENDED_TASK_PRIORITY(Inline);
		CONVERT_EXTENDED_TASK_PRIORITY(TaskEvent);

#if TASKGRAPH_NEW_FRONTEND
		CONVERT_EXTENDED_TASK_PRIORITY(GameThreadNormalPri);
		CONVERT_EXTENDED_TASK_PRIORITY(GameThreadHiPri);
		CONVERT_EXTENDED_TASK_PRIORITY(GameThreadNormalPriLocalQueue);
		CONVERT_EXTENDED_TASK_PRIORITY(GameThreadHiPriLocalQueue);

		CONVERT_EXTENDED_TASK_PRIORITY(RenderThreadNormalPri);
		CONVERT_EXTENDED_TASK_PRIORITY(RenderThreadHiPri);
		CONVERT_EXTENDED_TASK_PRIORITY(RenderThreadNormalPriLocalQueue);
		CONVERT_EXTENDED_TASK_PRIORITY(RenderThreadHiPriLocalQueue);

		CONVERT_EXTENDED_TASK_PRIORITY(RHIThreadNormalPri);
		CONVERT_EXTENDED_TASK_PRIORITY(RHIThreadHiPri);
		CONVERT_EXTENDED_TASK_PRIORITY(RHIThreadNormalPriLocalQueue);
		CONVERT_EXTENDED_TASK_PRIORITY(RHIThreadHiPriLocalQueue);
#endif

#undef CONVERT_EXTENDED_TASK_PRIORITY

		return false;
	}

	FString FTaskPriorityCVar::CreateFullHelpText(const TCHAR* Name, const TCHAR* OriginalHelp)
	{
		TStringBuilder<1024> TaskPriorities;
		for (int i = 0; i != (int)ETaskPriority::Count; ++i)
		{
			TaskPriorities.Append(ToString((ETaskPriority)i));
			TaskPriorities.Append(TEXT(", "));
		}
		TaskPriorities.RemoveSuffix(2); // remove the last ", "

		TStringBuilder<1024> ExtendedTaskPriorities;
		for (int i = 0; i != (int)EExtendedTaskPriority::Count; ++i)
		{
			ExtendedTaskPriorities.Append(ToString((EExtendedTaskPriority)i));
			ExtendedTaskPriorities.Append(TEXT(", "));
		}
		ExtendedTaskPriorities.RemoveSuffix(2); // remove the last ", "

		return FString::Printf(
			TEXT("%s\n")
			TEXT("Arguments are task priority and extended task priority (optional) separated by a space: [TaskPriority] [ExtendedTaskPriority]\n")
			TEXT("where TaskPriority is in [%s]\n")
			TEXT("and ExtendedTaskPriority is in [%s].\n")
			TEXT("Example: \"%s %s %s\" or \"%s\"")
			, OriginalHelp, *TaskPriorities, *ExtendedTaskPriorities, Name, ToString((ETaskPriority)0), ToString((EExtendedTaskPriority)0), ToString((ETaskPriority)0));
	}

	FString FTaskPriorityCVar::ConfigStringFromPriorities(ETaskPriority InPriority, EExtendedTaskPriority InExtendedPriority)
	{
		return FString{ ToString(InPriority) } + TEXT(" ") + ToString(InExtendedPriority);
	}

	void FTaskPriorityCVar::OnSettingChanged(IConsoleVariable* InVariable)
	{
		FString PriorityStr, ExtendedPriorityStr;
		static FString Delimiter{ " " };
		if (RawSetting.Split(Delimiter, &PriorityStr, &ExtendedPriorityStr))
		{
			verify(ToTaskPriority(*PriorityStr, Priority));
			verify(ToExtendedTaskPriority(*ExtendedPriorityStr, ExtendedPriority));
		}
		else
		{
			verify(ToTaskPriority(*RawSetting, Priority));
			ExtendedPriority = EExtendedTaskPriority::None;
		}
	}

	namespace Private
	{
		// task priority translation from the old API to the new API
		void TranslatePriority(ENamedThreads::Type ThreadType, ETaskPriority& OutPriority, EExtendedTaskPriority& OutExtendedPriority)
		{
			using namespace UE::Tasks;

#if TASKGRAPH_NEW_FRONTEND

			ENamedThreads::Type ThreadIndex = ENamedThreads::GetThreadIndex(ThreadType);
			if (ThreadIndex != ENamedThreads::AnyThread)
			{
				check(ThreadIndex == ENamedThreads::GameThread || ThreadIndex == ENamedThreads::GetRenderThread() || ThreadIndex == ENamedThreads::RHIThread);
				EExtendedTaskPriority ConversionMap[] =
				{
					EExtendedTaskPriority::RHIThreadNormalPri,
					EExtendedTaskPriority::GameThreadNormalPri,
					EExtendedTaskPriority::RenderThreadNormalPri
				};
				OutExtendedPriority = ConversionMap[ThreadIndex - ENamedThreads::RHIThread];
				OutExtendedPriority = (EExtendedTaskPriority)((int32)OutExtendedPriority + (ENamedThreads::GetTaskPriority(ThreadType) != ENamedThreads::NormalTaskPriority ? 1 : 0));
				OutExtendedPriority = (EExtendedTaskPriority)((int32)OutExtendedPriority + (ENamedThreads::GetQueueIndex(ThreadType) != ENamedThreads::MainQueue ? 2 : 0));
				OutPriority = ETaskPriority::Count;
			}
			else
			{
				OutExtendedPriority = EExtendedTaskPriority::None;
				uint32 ThreadPriority = GetThreadPriorityIndex(ThreadType);
				check(ThreadPriority < uint32(ENamedThreads::NumThreadPriorities));
				ETaskPriority ConversionMap[int(ENamedThreads::NumThreadPriorities)] = { ETaskPriority::Normal, ETaskPriority::High, ETaskPriority::BackgroundNormal };
				OutPriority = ConversionMap[ThreadPriority];
			}

			if (OutPriority == ETaskPriority::BackgroundNormal && GetTaskPriority(ThreadType))
			{
				OutPriority = ETaskPriority::BackgroundHigh;
			}

#else
			uint32 ThreadPriority = GetThreadPriorityIndex(ThreadType);
			check(ThreadPriority < uint32(ENamedThreads::NumThreadPriorities));
			ETaskPriority Conversion[int(ENamedThreads::NumThreadPriorities)] = { ETaskPriority::Normal, ETaskPriority::High, ETaskPriority::BackgroundNormal };
			OutPriority = Conversion[ThreadPriority];

			if (OutPriority == ETaskPriority::BackgroundNormal && GetTaskPriority(ThreadType))
			{
				OutPriority = ETaskPriority::BackgroundHigh;
			}
#endif
		}

		ENamedThreads::Type TranslatePriority(ETaskPriority Priority)
		{
			checkSlow(Priority < ETaskPriority::Count);

			ENamedThreads::Type ConversionMap[] =
			{
				ENamedThreads::AnyHiPriThreadNormalTask,
				ENamedThreads::AnyNormalThreadNormalTask,
				ENamedThreads::AnyBackgroundHiPriTask,
				ENamedThreads::AnyBackgroundThreadNormalTask,
				ENamedThreads::AnyBackgroundThreadNormalTask // same as above
			};

			return ConversionMap[(int32)Priority];
		}

#if !TASKGRAPH_NEW_FRONTEND

		ENamedThreads::Type TranslatePriority(ETaskPriority Priority, EExtendedTaskPriority /*ExtendedPriority*/)
		{
			checkSlow(Priority <= ETaskPriority::Count);
			return TranslatePriority(Priority);
		}

#else // !TASKGRAPH_NEW_FRONTEND

		// task priority translation from the new API to the old API
		ENamedThreads::Type TranslatePriority(EExtendedTaskPriority Priority)
		{
			checkf(Priority >= EExtendedTaskPriority::GameThreadNormalPri && Priority < EExtendedTaskPriority::Count, TEXT("only named threads can call this method: %d"), Priority);

			int32 ConversionMap[] =
			{
					ENamedThreads::GameThread, // GameThreadNormalPri
					ENamedThreads::GameThread | ENamedThreads::HighTaskPriority, // GameThreadHiPri
					ENamedThreads::GameThread | ENamedThreads::LocalQueue, // GameThreadNormalPriLocalQueue
					ENamedThreads::GameThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue, // GameThreadHiPriLocalQueue

					ENamedThreads::GetRenderThread(), // RenderThreadNormalPri
					ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority, // RenderThreadHiPri
					ENamedThreads::GetRenderThread() | ENamedThreads::LocalQueue, // RenderThreadNormalPriLocalQueue
					ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue, // RenderThreadHiPriLocalQueue

					ENamedThreads::RHIThread, // RHIThreadNormalPri
					ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority, // RHIThreadHiPri
					ENamedThreads::RHIThread | ENamedThreads::LocalQueue, // RHIThreadNormalPriLocalQueue
					ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue // RHIThreadHiPriLocalQueue
			};

			return (ENamedThreads::Type)ConversionMap[(int32)Priority - (int32)EExtendedTaskPriority::GameThreadNormalPri];
		}

		ENamedThreads::Type TranslatePriority(ETaskPriority Priority, EExtendedTaskPriority ExtendedPriority)
		{
			checkSlow(Priority <= ETaskPriority::Count);
			checkSlow(ExtendedPriority <= EExtendedTaskPriority::Count);

			return ExtendedPriority < EExtendedTaskPriority::GameThreadNormalPri || ExtendedPriority == EExtendedTaskPriority::Count ? TranslatePriority(Priority) : TranslatePriority(ExtendedPriority);
		}

#endif // !TASKGRAPH_NEW_FRONTEND
	}
}
