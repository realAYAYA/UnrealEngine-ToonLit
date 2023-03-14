// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Fundamental/Scheduler.h"
#include "Async/Fundamental/Task.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Tasks/Task.h"
#include "Tasks/TaskPrivate.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTemplate.h"

#include <atomic>
#include <type_traits>

namespace UE::Tasks
{
	// A chain of tasks that are executed one after another. Can be used to synchronise access to a shared resource as FPipe guarantees 
	// non-concurrent tasks execution. FPipe is a replacement for named threads because it's lightweight and flexible -
	// there can be a large dynamic number of pipes each controlling its own shared resource. Can be used as a replacement for
	// dedicated threads.
	// Execution order is not specified, only that tasks from the same pipe are not executed concurrently.
	// A pipe must be alive until its last task is completed.
	// See `FTasksPipeTest` for tests and examples.
	class FPipe
	{
	public:
		UE_NONCOPYABLE(FPipe);

		// @param InDebugName helps to identify the pipe in debugger and profiler. `UE_SOURCE_LOCATION` can be used as an auto-generated
		// unique name.
		explicit FPipe(const TCHAR* InDebugName)
			: DebugName(InDebugName)
		{}

		~FPipe()
		{
			check(!HasWork());
		}

		// returns `true` if the pipe has any not completed tasks
		bool HasWork() const
		{
			return LastTask.load(std::memory_order_relaxed) != nullptr;
		}

		// waits until the pipe is empty (its last task is executed)
		// should be used only after no more tasks are launched in the pipe, e.g. preparing for the pipe destruction
		void WaitUntilEmpty()
		{
			LowLevelTasks::FScheduler::Get().BusyWaitUntil([this] { return !HasWork(); });
		}

		// launches a task in the pipe
		// @param InDebugName helps to identify the task in debugger and profiler
		// @param TaskBody a callable with no parameters, usually a lambda but can be also a functor object 
		// or a pointer to a function. TaskBody can return results.
		// @Priority - task priority, can affect task scheduling once it's passed the pipe
		// @return Task instance that can be used to wait for task completion or to obtain the result of task execution
		template<typename TaskBodyType>
		TTask<TInvokeResult_T<TaskBodyType>> Launch
		(
			const TCHAR* InDebugName, 
			TaskBodyType&& TaskBody, 
			ETaskPriority Priority = ETaskPriority::Default,
			EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
		)
		{
			using FResult = TInvokeResult_T<TaskBodyType>;
			using FExecutableTask = Private::TExecutableTask<std::decay_t<TaskBodyType>>;

			FExecutableTask* Task = FExecutableTask::Create(InDebugName, Forward<TaskBodyType>(TaskBody), Priority, ExtendedPriority);
			Task->SetPipe(*this);
			Task->TryLaunch();
			return TTask<FResult>{ Task };
		}

		// launches a task in the pipe, with multiple prerequisites that must be completed before the task is scheduled
		// @param InDebugName helps to identify the task in debugger and profiler
		// @param TaskBody a callable with no parameters, usually a lambda but can be also a functor object 
		// or a pointer to a function. TaskBody can return results.
		// @Priority - task priority, can affect task scheduling once it's passed the pipe
		// @return Task instance that can be used to wait for task completion or to obtain the result of task execution
		template<typename TaskBodyType, typename PrerequisitesCollectionType>
		TTask<TInvokeResult_T<TaskBodyType>> Launch
		(
			const TCHAR* InDebugName, 
			TaskBodyType&& TaskBody, 
			PrerequisitesCollectionType&& Prerequisites, 
			ETaskPriority Priority = ETaskPriority::Default,
			EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None
		)
		{
			using FResult = TInvokeResult_T<TaskBodyType>;
			using FExecutableTask = Private::TExecutableTask<std::decay_t<TaskBodyType>>;

			FExecutableTask* Task = FExecutableTask::Create(InDebugName, Forward<TaskBodyType>(TaskBody), Priority, ExtendedPriority);
			Task->SetPipe(*this);
			Task->AddPrerequisites(Forward<PrerequisitesCollectionType>(Prerequisites));
			Task->TryLaunch();
			return TTask<FResult>{ Task };
		}

		// checks if pipe's task is being executed by the current thread. Allows to check if accessing a resource protected by a pipe
		// is thread-safe
		CORE_API bool IsInContext() const;

	private:
		friend class Private::FTaskBase;

		// pushes given task into the pipe: adds the task as a subsequent to the last task if any and sets it as the new last task
		// returns the previous piped task if we managed to register the given task as its subsequent, otherwise nullptr
		Private::FTaskBase* PushIntoPipe(Private::FTaskBase& Task);
		// pipe holds a "weak" reference to a task. the task must be cleared from the pipe when its execution finished before its completion, 
		// otherwise the next piped task can try to add itself as a subsequent to already destroyed task
		void ClearTask(Private::FTaskBase& Task);

		// notifications about pipe's task execution
		void ExecutionStarted();
		void ExecutionFinished();

	private:
		std::atomic<Private::FTaskBase*> LastTask{ nullptr }; // the last task that 
		// a counter of how many threads trying to push a task into a pipe right now
		std::atomic<int32> PushingThreadsNum{ 0 };

	public:
		FORCENOINLINE const TCHAR* GetDebugName() const
		{
			return DebugName;
		}

	private:
		const TCHAR* const DebugName;
	};
}
