// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/DiskChunkStoreStatistics.h"
#include "CoreMinimal.h"
#include "Core/AsyncHelpers.h"
#include "Data/ChunkData.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/Statistics/FileOperationTracker.h"

namespace BuildPatchServices
{
	class FDiskChunkStoreStatistics
		: public IDiskChunkStoreStatistics
	{
	public:
		FDiskChunkStoreStatistics(IInstallerAnalytics* InInstallerAnalytics, IFileOperationTracker* FileOperationTracker);
		~FDiskChunkStoreStatistics();

	public:
		// IDiskChunkStoreStat interface begin.
		virtual void OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult) override;
		virtual void OnBeforeChunkLoad(const FGuid& ChunkId) override;
		virtual void OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult) override;
		virtual void OnCacheUseUpdated(int32 ChunkCount) override;
		// IDiskChunkStoreStat interface end.

		// IDiskChunkStoreStatistics interface begin.
		virtual int32 GetNumSuccessfulLoads() const override;
		virtual int32 GetNumFailedLoads() const override;
		virtual int32 GetNumSuccessfulSaves() const override;
		virtual int32 GetNumFailedSaves() const override;
		// IDiskChunkStoreStatistics interface end.

	private:
		IInstallerAnalytics* const InstallerAnalytics;
		IFileOperationTracker* const FileOperationTracker;
		FThreadSafeInt32 NumSuccessfulLoads;
		FThreadSafeInt32 NumSuccessfulSaves;
		FThreadSafeInt32 NumFailedLoads;
		FThreadSafeInt32 NumFailedSaves;
	};

	FDiskChunkStoreStatistics::FDiskChunkStoreStatistics(IInstallerAnalytics* InInstallerAnalytics, IFileOperationTracker* InFileOperationTracker)
		: InstallerAnalytics(InInstallerAnalytics)
		, FileOperationTracker(InFileOperationTracker)
		, NumSuccessfulLoads(0)
		, NumSuccessfulSaves(0)
		, NumFailedLoads(0)
		, NumFailedSaves(0)
	{
	}

	FDiskChunkStoreStatistics::~FDiskChunkStoreStatistics()
	{
	}

	void FDiskChunkStoreStatistics::OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult)
	{
		if (SaveResult == EChunkSaveResult::Success)
		{
			NumSuccessfulSaves.Increment();
			FileOperationTracker->OnDataStateUpdate(ChunkId, EFileOperationState::PendingLocalDataStore);
		}
		else
		{
			InstallerAnalytics->RecordChunkCacheError(ChunkId, ChunkFilename, FPlatformMisc::GetLastError(), TEXT("DiskChunkStoreSave"), ToString(SaveResult));
			NumFailedSaves.Increment();
		}
	}

	void FDiskChunkStoreStatistics::OnBeforeChunkLoad(const FGuid& ChunkId)
	{
		FileOperationTracker->OnDataStateUpdate(ChunkId, EFileOperationState::RetrievingLocalDataStore);
	}

	void FDiskChunkStoreStatistics::OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult)
	{
		if (LoadResult == EChunkLoadResult::Success)
		{
			NumSuccessfulLoads.Increment();
		}
		else
		{
			InstallerAnalytics->RecordChunkCacheError(ChunkId, ChunkFilename, FPlatformMisc::GetLastError(), TEXT("DiskChunkStoreLoad"), ToString(LoadResult));
			NumFailedLoads.Increment();
		}
	}

	void FDiskChunkStoreStatistics::OnCacheUseUpdated(int32 ChunkCount)
	{
	}

	int32 FDiskChunkStoreStatistics::GetNumSuccessfulLoads() const
	{
		return NumSuccessfulLoads.GetValue();
	}

	int32 FDiskChunkStoreStatistics::GetNumFailedLoads() const
	{
		return NumFailedLoads.GetValue();
	}

	int32 FDiskChunkStoreStatistics::GetNumSuccessfulSaves() const
	{
		return NumSuccessfulSaves.GetValue();
	}

	int32 FDiskChunkStoreStatistics::GetNumFailedSaves() const
	{
		return NumFailedSaves.GetValue();
	}

	IDiskChunkStoreStatistics* FDiskChunkStoreStatisticsFactory::Create(IInstallerAnalytics* InstallerAnalytics, IFileOperationTracker* FileOperationTracker)
	{
		check(InstallerAnalytics != nullptr);
		check(FileOperationTracker != nullptr);
		return new FDiskChunkStoreStatistics(InstallerAnalytics, FileOperationTracker);
	}
};