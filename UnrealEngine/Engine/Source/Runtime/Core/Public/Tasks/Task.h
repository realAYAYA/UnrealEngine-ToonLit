// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/TaskPrivate.h"
#include "Async/Fundamental/Task.h"
#include "Containers/StaticArray.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "CoreTypes.h"

namespace UE::Tasks
{
	template<typename ResultType>
	class TTask;

	template<typename TaskCollectionType>
	bool Wait(const TaskCollectionType& Tasks, FTimespan InTimeout = FTimespan::MaxValue());

	template<typename TaskCollectionType>
	bool BusyWait(const TaskCollectionType& Tasks, FTimespan InTimeout = FTimespan::MaxValue());

	template<typename TaskType> void AddNested(const TaskType& Nested);

	namespace Private
	{
		template<typename TaskCollectionType>
		TArray<TaskTrace::FId> GetTraceIds(const TaskCollectionType& Tasks);

		// a common part of the generic `TTask<ResultType>` and its `TTask<void>` specialisation
		class FTaskHandle
		{
			// friends to get access to `Pimpl`
			friend FTaskBase;

			template<int32 Index, typename ArrayType, typename FirstTaskType, typename... OtherTasksTypes>
			friend void PrerequisitesUnpacker(ArrayType& Array, FirstTaskType& FirstTask, OtherTasksTypes&... OtherTasks);

			template<int32 Index, typename ArrayType, typename TaskType>
			friend void PrerequisitesUnpacker(ArrayType& Array, TaskType& FirstTask);

			template<typename TaskCollectionType>
			friend bool TryRetractAndExecute(const TaskCollectionType& Tasks, FTimeout Timeout);

			template<typename TaskCollectionType>
			friend TArray<TaskTrace::FId> GetTraceIds(const TaskCollectionType& Tasks);

			template<typename TaskCollectionType>
			friend bool UE::Tasks::Wait(const TaskCollectionType& Tasks, FTimespan InTimeout);

			template<typename TaskCollectionType>
			friend bool UE::Tasks::BusyWait(const TaskCollectionType& Tasks, FTimespan InTimeout);

			template<typename TaskType>
			friend void UE::Tasks::AddNested(const TaskType& Nested);

		protected:
			explicit FTaskHandle(FTaskBase* Other)
				: Pimpl(Other, /*bAddRef = */false)
			{}

		public:
			using FTaskHandleId = void;

			FTaskHandle() = default;

			bool IsValid() const
			{
				return Pimpl.IsValid();
			}

			// checks if task's execution is done
			bool IsCompleted() const
			{
				return !IsValid() || Pimpl->IsCompleted();
			}

			// waits for task's completion with timeout. Tries to retract the task and execute it in-place, if failed - blocks until the task 
			// is completed by another thread. If timeout is zero, tries to retract the task and returns immedially after that.
			// @return true if the task is completed
			bool Wait(FTimespan Timeout = FTimespan::MaxValue()) const
			{
				return !IsValid() || Pimpl->Wait(FTimeout{ Timeout });
			}

			// waits for task's completion for at least the specified amount of time, while executing other tasks.
			// the call can return much later than the given timeout
			// @return true if the task is completed
			bool BusyWait(FTimespan Timeout = FTimespan::MaxValue()) const
			{
				return !IsValid() || Pimpl->BusyWait(FTimeout{ Timeout });
			}

			// waits for task's completion or the given condition becomes true, while executing other tasks.
			// the call can return much later than the given condition became true
			// @return true if the task is completed
			template<typename ConditionType>
			bool BusyWait(ConditionType&& Condition) const
			{
				return !IsValid() || Pimpl->BusyWait(Forward<ConditionType>(Condition));
			}

			// launches a task for asynchronous execution
			// @param DebugName - a unique name for task identification in debugger and profiler, is compiled out in test/shipping builds
			// @param TaskBody - a functor that will be executed asynchronously
			// @param Priority - task priority that affects when the task will be executed
			// @return a trivially relocatable instance that can be used to wait for task completion or to obtain task execution result
			template<typename TaskBodyType>
			void Launch(
				const TCHAR* DebugName,
				TaskBodyType&& TaskBody,
				ETaskPriority Priority = ETaskPriority::Normal,
				EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
			)
			{
				check(!IsValid());

				using FExecutableTask = Private::TExecutableTask<std::decay_t<TaskBodyType>>;
				FExecutableTask* Task = FExecutableTask::Create(DebugName, Forward<TaskBodyType>(TaskBody), Priority, ExtendedPriority);
				// this must happen before launching, to support an ability to access the task itself from inside it
				*Pimpl.GetInitReference() = Task;
				Task->TryLaunch(sizeof(*Task));
			}

