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

		class FTaskPipelinePreImport
		{
		private:
			TWeakObjectPtr<UInterchangePipelineBase> PipelineBase;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		public:
			FTaskPipelinePreImport(TWeakObjectPtr<UInterchangePipelineBase> InPipelineBase, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
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

				return PipelineBase.Get()->ScriptedCanExecuteOnAnyThread(EInterchangePipelineTask::PreFactoryImport) ? ENamedThreads::AnyBackgroundThreadNormalTask : ENamedThreads::GameThread;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskPipelinePreImport, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};

		class FTaskPipelinePostImport
		{
		private:
			int32 SourceIndex;
			int32 PipelineIndex;
			TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;

		public:
			FTaskPipelinePostImport(int32 InSourceIndex, int32 InPipelineIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper)
				: SourceIndex(InSourceIndex)
				, PipelineIndex(InPipelineIndex)
				, WeakAsyncHelper(InAsyncHelper)
			{
			}

			ENamedThreads::Type GetDesiredThread()
			{
				TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
				//Always use the game thread if there is some error
				if (!ensure(AsyncHelper.IsValid()))
				{
					return ENamedThreads::GameThread;
				}
				if (!AsyncHelper->Pipelines.IsValidIndex(PipelineIndex))
				{
					return ENamedThreads::GameThread;
				}

				//Scripted (python) cannot run outside of the game thread, it will lock forever if we do this
				if (AsyncHelper->Pipelines[PipelineIndex]->IsScripted())
				{
					return ENamedThreads::GameThread;
				}

				//Ask the pipeline implementation
				if (AsyncHelper->Pipelines[PipelineIndex]->ScriptedCanExecuteOnAnyThread(EInterchangePipelineTask::PostFactoryImport))
				{
					return ENamedThreads::AnyBackgroundThreadNormalTask;
				}

				return ENamedThreads::GameThread;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskPipelinePostImport, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};


	} //ns Interchange
}//ns UE
