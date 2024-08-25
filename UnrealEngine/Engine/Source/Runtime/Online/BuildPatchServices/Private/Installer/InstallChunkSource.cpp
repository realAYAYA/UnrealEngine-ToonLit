// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/InstallChunkSource.h"

#include "Algo/Transform.h"
#include "Containers/Queue.h"
#include "Core/BlockStructure.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"

#include "Common/FileSystem.h"
#include "Common/StatsCollector.h"
#include "Installer/InstallerError.h"
#include "Installer/ChunkStore.h"
#include "Installer/ChunkReferenceTracker.h"
#include "BuildPatchHash.h"
#include "IBuildManifestSet.h"

DECLARE_LOG_CATEGORY_CLASS(LogInstallChunkSource, Warning, All);

namespace BuildPatchServices
{
	class FInstallChunkSource : public IInstallChunkSource
	{
	public:
		FInstallChunkSource(FInstallSourceConfig InConfiguration, IFileSystem* FileSystem, IChunkStore* InChunkStore, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallChunkSourceStat* InInstallChunkSourceStat, const TMultiMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, IBuildManifestSet* ManifestSet);
		~FInstallChunkSource();

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IChunkSource interface begin.
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override;
		virtual bool AddRepeatRequirement(const FGuid& RepeatRequirement) override;
		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override;
		// IChunkSource interface end.

		// IInstallChunkSource interface begin.
		virtual const TSet<FGuid>& GetAvailableChunks() const override;
		virtual void HarvestRemainingChunksFromFile(const FString& BuildFile) override;
		// IInstallChunkSource interface end.

	private:
		void FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const;
		bool LoadFromBuild(const FGuid& DataId);
		FSHAHash GetShaHashForDataSet(const uint8* ChunkData, const uint32 ChunkSize);

	private:
		// Configuration.
		const FInstallSourceConfig Configuration;
		// Dependencies.
		IFileSystem* FileSystem;
		IChunkStore* ChunkStore;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IInstallerError* InstallerError;
		IInstallChunkSourceStat* InstallChunkSourceStat;
		// Control signals.
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
		// Handling of chunks we lose access to.
		TFunction<void(TSet<FGuid>)> UnavailableChunksCallback;
		TSet<FGuid> UnavailableChunks;
		// Keep a separate copy of failed chunks internally.
		TSet<FGuid> FailedChunks;
		// Storage of enumerated chunks.
		TSet<FGuid> AvailableInBuilds;
		TArray<TPair<FString, FBuildPatchAppManifestRef>> InstallationSources;
		TSet<FGuid> PlacedInStore;
		// Track additional requests.
		TSet<FGuid> RuntimeRequests;
		// Communication and storage of incoming repeat requirements.
		TQueue<FGuid, EQueueMode::Mpsc> RepeatRequirementMessages;
	};

