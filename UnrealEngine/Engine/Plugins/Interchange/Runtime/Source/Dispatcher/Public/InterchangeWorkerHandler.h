// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"
#include "InterchangeCommands.h"
#include "InterchangeDispatcherNetworking.h"
#include "InterchangeDispatcherTask.h"

namespace UE
{
	namespace Interchange
	{
		namespace Dispatcher
		{
			class FTaskProcessCommand;
		}

		class FInterchangeDispatcher;

		//Handle a Worker by socket communication
		class INTERCHANGEDISPATCHER_API FInterchangeWorkerHandler
		{
			enum class EWorkerState
			{
				Uninitialized,
				Processing, // send task and receive result
				Closing, // in the process of terminating
				Terminated, // aka. Not Alive
			};

			enum class EWorkerErrorState
			{
				Ok,
				ConnectionFailed_NotBound,
				ConnectionFailed_NoClient,
				ConnectionLost,
				ConnectionLost_SendFailed,
				WorkerProcess_CantCreate,
				WorkerProcess_Lost,
			};

		public:
			FInterchangeWorkerHandler(FInterchangeDispatcher& InDispatcher, FString& InResultFolder);
			~FInterchangeWorkerHandler();

			void Run();
			bool IsAlive() const;
			void Stop();
			void StopBlocking();

			bool IsPingCommandReceived() { return bPingCommandReceived; }

			FSimpleMulticastDelegate OnWorkerHandlerExitLoop;
		protected:
			void ProcessCommand(ICommand& Command);
		private:
			void RunInternal();
			void StartWorkerProcess();
			void ValidateConnection();

			void ProcessCommand(FPingCommand& PingCommand);
			void ProcessCommand(FErrorCommand& ErrorCommand);
			void ProcessCommand(FCompletedTaskCommand& RunTaskCommand);
			void ProcessCommand(FCompletedQueryTaskProgressCommand& CompletedQueryTaskProgressCommand);
			const TCHAR* EWorkerErrorStateAsString(EWorkerErrorState e);

			void KillAllCurrentTasks();

		private:
			FInterchangeDispatcher& Dispatcher;

			// Send and receive commands
			FNetworkServerNode NetworkInterface;
			FCommandQueue CommandIO;
			FThread IOThread;
			FString ThreadName;

			// External process
			FProcHandle WorkerHandle;
			TAtomic<EWorkerState> WorkerState;
			EWorkerErrorState ErrorState;

			// self
			FString ResultFolder;
			FCriticalSection CurrentTasksLock;
			TArray<int32> CurrentTasks;
			bool bShouldTerminate;
			double LastProgressMessageTime;

			//When the worker start, it send a ping command. This flag is turn on when we receive the ping command
			bool bPingCommandReceived = false;

			friend Dispatcher::FTaskProcessCommand;
		};
	} //ns Interchange
}//ns UE
