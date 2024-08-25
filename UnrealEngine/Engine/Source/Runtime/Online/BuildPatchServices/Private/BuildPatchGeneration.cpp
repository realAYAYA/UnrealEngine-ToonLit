// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchGeneration.h"
#include "Templates/UniquePtr.h"
#include "Templates/Greater.h"
#include "Templates/Tuple.h"
#include "Algo/Sort.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CommandLine.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "BuildPatchManifest.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchHash.h"
#include "Core/BlockStructure.h"
#include "Core/BlockData.h"
#include "Common/FileSystem.h"
#include "Data/ChunkData.h"
#include "Generation/DataScanner.h"
#include "Generation/BuildStreamer.h"
#include "Generation/CloudEnumeration.h"
#include "Generation/ManifestBuilder.h"
#include "Generation/FileAttributesParser.h"
#include "Generation/ChunkWriter.h"
#include "Generation/ChunkMatchProcessor.h"
#include "BuildPatchUtil.h"

using namespace BuildPatchServices;

DECLARE_LOG_CATEGORY_EXTERN(LogPatchGeneration, Log, All);
DEFINE_LOG_CATEGORY(LogPatchGeneration);

namespace BuildPatchServices
{
	struct FScannerDetails
	{
	public:
		FScannerDetails(int32 InLayer, uint64 InLayerOffset, bool bInIsFinalScanner, uint64 InPaddingSize, TArray<uint8> InData, FBlockStructure InStructure, const TArray<uint32>& ChunkWindowSizes, const ICloudEnumeration* CloudEnumeration, FStatsCollector* StatsCollector)
			: Layer(InLayer)
			, LayerOffset(InLayerOffset)
			, bIsFinalScanner(bInIsFinalScanner)
			, PaddingSize(InPaddingSize)
			, Data(MoveTemp(InData))
			, Structure(MoveTemp(InStructure))
			, Scanner(FDataScannerFactory::Create(ChunkWindowSizes, Data, CloudEnumeration, StatsCollector))
		{}

	public:
		const int32 Layer;
		const uint64 LayerOffset;
		const bool bIsFinalScanner;
		const uint64 PaddingSize;
		const TArray<uint8> Data;
		const FBlockStructure Structure;
		const TUniquePtr<IDataScanner> Scanner;
	};
}

namespace PatchGenerationHelpers
{
	int32 GetMaxScannerBacklogCount()
	{
		int32 MaxScannerBacklogCount = 75;
		GConfig->GetInt(TEXT("BuildPatchServices"), TEXT("MaxScannerBacklog"), MaxScannerBacklogCount, GEngineIni);
		MaxScannerBacklogCount = FMath::Clamp<int32>(MaxScannerBacklogCount, 5, 500);
		return MaxScannerBacklogCount;
	}

	bool ScannerArrayFull(const TArray<TUniquePtr<FScannerDetails>>& Scanners)
	{
		static int32 MaxScannerBacklogCount = GetMaxScannerBacklogCount();
		return (FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners()) || (Scanners.Num() >= MaxScannerBacklogCount);
	}

	FSHAHash GetShaForDataSet(const uint8* DataSet, uint32 DataSize)
	{
		FSHAHash SHAHash;
		FSHA1::HashBuffer(DataSet, DataSize, SHAHash.Hash);
		return SHAHash;
	}

}