			// launches a task for asynchronous execution, with prerequisites that must be completed before the task is scheduled
			// @param DebugName - a unique name for task identification in debugger and profiler, is compiled out in test/shipping builds
			// @param TaskBody - a functor that will be executed asynchronously
			// @param Prerequisites - tasks or task events that must be completed before the task being launched can be scheduled, accepts any 
			// iterable collection (.begin()/.end()), `Tasks::Prerequisites()` helper is recommended to create such collection on the fly
			// @param Priority - task priority that affects when the task will be executed
			// @return a trivially relocatable instance that can be used to wait for task completion or to obtain task execution result
			template<typename TaskBodyType, typename PrerequisitesCollectionType>
			void Launch(
				const TCHAR* DebugName,
				TaskBodyType&& TaskBody,
				PrerequisitesCollectionType&& Prerequisites,
				ETaskPriority Priority = ETaskPriority::Normal,
				EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
			)
			{
				check(!IsValid());

				using FExecutableTask = Private::TExecutableTask<std::decay_t<TaskBodyType>>;
				FExecutableTask* Task = FExecutableTask::Create(DebugName, Forward<TaskBodyType>(TaskBody), Priority, ExtendedPriority);
				Task->AddPrerequisites(Forward<PrerequisitesCollectionType>(Prerequisites));
				// this must happen before launching, to support an ability to access the task itself from inside it
				*Pimpl.GetInitReference() = Task;
				Task->TryLaunch(sizeof(*Task));
			}

			bool IsAwaitable() const
			{
				return IsValid() && Pimpl->IsAwaitable();
			}

			bool operator==(const FTaskHandle& Other) const
			{
				return Pimpl == Other.Pimpl;
			}

			bool operator!=(const FTaskHandle& Other) const
			{
				return Pimpl != Other.Pimpl;
			}

		protected:
			TRefCountPtr<FTaskBase> Pimpl;
		};
	}

	// a movable/copyable handle of `Private::TTaskWithResult*` with the API adopted for public usage.
	// implements Pimpl idiom
	template<typename ResultType>
	class TTask : public Private::FTaskHandle
	{
	public:
		TTask() = default;

		// waits until the task is completed and returns task's result
		ResultType& GetResult()
		{
			check(IsValid());
			FTaskHandle::Wait();
			return static_cast<Private::TTaskWithResult<ResultType>*>(Pimpl.GetReference())->GetResult();
		}

	private:
		friend FPipe;

		// private constructor, valid instances can be created only by launching (see friends)
		explicit TTask(Private::FTaskBase* Other)
			: FTaskHandle(Other)
		{}
	};

	template<>
	class TTask<void> : public Private::FTaskHandle
	{
	public:
		TTask() = default;

		void GetResult()
		{
			check(IsValid()); // to be consistent with a generic `TTask<ResultType>::GetResult()`
			Wait();
		}

	private:
		friend FPipe;

		// private constructor, valid instances can be created only by launching (see friends)
		explicit TTask(Private::FTaskBase* Other)
			: FTaskHandle(Other)
		{}
	};

	// A synchronisation primitive, a recommended substitution of `FEvent` for signalling between tasks. If used as a task prerequisite or 
	// a nested task, it doesn't block a worker thread. Optionally can use "busy waiting" - executing tasks while waiting.
	class FTaskEvent : public Private::FTaskHandle
	{
	public:
		explicit FTaskEvent(const TCHAR* DebugName)
			: Private::FTaskHandle(Private::FTaskEventBase::Create(DebugName))
		{
		}

		// all prerequisites must be added before triggering the event
		template<typename PrerequisitesType>
		void AddPrerequisites(const PrerequisitesType& Prerequisites)
		{
			Pimpl->AddPrerequisites(Prerequisites);
		}

		void Trigger()
		{
			if (!IsCompleted()) // event can be triggered multiple times
			{
				// An event is not "in the system" until it's triggered, and should be kept alive only by external references. Once it's triggered it's in the system 
				// and can outlive external references, so we need to keep it alive by holding an internal reference. It will be released when the event is signalled
				Pimpl->AddRef();
				Pimpl->TryLaunch(sizeof(*Pimpl));
			}
		}
	};

