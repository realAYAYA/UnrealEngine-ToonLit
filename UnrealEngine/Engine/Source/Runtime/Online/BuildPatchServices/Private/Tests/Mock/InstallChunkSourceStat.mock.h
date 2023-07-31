// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/InstallChunkSource.h"
#include "Common/StatsCollector.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockInstallChunkSourceStat
		: public IInstallChunkSourceStat
	{
	public:
		typedef TTuple<double, TArray<FGuid>> FBatchStarted;
		typedef TTuple<double, FGuid> FLoadStarted;
		typedef TTuple<double, FGuid, ELoadResult, ISpeedRecorder::FRecord> FLoadComplete;

	public:
		virtual void OnBatchStarted(const TArray<FGuid>& ChunkIds) override
		{
			if (OnBatchStartedFunc != nullptr)
			{
				OnBatchStartedFunc(ChunkIds);
			}
			RxBatchStarted.Emplace(FStatsCollector::GetSeconds(), ChunkIds);
		}

		virtual void OnLoadStarted(const FGuid& ChunkId) override
		{
			if (OnLoadStartedFunc != nullptr)
			{
				OnLoadStartedFunc(ChunkId);
			}
			RxLoadStarted.Emplace(FStatsCollector::GetSeconds(), ChunkId);
		}

		virtual void OnLoadComplete(const FGuid& ChunkId, const ELoadResult& Result, const ISpeedRecorder::FRecord& Record) override
		{
			if (OnLoadCompleteFunc != nullptr)
			{
				OnLoadCompleteFunc(ChunkId, Result, Record);
			}
			RxLoadComplete.Emplace(FStatsCollector::GetSeconds(), ChunkId, Result, Record);
		}

		virtual void OnAcceptedNewRequirements(const TSet<FGuid>& ChunkIds) override
		{
		}

	public:
		TArray<FBatchStarted> RxBatchStarted;
		TArray<FLoadStarted> RxLoadStarted;
		TArray<FLoadComplete> RxLoadComplete;
		TFunction<void(const TArray<FGuid>&)> OnBatchStartedFunc;
		TFunction<void(const FGuid&)> OnLoadStartedFunc;
		TFunction<void(const FGuid&, const ELoadResult&, const ISpeedRecorder::FRecord&)> OnLoadCompleteFunc;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
