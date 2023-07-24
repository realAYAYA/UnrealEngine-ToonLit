// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/CloudEnumeration.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Core/BlockStructure.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCloudEnumeration, Log, All);
DEFINE_LOG_CATEGORY(LogCloudEnumeration);

namespace BuildPatchServices
{
	class FCloudEnumeration
		: public ICloudEnumeration
	{
	public:
		FCloudEnumeration(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const EFeatureLevel& OutputFeatureLevel, FStatsCollector* StatsCollector);
		virtual ~FCloudEnumeration();

		virtual bool IsComplete() const override;
		virtual const TSet<uint32>& GetUniqueWindowSizes() const override;
		virtual const TMap<uint64, TSet<FGuid>>& GetChunkInventory() const override;
		virtual const TMap<FGuid, int64>& GetChunkFileSizes() const override;
		virtual const TMap<FGuid, FSHAHash>& GetChunkShaHashes() const override;
		virtual const TMap<FGuid, uint32>& GetChunkWindowSizes() const override;
		virtual bool IsChunkFeatureLevelMatch(const FGuid& ChunkId) const override;
		virtual const uint64& GetChunkHash(const FGuid& ChunkId) const override;
		virtual const FSHAHash& GetChunkShaHash(const FGuid& ChunkId) const override;
		virtual const TMap<FSHAHash, TSet<FGuid>>& GetIdenticalChunks() const override;
	private:
		void EnumerateCloud();
		TFuture<FBuildPatchAppManifestPtr> AsyncLoadManifest(const FString& ManifestFilename);
		void EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest);
		TTuple<TSet<uint32>, TMap<FGuid, uint32>> CalculateChunkWindowSizes(const FBuildPatchAppManifestRef& Manifest);

