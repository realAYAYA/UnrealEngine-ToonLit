// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDispatcher.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InterchangeDispatcherConfig.h"
#include "InterchangeDispatcherLog.h"
#include "InterchangeDispatcherTask.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace UE
{
	namespace Interchange
	{

		FInterchangeDispatcher::FInterchangeDispatcher(const FString& InResultFolder)
			: NextTaskIndex(0)
			, CompletedTaskCount(0)
			, ResultFolder(InResultFolder)
		{
		}

		int32 FInterchangeDispatcher::AddTask(const FString& JsonDescription)
		{
			if (!WorkerHandler->IsAlive())
			{
				return INDEX_NONE;
			}
			FScopeLock Lock(&TaskPoolCriticalSection);
			int32 TaskIndex = TaskPool.Emplace(JsonDescription);
			TaskPool[TaskIndex].Index = TaskIndex;
			return TaskIndex;
		}
		int32 FInterchangeDispatcher::AddTask(const FString& JsonDescription, FInterchangeDispatcherTaskCompleted TaskCompledDelegate)
		{
			int32 TaskIndex = AddTask(JsonDescription);
			if (TaskIndex != INDEX_NONE)
			{
				TaskPool[TaskIndex].OnTaskCompleted = TaskCompledDelegate;
			}
			return TaskIndex;
		}

		TOptional<FTask> FInterchangeDispatcher::GetNextTask()
		{
			FScopeLock Lock(&TaskPoolCriticalSection);

			while (TaskPool.IsValidIndex(NextTaskIndex) && TaskPool[NextTaskIndex].State != ETaskState::UnTreated)
			{
				NextTaskIndex++;
			}

			if (!TaskPool.IsValidIndex(NextTaskIndex))
			{
				return TOptional<FTask>();
			}

			TaskPool[NextTaskIndex].State = ETaskState::Running;
			TaskPool[NextTaskIndex].RunningStateStartTime = FPlatformTime::Seconds();
			return TaskPool[NextTaskIndex++];
		}

		void FInterchangeDispatcher::SetTaskState(int32 TaskIndex, ETaskState TaskState, const FString& JsonResult, const TArray<FString>& JSonMessages)
		{
			FString JsonDescription;
			{
				FScopeLock Lock(&TaskPoolCriticalSection);

				if (!ensure(TaskPool.IsValidIndex(TaskIndex)))
				{
					return;
				}

				FTask& Task = TaskPool[TaskIndex];
				if (Task.State == ETaskState::ProcessOk || Task.State == ETaskState::ProcessFailed)
				{
					//Task was already processed, we cannot set its state after it was process
					return;
				}
				Task.State = TaskState;
				Task.JsonResult = JsonResult;
				Task.JsonMessages = JSonMessages;
				JsonDescription = Task.JsonDescription;

				if (TaskState == ETaskState::ProcessOk
					|| TaskState == ETaskState::ProcessFailed)
				{
					//Call the task completion delegate
					Task.OnTaskCompleted.ExecuteIfBound(TaskIndex);
					CompletedTaskCount++;
				}

				if (TaskState == ETaskState::UnTreated)
				{
					NextTaskIndex = TaskIndex;
				}
			}

			UE_CLOG(TaskState == ETaskState::ProcessOk, LogInterchangeDispatcher, Verbose, TEXT("Json processed: %s"), *JsonDescription);
			UE_CLOG(TaskState == ETaskState::UnTreated, LogInterchangeDispatcher, Warning, TEXT("Json resubmitted: %s"), *JsonDescription);
			UE_CLOG(TaskState == ETaskState::ProcessFailed, LogInterchangeDispatcher, Error, TEXT("Json processing failure: %s"), *JsonDescription);
		}

		void FInterchangeDispatcher::GetTaskState(int32 TaskIndex, ETaskState& TaskState, double& TaskRunningStateStartTime)
		{
			FScopeLock Lock(&TaskPoolCriticalSection);

			if (!ensure(TaskPool.IsValidIndex(TaskIndex)))
			{
				return;
			}

			FTask& Task = TaskPool[TaskIndex];
			TaskState = Task.State;
			TaskRunningStateStartTime = Task.RunningStateStartTime;
		}

		void FInterchangeDispatcher::GetTaskState(int32 TaskIndex, ETaskState& TaskState, FString& JsonResult, TArray<FString>& JSonMessages)
		{
			FScopeLock Lock(&TaskPoolCriticalSection);

			if (!ensure(TaskPool.IsValidIndex(TaskIndex)))
			{
				return;
			}

			FTask& Task = TaskPool[TaskIndex];
			TaskState = Task.State;
			JsonResult = Task.JsonResult;
			JSonMessages = Task.JsonMessages;
		}

		void FInterchangeDispatcher::StartProcess()
		{
			//Start the process
			SpawnHandler();
		}

		void FInterchangeDispatcher::StopProcess(bool bBlockUntilTerminated)
		{
			if (IsHandlerAlive())
			{
				if (bBlockUntilTerminated)
				{
					WorkerHandler->StopBlocking();
				}
				else
				{
					WorkerHandler->Stop();
				}
			}
			EmptyQueueTasks();
		}

		void FInterchangeDispatcher::TerminateProcess()
		{
			//Empty the cache folder
			if (IFileManager::Get().DirectoryExists(*ResultFolder))
			{
				const bool RequireExists = false;
				//Delete recursively folder's content
				const bool Tree = true;
				IFileManager::Get().DeleteDirectory(*ResultFolder, RequireExists, Tree);
			}
			//Terminate the process
			CloseHandler();
		}

		void FInterchangeDispatcher::WaitAllTaskToCompleteExecution()
		{
			if (!WorkerHandler.IsValid())
			{
				UE_LOG(LogInterchangeDispatcher, Error, TEXT("Cannot execute tasks before starting the process"));
				return;
			}

			bool bLogRestartError = true;
			while (!IsOver())
			{

				if (!IsHandlerAlive())
				{
					break;
				}

				FPlatformProcess::Sleep(0.1f);
			}

			if (!IsOver())
			{
				UE_LOG(LogInterchangeDispatcher, Warning,
					   TEXT("Begin local processing. (Multi Process failed to consume all the tasks)\n")
					   TEXT("See workers logs: %sPrograms/InterchangeWorker/Saved/Logs"), *FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
			}
			else
			{
				UE_LOG(LogInterchangeDispatcher, Verbose, TEXT("Multi Process ended and consumed all the tasks"));
			}
		}

		bool FInterchangeDispatcher::IsOver()
		{
			FScopeLock Lock(&TaskPoolCriticalSection);
			return CompletedTaskCount == TaskPool.Num();
		}

		//this is a static function
		bool FInterchangeDispatcher::IsInterchangeWorkerAvailable()
		{
			FString RandomGuid = FGuid::NewGuid().ToString(EGuidFormats::Base36Encoded);
			const FString TmpResultFolder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("Interchange/Temp/") + RandomGuid);
			TUniquePtr<UE::Interchange::FInterchangeDispatcher> Dispatcher = MakeUnique<UE::Interchange::FInterchangeDispatcher>(TmpResultFolder);
			bool bWorkerValid = false;
			if (ensure(Dispatcher.IsValid()))
			{
				Dispatcher->StartProcess();
				//Wait the conection handshake is done
				//The interchange worker will validate the version and issue a error message if the connection is incorrect
				//If version is good the interchange worker will issue a ping command
				while (Dispatcher->IsHandlerAlive() && Dispatcher->GetInterchangeWorkerFatalError().IsEmpty())
				{
					if (Dispatcher->WorkerHandler->IsPingCommandReceived())
					{
						break;
					}
					else
					{
						FPlatformProcess::Sleep(0.001f);
					}
				}
				if (Dispatcher->IsHandlerAlive())
				{
					Dispatcher->WaitAllTaskToCompleteExecution();
					bWorkerValid = Dispatcher->GetInterchangeWorkerFatalError().IsEmpty();
				}
				Dispatcher->CloseHandler();
			}
			return bWorkerValid;
		}

		void FInterchangeDispatcher::SpawnHandler()
		{
			WorkerHandler = MakeUnique<FInterchangeWorkerHandler>(*this, ResultFolder);
			WorkerHandler->OnWorkerHandlerExitLoop.AddLambda([this]()
			{
				EmptyQueueTasks();
			});
		}

		bool FInterchangeDispatcher::IsHandlerAlive()
		{
			return WorkerHandler.IsValid() && WorkerHandler->IsAlive();
		}

		void FInterchangeDispatcher::CloseHandler()
		{
			StopProcess(false);
			WorkerHandler.Reset();
		}

		void FInterchangeDispatcher::EmptyQueueTasks()
		{
			//Make sure all queue tasks are completed to process fail,
			//This ensure any wait on completed delegate like promise of a future is unblock.
			TOptional<FTask> NextTask = GetNextTask();
			while (NextTask.IsSet())
			{
				TArray<FString> GarbageMessages;
				SetTaskState(NextTask->Index, ETaskState::ProcessFailed, FString(), GarbageMessages);
				NextTask = GetNextTask();
			}
		}

	} //ns Interchange
}//ns UE