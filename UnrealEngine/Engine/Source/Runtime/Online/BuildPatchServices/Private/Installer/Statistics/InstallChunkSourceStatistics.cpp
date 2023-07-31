// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/InstallChunkSourceStatistics.h"
#include "HAL/ThreadSafeBool.h"
#include "Core/AsyncHelpers.h"
#include "Common/SpeedRecorder.h"
#include "Common/StatsCollector.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/Statistics/FileOperationTracker.h"

namespace BuildPatchServices
{
	class FInstallChunkSourceStatistics
		: public IInstallChunkSourceStatistics
	{
	public:
		FInstallChunkSourceStatistics(ISpeedRecorder* SpeedRecorder, IInstallerAnalytics* InstallerAnalytics, IFileOperationTracker* FileOperationTracker);
		~FInstallChunkSourceStatistics();

		// IInstallChunkSourceStat interface begin.
		virtual void OnBatchStarted(const TArray<FGuid>& ChunkIds) override;
		virtual void OnLoadStarted(const FGuid& ChunkId) override;
		virtual void OnLoadComplete(const FGuid& ChunkId, const ELoadResult& Result, const ISpeedRecorder::FRecord& Record) override;
		virtual void OnAcceptedNewRequirements(const TSet<FGuid>& ChunkIds) override;
		// IInstallChunkSourceStat interface end.

		// IInstallChunkSourceStatistics interface begin.
		virtual uint64 GetBytesRead() const;
		virtual int32 GetNumSuccessfulChunkRecycles() const override;
		virtual int32 GetNumFailedChunkRecycles() const override;
		virtual bool IsCurrentlyReading() const override;
		// IInstallChunkSourceStatistics interface end.

	private:
		ISpeedRecorder* SpeedRecorder;
		IInstallerAnalytics* InstallerAnalytics;
		IFileOperationTracker* FileOperationTracker;
		FThreadSafeInt32 NumSuccessfulLoads;
		FThreadSafeInt32 NumFailedLoads;
		FThreadSafeInt64 TotalBytesRead;
		FThreadSafeBool bIsReading;
		FThreadSafeInt64 LastReadCycles;
	};

	FInstallChunkSourceStatistics::FInstallChunkSourceStatistics(ISpeedRecorder* InSpeedRecorder, IInstallerAnalytics* InInstallerAnalytics, IFileOperationTracker* InFileOperationTracker)
		: SpeedRecorder(InSpeedRecorder)
		, InstallerAnalytics(InInstallerAnalytics)
		, FileOperationTracker(InFileOperationTracker)
		, NumSuccessfulLoads(0)
		, NumFailedLoads(0)
		, TotalBytesRead(0)
		, bIsReading(false)
		, LastReadCycles(0)
	{
	}

	FInstallChunkSourceStatistics::~FInstallChunkSourceStatistics()
	{
	}

	void FInstallChunkSourceStatistics::OnBatchStarted(const TArray<FGuid>& ChunkIds)
	{
		FileOperationTracker->OnDataStateUpdate(ChunkIds, EFileOperationState::RetrievingLocalInstallData);
	}

	void FInstallChunkSourceStatistics::OnLoadStarted(const FGuid& ChunkId)
	{
		bIsReading = true;
		FileOperationTracker->OnDataStateUpdate(ChunkId, EFileOperationState::RetrievingLocalInstallData);
	}

	void FInstallChunkSourceStatistics::OnLoadComplete(const FGuid& ChunkId, const ELoadResult& Result, const ISpeedRecorder::FRecord& Record)
	{
		if (Result == ELoadResult::Success)
		{
			NumSuccessfulLoads.Increment();
		}
		else
		{
			FileOperationTracker->OnDataStateUpdate(ChunkId, EFileOperationState::PendingRemoteCloudData);
			InstallerAnalytics->RecordChunkCacheError(ChunkId, TEXT(""), FPlatformMisc::GetLastError(), TEXT("InstallChunkSourceLoad"), ToString(Result));
			NumFailedLoads.Increment();
		}
		SpeedRecorder->AddRecord(Record);
		TotalBytesRead.Add(Record.Size);
		bIsReading = false;
		LastReadCycles = Record.CyclesEnd;
	}

	void FInstallChunkSourceStatistics::OnAcceptedNewRequirements(const TSet<FGuid>& ChunkIds)
	{
		FileOperationTracker->OnDataStateUpdate(ChunkIds, EFileOperationState::PendingLocalInstallData);
	}

	uint64 FInstallChunkSourceStatistics::GetBytesRead() const
	{
		return TotalBytesRead.GetValue();
	}

	int32 FInstallChunkSourceStatistics::GetNumSuccessfulChunkRecycles() const
	{
		return NumSuccessfulLoads.GetValue();
	}

	int32 FInstallChunkSourceStatistics::GetNumFailedChunkRecycles() const
	{
		return NumFailedLoads.GetValue();
	}

	bool FInstallChunkSourceStatistics::IsCurrentlyReading() const
	{
		const int64 CyclesSinceRead = FStatsCollector::GetCycles() - LastReadCycles.GetValue();
		return bIsReading || FStatsCollector::CyclesToSeconds(CyclesSinceRead) < 0.05;
	}

	IInstallChunkSourceStatistics* FInstallChunkSourceStatisticsFactory::Create(ISpeedRecorder* SpeedRecorder, IInstallerAnalytics* InstallerAnalytics, IFileOperationTracker* FileOperationTracker)
	{
		check(SpeedRecorder != nullptr);
		check(InstallerAnalytics != nullptr);
		check(FileOperationTracker != nullptr);
		return new FInstallChunkSourceStatistics(SpeedRecorder, InstallerAnalytics, FileOperationTracker);
	}
};