// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/TaskManager.h"

#include "HAL/PlatformCrt.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/CoreStats.h"
#include "Misc/QueuedThreadPool.h"
#include "Stats/Stats2.h"
#include "Templates/RefCounting.h"


namespace mu
{

	/**
	 * Base class for asynchronous functions that are executed in the Task Graph system.
	 */
	class FMutableCoreGraphTaskBase
	{
	public:

		/**
		 * Creates and initializes a new instance.
		 */
		FMutableCoreGraphTaskBase(mu::Task* InTask, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
			: Task(InTask)
			, DesiredThread(InDesiredThread)
		{ }

		/**
		 * Performs the actual task.
		 *
		 * @param CurrentThread The thread that this task is executing on.
		 * @param MyCompletionGraphEvent The completion event.
		 */
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			Task->Run();
		}

		/**
		 * Returns the name of the thread that this task should run on.
		 *
		 * @return Always run on any thread.
		 */
		ENamedThreads::Type GetDesiredThread()
		{
			return DesiredThread;
		}

		/**
		 * Gets the task's stats tracking identifier.
		 *
		 * @return Stats identifier.
		 */
		TStatId GetStatId() const
		{
			return GET_STATID(STAT_TaskGraph_OtherTasks);
		}

		/**
		 * Gets the mode for tracking subsequent tasks.
		 *
		 * @return Always track subsequent tasks.
		 */
		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}

	private:

		/** The function to execute on the Task Graph. */
		mu::Task* Task = nullptr;

		/** The desired execution thread. */
		ENamedThreads::Type DesiredThread;
	};


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
	void TaskManager::AddTask(TaskManager::Task* task)
	{
		//if (!UseConcurrency)
		{
			task->Run();
			task->Complete();
			delete task;
			return;
		}

		// Resolve the completed tasks
		for (int32 i = 0; i < m_running.Num(); )
		{
			TPair<Task*, FGraphEventRef>& Data = m_running[i];

			if (Data.Value->IsComplete())
			{
				Data.Key->Complete();
				delete Data.Key;

				m_running[i] = m_running.Last();
				m_running.Pop();
			}
			else
			{
				++i;
			}
		}

		// Add the new task
		int maxLiveTasks = FMath::Max(GThreadPool->GetNumThreads() * 8, 8);
		int maxPendingTasks = maxLiveTasks * 2;


		// If there are too many ongoing tasks, wait for some to complete to avoid using too much memory.
		m_pending.Add(task);
		while ((m_pending.Num() + m_running.Num()) >= (maxPendingTasks + maxLiveTasks)
			&&
			!m_running.IsEmpty())
		{
			TPair<Task*, FGraphEventRef> Data = m_running.Pop();
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Data.Value);
			Data.Key->Complete();
			delete Data.Key;
		}

		// Send more tasks
		while (!m_pending.IsEmpty() && m_running.Num() < maxLiveTasks)
		{
			SendOne();
		}
	}


	void TaskManager::SendOne()
	{
		TPair<Task*, FGraphEventRef> Data;
		Data.Key = m_pending.Pop();
		Data.Value =  TGraphTask<mu::FMutableCoreGraphTaskBase>::CreateTask().ConstructAndDispatchWhenReady(Data.Key, ENamedThreads::AnyThread);
	}


    //---------------------------------------------------------------------------------------------
    void TaskManager::CompleteTasks()
    {
		if (!UseConcurrency) 
			// Tasks already ran and completed on submission
			return;

		// Send all the pending tasks
		while (!m_pending.IsEmpty())
		{
			SendOne();
		}

		// Wait for completion of all tasks.
		for (TPair<Task*, FGraphEventRef>& Data : m_running)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Data.Value);
			Data.Key->Complete();
			delete Data.Key;
		}
		m_running.Reset();
    }

}