	// launches a task for asynchronous execution
	// @param DebugName - a unique name for task identification in debugger and profiler, is compiled out in test/shipping builds
	// @param TaskBody - a functor that will be executed asynchronously
	// @param Priority - task priority that affects when the task will be executed
	// @return a trivially relocatable instance that can be used to wait for task completion or to obtain task execution result
	template<typename TaskBodyType>
	TTask<TInvokeResult_T<TaskBodyType>> Launch(
		const TCHAR* DebugName,
		TaskBodyType&& TaskBody,
		ETaskPriority Priority = ETaskPriority::Normal,
		EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
	)
	{
		using FResult = TInvokeResult_T<TaskBodyType>;
		TTask<FResult> Task;
		Task.Launch(DebugName, Forward<TaskBodyType>(TaskBody), Priority, ExtendedPriority);
		return Task;
	}

	// launches a task for asynchronous execution, with prerequisites that must be completed before the task is scheduled
	// @param DebugName - a unique name for task identification in debugger and profiler, is compiled out in test/shipping builds
	// @param TaskBody - a functor that will be executed asynchronously
	// @param Prerequisites - tasks or task events that must be completed before the task being launched can be scheduled, accepts any 
	// iterable collection (.begin()/.end()), `Tasks::Prerequisites()` helper is recommended to create such collection on the fly
	// @param Priority - task priority that affects when the task will be executed
	// @return a trivially relocatable instance that can be used to wait for task completion or to obtain task execution result
	template<typename TaskBodyType, typename PrerequisitesCollectionType>
	TTask<TInvokeResult_T<TaskBodyType>> Launch(
		const TCHAR* DebugName,
		TaskBodyType&& TaskBody,
		PrerequisitesCollectionType&& Prerequisites,
		ETaskPriority Priority = ETaskPriority::Normal,
		EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
	)
	{
		using FResult = TInvokeResult_T<TaskBodyType>;
		TTask<FResult> Task;
		Task.Launch(DebugName, Forward<TaskBodyType>(TaskBody), Forward<PrerequisitesCollectionType>(Prerequisites), Priority, ExtendedPriority);
		return Task;
	}

	namespace Private
	{
		template<typename TaskCollectionType>
		TArray<TaskTrace::FId> GetTraceIds(const TaskCollectionType& Tasks)
		{
#if UE_TASK_TRACE_ENABLED
			TArray<TaskTrace::FId> TasksIds;
			TasksIds.Reserve(Tasks.Num());

			for (auto& Task : Tasks)
			{
				if (Task.IsValid())
				{
					TasksIds.Add(Task.Pimpl->GetTraceId());
				}
			}

			return TasksIds;
#else
			return {};
#endif
		}
	}

	inline void Wait(Private::FTaskHandle& Task)
	{
		Task.Wait();
	}

	// wait for multiple tasks, with timeout
	// @param TaskCollectionType - an iterable collection of `TTask<T>`, e.g. `TArray<FTask>`
	template<typename TaskCollectionType>
	bool Wait(const TaskCollectionType& Tasks, FTimespan InTimeout/* = FTimespan::MaxValue()*/)
	{
		TaskTrace::FWaitingScope WaitingScope(Private::GetTraceIds(Tasks));
		TRACE_CPUPROFILER_EVENT_SCOPE(Tasks::Wait);

		FTimeout Timeout{ InTimeout };

		// ignore the result as we still have to make sure the task is completed upon returning from this function call
		Private::TryRetractAndExecute(Tasks, Timeout);

		// the event must be alive for the task and this function lifetime, we don't know which one will be finished first as waiting can time out
		// before the waiting task is completed
		FSharedEventRef CompletionEvent;
		auto WaitingTaskBody = [CompletionEvent] { CompletionEvent->Trigger(); };
		using FWaitingTask = Private::TExecutableTask<decltype(WaitingTaskBody)>;

		TRefCountPtr<FWaitingTask> WaitingTask{ FWaitingTask::Create(TEXT("Waiting Task"), MoveTemp(WaitingTaskBody), ETaskPriority::Default /* doesn't matter */, EExtendedTaskPriority::Inline), /*bAddRef=*/ false};
		WaitingTask->AddPrerequisites(Tasks);

		if (WaitingTask->TryLaunch(sizeof(WaitingTask)))
		{	// was executed inline
			check(WaitingTask->IsCompleted());
			return true;
		}

		return CompletionEvent->Wait(Timeout.GetRemainingRoundedUpMilliseconds());
	}