	FInstallChunkSource::FInstallChunkSource(FInstallSourceConfig InConfiguration, IFileSystem* InFileSystem, IChunkStore* InChunkStore, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallChunkSourceStat* InInstallChunkSourceStat, const TMultiMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, IBuildManifestSet* ManifestSet)
		: Configuration(MoveTemp(InConfiguration))
		, FileSystem(InFileSystem)
		, ChunkStore(InChunkStore)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, InstallerError(InInstallerError)
		, InstallChunkSourceStat(InInstallChunkSourceStat)
		, bIsPaused(false)
		, bShouldAbort(false)
		, UnavailableChunksCallback(nullptr)
	{
		// Cache faster lookup information.
		TSet<FGuid> ReferencedChunks;
		ManifestSet->GetReferencedChunks(ReferencedChunks);
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InInstallationSources)
		{
			if (Pair.Value->EnumerateProducibleChunks(Pair.Key, ReferencedChunks, AvailableInBuilds) > 0)
			{
				InstallationSources.Add(Pair);
			}
		}
		UE_LOG(LogInstallChunkSource, Log, TEXT("Useful Sources:%d. Available Chunks:%d."), InstallationSources.Num(), AvailableInBuilds.Num());
	}

	FInstallChunkSource::~FInstallChunkSource()
	{
	}

	void FInstallChunkSource::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FInstallChunkSource::Abort()
	{
		bShouldAbort = true;
	}

	IChunkDataAccess* FInstallChunkSource::Get(const FGuid& DataId)
	{
		// Get from our store
		IChunkDataAccess* ChunkData = ChunkStore->Get(DataId);
		if (ChunkData == nullptr)
		{
			// If available, load next batch into store.
			if (AvailableInBuilds.Contains(DataId))
			{
				// 'Forget' any repeat requirements.
				FGuid RepeatRequirement;
				while (RepeatRequirementMessages.Dequeue(RepeatRequirement))
				{
					PlacedInStore.Remove(RepeatRequirement);
				}
				// Select the next X chunks that are locally available.
				TFunction<bool(const FGuid&)> SelectPredicate = [this](const FGuid& ChunkId) { return AvailableInBuilds.Contains(ChunkId) && (!Configuration.ChunkIgnoreSet.Contains(ChunkId) || RuntimeRequests.Contains(ChunkId)); };
				// Grab all the chunks relevant to this source to fill the store.
				int32 SearchLength = FMath::Max(ChunkStore->GetSize(), Configuration.BatchFetchMinimum);
				TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker->SelectFromNextReferences(SearchLength, SelectPredicate);
				// Remove already loaded chunks.
				TFunction<bool(const FGuid&)> RemovePredicate = [this](const FGuid& ChunkId) { return PlacedInStore.Contains(ChunkId) || FailedChunks.Contains(ChunkId); };
				BatchLoadChunks.RemoveAll(RemovePredicate);
				// Clamp to configured max.
				BatchLoadChunks.SetNum(FMath::Min(BatchLoadChunks.Num(), Configuration.BatchFetchMaximum), EAllowShrinking::No);
				// Ensure requested chunk is in the array.
				BatchLoadChunks.AddUnique(DataId);
				// Call to stat.
				InstallChunkSourceStat->OnBatchStarted(BatchLoadChunks);
				// Load this batch
				for (int32 ChunkIdx = 0; ChunkIdx < BatchLoadChunks.Num() && !bShouldAbort; ++ChunkIdx)
				{
					const FGuid& BatchLoadChunk = BatchLoadChunks[ChunkIdx];
					LoadFromBuild(BatchLoadChunk);
				}
				// Get from store again.
				ChunkData = ChunkStore->Get(DataId);
				// Dump out unavailable chunks on the incoming IO thread.
				if (UnavailableChunksCallback && UnavailableChunks.Num() > 0)
				{
					UnavailableChunksCallback(MoveTemp(UnavailableChunks));
				}
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FInstallChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		TSet<FGuid> Unhandled = NewRequirements.Difference(AvailableInBuilds);
		TSet<FGuid> Accepted = NewRequirements.Intersect(AvailableInBuilds);
		RuntimeRequests.Append(Accepted);
		InstallChunkSourceStat->OnAcceptedNewRequirements(Accepted);
		return Unhandled;
	}

	bool FInstallChunkSource::AddRepeatRequirement(const FGuid& RepeatRequirement)
	{
		if (AvailableInBuilds.Contains(RepeatRequirement))
		{
			RepeatRequirementMessages.Enqueue(RepeatRequirement);
			return true;
		}
		return false;
	}

	void FInstallChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		UnavailableChunksCallback = Callback;
	}

	const TSet<FGuid>& FInstallChunkSource::GetAvailableChunks() const
	{
		return AvailableInBuilds;
	}

	void FInstallChunkSource::HarvestRemainingChunksFromFile(const FString& FilePath)
	{
		const FFileManifest* FileManifest = nullptr;
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InstallationSources)
		{
			if (FilePath.StartsWith(Pair.Key))
			{
				FString BuildRelativeFilePath = FilePath;
				FPaths::MakePathRelativeTo(BuildRelativeFilePath, *(Pair.Key / TEXT("")));
				FileManifest = Pair.Value->GetFileManifest(BuildRelativeFilePath);
				break;
			}
		}
		if (FileManifest != nullptr)
		{
			// Collect all chunks in this file.
			TSet<FGuid> FileManifestChunks;
			Algo::Transform(FileManifest->ChunkParts, FileManifestChunks, &FChunkPart::Guid);
			FileManifestChunks = FileManifestChunks.Intersect(AvailableInBuilds);
			// Select all chunks still required from this file.
			TFunction<bool(const FGuid&)> SelectPredicate = [this, &FileManifestChunks](const FGuid& ChunkId)
			{
				return !PlacedInStore.Contains(ChunkId) && (FileManifestChunks.Contains(ChunkId) && (!Configuration.ChunkIgnoreSet.Contains(ChunkId) || RuntimeRequests.Contains(ChunkId)));
			};
			TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker->GetNextReferences(TNumericLimits<int32>::Max(), SelectPredicate);
			if (BatchLoadChunks.Num() > 0)
			{
				// Call to stat.
				InstallChunkSourceStat->OnBatchStarted(BatchLoadChunks);
				// Load the batch.
				for (int32 ChunkIdx = 0; ChunkIdx < BatchLoadChunks.Num() && !bShouldAbort; ++ChunkIdx)
				{
					const FGuid& BatchLoadChunk = BatchLoadChunks[ChunkIdx];
					LoadFromBuild(BatchLoadChunk);
				}
			}
		}
	}

	void FInstallChunkSource::FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const
	{
		uint64 ChunkHash;
		*FoundInstallDirectory = nullptr;
		*FoundInstallManifest = nullptr;
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InstallationSources)
		{
			// GetChunkHash can be used as a check for whether this manifest references this chunk.
			if (Pair.Value->GetChunkHash(DataId, ChunkHash))
			{
				*FoundInstallDirectory = &Pair.Key;
				*FoundInstallManifest = &Pair.Value.Get();
				return;
			}
		}
	}

	bool FInstallChunkSource::LoadFromBuild(const FGuid& DataId)
	{
		// Find the location of this chunk.
		const FString* FoundInstallDirectory;
		const FBuildPatchAppManifest* FoundInstallManifest;
		FindChunkLocation(DataId, &FoundInstallDirectory, &FoundInstallManifest);
		if (FoundInstallDirectory == nullptr || FoundInstallManifest == nullptr)
		{
			return false;
		}

		// Attempt construction of the chunk from the parts.
		ISpeedRecorder::FRecord LoadRecord;
		LoadRecord.Size = 0;
		InstallChunkSourceStat->OnLoadStarted(DataId);
		TUniquePtr<FArchive> FileArchive;
		FString FileOpened;
		int64 FileSize = 0;

		// We must have a hash for this chunk or else we cant verify it
		EChunkHashFlags HashType = EChunkHashFlags::None;
		uint64 ChunkHash = 0;
		FSHAHash ChunkShaHash;
		if (FoundInstallManifest->GetChunkShaHash(DataId, ChunkShaHash))
		{
			HashType |= EChunkHashFlags::Sha1;
		}
		if (FoundInstallManifest->GetChunkHash(DataId, ChunkHash))
		{
			HashType |= EChunkHashFlags::RollingPoly64;
		}
		LoadRecord.CyclesStart = FStatsCollector::GetCycles();
		IInstallChunkSourceStat::ELoadResult LoadResult = HashType == EChunkHashFlags::None ? IInstallChunkSourceStat::ELoadResult::MissingHashInfo : IInstallChunkSourceStat::ELoadResult::Success;
		if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
		{
			// Get the list of data pieces we need to load.
			TArray<FFileChunkPart> FileChunkParts = FoundInstallManifest->GetFilePartsForChunk(DataId);
			const FChunkInfo* ChunkInfoPtr = FoundInstallManifest->GetChunkInfo(DataId);
			LoadResult = ChunkInfoPtr == nullptr || FileChunkParts.Num() <= 0 ? IInstallChunkSourceStat::ELoadResult::MissingPartInfo : IInstallChunkSourceStat::ELoadResult::Success;
			if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
			{
				FBlockStructure ChunkBlocks;
				const int32 InitialDataSize = 1024*1024;
				TArray<uint8> TempArray;
				uint8* TempChunkConstruction = nullptr;
				uint32 LoadedChunkSize = 0;
				bool bLoadedWholeChunk = false;
				for (int32 FileChunkPartsIdx = 0; FileChunkPartsIdx < FileChunkParts.Num() && !bLoadedWholeChunk && !bShouldAbort; ++FileChunkPartsIdx)
				{
					const FFileChunkPart& FileChunkPart = FileChunkParts[FileChunkPartsIdx];
					LoadedChunkSize = FMath::Max<uint32>(LoadedChunkSize, FileChunkPart.ChunkPart.Offset + FileChunkPart.ChunkPart.Size);
					TempArray.SetNumZeroed(FMath::Max<uint32>(LoadedChunkSize, InitialDataSize));
					TempChunkConstruction = TempArray.GetData();
					FString FullFilename = *FoundInstallDirectory / FileChunkPart.Filename;
					// Close current build file ?
					if (FileArchive.IsValid() && FileOpened != FullFilename)
					{
						FileArchive->Close();
						FileArchive.Reset();
						FileOpened.Empty();
						FileSize = 0;
					}
					// Open build file ?
					if (!FileArchive.IsValid())
					{
						FileArchive = FileSystem->CreateFileReader(*FullFilename);
						LoadResult = !FileArchive.IsValid() ? IInstallChunkSourceStat::ELoadResult::OpenFileFail : IInstallChunkSourceStat::ELoadResult::Success;
						if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
						{
							FileOpened = FullFilename;
							FileSize = FileArchive->TotalSize();
						}
					}
					// Grab the section of the file.
					if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
					{
						// Make sure we don't attempt to read off the end of the file
						const int64 LastRequiredByte = FileChunkPart.FileOffset + FileChunkPart.ChunkPart.Size;
						LoadResult = FileSize < LastRequiredByte ? IInstallChunkSourceStat::ELoadResult::IncorrectFileSize : IInstallChunkSourceStat::ELoadResult::Success;
						if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
						{
							FileArchive->Seek(FileChunkPart.FileOffset);
							FileArchive->Serialize(TempChunkConstruction + FileChunkPart.ChunkPart.Offset, FileChunkPart.ChunkPart.Size);
							ChunkBlocks.Add(FileChunkPart.ChunkPart.Offset, FileChunkPart.ChunkPart.Size);
							LoadRecord.Size += FileChunkPart.ChunkPart.Size;
						}
					}
					bLoadedWholeChunk = ChunkBlocks.GetHead() && ChunkBlocks.GetHead() == ChunkBlocks.GetTail() && ChunkBlocks.GetHead()->GetSize() == ChunkInfoPtr->WindowSize;
					// Wait while paused
					while (bIsPaused && !bShouldAbort)
					{
						FPlatformProcess::Sleep(0.5f);
					}
				}
				if (bLoadedWholeChunk)
				{
					LoadResult = IInstallChunkSourceStat::ELoadResult::Success;
				}

				// Mark not success if we gave up due to being aborted.
				if (bShouldAbort)
				{
					LoadResult = IInstallChunkSourceStat::ELoadResult::Aborted;
				}

				// Check chunk hash
				if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
				{
					const bool bUseSha = (HashType & EChunkHashFlags::Sha1) != EChunkHashFlags::None;
					const bool bHashCheckOk = bUseSha ? GetShaHashForDataSet(TempChunkConstruction, LoadedChunkSize) == ChunkShaHash : FRollingHash::GetHashForDataSet(TempChunkConstruction, LoadedChunkSize) == ChunkHash;
					if (bHashCheckOk == false)
					{
						LoadResult = IInstallChunkSourceStat::ELoadResult::HashCheckFailed;
					}
				}

				// Save the chunk to the store if all went well.
				if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
				{
					// Create the ChunkFile data structure
					IChunkDataAccess* NewChunkFile = FChunkDataAccessFactory::Create(LoadedChunkSize);

					// Lock data
					FChunkHeader* ChunkHeader;
					uint8* ChunkData;
					NewChunkFile->GetDataLock(&ChunkData, &ChunkHeader);

					// Copy the data
					FMemoryReader MemReader(TempArray);
					MemReader.Serialize(ChunkData, LoadedChunkSize);

					// Setup the header
					ChunkHeader->Guid = DataId;
					ChunkHeader->StoredAs = EChunkStorageFlags::None;
					ChunkHeader->DataSizeCompressed = LoadedChunkSize;
					ChunkHeader->DataSizeUncompressed = LoadedChunkSize;
					ChunkHeader->HashType = HashType;
					ChunkHeader->RollingHash = ChunkHash;
					FMemory::Memcpy(ChunkHeader->SHAHash.Hash, ChunkShaHash.Hash, FSHA1::DigestSize);

					// Release data
					NewChunkFile->ReleaseDataLock();

					// Add it to our cache.
					PlacedInStore.Add(DataId);
					ChunkStore->Put(DataId, TUniquePtr<IChunkDataAccess>(NewChunkFile));
				}

				// Close any open file
				if (FileArchive.IsValid())
				{
					FileArchive->Close();
					FileArchive.Reset();
				}
			}
		}
		LoadRecord.CyclesEnd = FStatsCollector::GetCycles();
		InstallChunkSourceStat->OnLoadComplete(DataId, LoadResult, LoadRecord);
		if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
		{
			return true;
		}
		else
		{
			UnavailableChunks.Add(DataId);
			FailedChunks.Add(DataId);
			return false;
		}
	}

	FSHAHash FInstallChunkSource::GetShaHashForDataSet(const uint8* ChunkData, const uint32 ChunkSize)
	{
		FSHAHash ShaHashCheck;
		FSHA1::HashBuffer(ChunkData, ChunkSize, ShaHashCheck.Hash);
		return ShaHashCheck;
	}
	
	IInstallChunkSource* FInstallChunkSourceFactory::Create(FInstallSourceConfig Configuration, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallChunkSourceStat* InstallChunkSourceStat, const TMultiMap<FString, FBuildPatchAppManifestRef>& InstallationSources, IBuildManifestSet* ManifestSet)
	{
		check(FileSystem != nullptr);
		check(ChunkStore != nullptr);
		check(ChunkReferenceTracker != nullptr);
		check(InstallerError != nullptr);
		check(InstallChunkSourceStat != nullptr);
		return new FInstallChunkSource(MoveTemp(Configuration), FileSystem, ChunkStore, ChunkReferenceTracker, InstallerError, InstallChunkSourceStat, InstallationSources, ManifestSet);
	}

	const TCHAR* ToString(const IInstallChunkSourceStat::ELoadResult& LoadResult)
	{
		switch(LoadResult)
		{
			case IInstallChunkSourceStat::ELoadResult::Success:
				return TEXT("Success");
			case IInstallChunkSourceStat::ELoadResult::MissingHashInfo:
				return TEXT("MissingHashInfo");
			case IInstallChunkSourceStat::ELoadResult::MissingPartInfo:
				return TEXT("MissingPartInfo");
			case IInstallChunkSourceStat::ELoadResult::OpenFileFail:
				return TEXT("OpenFileFail");
			case IInstallChunkSourceStat::ELoadResult::IncorrectFileSize:
				return TEXT("IncorrectFileSize");
			case IInstallChunkSourceStat::ELoadResult::HashCheckFailed:
				return TEXT("HashCheckFailed");
			case IInstallChunkSourceStat::ELoadResult::Aborted:
				return TEXT("Aborted");
			default:
				return TEXT("Unknown");
		}
	}
}