bool FBuildDataGenerator::ChunkBuildDirectory(const BuildPatchServices::FChunkBuildConfiguration& Settings)
{
	const uint64 StartTime = FStatsCollector::GetCycles();
	bool bIsGameThread = IsInGameThread();

	// Check for the required output filename.
	if (Settings.OutputFilename.IsEmpty())
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Manifest OutputFilename was not provided"));
		return false;
	}

	// Ensure that cloud directory exists, and create it if not.
	IFileManager::Get().MakeDirectory(*Settings.CloudDirectory, true);
	if (!IFileManager::Get().DirectoryExists(*Settings.CloudDirectory))
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Unable to create specified cloud directory %s"), *Settings.CloudDirectory);
		return false;
	}

	// Output to log for builder info.
	UE_LOG(LogPatchGeneration, Log, TEXT("Running NEW Chunks Patch Generation for: %u:%s %s"), Settings.AppId, *Settings.AppName, *Settings.BuildVersion);

	// The last time we logged out data processed.
	double LastProgressLog = FPlatformTime::Seconds();
	const double TimeGenStarted = LastProgressLog;

	// Load settings from config.
	float GenerationScannerSizeMegabytes = 32.5f;
	float StatsLoggerTimeSeconds = 10.0f;
	GConfig->GetFloat(TEXT("BuildPatchServices"), TEXT("GenerationScannerSizeMegabytes"), GenerationScannerSizeMegabytes, GEngineIni);
	GConfig->GetFloat(TEXT("BuildPatchServices"), TEXT("StatsLoggerTimeSeconds"), StatsLoggerTimeSeconds, GEngineIni);
	GenerationScannerSizeMegabytes = FMath::Clamp<float>(GenerationScannerSizeMegabytes, 10.0f, 500.0f);
	StatsLoggerTimeSeconds = FMath::Clamp<float>(StatsLoggerTimeSeconds, 1.0f, 60.0f);
	const uint64 ScannerDataSize = GenerationScannerSizeMegabytes * 1048576;

	// Create stat collector.
	TUniquePtr<FStatsCollector> StatsCollector(FStatsCollectorFactory::Create());

	// Setup Generation stats.
	volatile int64* StatTotalTime = StatsCollector->CreateStat(TEXT("Generation: Total Time"), EStatFormat::Timer);
	volatile int64* StatLayers = StatsCollector->CreateStat(TEXT("Generation: Layers"), EStatFormat::Value);
	volatile int64* StatNumScanners = StatsCollector->CreateStat(TEXT("Generation: Scanner Backlog"), EStatFormat::Value);
	volatile int64* StatUnknownDataAlloc = StatsCollector->CreateStat(TEXT("Generation: Unmatched Buffers Allocation"), EStatFormat::DataSize);
	volatile int64* StatUnknownDataNum = StatsCollector->CreateStat(TEXT("Generation: Unmatched Buffers Use"), EStatFormat::DataSize);
	int64 MaxLayer = 0;

	// Create a chunk writer.
	TUniquePtr<IFileSystem> FileSystem(FFileSystemFactory::Create());
	TUniquePtr<IChunkDataSerialization> ChunkDataSerialization(FChunkDataSerializationFactory::Create(FileSystem.Get(), Settings.FeatureLevel));
	TUniquePtr<IParallelChunkWriter> ChunkWriter(FParallelChunkWriterFactory::Create({5, 5, 50, 8, Settings.CloudDirectory, Settings.FeatureLevel}, FileSystem.Get(), ChunkDataSerialization.Get(), StatsCollector.Get()));

	// Create a manifest details.
	FManifestDetails ManifestDetails;
	ManifestDetails.FeatureLevel = Settings.FeatureLevel;
	ManifestDetails.AppId = Settings.AppId;
	ManifestDetails.AppName = Settings.AppName;
	ManifestDetails.BuildVersion = Settings.BuildVersion;
	ManifestDetails.LaunchExe = Settings.LaunchExe;
	ManifestDetails.LaunchCommand = Settings.LaunchCommand;
	ManifestDetails.PrereqIds = Settings.PrereqIds;
	ManifestDetails.PrereqName = Settings.PrereqName;
	ManifestDetails.PrereqPath = Settings.PrereqPath;
	ManifestDetails.PrereqArgs = Settings.PrereqArgs;
	ManifestDetails.CustomFields = Settings.CustomFields;

	// Load the required file attributes.
	if (!Settings.AttributeListFile.IsEmpty())
	{
		FFileAttributesParserRef FileAttributesParser = FFileAttributesParserFactory::Create();
		if (!FileAttributesParser->ParseFileAttributes(Settings.AttributeListFile, ManifestDetails.FileAttributesMap))
		{
			UE_LOG(LogPatchGeneration, Error, TEXT("Attributes list file did not parse %s"), *Settings.AttributeListFile);
			return false;
		}
	}

	// Enumerate Chunks.
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	TUniquePtr<ICloudEnumeration> CloudEnumeration(FCloudEnumerationFactory::Create(Settings.CloudDirectory, Cutoff, Settings.FeatureLevel, StatsCollector.Get()));

	// Start the build stream.
	FDirectoryBuildStreamerConfig BuildStreamConfig({Settings.RootDirectory, Settings.InputListFile, Settings.IgnoreListFile});
	FDirectoryBuildStreamerDependencies BuildStreamDependencies({StatsCollector.Get(), FileSystem.Get()});
	TUniquePtr<IDirectoryBuildStreamer> BuildStream(FBuildStreamerFactory::Create(MoveTemp(BuildStreamConfig), MoveTemp(BuildStreamDependencies)));
	TArray<FString> EnumeratedFiles = BuildStream->GetAllFilenames();

	// Check existence of launch exe, if specified.
	if (!Settings.LaunchExe.IsEmpty() && !EnumeratedFiles.Contains(FPaths::Combine(Settings.RootDirectory, Settings.LaunchExe)))
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Provided launch executable file was not found within the build root. %s"), *Settings.LaunchExe);
		return false;
	}

	// Check existence of prereq exe, if specified.
	if (!Settings.PrereqPath.IsEmpty() && !EnumeratedFiles.Contains(FPaths::Combine(Settings.RootDirectory, Settings.PrereqPath)))
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Provided prerequisite executable file was not found within the build root. %s"), *Settings.PrereqPath);
		return false;
	}

	// We've got to wait for enumeration to complete as that shares a thread pool.
	while (CloudEnumeration->IsComplete() == false)
	{
		// Log collected stats.
		if (bIsGameThread)
		{
			GLog->FlushThreadedLogs();
		}
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		StatsCollector->LogStats(StatsLoggerTimeSeconds);

		// Sleep to allow other threads.
		FPlatformProcess::Sleep(0.01f);
	}

	// Grab the window sizes we are trying to match against.
	TArray<uint32> WindowSizes;
	if (Settings.bShouldMatchAnyWindowSize)
	{
		WindowSizes.Append(CloudEnumeration->GetUniqueWindowSizes().Array());
	}
	else
	{
		WindowSizes.Add(Settings.OutputChunkWindowSize);
	}
	Algo::Sort(WindowSizes, TGreater<uint32>());
	const uint32 LargestWindowSize = FMath::Max<uint32>(Settings.OutputChunkWindowSize, WindowSizes.Num() > 0 ? WindowSizes[0] : 0);
	const uint64 ScannerOverlapSize = LargestWindowSize - 1;

	// Construct the chunk match processor.
	TUniquePtr<IChunkMatchProcessor> ChunkMatchProcessor(FChunkMatchProcessorFactory::Create());

	// Keep a record of the new chunk inventory.
	TMap<uint64, TSet<FGuid>> ChunkInventory;
	TMap<FGuid, FSHAHash> ChunkShaHashes;

	// Tracking info per layer for rescanning.
	TMap<int32, uint64> LayerToScannerCount;
	TMap<int32, FBlockStructure> LayerToBuildSpaceStructure;
	TMap<int32, uint64> LayerToCreatedScannerOffset;
	TMap<int32, uint64> LayerToScannedSize;
	TMap<int32, uint64> LayerToTotalDataSize;
	TMap<int32, FBlockStructure> LayerToUnknownLayerSpaceStructure;
	TMap<int32, FBlockStructure> LayerToUnknownBuildSpaceStructure;
	TMap<int32, TBlockData<uint8>> LayerToLayerSpaceBlockData;

	// This is a blatant hack :(
	TMap<FGuid, uint32> OriginalWindowSizes;

	// Create the manifest builder.
	FManifestBuilderConfig ManifestBuilderConfig;
	ManifestBuilderConfig.bAllowEmptyBuilds = Settings.bAllowEmptyBuild;
	IManifestBuilderRef ManifestBuilder = FManifestBuilderFactory::Create(FileSystem.Get(), ManifestBuilderConfig, ManifestDetails);

	FBlockStructure AcceptedBuildSpaceMatches;
	FBlockStructure CreatedBuildSpaceMatches;
	TSet<FGuid> NewCreatedChunks;
	TMap<int32, FBlockStructure> LayerCreatingScannersTest;
	TMap<int32, FBlockStructure> LayerCreatingScannersLayerSpaceTest;
	TMap<int32, FBlockStructure> LayerCreatingChunksTest;
	TMap<int32, bool> LayerCreatingFinalTest;

	// Run the main loop.
	TArray<uint8> DataBuffer;
	uint64 DataBufferFirstIdx = 0;
	uint32 ReadLen = 0;
	TArray<TUniquePtr<FScannerDetails>> Scanners;
	bool bHasUnknownData = true;
	while (!BuildStream->HasAborted() && (!BuildStream->IsEndOfData() || Scanners.Num() > 0 || bHasUnknownData))
	{
		// Grab a scanner result.
		if (Scanners.Num() > 0 && Scanners[0]->Scanner->IsComplete())
		{
			FScannerDetails& ScannerDetails = *Scanners[0];
			TArray<FChunkMatch> ChunkMatches = ScannerDetails.Scanner->GetResultWhenComplete();
			for (FChunkMatch& ChunkMatch : ChunkMatches)
			{
				// Translate to build space.
				FBlockStructure BuildSpaceChunkStructure;
				const uint64 BytesFound = ScannerDetails.Structure.SelectSerialBytes(ChunkMatch.DataOffset, ChunkMatch.WindowSize, BuildSpaceChunkStructure);
				const bool bFoundOk = ScannerDetails.bIsFinalScanner || BytesFound == ChunkMatch.WindowSize;
				if (!bFoundOk)
				{
					// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
					UE_LOG(LogPatchGeneration, Error, TEXT("Chunk match was not within scanner's data structure."));
					return false;
				}

				ChunkMatch.DataOffset += ScannerDetails.LayerOffset;
				if (ChunkMatch.WindowSize != BytesFound)
				{
					OriginalWindowSizes.Add(ChunkMatch.ChunkGuid, ChunkMatch.WindowSize);
				}
				ChunkMatch.WindowSize = BytesFound;

				ChunkMatchProcessor->ProcessMatch(ScannerDetails.Layer, ChunkMatch, MoveTemp(BuildSpaceChunkStructure));
			}

			FBlockStructure OverlapStructure = LayerToBuildSpaceStructure.FindOrAdd(ScannerDetails.Layer).Intersect(ScannerDetails.Structure);
			uint64 OverlapBytes = BlockStructureHelpers::CountSize(OverlapStructure);
			check(OverlapBytes == ScannerOverlapSize || ScannerDetails.LayerOffset == 0);

			// Store the layer build space.
			LayerToBuildSpaceStructure.FindOrAdd(ScannerDetails.Layer).Add(ScannerDetails.Structure);

			// Add to layer space block data. We include padding that comes at the end of any layer as that may be included.
			const uint64 LayerDataStartIndex = ScannerDetails.LayerOffset == 0 ? 0 : ScannerOverlapSize;
			const uint64 LayerDataSize = ScannerDetails.LayerOffset == 0 ? ScannerDetails.Data.Num() : ScannerDetails.Data.Num() - ScannerOverlapSize;
			TBlockData<uint8>& LayerSpaceBlockData = LayerToLayerSpaceBlockData.FindOrAdd(ScannerDetails.Layer);
			LayerSpaceBlockData.AddData(FBlockStructure(ScannerDetails.LayerOffset + LayerDataStartIndex, LayerDataSize), ScannerDetails.Data.GetData() + LayerDataStartIndex, LayerDataSize);

			// Give some flush time to the processor.
			check(ScannerDetails.PaddingSize == 0 || ScannerDetails.bIsFinalScanner);
			const FBlockRange ScannerRange = FBlockRange::FromFirstAndSize(ScannerDetails.LayerOffset, ScannerDetails.Data.Num() - ScannerDetails.PaddingSize);
			const uint64 SafeFlushSize = ScannerDetails.bIsFinalScanner ? ScannerRange.GetLast() + 1 : ScannerRange.GetLast() - ScannerOverlapSize;
			ChunkMatchProcessor->FlushLayer(ScannerDetails.Layer, SafeFlushSize);
			if (ScannerDetails.bIsFinalScanner)
			{
				LayerToTotalDataSize.Add(ScannerDetails.Layer, SafeFlushSize);
			}

			// Remove scanner from list.
			LayerToScannerCount.FindOrAdd(ScannerDetails.Layer)--;
			Scanners.RemoveAt(0);
		}

		// Handle accepted chunk matches, and unknown data tracking.
		for (int32 LayerIdx = 0; LayerIdx <= MaxLayer; ++LayerIdx)
		{
			TArray<FMatchEntry> AcceptedChunkMatches;
			const FBlockStructure& LayerBuildSpaceStructure = LayerToBuildSpaceStructure.FindOrAdd(LayerIdx);
			uint64& LayerScannedSize = LayerToScannedSize.FindOrAdd(LayerIdx);
			const FBlockRange CollectionRange = ChunkMatchProcessor->CollectLayer(LayerIdx, AcceptedChunkMatches);
			if (CollectionRange.GetSize() > 0)
			{
				// Add new chunk matches to the manifest builder, and track new unknown data.
				TBlockData<uint8>& LayerSpaceBlockData = LayerToLayerSpaceBlockData.FindOrAdd(LayerIdx);
				FBlockStructure BlockDataToRemove;
				FBlockStructure NewUnknownLayerSpaceStructure(CollectionRange.GetFirst(), CollectionRange.GetSize());
				FBlockStructure NewUnknownBuildSpaceStructure;
				const uint64 BytesFound = LayerBuildSpaceStructure.SelectSerialBytes(CollectionRange.GetFirst(), CollectionRange.GetSize(), NewUnknownBuildSpaceStructure);
				checkSlow(BytesFound == CollectionRange.GetSize());
				const uint64 BeforeDataCount = LayerSpaceBlockData.GetDataCount();
				const uint64 BeforeStructureCount = CollectionRange.GetSize();
				for (FMatchEntry& AcceptedChunkMatch : AcceptedChunkMatches)
				{
					const FChunkMatch& ChunkMatch = AcceptedChunkMatch.ChunkMatch;
					const FBlockStructure& BlockStructure = AcceptedChunkMatch.BlockStructure;
					const FBlockStructure LayerSpaceStructure(ChunkMatch.DataOffset, ChunkMatch.WindowSize);

					check(BlockStructureHelpers::CountSize(LayerSpaceStructure.Intersect(NewUnknownLayerSpaceStructure)) == ChunkMatch.WindowSize);
					check(BlockStructureHelpers::CountSize(LayerSpaceStructure.Intersect(BlockDataToRemove)) == 0);
					check(BlockStructureHelpers::CountSize(NewUnknownBuildSpaceStructure.Intersect(BlockStructure)) == ChunkMatch.WindowSize);
					checkf(CreatedBuildSpaceMatches.Intersect(BlockStructure).GetHead() == nullptr, TEXT("ACCEPTEDCHUNK Overlap %llu bytes with created struct!"), BlockStructureHelpers::CountSize(CreatedBuildSpaceMatches.Intersect(BlockStructure)));
					checkf(AcceptedBuildSpaceMatches.Intersect(BlockStructure).GetHead() == nullptr, TEXT("ACCEPTEDCHUNK Overlap %llu bytes with accepted struct!"), BlockStructureHelpers::CountSize(AcceptedBuildSpaceMatches.Intersect(BlockStructure)));

					AcceptedBuildSpaceMatches.Add(BlockStructure);
					NewUnknownBuildSpaceStructure.Remove(BlockStructure);
					ManifestBuilder->AddChunkMatch(ChunkMatch.ChunkGuid, BlockStructure);
					// Do we need to re-save the chunk at current feature level?
					if (!CloudEnumeration->IsChunkFeatureLevelMatch(ChunkMatch.ChunkGuid))
					{
						// Grab the data.
						TArray<uint8> ChunkDataArray;
						LayerSpaceBlockData.CopyTo(ChunkDataArray, LayerSpaceStructure);
						check(ChunkDataArray.Num() == ChunkMatch.WindowSize);
						// Ensure padding if necessary.
						const uint32 TrueWindowSize = OriginalWindowSizes.Contains(ChunkMatch.ChunkGuid) ? OriginalWindowSizes[ChunkMatch.ChunkGuid] : ChunkMatch.WindowSize;
						ChunkDataArray.SetNumZeroed(TrueWindowSize);

						// Save it out.
						const uint64& ChunkHash = CloudEnumeration->GetChunkHash(ChunkMatch.ChunkGuid);
						const FSHAHash& ChunkSha = CloudEnumeration->GetChunkShaHash(ChunkMatch.ChunkGuid);
						check(ChunkHash == FRollingHash::GetHashForDataSet(ChunkDataArray.GetData(), TrueWindowSize));
						check(ChunkSha == PatchGenerationHelpers::GetShaForDataSet(ChunkDataArray.GetData(), TrueWindowSize));
						ChunkWriter->AddChunkData(MoveTemp(ChunkDataArray), ChunkMatch.ChunkGuid, ChunkHash, ChunkSha);
					}
					NewUnknownLayerSpaceStructure.Remove(LayerSpaceStructure);
					BlockDataToRemove.Add(LayerSpaceStructure);
				}
				const uint64 BlockDataToRemoveSize = BlockStructureHelpers::CountSize(BlockDataToRemove);
				LayerSpaceBlockData.RemoveData(BlockDataToRemove);
				const uint64 AfterDataCount = LayerSpaceBlockData.GetDataCount();
				const uint64 AfterStructureCount = BlockStructureHelpers::CountSize(NewUnknownLayerSpaceStructure);
				const uint64 RemovedDataCount = BeforeDataCount - AfterDataCount;
				const uint64 RemovedStructureCount = BeforeStructureCount - AfterStructureCount;
				check(BeforeDataCount >= AfterDataCount);
				check(BeforeStructureCount >= AfterStructureCount);
				check(RemovedDataCount == RemovedStructureCount);
				check(RemovedDataCount == BlockDataToRemoveSize);

				check(BlockStructureHelpers::CountSize(NewUnknownLayerSpaceStructure) == BlockStructureHelpers::CountSize(NewUnknownBuildSpaceStructure));

				// Grab layer tracking.
				FBlockStructure& UnknownLayerSpaceStructure = LayerToUnknownLayerSpaceStructure.FindOrAdd(LayerIdx);
				FBlockStructure& UnknownBuildSpaceStructure = LayerToUnknownBuildSpaceStructure.FindOrAdd(LayerIdx);

				// Expect to never get overlap with this new system.
				check(!BlockStructureHelpers::HasIntersection(UnknownLayerSpaceStructure, NewUnknownLayerSpaceStructure));
				check(!BlockStructureHelpers::HasIntersection(UnknownBuildSpaceStructure, NewUnknownBuildSpaceStructure));

				// Add unknown tracking to the structures.
				UnknownLayerSpaceStructure.Add(NewUnknownLayerSpaceStructure);
				UnknownBuildSpaceStructure.Add(NewUnknownBuildSpaceStructure);
				check(BlockStructureHelpers::CountSize(UnknownLayerSpaceStructure) == BlockStructureHelpers::CountSize(UnknownBuildSpaceStructure));

				// Count processed data
				LayerScannedSize = CollectionRange.GetLast() + 1;
			}
		}

		// Collect unknown data into new chunks.
		for (int32 LayerIdx = 0; LayerIdx <= MaxLayer; ++LayerIdx)
		{
			FBlockStructure& UnknownLayerSpaceStructure = LayerToUnknownLayerSpaceStructure.FindOrAdd(LayerIdx);
			FBlockStructure& UnknownBuildSpaceStructure = LayerToUnknownBuildSpaceStructure.FindOrAdd(LayerIdx);

			check(BlockStructureHelpers::CountSize(UnknownLayerSpaceStructure) == BlockStructureHelpers::CountSize(UnknownBuildSpaceStructure));

			const uint64& LayerScannedSize = LayerToScannedSize.FindOrAdd(LayerIdx);
			const bool bLayerComplete = LayerToTotalDataSize.Contains(LayerIdx) && (LayerScannedSize >= LayerToTotalDataSize[LayerIdx]);

			FBlockStructure ChunkedLayerSpaceStructure;
			FBlockStructure ChunkedBuildSpaceStructure;
			const FBlockEntry* UnknownLayerBlock = UnknownLayerSpaceStructure.GetHead();
			const bool bIsFinalSingleBlock = bLayerComplete && UnknownLayerBlock != nullptr && UnknownLayerBlock == UnknownLayerSpaceStructure.GetTail();
			uint64 UnknownBlockByteCount = 0;

			if (UnknownLayerBlock != nullptr)
			{
				UE_LOG(LogPatchGeneration, VeryVerbose, TEXT("Unknown layer[%d] data at %llu bytes"), LayerIdx, BlockStructureHelpers::CountSize(UnknownLayerSpaceStructure));
			}

			TBlockData<uint8>& LayerSpaceBlockData = LayerToLayerSpaceBlockData.FindOrAdd(LayerIdx);
			FBlockStructure BlockDataToRemove;
			while (UnknownLayerBlock != nullptr)
			{
				uint64 UnknownBlockOffset = UnknownLayerBlock->GetOffset();
				uint64 UnknownBlockSize = UnknownLayerBlock->GetSize();
				bool bFinalLayerChunk = false;
				while (UnknownBlockSize >= LargestWindowSize || (bIsFinalSingleBlock && !bFinalLayerChunk))
				{
					// Copy out the chunk data.
					FBlockStructure NewChunkLayerSpace(UnknownBlockOffset, FMath::Min<uint64>(Settings.OutputChunkWindowSize, UnknownBlockSize));
					checkf((BlockStructureHelpers::CountSize(NewChunkLayerSpace) == Settings.OutputChunkWindowSize) || bIsFinalSingleBlock, TEXT("Expected correct chunk size! (%llu == %u) || %d"), BlockStructureHelpers::CountSize(NewChunkLayerSpace), Settings.OutputChunkWindowSize, bIsFinalSingleBlock);
					TArray<uint8> NewChunkDataArray;
					LayerSpaceBlockData.CopyTo(NewChunkDataArray, NewChunkLayerSpace);
					check( bIsFinalSingleBlock || NewChunkDataArray.Num() == Settings.OutputChunkWindowSize);
					check(!bIsFinalSingleBlock || NewChunkDataArray.Num() == FMath::Min<uint64>(Settings.OutputChunkWindowSize, UnknownBlockSize));
					// Ensure padding if necessary.
					NewChunkDataArray.SetNumZeroed(Settings.OutputChunkWindowSize);

					// Create data for new chunk.
					const FGuid NewChunkGuid = FGuid::NewGuid();
					const uint64 NewChunkHash = FRollingHash::GetHashForDataSet(NewChunkDataArray.GetData(), Settings.OutputChunkWindowSize);
					const FSHAHash NewChunkSha = PatchGenerationHelpers::GetShaForDataSet(NewChunkDataArray.GetData(), Settings.OutputChunkWindowSize);

					// Save it out.
					ChunkWriter->AddChunkData(MoveTemp(NewChunkDataArray), NewChunkGuid, NewChunkHash, NewChunkSha);
					ChunkShaHashes.Add(NewChunkGuid, NewChunkSha);
					ChunkInventory.FindOrAdd(NewChunkHash).Add(NewChunkGuid);
					BlockDataToRemove.Add(NewChunkLayerSpace);

					UE_LOG(LogPatchGeneration, Verbose, TEXT("Created layer[%d] chunk @ %llu for %d out of %llu"), LayerIdx, UnknownBlockOffset, Settings.OutputChunkWindowSize, UnknownBlockSize);

					// Add to manifest builder.
					FBlockStructure BuildSpaceChunkStructure;
					const uint64 ChunkBuildSize = UnknownBuildSpaceStructure.SelectSerialBytes(UnknownBlockByteCount, Settings.OutputChunkWindowSize, BuildSpaceChunkStructure);
					bFinalLayerChunk = bIsFinalSingleBlock && (UnknownBlockSize == ChunkBuildSize);

					// Chunk build space should either be window size, or size minus any padding if the final piece.
					check( bIsFinalSingleBlock || ChunkBuildSize == Settings.OutputChunkWindowSize);
					check(!bIsFinalSingleBlock || ChunkBuildSize == FMath::Min<uint64>(Settings.OutputChunkWindowSize, UnknownBlockSize));

					// This new chunk must not overlap any previous chunks.
					check(CreatedBuildSpaceMatches.Intersect(BuildSpaceChunkStructure).GetHead() == nullptr);
					check(AcceptedBuildSpaceMatches.Intersect(BuildSpaceChunkStructure).GetHead() == nullptr);

					CreatedBuildSpaceMatches.Add(BuildSpaceChunkStructure);
					NewCreatedChunks.Add(NewChunkGuid);

					LayerCreatingChunksTest.FindOrAdd(LayerIdx).Add(BuildSpaceChunkStructure);
					ManifestBuilder->AddChunkMatch(NewChunkGuid, BuildSpaceChunkStructure);

					// Track data selected.
					ChunkedLayerSpaceStructure.Add(UnknownBlockOffset, ChunkBuildSize);
					ChunkedBuildSpaceStructure.Add(BuildSpaceChunkStructure);

					check(BlockStructureHelpers::CountSize(ChunkedLayerSpaceStructure) == BlockStructureHelpers::CountSize(ChunkedBuildSpaceStructure));

					UnknownBlockOffset += ChunkBuildSize;
					UnknownBlockSize -= ChunkBuildSize;
					UnknownBlockByteCount += ChunkBuildSize;
					check(!bFinalLayerChunk || UnknownBlockSize == 0);
				}
				UnknownBlockByteCount += UnknownBlockSize;
				UnknownLayerBlock = UnknownLayerBlock->GetNext();
				check(!bFinalLayerChunk || UnknownLayerBlock == nullptr);
			}
			UnknownLayerSpaceStructure.Remove(ChunkedLayerSpaceStructure);
			UnknownBuildSpaceStructure.Remove(ChunkedBuildSpaceStructure);
			LayerSpaceBlockData.RemoveData(BlockDataToRemove);
		}

		// Create new scanners from unknown data.
		while (!PatchGenerationHelpers::ScannerArrayFull(Scanners))
		{
			bool bScannerCreated = false;
			for (int32 LayerIdx = 0; LayerIdx <= MaxLayer; ++LayerIdx)
			{
				// Check that we have enough slack space in the data array to be queuing up more scanners on the next layer.
				const int32 NextLayer = LayerIdx + 1;
				const uint64 NextLayerSpaceBlockCount = LayerToLayerSpaceBlockData.FindOrAdd(NextLayer).GetDataCount();
				const int32 OneGigabyte = 1073741824;
				const bool bQueuedDataFull = NextLayerSpaceBlockCount > OneGigabyte;
				if (bQueuedDataFull)
				{
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Not making new scanners on next layer %d due to current backlog %llu bytes"), NextLayer, NextLayerSpaceBlockCount);
					break;
				}

				FBlockStructure& UnknownLayerSpaceStructure = LayerToUnknownLayerSpaceStructure.FindOrAdd(LayerIdx);
				FBlockStructure& UnknownBuildSpaceStructure = LayerToUnknownBuildSpaceStructure.FindOrAdd(LayerIdx);
				TBlockData<uint8>& LayerSpaceBlockData = LayerToLayerSpaceBlockData.FindOrAdd(LayerIdx);
				const uint64& LayerScannedSize = LayerToScannedSize.FindOrAdd(LayerIdx);
				const bool bLayerComplete = LayerToTotalDataSize.Contains(LayerIdx) && (LayerScannedSize >= LayerToTotalDataSize[LayerIdx]);

				FBlockStructure NewScannerBuildSpaceStructure;
				FBlockStructure NewScannerLayerSpaceStructure;
				const uint64 SelectedBuildSpaceSize = UnknownBuildSpaceStructure.SelectSerialBytes(0, ScannerDataSize, NewScannerBuildSpaceStructure);
				const uint64 UnknownDataSize = BlockStructureHelpers::CountSize(UnknownBuildSpaceStructure);

				// Make sure there are enough bytes available for a scanner, plus a chunk, so that we know no more chunks will get
				// made from this sequential unknown data.
				const uint64 RequiredScannerBytes = ScannerDataSize + LargestWindowSize;
				const bool bHasEnoughData = bLayerComplete || UnknownDataSize > RequiredScannerBytes;

				if (bHasEnoughData && (SelectedBuildSpaceSize == ScannerDataSize || (bLayerComplete && SelectedBuildSpaceSize > 0)))
				{
					check(bHasEnoughData || bLayerComplete);

					const uint64 SelectedLayerSpaceSize = UnknownLayerSpaceStructure.SelectSerialBytes(0, ScannerDataSize, NewScannerLayerSpaceStructure);
					check(SelectedBuildSpaceSize == SelectedLayerSpaceSize);
					bScannerCreated = true;
					LayerToScannerCount.FindOrAdd(NextLayer)++;
					MaxLayer = FMath::Max<int64>(MaxLayer, NextLayer);
					FStatsCollector::Set(StatLayers, MaxLayer);

					uint64& NextLayerScannerOffset = LayerToCreatedScannerOffset.FindOrAdd(NextLayer);
					TArray<uint8> ScannerData;
					LayerSpaceBlockData.CopyTo(ScannerData, NewScannerLayerSpaceStructure);
					check(ScannerData.Num() == SelectedLayerSpaceSize);

					const bool bIsFinalScanner = bLayerComplete && UnknownDataSize <= SelectedBuildSpaceSize;
					const uint64 PadSize = bIsFinalScanner ? ScannerOverlapSize : 0;
					ScannerData.AddZeroed(PadSize);

					// Test overlaps.
					FBlockStructure OverlapStructure = LayerCreatingScannersTest.FindOrAdd(NextLayer).Intersect(NewScannerBuildSpaceStructure);
					uint64 OverlapBytes = BlockStructureHelpers::CountSize(OverlapStructure);
					FBlockStructure OverlapLayerSpaceStructure = LayerCreatingScannersLayerSpaceTest.FindOrAdd(NextLayer).Intersect(NewScannerLayerSpaceStructure);
					uint64 OverlapLayerSpaceBytes = BlockStructureHelpers::CountSize(OverlapLayerSpaceStructure);
					check(OverlapLayerSpaceBytes == ScannerOverlapSize || NextLayerScannerOffset == 0);
					check(OverlapBytes == ScannerOverlapSize || NextLayerScannerOffset == 0);
					LayerCreatingScannersTest.FindOrAdd(NextLayer).Add(NewScannerBuildSpaceStructure);
					LayerCreatingScannersLayerSpaceTest.FindOrAdd(NextLayer).Add(NewScannerLayerSpaceStructure);

					// Check only one final scanner.
					if (bIsFinalScanner)
					{
						check(LayerCreatingFinalTest.Contains(NextLayer) == false);
						LayerCreatingFinalTest.Add(NextLayer, true);
					}

					UE_LOG(LogPatchGeneration, Verbose, TEXT("Creating scanner on layer %d at %llu. IsFinal:%d. Mapping:%s, BuildMapping:%s"), NextLayer, NextLayerScannerOffset, bIsFinalScanner, *NewScannerLayerSpaceStructure.ToString(), *NewScannerBuildSpaceStructure.ToString());
					Scanners.Emplace(new FScannerDetails(NextLayer, NextLayerScannerOffset, bIsFinalScanner, PadSize, MoveTemp(ScannerData), NewScannerBuildSpaceStructure, WindowSizes, CloudEnumeration.Get(), StatsCollector.Get()));
					NextLayerScannerOffset += ScannerDataSize - ScannerOverlapSize;

					// Remove blocks from structures.
					NewScannerBuildSpaceStructure.Empty();
					NewScannerLayerSpaceStructure.Empty();
					const uint64 SerialBytesToSelect = bIsFinalScanner ? ScannerDataSize : ScannerDataSize - ScannerOverlapSize;
					const uint64 SizeBuildRemoving = UnknownBuildSpaceStructure.SelectSerialBytes(0, SerialBytesToSelect, NewScannerBuildSpaceStructure);
					const uint64 SizeLayerRemoving = UnknownLayerSpaceStructure.SelectSerialBytes(0, SerialBytesToSelect, NewScannerLayerSpaceStructure);
					UnknownBuildSpaceStructure.Remove(NewScannerBuildSpaceStructure);
					UnknownLayerSpaceStructure.Remove(NewScannerLayerSpaceStructure);
					LayerSpaceBlockData.RemoveData(NewScannerLayerSpaceStructure);
					check(SizeBuildRemoving == SizeLayerRemoving);
					check(SizeBuildRemoving == SerialBytesToSelect || SizeBuildRemoving == UnknownDataSize);
					check(!bIsFinalScanner || BlockStructureHelpers::CountSize(UnknownBuildSpaceStructure) == 0);
					check(!bIsFinalScanner || BlockStructureHelpers::CountSize(UnknownLayerSpaceStructure) == 0);
				}
				else
				{
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Not making Layer[%d] unknown data scanners.. RequiredScannerBytes:%llu UnknownDataSize:%llu"), LayerIdx, RequiredScannerBytes, UnknownDataSize);
				}
			}
			// Stop when we cannot make scanners anymore.
			if (!bScannerCreated)
			{
				break;
			}
		}

		// Stream some build data.
		if (!PatchGenerationHelpers::ScannerArrayFull(Scanners))
		{
			// Check that we have enough slack space in the data array to be queuing up more scanners on layer 0.
			const uint64 BottomLayerSpaceBlockCount = LayerToLayerSpaceBlockData.FindOrAdd(0).GetDataCount();
			const int32 OneGigabyte = 1073741824;
			const bool bQueuedDataFull = BottomLayerSpaceBlockCount > OneGigabyte;
			if (bQueuedDataFull)
			{
				UE_LOG(LogPatchGeneration, Verbose, TEXT("Not making new scanners on layer 0 due to current backlog %llu bytes."), BottomLayerSpaceBlockCount);
			}
			else
			{
				// Create a scanner from new build data?
				if (!BuildStream->IsEndOfData())
				{
					// Keep the overlap data from previous scanner.
					int32 PreviousSize = DataBuffer.Num();
					if (PreviousSize > 0)
					{
						check(PreviousSize > ScannerOverlapSize);
						uint8* CopyTo = DataBuffer.GetData();
						uint8* CopyFrom = CopyTo + (PreviousSize - ScannerOverlapSize);
						FMemory::Memcpy(CopyTo, CopyFrom, ScannerOverlapSize);
						DataBuffer.SetNum(ScannerOverlapSize, EAllowShrinking::No);
						DataBufferFirstIdx += (PreviousSize - ScannerOverlapSize);
					}

					// Grab some data from the build stream.
					PreviousSize = DataBuffer.Num();
					DataBuffer.SetNumUninitialized(ScannerDataSize);
					const bool bWaitForData = true;
					ReadLen = BuildStream->DequeueData(DataBuffer.GetData() + PreviousSize, ScannerDataSize - PreviousSize, bWaitForData);
					DataBuffer.SetNum(PreviousSize + ReadLen, EAllowShrinking::No);

					// Only make a scanner if we are getting new data.
					if (ReadLen > 0)
					{
						// Pad scanner data if end of build
						uint64 PadSize = BuildStream->IsEndOfData() ? ScannerOverlapSize : 0;
						DataBuffer.AddZeroed(PadSize);

						// Create data scanner.
						const bool bIsFinalScanner = BuildStream->IsEndOfData();
						FBlockStructure Structure;
						Structure.Add(DataBufferFirstIdx, DataBuffer.Num() - PadSize);

						// Test overlaps.
						FBlockStructure OverlapStructure = LayerCreatingScannersTest.FindOrAdd(0).Intersect(Structure);
						FBlockStructure OverlapLayerSpaceStructure = LayerCreatingScannersLayerSpaceTest.FindOrAdd(0).Intersect(Structure);
						uint64 OverlapBytes = BlockStructureHelpers::CountSize(OverlapStructure);
						uint64 OverlapLayerSpaceBytes = BlockStructureHelpers::CountSize(OverlapLayerSpaceStructure);
						check(OverlapBytes == ScannerOverlapSize || DataBufferFirstIdx == 0);
						check(OverlapLayerSpaceBytes == ScannerOverlapSize || DataBufferFirstIdx == 0);
						LayerCreatingScannersTest.FindOrAdd(0).Add(Structure);
						LayerCreatingScannersLayerSpaceTest.FindOrAdd(0).Add(Structure);

						// Check only one final scanner.
						if (bIsFinalScanner)
						{
							check(LayerCreatingFinalTest.Contains(0) == false);
							LayerCreatingFinalTest.Add(0, true);
						}

						UE_LOG(LogPatchGeneration, Verbose, TEXT("Creating scanner on layer 0 at %llu. IsFinal:%d. Mapping:%s"), DataBufferFirstIdx, BuildStream->IsEndOfData(), *Structure.ToString());
						Scanners.Emplace(new FScannerDetails(0, DataBufferFirstIdx, BuildStream->IsEndOfData(), PadSize, DataBuffer, MoveTemp(Structure), WindowSizes, CloudEnumeration.Get(), StatsCollector.Get()));
						LayerToScannerCount.FindOrAdd(0)++;
					}
				}
			}
		}

		// Did we run out of unknown data?
		bHasUnknownData = false;
		for (const TPair<int32, FBlockStructure>& UnknownBuildSpaceStructurePair : LayerToUnknownBuildSpaceStructure)
		{
			if (UnknownBuildSpaceStructurePair.Value.GetHead() != nullptr)
			{
				bHasUnknownData = true;
				break;
			}
		}

		// Update some stats.
		int64 UnknownDataAlloc = 0;
		int64 UnknownDataNum = 0;
		for (const TPair<int32, TBlockData<uint8>>& LayerToLayerSpaceBlockDataPair : LayerToLayerSpaceBlockData)
		{
			UnknownDataNum += LayerToLayerSpaceBlockDataPair.Value.GetDataCount();
			UnknownDataAlloc += LayerToLayerSpaceBlockDataPair.Value.GetAllocatedSize();
		}
		FStatsCollector::Set(StatUnknownDataAlloc, UnknownDataAlloc);
		FStatsCollector::Set(StatUnknownDataNum, UnknownDataNum);
		FStatsCollector::Set(StatNumScanners, Scanners.Num());

		// Log collected stats.
		if (bIsGameThread)
		{
			GLog->FlushThreadedLogs();
		}
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		StatsCollector->LogStats(StatsLoggerTimeSeconds);

		// Sleep to allow other threads.
		FPlatformProcess::Sleep(0.01f);
	}

	if (BuildStream->HasAborted())
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Directory Build Streamer aborted"));
		return false;
	}

	// Complete chunk writer.
	FParallelChunkWriterSummaries ChunkWriterSummaries = ChunkWriter->OnProcessComplete();

	// Produce final stats log.
	const uint64 EndTime = FStatsCollector::GetCycles();
	FStatsCollector::Set(StatTotalTime, EndTime - StartTime);
	StatsCollector->LogStats();

	// Collect chunk info for the manifest builder.
	TMap<FGuid, FChunkInfo> ChunkInfoMap;
	TMap<FGuid, int64> ChunkFileSizes = CloudEnumeration->GetChunkFileSizes();
	TMap<FGuid, uint32> ChunkWindowSizes = CloudEnumeration->GetChunkWindowSizes();
	ChunkFileSizes.Append(ChunkWriterSummaries.ChunkOutputSizes);
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : CloudEnumeration->GetChunkInventory())
	{
		TSet<FGuid>& ChunkSet = ChunkInventory.FindOrAdd(ChunkInventoryPair.Key);
		ChunkSet = ChunkSet.Union(ChunkInventoryPair.Value);
	}
	ChunkShaHashes.Append(CloudEnumeration->GetChunkShaHashes());
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : ChunkInventory)
	{
		for (const FGuid& ChunkGuid : ChunkInventoryPair.Value)
		{
			if (ChunkShaHashes.Contains(ChunkGuid) && ChunkFileSizes.Contains(ChunkGuid))
			{
				FChunkInfo& ChunkInfo = ChunkInfoMap.FindOrAdd(ChunkGuid);
				ChunkInfo.Guid = ChunkGuid;
				ChunkInfo.Hash = ChunkInventoryPair.Key;
				FMemory::Memcpy(ChunkInfo.ShaHash.Hash, ChunkShaHashes[ChunkGuid].Hash, FSHA1::DigestSize);
				ChunkInfo.FileSize = ChunkFileSizes[ChunkGuid];
				ChunkInfo.GroupNumber = FCrc::MemCrc32(&ChunkGuid, sizeof(FGuid)) % 100;
				ChunkInfo.WindowSize = ChunkWindowSizes.Contains(ChunkGuid) ? ChunkWindowSizes[ChunkGuid] : Settings.OutputChunkWindowSize;
			}
		}
	}

	// Finalize the manifest data.
	TArray<FChunkInfo> ChunkInfoList;
	ChunkInfoMap.GenerateValueArray(ChunkInfoList);
	if (ManifestBuilder->FinalizeData(BuildStream->GetAllFiles(), MoveTemp(ChunkInfoList)) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Finalizing manifest failed."));
		return false;
	}
	uint64 NewChunkBytes = 0;
	for (const FGuid& NewChunk : NewCreatedChunks)
	{
		NewChunkBytes += ChunkWriterSummaries.ChunkOutputSizes[NewChunk];
	}
	UE_LOG(LogPatchGeneration, Log, TEXT("Created %d chunks (%llu build bytes) (%llu compressed bytes)"), NewCreatedChunks.Num(), BlockStructureHelpers::CountSize(CreatedBuildSpaceMatches), NewChunkBytes);
	UE_LOG(LogPatchGeneration, Log, TEXT("Completed in %s."), *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(*StatTotalTime)));

	// Save manifest out to the cloud directory.
	const FString OutputFilename = Settings.CloudDirectory / Settings.OutputFilename;
	if (ManifestBuilder->SaveToFile(OutputFilename) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Saving manifest failed."));
		return false;
	}
	UE_LOG(LogPatchGeneration, Log, TEXT("Saved manifest to %s."), *OutputFilename);

	return true;
}