	// wait for multiple tasks while executing other tasks
	template<typename TaskCollectionType>
	bool BusyWait(const TaskCollectionType& Tasks, FTimespan InTimeout/* = FTimespan::MaxValue()*/)
	{
		TaskTrace::FWaitingScope WaitingScope(Private::GetTraceIds(Tasks));
		TRACE_CPUPROFILER_EVENT_SCOPE(Tasks::BusyWait);

		FTimeout Timeout{ InTimeout };

		// ignore the result as we still have to make sure the task is completed upon returning from this function call
		Private::TryRetractAndExecute(Tasks, Timeout);

		for (auto& Task : Tasks)
		{
			if (Timeout || !Task.BusyWait(Timeout))
			{
				return false;
			}
		}

		return true;
	}

	using FTask = Private::FTaskHandle;

	namespace Private
	{
		template<int32 Index, typename ArrayType, typename FirstTaskType, typename... OtherTasksTypes>
		void PrerequisitesUnpacker(ArrayType& Array, FirstTaskType& FirstTask, OtherTasksTypes&... OtherTasks)
		{
			Array[Index] = FirstTask.Pimpl.GetReference();
			PrerequisitesUnpacker<Index + 1>(Array, OtherTasks...);
		}

		template<int32 Index, typename ArrayType, typename TaskType>
		void PrerequisitesUnpacker(ArrayType& Array, TaskType& Task)
		{
			Array[Index] = Task.Pimpl.GetReference();
		}
	}

	template<typename... TaskTypes, 
		typename std::decay_t<decltype(std::declval<TTuple<TaskTypes...>>().template Get<0>())>::FTaskHandleId* = nullptr>
	TStaticArray<Private::FTaskBase*, sizeof...(TaskTypes)> Prerequisites(TaskTypes&... Tasks)
	{
		TStaticArray<Private::FTaskBase*, sizeof...(TaskTypes)> Res;
		Private::PrerequisitesUnpacker<0>(Res, Tasks...);
		return Res;
	}

	template<typename TaskCollectionType>
	const TaskCollectionType& Prerequisites(const TaskCollectionType& Tasks)
	{
		return Tasks;
	}

	// Adds the nested task to the task that is being currently executed by the current thread. A parent task is not flagged completed
	// until all nested tasks are completed. It's similar to explicitly waiting for a sub-task at the end of its parent task, except explicit waiting
	// blocks the worker executing the parent task until the sub-task is completed. With nested tasks, the worker won't be blocked.
	template<typename TaskType>
	void AddNested(const TaskType& Nested)
	{
		Private::FTaskBase* Parent = Private::GetCurrentTask();
		check(Parent != nullptr);
		Parent->AddNested(*Nested.Pimpl);
	}

	// Console variable for configuring task priorities
	// Example:
	//		FTaskPriorityCVar CVar{ TEXT("CVarName"), TEXT("CVarHelp"), ETaskPriority::Normal, EExtendedTaskPriority::None };
	//		Launch(UE_SOURCE_LOCATION, [] {}, CVar.GetTaskPriority(), CVar.GetExtendedTaskPriority()).Wait();
	class FTaskPriorityCVar
	{
	public:
		FTaskPriorityCVar(const TCHAR* Name, const TCHAR* Help, ETaskPriority DefaultPriority, EExtendedTaskPriority DefaultExtendedPriority)
			: RawSetting(ConfigStringFromPriorities(DefaultPriority, DefaultExtendedPriority))
			, FullHelpText(CreateFullHelpText(Name, Help))
			, Variable(Name, RawSetting, *FullHelpText, FConsoleVariableDelegate::CreateRaw(this, &FTaskPriorityCVar::OnSettingChanged), ECVF_Default)
			, Priority(DefaultPriority)
			, ExtendedPriority(DefaultExtendedPriority)
		{
		}

		ETaskPriority GetTaskPriority() const
		{
			return Priority;
		}

		EExtendedTaskPriority GetExtendedTaskPriority() const
		{
			return ExtendedPriority;
		}

	private:
		static FString CreateFullHelpText(const TCHAR* Name, const TCHAR* OriginalHelp);
		static FString ConfigStringFromPriorities(ETaskPriority InPriority, EExtendedTaskPriority InExtendedPriority);
		void OnSettingChanged(IConsoleVariable* Variable);

	private:
		FString RawSetting;
		FString FullHelpText;
		FAutoConsoleVariableRef Variable;
		ETaskPriority Priority;
		EExtendedTaskPriority ExtendedPriority;
	};

	// creates and returns an already completed task holding a result value constructed from given args
	template<typename ResultType, typename... ArgTypes>
	TTask<ResultType> MakeCompletedTask(ArgTypes&&... Args)
	{
		return Launch(
			UE_SOURCE_LOCATION,
			[&] { return ResultType(Forward<ArgTypes>(Args)...); },
			ETaskPriority::Default, // doesn't matter
			EExtendedTaskPriority::Inline);
	}
}
