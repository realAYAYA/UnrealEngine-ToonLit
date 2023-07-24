// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Thread.h"
#include "InterchangeDispatcherTask.h"
#include "InterchangeWorkerHandler.h"

namespace UE
{
	namespace Interchange
	{

		// Handle a list of tasks, and a set of external workers to consume them.
		class INTERCHANGEDISPATCHER_API FInterchangeDispatcher
		{
		public:
			FInterchangeDispatcher(const FString& InResultFolder);
			~FInterchangeDispatcher() { TerminateProcess(); }

			int32 AddTask(const FString& JsonDescription);
			int32 AddTask(const FString& JsonDescription, FInterchangeDispatcherTaskCompleted TaskCompledDelegate);
			TOptional<FTask> GetNextTask();
			void SetTaskState(int32 TaskIndex, ETaskState TaskState, const FString& JsonResult, const TArray<FString>& JSonMessages);
			void GetTaskState(int32 TaskIndex, ETaskState& TaskState, double& TaskRunningStateStartTime);
			void GetTaskState(int32 TaskIndex, ETaskState& TaskState, FString& JsonResult, TArray<FString>& JSonMessages);

			void StartProcess();
			void StopProcess(bool bBlockUntilTerminated);
			void TerminateProcess();
			void WaitAllTaskToCompleteExecution();
			bool IsOver();

			void SetInterchangeWorkerFatalError(FString& ErrorMessage)
			{
				InterchangeWorkerFatalError = MoveTemp(ErrorMessage);
			}

			FString GetInterchangeWorkerFatalError()
			{
				return InterchangeWorkerFatalError;
			}

			static bool IsInterchangeWorkerAvailable();
		private:
			void SpawnHandler();
			bool IsHandlerAlive();
			void CloseHandler();
			void EmptyQueueTasks();

			// Tasks
			FCriticalSection TaskPoolCriticalSection;
			TArray<FTask> TaskPool;
			int32 NextTaskIndex;
			int32 CompletedTaskCount;

			/** Path where the result files are dump */
			FString ResultFolder;

			FString InterchangeWorkerFatalError;

			// Workers
			TUniquePtr<FInterchangeWorkerHandler> WorkerHandler = nullptr;
		};

	} //ns Interchange
}//ns UE