	private:
		const FString CloudDirectory;
		const FDateTime ManifestAgeThreshold;
		const TCHAR* FeatureLevelChunkSubdir;
		TMap<uint64, TSet<FGuid>> ChunkInventory;
		TMap<FGuid, int64> ChunkFileSizes;
		TMap<FGuid, uint64> ChunkHashes;
		TMap<FGuid, FSHAHash> ChunkShaHashes;
		TSet<uint32> UniqueWindowSizes;
		TMap<FGuid, uint32> ChunkWindowSizes;
		TMap<uint32, TSet<FGuid>> WindowsSizeChunks;
		TSet<FGuid> FeatureLevelMatchedChunks;
		TMap<FSHAHash, TSet<FGuid>> IdenticalChunks;
		FStatsCollector* StatsCollector;
		TFuture<void> Future;
		volatile FStatsCollector::FAtomicValue* StatManifestsLoaded;
		volatile FStatsCollector::FAtomicValue* StatManifestsRejected;
		volatile FStatsCollector::FAtomicValue* StatChunksEnumerated;
		volatile FStatsCollector::FAtomicValue* StatChunksRejected;
		volatile FStatsCollector::FAtomicValue* StatTotalTime;
		volatile FStatsCollector::FAtomicValue* StatUniqueWindowSizes;
	};

	FCloudEnumeration::FCloudEnumeration(const FString& InCloudDirectory, const FDateTime& InManifestAgeThreshold, const EFeatureLevel& OutputFeatureLevel, FStatsCollector* InStatsCollector)
		: CloudDirectory(InCloudDirectory)
		, ManifestAgeThreshold(InManifestAgeThreshold)
		, FeatureLevelChunkSubdir(ManifestVersionHelpers::GetChunkSubdir(OutputFeatureLevel))
		, StatsCollector(InStatsCollector)
	{
		// Create statistics.
		StatManifestsLoaded = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Manifests Loaded"), EStatFormat::Value);
		StatManifestsRejected = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Manifests Rejected"), EStatFormat::Value);
		StatChunksEnumerated = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Chunks Enumerated"), EStatFormat::Value);
		StatChunksRejected = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Chunks Rejected"), EStatFormat::Value);
		StatTotalTime = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Enumeration Time"), EStatFormat::Timer);
		StatUniqueWindowSizes = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Unique Window Sizes"), EStatFormat::Value);

		// Queue thread.
		TFunction<void()> Task = [this]() { EnumerateCloud(); };
		Future = Async(EAsyncExecution::Thread, MoveTemp(Task));
	}

	FCloudEnumeration::~FCloudEnumeration()
	{
		Future.Wait();
	}

	bool FCloudEnumeration::IsComplete() const
	{
		return Future.IsReady();
	}

	const TSet<uint32>& FCloudEnumeration::GetUniqueWindowSizes() const
	{
		Future.Wait();
		return UniqueWindowSizes;
	}

	const TMap<uint64, TSet<FGuid>>& FCloudEnumeration::GetChunkInventory() const
	{
		Future.Wait();
		return ChunkInventory;
	}

	const TMap<FGuid, int64>& FCloudEnumeration::GetChunkFileSizes() const
	{
		Future.Wait();
		return ChunkFileSizes;
	}

	const TMap<FGuid, FSHAHash>& FCloudEnumeration::GetChunkShaHashes() const
	{
		Future.Wait();
		return ChunkShaHashes;
	}

	const TMap<FGuid, uint32>& FCloudEnumeration::GetChunkWindowSizes() const
	{
		Future.Wait();
		return ChunkWindowSizes;
	}

	bool FCloudEnumeration::IsChunkFeatureLevelMatch(const FGuid& ChunkId) const
	{
		Future.Wait();
		return FeatureLevelMatchedChunks.Contains(ChunkId);
	}

	const uint64& FCloudEnumeration::GetChunkHash(const FGuid& ChunkId) const
	{
		Future.Wait();
		return ChunkHashes[ChunkId];
	}

	const FSHAHash& FCloudEnumeration::GetChunkShaHash(const FGuid& ChunkId) const
	{
		Future.Wait();
		return ChunkShaHashes[ChunkId];
	}

	const TMap<FSHAHash, TSet<FGuid>>& FCloudEnumeration::GetIdenticalChunks() const
	{
		Future.Wait();
		return IdenticalChunks;
	}

	void FCloudEnumeration::EnumerateCloud()
	{
		uint64 EnumerationTimer;

		IFileManager& FileManager = IFileManager::Get();

		// Find all manifest files
		FStatsCollector::AccumulateTimeBegin(EnumerationTimer);
		if (FileManager.DirectoryExists(*CloudDirectory))
		{
			TArray<FString> AllManifestFilenames;
			FileManager.FindFiles(AllManifestFilenames, *(CloudDirectory / TEXT("*.manifest")), true, false);
			FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
			FStatsCollector::AccumulateTimeBegin(EnumerationTimer);

			// Load all manifest files
			TArray<TFuture<FBuildPatchAppManifestPtr>> AllManifestFutures;
			for (const FString& ManifestFilename : AllManifestFilenames)
			{
				AllManifestFutures.Add(AsyncLoadManifest(CloudDirectory / ManifestFilename));
			}
			// Process all manifest files
			for (const TFuture<FBuildPatchAppManifestPtr>& ManifestFuture : AllManifestFutures)
			{
				// Determine chunks from manifest file
				FBuildPatchAppManifestPtr BuildManifest = ManifestFuture.Get();
				if (BuildManifest.IsValid())
				{
					EnumerateManifestData(BuildManifest.ToSharedRef());
				}
				FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
				FStatsCollector::AccumulateTimeBegin(EnumerationTimer);
			}
		}
		else
		{
			UE_LOG(LogCloudEnumeration, Log, TEXT("Cloud directory does not exist: %s"), *CloudDirectory);
		}
		FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
	}

	TFuture<FBuildPatchAppManifestPtr> FCloudEnumeration::AsyncLoadManifest(const FString& ManifestFilename)
	{
		return Async(EAsyncExecution::ThreadPool, [this, ManifestFilename]() -> FBuildPatchAppManifestPtr
		{
			if (IFileManager::Get().GetTimeStamp(*ManifestFilename) < ManifestAgeThreshold)
			{
				FStatsCollector::Accumulate(StatManifestsRejected, 1);
				return nullptr;
			}
			FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
			if (BuildManifest->LoadFromFile(ManifestFilename))
			{
				FStatsCollector::Accumulate(StatManifestsLoaded, 1);
				return BuildManifest;
			}
			else
			{
				FStatsCollector::Accumulate(StatManifestsRejected, 1);
				UE_LOG(LogCloudEnumeration, Warning, TEXT("Could not read Manifest file. Data recognition will suffer (%s)"), *ManifestFilename);
			}
			return nullptr;
		});
	}

	void FCloudEnumeration::EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest)
	{
		typedef TTuple<TSet<uint32>, TMap<FGuid, uint32>> FWindowSizes;
		// Check if this chunk will already beling to our matching feature level sub dir - ptr==ptr test is fine, no need to string compare.
		const bool bMatchingChunkSubdir = FeatureLevelChunkSubdir == ManifestVersionHelpers::GetChunkSubdir(Manifest->GetFeatureLevel());
		TFuture<FWindowSizes> CalculateChunkWindowSizesFuture = Async(EAsyncExecution::TaskGraph, [&](){ return CalculateChunkWindowSizes(Manifest); });
		TArray<FGuid> DataList;
		Manifest->GetDataList(DataList);
		if (!Manifest->IsFileDataManifest())
		{
			for (const FGuid& DataGuid : DataList)
			{
				const FChunkInfo* ChunkInfo = Manifest->GetChunkInfo(DataGuid);
				const bool bChunkAccepted = ChunkInfo != nullptr && ChunkInfo->Hash != 0;
				if (bChunkAccepted)
				{
					bool bIsAlreadyInSet = false;
					ChunkInventory.FindOrAdd(ChunkInfo->Hash).Add(DataGuid, &bIsAlreadyInSet);
					if (!bIsAlreadyInSet)
					{
						// Added new chunk, fill out all the various tracking data.
						ChunkFileSizes.Add(DataGuid, Manifest->GetDataSize(DataGuid));
						ChunkHashes.Add(DataGuid, ChunkInfo->Hash);
						FMemory::Memcpy(ChunkShaHashes.FindOrAdd(DataGuid).Hash, ChunkInfo->ShaHash.Hash, FSHA1::DigestSize);
						UniqueWindowSizes.Add(ChunkInfo->WindowSize);
						ChunkWindowSizes.Add(DataGuid, ChunkInfo->WindowSize);
						IdenticalChunks.FindOrAdd(ChunkInfo->ShaHash).Add(DataGuid);
						FStatsCollector::Accumulate(StatChunksEnumerated, 1);
					}
					if (bMatchingChunkSubdir)
					{
						FeatureLevelMatchedChunks.Add(DataGuid);
					}
				}
				else
				{
					FStatsCollector::Accumulate(StatChunksRejected, 1);
				}
			}
		}
		else
		{
			FStatsCollector::Accumulate(StatManifestsRejected, 1);
		}
		FWindowSizes CalculatedChunkWindowSizes = CalculateChunkWindowSizesFuture.Get();
		UniqueWindowSizes.Append(CalculatedChunkWindowSizes.Get<0>());
		ChunkWindowSizes.Append(CalculatedChunkWindowSizes.Get<1>());
		for (const TPair<FGuid, uint32>& CalculatedChunkWindowSizePair : CalculatedChunkWindowSizes.Get<1>())
		{
			WindowsSizeChunks.FindOrAdd(CalculatedChunkWindowSizePair.Value).Add(CalculatedChunkWindowSizePair.Key);
		}
		FStatsCollector::Set(StatUniqueWindowSizes, UniqueWindowSizes.Num());
	}

	TTuple<TSet<uint32>, TMap<FGuid, uint32>> FCloudEnumeration::CalculateChunkWindowSizes(const FBuildPatchAppManifestRef& Manifest)
	{
		using namespace BuildPatchServices;
		TTuple<TSet<uint32>, TMap<FGuid, uint32>> DiscoveredDetails;
		// We only need to perform calculations if the manifest is a specific version, otherwise the default, or the serialised value, is the correct one to use.
		if (Manifest->GetFeatureLevel() == EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo)
		{
			TMap<FGuid, FBlockStructure> ChunkBlockStructures;
			TMap<uint32, TSet<FGuid>> WindowSizeChunks;
			TArray<FString> Files;
			Manifest->GetFileList(Files);
			for (const FString& File : Files)
			{
				const FFileManifest* FileManifest = Manifest->GetFileManifest(File);
				if (FileManifest != nullptr)
				{
					for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
					{
						FBlockStructure& ChunkBlockStructure = ChunkBlockStructures.FindOrAdd(ChunkPart.Guid);
						ChunkBlockStructure.Add(ChunkPart.Offset, ChunkPart.Size);
					}
				}
			}
			for (const TPair<FGuid, FBlockStructure>& ChunkBlockStructurePair : ChunkBlockStructures)
			{
				const FBlockStructure& ChunkBlockStructure = ChunkBlockStructurePair.Value;
				// If we got a single block, from 0 to n, that's going to be the window size.
				if (ChunkBlockStructure.GetHead() != nullptr && ChunkBlockStructure.GetHead() == ChunkBlockStructure.GetTail() && ChunkBlockStructure.GetHead()->GetOffset() == 0)
				{
					const uint32 ChunkWindowSize = static_cast<uint32>(ChunkBlockStructure.GetHead()->GetSize());
					DiscoveredDetails.Get<0>().Add(ChunkWindowSize);
					DiscoveredDetails.Get<1>().Add(ChunkBlockStructurePair.Key, ChunkWindowSize);
					WindowSizeChunks.FindOrAdd(ChunkWindowSize).Add(ChunkBlockStructurePair.Key);
				}
			}
			// We check any chunks that have their own unique size as these were probably padded.
			TSet<FGuid> ChunksToCheck;
			for (const TPair<uint32, TSet<FGuid>>& WindowSizeChunksPair : WindowSizeChunks)
			{
				if (WindowSizeChunksPair.Value.Num() <= 1)
				{
					DiscoveredDetails.Get<0>().Remove(WindowSizeChunksPair.Key);
					for (const FGuid& TheChunk : WindowSizeChunksPair.Value)
					{
						DiscoveredDetails.Get<1>().Remove(TheChunk);
						ChunksToCheck.Add(TheChunk);
					}
				}
			}
			for (const FGuid& ChunkToCheck : ChunksToCheck)
			{
				FString FinalChunkPartFilename = FBuildPatchUtils::GetDataFilename(Manifest, CloudDirectory, ChunkToCheck);
				IFileManager& FileManager = IFileManager::Get();
				TUniquePtr<FArchive> FinalChunkPartFile(FileManager.CreateFileReader(*FinalChunkPartFilename));
				if (FinalChunkPartFile.IsValid())
				{
					FChunkHeader ChunkHeader;
					*FinalChunkPartFile << ChunkHeader;
					DiscoveredDetails.Get<0>().Add(ChunkHeader.DataSizeUncompressed);
					DiscoveredDetails.Get<1>().Add(ChunkToCheck, ChunkHeader.DataSizeUncompressed);
				}
			}
		}
		return DiscoveredDetails;
	}

	ICloudEnumeration* FCloudEnumerationFactory::Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const EFeatureLevel& OutputFeatureLevel, FStatsCollector* StatsCollector)
	{
		return new FCloudEnumeration(CloudDirectory, ManifestAgeThreshold, OutputFeatureLevel, StatsCollector);
	}
}
