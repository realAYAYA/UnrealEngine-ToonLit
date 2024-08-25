// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "Stats/Stats.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace UE
{
	namespace Interchange
	{

		class FTaskPipeline
		{
		private:
			TWeakObjectPtr<UInterchangePipelineBase> PipelineBase;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		public:
			FTaskPipeline(TWeakObjectPtr<UInterchangePipelineBase> InPipelineBase, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: PipelineBase(InPipelineBase)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			ENamedThreads::Type GetDesiredThread()
			{
				if (!ensure(PipelineBase.IsValid()))
				{
					return ENamedThreads::GameThread;
				}
				
				//Scripted (python) cannot run outside of the game thread, it will lock forever if we do this
				if (PipelineBase.Get()->IsScripted())
				{
					return ENamedThreads::GameThread;
				}

				return PipelineBase.Get()->CanExecuteOnAnyThread(EInterchangePipelineTask::PostTranslator) ? ENamedThreads::AnyBackgroundThreadNormalTask : ENamedThreads::GameThread;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskPipeline, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};

		//We want to be sure any asset compilation is finish before calling FTaskPostImport, we use a async task to wait until they are done
		class FTaskWaitAssetCompilation
		{
		private:
			int32 SourceIndex;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;

		public:
			FTaskWaitAssetCompilation(int32 InSourceIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: SourceIndex(InSourceIndex)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			ENamedThreads::Type GetDesiredThread()
			{
				return ENamedThreads::AnyBackgroundThreadNormalTask;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskWaitAssetCompilation, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};

		class FTaskPostImport
		{
		private:
			int32 SourceIndex;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;

		public:
			FTaskPostImport(int32 InSourceIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: SourceIndex(InSourceIndex)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			ENamedThreads::Type GetDesiredThread()
			{
				return ENamedThreads::GameThread;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskPostImport, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};
	} //ns Interchange
}//ns UE
