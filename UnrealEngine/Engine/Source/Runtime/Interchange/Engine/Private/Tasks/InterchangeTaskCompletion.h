// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "InterchangeManager.h"
#include "Stats/Stats.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace UE
{
	namespace Interchange
	{
		class FTaskPreCompletion
		{
		private:
			UInterchangeManager* InterchangeManager;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		public:
			FTaskPreCompletion(UInterchangeManager* InInterchangeManager, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: InterchangeManager(InInterchangeManager)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			static FORCEINLINE ENamedThreads::Type GetDesiredThread()
			{
				return ENamedThreads::GameThread;
			}
			static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskPreCompletion, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};

		class FTaskCompletion
		{
		private:
			UInterchangeManager* InterchangeManager;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		public:
			FTaskCompletion(UInterchangeManager* InInterchangeManager, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: InterchangeManager(InInterchangeManager)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			static FORCEINLINE ENamedThreads::Type GetDesiredThread()
			{
				return ENamedThreads::GameThread;
			}
			static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
			{
				//In case we need to know when the task is done we need track subsequent to get a valid FGraphEventRef when we create the task
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskCompletion, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};

	} //ns Interchange
}//ns UE

