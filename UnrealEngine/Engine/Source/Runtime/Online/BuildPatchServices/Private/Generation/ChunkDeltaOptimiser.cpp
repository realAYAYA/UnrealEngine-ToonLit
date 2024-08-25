// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkDeltaOptimiser.h"

#include "Containers/List.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/Async.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"

#include "Core/MeanValue.h"
#include "Core/Platform.h"
#include "Core/ProcessTimer.h"
#include "Common/ChunkDataSizeProvider.h"
#include "Common/SpeedRecorder.h"
#include "Common/FileSystem.h"
#include "Common/HttpManager.h"
#include "Common/StatsCollector.h"
#include "Generation/BuildStreamer.h"
#include "Generation/ChunkMatchProcessor.h"
#include "Generation/ChunkWriter.h"
#include "Generation/ChunkSearch.h"
#include "Generation/DataScanner.h"
#include "Generation/DeltaEnumeration.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/MessagePump.h"
#include "Installer/InstallerError.h"
#include "Installer/Statistics/CloudChunkSourceStatistics.h"
#include "BuildPatchHash.h"
#include "BuildPatchUtil.h"
#include "IBuildManifestSet.h"

#include "Misc/CommandLine.h"

DECLARE_LOG_CATEGORY_CLASS(LogChunkDeltaOptimiser, Log, All);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDeltaJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDeltaJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDeltaJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDeltaJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace DeltaOptimiseHelpers
{
	using namespace BuildPatchServices;

	FSHAHash GetShaForDataSet(const uint8* Data, uint32 Size)
	{
		FSHAHash SHAHash;
		FSHA1::HashBuffer(Data, Size, SHAHash.Hash);
		return SHAHash;
	}

	FSHAHash GetShaForDataSet(const TArray<uint8>& Data)
	{
		return GetShaForDataSet(Data.GetData(), Data.Num());
	}

	int32 GetMaxScannerBacklogCount()
	{
		int32 MaxScannerBacklogCount = 75;
		GConfig->GetInt(TEXT("BuildPatchServices"), TEXT("MaxScannerBacklog"), MaxScannerBacklogCount, GEngineIni);
		MaxScannerBacklogCount = FMath::Clamp<int32>(MaxScannerBacklogCount, 5, 500);
		return MaxScannerBacklogCount;
	}

	bool HasUnusedCpu()
	{
		static const int32 NumThreadsAvailable = GThreadPool->GetNumThreads();
		const bool bHasUnusedCpu = NumThreadsAvailable > FDataScannerCounter::GetNumRunningScanners();
#if UE_BUILD_DEBUG
		static const bool bSingleScannerThread = FParse::Param(FCommandLine::Get(), TEXT("singlescanneronly"));
		return bSingleScannerThread ? false : bHasUnusedCpu;
#else
		return bHasUnusedCpu;
#endif
	}

	template <typename T>
	bool BacklogIsFull(const TArray<T>& Scanners)
	{
		static int32 MaxScannerBacklogCount = GetMaxScannerBacklogCount();
		return Scanners.Num() >= MaxScannerBacklogCount;
	}

	template <typename T>
	bool ScannerArrayFull(const TArray<T>& Scanners)
	{
		const bool bScannerArrayFull = (FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners()) || BacklogIsFull(Scanners);
#if UE_BUILD_DEBUG
		static const bool bSingleScannerThread = FParse::Param(FCommandLine::Get(), TEXT("singlescanneronly"));
		return bSingleScannerThread ? (FDataScannerCounter::GetNumIncompleteScanners() + FDataScannerCounter::GetNumRunningScanners()) > 0 : bScannerArrayFull;
#else
		return bScannerArrayFull;
#endif
	}

	FChunkPart SelectBytes(const FChunkPart& FullPart, uint32 LeftChop, uint32 Size)
	{
		FChunkPart Selected = FullPart;
		Selected.Offset += LeftChop;
		Selected.Size = Size;
		return Selected;
	}

	void StompChunkPart(const FChunkPart& NewMatchPart, const FBlockStructure& NewMatchBlocks, FChunkSearcher& ChunkSearcher, TSet<FChunkSearcher::FFileNode*>& UpdatedFiles)
	{
		uint64 NewMatchPartStart = 0;
		ChunkSearcher.ForEachOverlap(NewMatchBlocks, [&](const FBlockRange& OverlapRange, FChunkSearcher::FFileDListNode* File, FChunkSearcher::FChunkDListNode* Chunk)
		{
			FChunkSearcher::FChunkNode& ChunkNode = Chunk->GetValue();
			UpdatedFiles.Add(&File->GetValue());
			const FChunkPart NewMatchPartBlock = DeltaOptimiseHelpers::SelectBytes(NewMatchPart, NewMatchPartStart, OverlapRange.GetSize());

			NewMatchPartStart += OverlapRange.GetSize();
			// If we fully replace this part.
			if (OverlapRange == ChunkNode.BuildRange)
			{
				ChunkNode.ChunkPart = NewMatchPartBlock;
			}
			// If we insert before this part, left chopping it.
			else if (OverlapRange.GetFirst() == ChunkNode.BuildRange.GetFirst())
			{
				// Make the new node.
				const FChunkSearcher::FChunkNode NewMatchChunkNode(NewMatchPartBlock, FBlockRange::FromFirstAndSize(ChunkNode.BuildRange.GetFirst(), OverlapRange.GetSize()));
				// Left chop current node.
				ChunkNode.ChunkPart.Offset += OverlapRange.GetSize();
				ChunkNode.ChunkPart.Size -= OverlapRange.GetSize();
				ChunkNode.BuildRange = FBlockRange::FromFirstAndLast(NewMatchChunkNode.BuildRange.GetLast() + 1, ChunkNode.BuildRange.GetLast());
				// Insert new node before current node.
				ListHelpers::InsertBefore(NewMatchChunkNode, File->GetValue().ChunkParts, Chunk);
			}
			// If we insert after this part, right chopping it.
			else if (OverlapRange.GetLast() == ChunkNode.BuildRange.GetLast())
			{
				// Right chop current node.
				ChunkNode.ChunkPart.Size -= OverlapRange.GetSize();
				ChunkNode.BuildRange = FBlockRange::FromFirstAndSize(ChunkNode.BuildRange.GetFirst(), ChunkNode.ChunkPart.Size);
				// Make the new node.
				const FChunkSearcher::FChunkNode NewMatchChunkNode(NewMatchPartBlock, FBlockRange::FromFirstAndSize(ChunkNode.BuildRange.GetLast() + 1, OverlapRange.GetSize()));
				// Insert chunk part after.
				ListHelpers::InsertAfter(NewMatchChunkNode, File->GetValue().ChunkParts, Chunk);
			}
			// If we insert inside this part.
			else
			{
				// Make the right side.
				const uint32 LeftChopSize = (OverlapRange.GetLast() - ChunkNode.BuildRange.GetFirst()) + 1;
				const uint32 RightSideSize = ChunkNode.BuildRange.GetSize() - LeftChopSize;
				const FChunkPart RightSide = DeltaOptimiseHelpers::SelectBytes(ChunkNode.ChunkPart, LeftChopSize, RightSideSize);
				const FChunkSearcher::FChunkNode RightSideChunkNode(RightSide, FBlockRange::FromFirstAndSize(OverlapRange.GetLast() + 1, RightSideSize));
				// Make the middle piece.
				const FChunkSearcher::FChunkNode MiddleChunkNode(NewMatchPartBlock, OverlapRange);
				// Right chop current node.
				ChunkNode.ChunkPart.Size = OverlapRange.GetFirst() - ChunkNode.BuildRange.GetFirst();
				ChunkNode.BuildRange = FBlockRange::FromFirstAndSize(ChunkNode.BuildRange.GetFirst(), ChunkNode.ChunkPart.Size);
				check(OverlapRange == FBlockRange::FromFirstAndLast(ChunkNode.BuildRange.GetLast() + 1, RightSideChunkNode.BuildRange.GetFirst() - 1));
				// Insert right side part after current.
				ListHelpers::InsertAfter(RightSideChunkNode, File->GetValue().ChunkParts, Chunk);
				// Insert middle part after current (thus before right side).
				ListHelpers::InsertAfter(MiddleChunkNode, File->GetValue().ChunkParts, Chunk);
			}
		});
	}

	void MakeScannerLocalList(FChunkSearcher& ChunkSearcher, IDeltaChunkEnumeration* Enumeration, const FBlockStructure& BuildStructure, FScannerFilesList& Result)
	{
		uint64 FirstByte = 0;
		ChunkSearcher.ForEachOverlap(BuildStructure, [&](const FBlockRange& OverlapRange, FChunkSearcher::FFileDListNode* File, FChunkSearcher::FChunkDListNode* Chunk)
		{
			const FFilenameId FilenameId = Enumeration->MakeFilenameId(File->GetValue().Manifest->Filename);
			const TArray<FString>& FileTagset = File->GetValue().Manifest->InstallTags;
			const FBlockRange& FileRange = File->GetValue().BuildRange;
			const uint64 FileOffset = OverlapRange.GetFirst() - FileRange.GetFirst();
			Result.AddTail(FScannerFileElement{FBlockRange::FromFirstAndSize(FirstByte, OverlapRange.GetSize()), FilenameId, FileTagset, FileOffset});
			FirstByte += OverlapRange.GetSize();
		});
		check(BlockStructureHelpers::CountSize(BuildStructure) == FirstByte);
	}
}

namespace DeltaStats
{
	class FNoMemoryChunkStoreStat
		: public BuildPatchServices::IMemoryChunkStoreStat
	{
	public:
		FNoMemoryChunkStoreStat() { }
		~FNoMemoryChunkStoreStat() { }

		// IMemoryChunkStoreStat interface begin.
		virtual void OnChunkStored(const FGuid& ChunkId) override { }
		virtual void OnChunkReleased(const FGuid& ChunkId) override { }
		virtual void OnChunkBooted(const FGuid& ChunkId) override { }
		virtual void OnStoreUseUpdated(int32 ChunkCount) override { }
		virtual void OnStoreSizeUpdated(int32 Size) override { }
		// IMemoryChunkStoreStat interface end.
	};

	class FNoCloudChunkSourceStat
		: public BuildPatchServices::ICloudChunkSourceStat
	{
	public:
		FNoCloudChunkSourceStat() { }
		~FNoCloudChunkSourceStat() { }

		// ICloudChunkSourceStat interface begin.
		virtual void OnDownloadRequested(const FGuid& ChunkId) override { }
		virtual void OnDownloadSuccess(const FGuid& ChunkId) override { }
		virtual void OnDownloadFailed(const FGuid& ChunkId, const FString& Url) override { }
		virtual void OnDownloadCorrupt(const FGuid& ChunkId, const FString& Url, BuildPatchServices::EChunkLoadResult LoadResult) override { }
		virtual void OnDownloadAborted(const FGuid& ChunkId, const FString& Url, double DownloadTimeMean, double DownloadTimeStd, double DownloadTime, double BreakingPoint) override { }
		virtual void OnReceivedDataUpdated(int64 TotalBytes) override { }
		virtual void OnRequiredDataUpdated(int64 TotalBytes) override { }
		virtual void OnDownloadHealthUpdated(EBuildPatchDownloadHealth DownloadHealth) override { }
		virtual void OnSuccessRateUpdated(float SuccessRate) override { }
		virtual void OnActiveRequestCountUpdated(uint32 RequestCount) override { }
		virtual void OnAcceptedNewRequirements(const TSet<FGuid>& ChunkIds) override { }
		// ICloudChunkSourceStat interface end.
	};
}

namespace DeltaFactories
{
	using namespace BuildPatchServices;

	class FChunkReferenceTrackerFactory : public IManifestBuildStreamer::IChunkReferenceTrackerFactory
	{
	public:
		FChunkReferenceTrackerFactory() { }
		virtual ~FChunkReferenceTrackerFactory() { }

		// IManifestBuildStreamer::IChunkReferenceTrackerFactory interface begin.
		virtual IChunkReferenceTracker* Create(IManifestBuildStreamer::FCustomChunkReferences CustomChunkReferences) override
		{
			return BuildPatchServices::FChunkReferenceTrackerFactory::Create(MoveTemp(CustomChunkReferences));
		}
		// IManifestBuildStreamer::IChunkReferenceTrackerFactory interface end.
	};

	struct FCloudChunkSourceFactoryShared
	{
	public:
		IFileSystem* FileSystem;
		IDownloadService* DownloadService;
		IChunkDataSerialization* ChunkDataSerialization;
		IMessagePump* MessagePump;
		IBuildManifestSet* ManifestSet;
	};

	class FCloudChunkSourceFactory : public IManifestBuildStreamer::ICloudChunkSourceFactory
	{
	private:
		struct FInstanceDependancies
		{
			TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy;
			TUniquePtr<IMemoryChunkStore> CloudChunkStore;
			TUniquePtr<IDownloadConnectionCount> ConnectionCount;
		};

	public:
		FCloudChunkSourceFactory(const FString& CloudDir, FCloudChunkSourceFactoryShared InShared)
			: Shared(MoveTemp(InShared))
			, CloudSourceConfig({CloudDir})
			, Platform(FPlatformFactory::Create())
			, MemoryChunkStoreStat(new DeltaStats::FNoMemoryChunkStoreStat())
			, InstallerError(FInstallerErrorFactory::Create())
			, CloudChunkSourceStat(new DeltaStats::FNoCloudChunkSourceStat())
		{
			CloudSourceConfig.bBeginDownloadsOnFirstGet = false;
			CloudSourceConfig.MaxRetryCount = 30;
		}

		virtual ~FCloudChunkSourceFactory()
		{
		}

		// IManifestBuildStreamer::ICloudChunkSourceFactory interface begin.
		virtual ICloudChunkSource* Create(IChunkReferenceTracker* ChunkReferenceTracker) override
		{
			InstanceDependancies.AddDefaulted();
			FInstanceDependancies& Dependancies = InstanceDependancies.Last();
			Dependancies.MemoryEvictionPolicy.Reset(FChunkEvictionPolicyFactory::Create(ChunkReferenceTracker));
			Dependancies.CloudChunkStore.Reset(FMemoryChunkStoreFactory::Create(
				100,
				Dependancies.MemoryEvictionPolicy.Get(),
				nullptr,
				MemoryChunkStoreStat.Get()));
			Dependancies.ConnectionCount.Reset(BuildPatchServices::FDownloadConnectionCountFactory::Create(ConnectionCountConfig, nullptr));
			ICloudChunkSource* CloudChunkSource = BuildPatchServices::FCloudChunkSourceFactory::Create(
				CloudSourceConfig,
				Platform.Get(),
				Dependancies.CloudChunkStore.Get(),
				Shared.DownloadService,
				ChunkReferenceTracker,
				Shared.ChunkDataSerialization,
				Shared.MessagePump,
				InstallerError.Get(),
				Dependancies.ConnectionCount.Get(),
				CloudChunkSourceStat.Get(),
				Shared.ManifestSet,
				ChunkReferenceTracker->GetReferencedChunks());

			TFunction<void(const FGuid&)> LostChunkCallback = [CloudChunkSource](const FGuid& LostChunk)
			{
				CloudChunkSource->AddRepeatRequirement(LostChunk);
			};
			Dependancies.CloudChunkStore->SetLostChunkCallback(LostChunkCallback);

			return CloudChunkSource;
		}
		// IManifestBuildStreamer::ICloudChunkSourceFactory interface end.

	private:
		FCloudChunkSourceFactoryShared Shared;
		FCloudSourceConfig CloudSourceConfig;
		FDownloadConnectionCountConfig ConnectionCountConfig;
		TUniquePtr<IPlatform> Platform;
		TUniquePtr<IMemoryChunkStoreStat> MemoryChunkStoreStat;
		TUniquePtr<IInstallerError> InstallerError;
		TUniquePtr<ICloudChunkSourceStat> CloudChunkSourceStat;
		TArray<FInstanceDependancies> InstanceDependancies;
	};
}

namespace BuildPatchServices
{
	class FChunkMatchStomper
	{
	public:
		typedef TTuple<TArray<FChunkPart>, FBlockStructure> FNewMatch;
		typedef TQueue<FNewMatch, EQueueMode::Spsc> FNewMatchQueue;

		FChunkMatchStomper(const FBuildPatchAppManifest& InManifestA, const FBuildPatchAppManifest& InManifestB)
			: ManifestA(InManifestA)
			, ManifestB(InManifestB)
			, BuildAFiles(ListHelpers::GetFileList(ManifestA))
			, BuildBFiles(ListHelpers::GetFileList(ManifestB))
			, bExpectsMoreData(true)
			, ThreadTrigger(FPlatformProcess::GetSynchEventFromPool(true))
		{
			FileManifestListFuture = Async(EAsyncExecution::Thread, [this]() { return AsyncRun(); });
		}

		~FChunkMatchStomper()
		{
			// Ensures the thread work completes.
			bExpectsMoreData = false;
			ThreadTrigger->Trigger();
			FileManifestListFuture.Wait();
			FPlatformProcess::ReturnSynchEventToPool(ThreadTrigger);
		}

		FFileManifestList AsyncRun()
		{
			FChunkSearcher SearcherB(ManifestB);
			TSet<FChunkSearcher::FFileNode*> UpdatedFiles;

			// Start with searcher B invalidating unknown chunks.
			FChunkSearcher::FFileDListNode* FileBNode = SearcherB.GetHead();
			while (FileBNode)
			{
				FChunkSearcher::FChunkDListNode* ChunkBNode = FileBNode->GetValue().ChunkParts.GetHead();
				while (ChunkBNode)
				{
					if (ManifestA.GetChunkInfo(ChunkBNode->GetValue().ChunkPart.Guid) == nullptr)
					{
						ChunkBNode->GetValue().ChunkPart.Guid.Invalidate();
					}
					ChunkBNode = ChunkBNode->GetNextNode();
				}
				FileBNode = FileBNode->GetNextNode();
			}

			bool bHasNewMatch = false;
			FNewMatch NewMatch;
			while ((bHasNewMatch = NewMatchQueue.Dequeue(NewMatch), bHasNewMatch) || bExpectsMoreData)
			{
				if (bHasNewMatch)
				{
					const TArray<FChunkPart>& NewChunkParts = NewMatch.Get<0>();
					const FBlockStructure& BuildBStructure = NewMatch.Get<1>();
					uint64 ByteCount = 0;
					for (const FChunkPart& NewChunkPart : NewChunkParts)
					{
						FBlockStructure PartStructure;
						BuildBStructure.SelectSerialBytes(ByteCount, NewChunkPart.Size, PartStructure);
						DeltaOptimiseHelpers::StompChunkPart(NewChunkPart, PartStructure, SearcherB, UpdatedFiles);
						ByteCount += NewChunkPart.Size;
					}
				}
				else
				{
					ThreadTrigger->Wait(1000);
					ThreadTrigger->Reset();
				}
			}

			// Ensure priority to original matches?
			ClobberAllKnownChunks(SearcherB, UpdatedFiles);

			// Collapse all adjacent chunkparts.
			FileBNode = SearcherB.GetHead();
			while (FileBNode)
			{
				MergeAdjacentChunkParts(FileBNode->GetValue().ChunkParts);
				FileBNode = FileBNode->GetNextNode();
			}

			return SearcherB.BuildNewFileManifestList();
		}

		void ReplaceChunkReferences(const TArray<FChunkPart>& NewChunkReferences, const FBlockStructure& BuildBStructure)
		{
			checkf(bExpectsMoreData, TEXT("You can't provide more data after collecting the result."));
			NewMatchQueue.Enqueue(FNewMatch{NewChunkReferences, BuildBStructure});
			ThreadTrigger->Trigger();
		}

		FFileManifestList GetNewFileManifests()
		{
			bExpectsMoreData = false;
			ThreadTrigger->Trigger();
			return FileManifestListFuture.Get();
		}

	private:
		void ClobberAllKnownChunks(FChunkSearcher& ChunkSearcher, TSet<FChunkSearcher::FFileNode*>& UpdatedFiles)
		{
			uint64 BuildFileFirst = 0;
			uint64 ChunkPartFirst = 0;
			for (const FString& BuildFilename : BuildBFiles)
			{
				const FFileManifest* FileManifest = ManifestB.GetFileManifest(BuildFilename);
				check(FileManifest != nullptr);
				const FBlockRange FileRange = FBlockRange::FromFirstAndSize(BuildFileFirst, FileManifest->FileSize);
				if (FileRange.GetSize() > 0)
				{
					ChunkPartFirst = FileRange.GetFirst();
					for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
					{
						const FBlockRange ChunkPartRange = FBlockRange::FromFirstAndSize(ChunkPartFirst, ChunkPart.Size);
						if (ManifestA.GetChunkInfo(ChunkPart.Guid) != nullptr)
						{
							DeltaOptimiseHelpers::StompChunkPart(ChunkPart, FBlockStructure(ChunkPartRange.GetFirst(), ChunkPartRange.GetSize()), ChunkSearcher, UpdatedFiles);
						}
						ChunkPartFirst += ChunkPartRange.GetSize();
					}
				}
				check(ChunkPartFirst == (BuildFileFirst + FileRange.GetSize()));
				BuildFileFirst += FileRange.GetSize();
			}
		}

		void MergeAdjacentChunkParts(FChunkSearcher::FChunkDList& ChunkParts)
		{
			FChunkSearcher::FChunkDListNode* ChunkNode = ChunkParts.GetHead();
			while (ChunkNode)
			{
				FChunkSearcher::FChunkDListNode* NextChunkNode = ChunkNode->GetNextNode();
				while (NextChunkNode)
				{
					// Assert if we skipped build data
					check((ChunkNode->GetValue().BuildRange.GetLast() + 1) == NextChunkNode->GetValue().BuildRange.GetFirst());
					FChunkPart& ThisChunkPart = ChunkNode->GetValue().ChunkPart;
					FChunkPart& NextChunkPart = NextChunkNode->GetValue().ChunkPart;
					const FBlockRange LastMatchPartRange = FBlockRange::FromFirstAndSize(ThisChunkPart.Offset, ThisChunkPart.Size);
					const FBlockRange ThisMatchPartRange = FBlockRange::FromFirstAndSize(NextChunkPart.Offset, NextChunkPart.Size);
					const bool bBothInvalid = !NextChunkPart.Guid.IsValid() && !ThisChunkPart.Guid.IsValid();
					const bool bBothSamePadding = ThisChunkPart.IsPadding() && NextChunkPart.IsPadding() && ThisChunkPart.GetPaddingByte() == NextChunkPart.GetPaddingByte();
					const bool bSameChunk = NextChunkPart.Guid == ThisChunkPart.Guid;
					const bool bAdjacentData = (LastMatchPartRange.GetLast() + 1) == ThisMatchPartRange.GetFirst();
					bool bMerged = false;
					FChunkSearcher::FChunkDListNode* NextNextChunkNode = NextChunkNode->GetNextNode();
					if (bBothInvalid)
					{
						const uint64 TotalSize = NextChunkNode->GetValue().BuildRange.GetSize() + ChunkNode->GetValue().BuildRange.GetSize();
						if (TotalSize < TNumericLimits<uint32>::Max())
						{
							ThisChunkPart.Size = TotalSize;
							ChunkNode->GetValue().BuildRange = FBlockRange::FromFirstAndSize(ChunkNode->GetValue().BuildRange.GetFirst(), TotalSize);
							ChunkParts.RemoveNode(NextChunkNode);
							bMerged = true;
						}
					}
					else if (bBothSamePadding)
					{
						const uint64 TotalSize = NextChunkNode->GetValue().BuildRange.GetSize() + ChunkNode->GetValue().BuildRange.GetSize();
						if (TotalSize < PaddingChunk::ChunkSize)
						{
							ThisChunkPart.Offset = 0;
							ThisChunkPart.Size = TotalSize;
							ChunkNode->GetValue().BuildRange = FBlockRange::FromFirstAndSize(ChunkNode->GetValue().BuildRange.GetFirst(), TotalSize);
							ChunkParts.RemoveNode(NextChunkNode);
							bMerged = true;
						}
					}
					else if (bSameChunk && bAdjacentData)
					{
						const FBlockRange MergedPartRange = FBlockRange::FromMerge(ThisMatchPartRange, LastMatchPartRange);
						ThisChunkPart.Offset = MergedPartRange.GetFirst();
						ThisChunkPart.Size = MergedPartRange.GetSize();
						ChunkNode->GetValue().BuildRange = FBlockRange::FromMerge(ChunkNode->GetValue().BuildRange, NextChunkNode->GetValue().BuildRange);
						ChunkParts.RemoveNode(NextChunkNode);
						bMerged = true;
					}
					if (!bMerged)
					{
						ChunkNode = NextChunkNode;
					}
					NextChunkNode = NextNextChunkNode;
				}
				ChunkNode = NextChunkNode;
			}
		}

	private:
		const FBuildPatchAppManifest& ManifestA;
		const FBuildPatchAppManifest& ManifestB;
		const TArray<FString> BuildAFiles;
		const TArray<FString> BuildBFiles;
		FThreadSafeBool bExpectsMoreData;
		FEvent* ThreadTrigger;
		TFuture<FFileManifestList> FileManifestListFuture;
		FNewMatchQueue NewMatchQueue;
	};
}

namespace BuildPatchServices
{
	struct FDeltaScannerEntry
	{
	public:
		FDeltaScannerEntry()
			: bIsFinalScanner(false)
			, bWasFork(false)
			, Offset(0)
		{ }

	public:
		TArray<uint8> Data;
		FScannerFilesList FilesList;
		TUniquePtr<IDataScanner> Scanner;
		bool bIsFinalScanner;
		bool bWasFork;
		uint64 Offset;
	};

	class FChunkDeltaOptimiser
		: public IChunkDeltaOptimiser
	{
	public:
		FChunkDeltaOptimiser(const FChunkDeltaOptimiserConfiguration& InConfiguration);
		~FChunkDeltaOptimiser();

		// IChunkDeltaOptimiser interface begin.
		virtual bool Run() override;
		// IChunkDeltaOptimiser interface end.

	private:
		TArray<FString> AsyncRun();
		void HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download);
		FBlockStructure GetDesiredBytes(const FBuildPatchAppManifestPtr& Manifest, const TSet<FGuid>& Chunks);

	private:
		const FChunkDeltaOptimiserConfiguration Configuration;
		FTSTicker& CoreTicker;
		FDownloadCompleteDelegate DownloadCompleteDelegate;
		FDownloadProgressDelegate DownloadProgressDelegate;
		TUniquePtr<IFileSystem> FileSystem;
		TUniquePtr<IHttpManager> HttpManager;
		TUniquePtr<IChunkDataSizeProvider> ChunkDataSizeProvider;
		TUniquePtr<ISpeedRecorder> DownloadSpeedRecorder;
		TUniquePtr<IInstallerAnalytics> InstallerAnalytics;
		TUniquePtr<IDownloadServiceStatistics> DownloadServiceStatistics;
		TUniquePtr<IDownloadService> DownloadService;
		TUniquePtr<IMessagePump> MessagePump;
		TUniquePtr<FStatsCollector> StatsCollector;
		FThreadSafeBool bShouldRun;
		FThreadSafeBool bSuccess;

		// Manifest downloading
		int32 RequestIdManifestA;
		int32 RequestIdManifestB;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestA;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestB;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestA;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestB;
	};

	FChunkDeltaOptimiser::FChunkDeltaOptimiser(const FChunkDeltaOptimiserConfiguration& InConfiguration)
		: Configuration(InConfiguration)
		, CoreTicker(FTSTicker::GetCoreTicker())
		, DownloadCompleteDelegate(FDownloadCompleteDelegate::CreateRaw(this, &FChunkDeltaOptimiser::HandleDownloadComplete))
		, DownloadProgressDelegate()
		, FileSystem(FFileSystemFactory::Create())
		, HttpManager(FHttpManagerFactory::Create())
		, ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create())
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(nullptr))
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), ChunkDataSizeProvider.Get(), InstallerAnalytics.Get()))
		, DownloadService(FDownloadServiceFactory::Create(HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
		, MessagePump(FMessagePumpFactory::Create())
		, StatsCollector(FStatsCollectorFactory::Create())
		, bShouldRun(true)
		, RequestIdManifestA(INDEX_NONE)
		, RequestIdManifestB(INDEX_NONE)
		, PromiseManifestA()
		, PromiseManifestB()
		, FutureManifestA(PromiseManifestA.GetFuture())
		, FutureManifestB(PromiseManifestB.GetFuture())
	{
	}

	FChunkDeltaOptimiser::~FChunkDeltaOptimiser()
	{
	}

	bool FChunkDeltaOptimiser::Run()
	{
		// Run any core initialisation required.
		FHttpModule::Get();

		// Setup Generation stats.
		volatile int64* StatTotalTime = StatsCollector->CreateStat(TEXT("Generation: Total Time"), EStatFormat::Timer);
		volatile int64* StatStreamSpeed = StatsCollector->CreateStat(TEXT("Generation: Data stream speed"), EStatFormat::DataSpeed);
		const uint64 StartTime = FStatsCollector::GetCycles();
		const float SpeedStatOverTime = TNumericLimits<float>::Max();

		// Kick off Manifest downloads.
		RequestIdManifestA = DownloadService->RequestFile(Configuration.ManifestAUri, DownloadCompleteDelegate, DownloadProgressDelegate);
		RequestIdManifestB = DownloadService->RequestFile(Configuration.ManifestBUri, DownloadCompleteDelegate, DownloadProgressDelegate);

		// Start the generation thread.
		TFuture<TArray<FString>> Thread = Async(EAsyncExecution::Thread, [this](){ return AsyncRun(); });

		// Main timers.
		double DeltaTime = 0.0;
		double LastTime = FPlatformTime::Seconds();

		// Setup desired frame times.
		float MainsFramerate = 100.0f;
		const float MainsFrameTime = 1.0f / MainsFramerate;

		// Load settings from config.
		float StatsLoggerTimeSeconds = 10.0f;
		GConfig->GetFloat(TEXT("BuildPatchServices"), TEXT("StatsLoggerTimeSeconds"), StatsLoggerTimeSeconds, GEngineIni);
		StatsLoggerTimeSeconds = FMath::Clamp<float>(StatsLoggerTimeSeconds, 1.0f, 60.0f);

		// Run the main loop.
		while (bShouldRun)
		{
			// Increment global frame counter once for each app tick.
			GFrameCounter++;

			// Application tick.
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTSTicker::GetCoreTicker().Tick(DeltaTime);

			// Message pump.
			MessagePump->PumpMessages();

			// Log collected stats.
			GLog->FlushThreadedLogs();
			FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
			FStatsCollector::Set(StatStreamSpeed, DownloadSpeedRecorder->GetAverageSpeed(SpeedStatOverTime));
			StatsCollector->LogStats(StatsLoggerTimeSeconds);

			// Control frame rate.
			FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

			// Calculate deltas.
			const double AppTime = FPlatformTime::Seconds();
			DeltaTime = AppTime - LastTime;
			LastTime = AppTime;
		}

		// Log collected stats.
		TArray<FString> FinalStatLogs = Thread.Get();
		GLog->FlushThreadedLogs();
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		FStatsCollector::Set(StatStreamSpeed, DownloadSpeedRecorder->GetAverageSpeed(SpeedStatOverTime));
		StatsCollector->LogStats();
		for (const FString& LogLine : FinalStatLogs)
		{
			UE_LOG(LogChunkDeltaOptimiser, Display, TEXT("%s"), *LogLine);
		}

		// Return thread success.
		return bSuccess;
	}

	TArray<FString> FChunkDeltaOptimiser::AsyncRun()
	{
		const FNumberFormattingOptions PercentFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(1).SetMinimumFractionalDigits(1).SetRoundingMode(ERoundingMode::ToZero);
		FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
		FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();
		TArray<FString> FinalStatLogs;
		bSuccess = true;
		if (ManifestA.IsValid() == false)
		{
			UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Could not download ManifestA from %s."), *Configuration.ManifestAUri);
			bSuccess = false;
		}
		if (ManifestB.IsValid() == false)
		{
			UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Could not download ManifestB from %s."), *Configuration.ManifestBUri);
			bSuccess = false;
		}
		if (bSuccess)
		{
			UE_LOG(LogChunkDeltaOptimiser, Display, TEXT("Running optimisation for patching %s -> %s"), *ManifestA->GetVersionString(), *ManifestB->GetVersionString());
			FProcessTimer ProcessTimer;
			FProcessTimer ChunkingTimer;
			FProcessTimer ScanningTimer;
			ProcessTimer.Start();

			TSet<FGuid> ChunksA;
			TSet<FGuid> ChunksB;
			ManifestA->GetDataList(ChunksA);
			ManifestB->GetDataList(ChunksB);

			// Check for ManifestA -> ManifestB compatibility. We don't yet support downgrading chunk version, only upgrading.
			const TCHAR* ManifestAChunkSubdir = ManifestVersionHelpers::GetChunkSubdir(ManifestA->ManifestMeta.FeatureLevel);
			const TCHAR* ManifestBChunkSubdir = ManifestVersionHelpers::GetChunkSubdir(ManifestB->ManifestMeta.FeatureLevel);
			const bool bUsingDifferentChunkSubdir = ManifestAChunkSubdir != ManifestBChunkSubdir;
			const bool bIsDowngrade = ManifestB->ManifestMeta.FeatureLevel < ManifestA->ManifestMeta.FeatureLevel;
			if (bUsingDifferentChunkSubdir && bIsDowngrade)
			{
				UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Destination manifest does not support source manifest's FeatureLevel (%s [%d] -> %s [%d])."), FeatureLevelToString(ManifestA->ManifestMeta.FeatureLevel), (int32)ManifestA->ManifestMeta.FeatureLevel, FeatureLevelToString(ManifestB->ManifestMeta.FeatureLevel), (int32)ManifestB->ManifestMeta.FeatureLevel);
				bSuccess = false;
			}

			// Check for output chunk size compatibility changes.
			const uint32 OutputChunkSize = ManifestB->ManifestMeta.FeatureLevel >= EFeatureLevel::VariableSizeChunks ? Configuration.OutputChunkSize : 1024*1024;
			if (Configuration.OutputChunkSize != OutputChunkSize)
			{
				UE_LOG(LogChunkDeltaOptimiser, Log, TEXT("Destination manifest does not support EFeatureLevel::VariableSizeChunks, reverting OutputChunkSize to %u."), OutputChunkSize);
			}

			// Check that an optimisation does not already exist, and skip long process if so.
			FBuildPatchAppManifest DeltaManifest;
			const FString OutputDeltaFilename = Configuration.CloudDirectory / FBuildPatchUtils::GetChunkDeltaFilename(*ManifestA.Get(), *ManifestB.Get());
			const bool bDeltaPreviouslyCompleted = FileSystem->FileExists(*OutputDeltaFilename);
			if (bDeltaPreviouslyCompleted)
			{
				if (DeltaManifest.LoadFromFile(OutputDeltaFilename))
				{
					FinalStatLogs.Add(FString::Printf(TEXT("** Chunk delta optimisation already completed for provided manifests. **")));
					FinalStatLogs.Add(FString::Printf(TEXT("Loaded optimised delta file %s"), *OutputDeltaFilename));
				}
				else
				{
					UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Optimised delta completed previously but could not be loaded %s."), *OutputDeltaFilename);
					bSuccess = false;
				}
			}

			// Check for aborting if original delta is over provided threshold.
			TSet<FString> TagsA, TagsB;
			ManifestA->GetFileTagList(TagsA);
			ManifestB->GetFileTagList(TagsB);
			const uint64 OriginalUnknownCompressedBytes = (uint64)ManifestB->GetDeltaDownloadSize(TagsB, ManifestA.ToSharedRef(), TagsA);
			const bool bOverAbortThreshold = OriginalUnknownCompressedBytes >= Configuration.DiffAbortThreshold;
			if (!bDeltaPreviouslyCompleted && bOverAbortThreshold)
			{
				FinalStatLogs.Add(FString::Printf(TEXT("** Aborting delta optimisation due to original delta over threshold. **")));
				FinalStatLogs.Add(FString::Printf(TEXT("%llu >= %llu"), OriginalUnknownCompressedBytes, Configuration.DiffAbortThreshold));
			}

			const bool bRunProcess = bSuccess && !bDeltaPreviouslyCompleted && !bOverAbortThreshold;
			if (bRunProcess)
			{
				// Runtime composition.
				TUniquePtr<IChunkDataSerialization> ChunkDataSerializationReader(FChunkDataSerializationFactory::Create(FileSystem.Get()));
				TUniquePtr<DeltaFactories::FChunkReferenceTrackerFactory> ChunkReferenceTrackerFactory(new DeltaFactories::FChunkReferenceTrackerFactory());
				TUniquePtr<IBuildManifestSet> SetA(FBuildManifestSetFactory::Create({ FInstallerAction::MakeInstall(ManifestA.ToSharedRef()) }));
				TUniquePtr<IBuildManifestSet> SetB(FBuildManifestSetFactory::Create({ FInstallerAction::MakeInstall(ManifestB.ToSharedRef()) }));
				DeltaFactories::FCloudChunkSourceFactoryShared CloudChunkSourceFactorySharedA;
				CloudChunkSourceFactorySharedA.FileSystem = FileSystem.Get();
				CloudChunkSourceFactorySharedA.DownloadService = DownloadService.Get();
				CloudChunkSourceFactorySharedA.ChunkDataSerialization = ChunkDataSerializationReader.Get();
				CloudChunkSourceFactorySharedA.MessagePump = MessagePump.Get();
				CloudChunkSourceFactorySharedA.ManifestSet = SetA.Get();
				DeltaFactories::FCloudChunkSourceFactoryShared CloudChunkSourceFactorySharedB;
				CloudChunkSourceFactorySharedB.FileSystem = FileSystem.Get();
				CloudChunkSourceFactorySharedB.DownloadService = DownloadService.Get();
				CloudChunkSourceFactorySharedB.ChunkDataSerialization = ChunkDataSerializationReader.Get();
				CloudChunkSourceFactorySharedB.MessagePump = MessagePump.Get();
				CloudChunkSourceFactorySharedB.ManifestSet = SetB.Get();
				TUniquePtr<DeltaFactories::FCloudChunkSourceFactory> CloudChunkSourceFactoryA(new DeltaFactories::FCloudChunkSourceFactory(Configuration.CloudDirectory, CloudChunkSourceFactorySharedA));
				TUniquePtr<DeltaFactories::FCloudChunkSourceFactory> CloudChunkSourceFactoryB(new DeltaFactories::FCloudChunkSourceFactory(Configuration.CloudDirectory, CloudChunkSourceFactorySharedB));

				// Buffer for data streaming.
				const EAllowShrinking AllowShrinking = EAllowShrinking::No;
				const uint32 StreamBufferReadSize = Configuration.ScanWindowSize * 32;
				const uint32 ScannerDataSize = StreamBufferReadSize;
				TArray<uint8> StreamBuffer;
				StreamBuffer.Reserve(StreamBufferReadSize + Configuration.ScanWindowSize);

				// Calculate the desired bytes for manifest streams.
				FBlockStructure ManifestADesiredBytes = GetDesiredBytes(ManifestA, ChunksA.Difference(ChunksB));
				FBlockStructure ManifestBDesiredBytes = GetDesiredBytes(ManifestB, ChunksB.Difference(ChunksA));
				const uint64 ManifestBStreamSize = BlockStructureHelpers::CountSize(ManifestBDesiredBytes);

				// Start the ManifestA stream and chunk enumeration.
				FManifestBuildStreamerConfig ManifestAStreamConfig({Configuration.CloudDirectory, ManifestADesiredBytes});
				FManifestBuildStreamerDependencies ManifestAStreamDependencies({ChunkReferenceTrackerFactory.Get(), CloudChunkSourceFactoryA.Get(), StatsCollector.Get(), ManifestA.Get()});
				TUniquePtr<IManifestBuildStreamer> ManifestAStream(FBuildStreamerFactory::Create(MoveTemp(ManifestAStreamConfig), MoveTemp(ManifestAStreamDependencies)));

				// First we re-chunk prev unknown build parts into the scanner window size.
				TUniquePtr<IDeltaChunkEnumeration> DeltaChunkEnumeration(FDeltaChunkEnumerationFactory::Create(ManifestAStream.Get(), StatsCollector.Get(), *ManifestA.Get(), Configuration.ScanWindowSize));
				ChunkingTimer.Start();
				DeltaChunkEnumeration->Run();
				ChunkingTimer.Stop();

				// Setup scanning stats.
				volatile int64* StatScannerBacklog = StatsCollector->CreateStat(TEXT("BuildB: Scanner backlog"), EStatFormat::Value);
				volatile int64* StatScannerForks = StatsCollector->CreateStat(TEXT("BuildB: Scanner forks"), EStatFormat::Value);
				volatile int64* StatScanningTime = StatsCollector->CreateStat(TEXT("BuildB: Scanning time"), EStatFormat::Timer);
				volatile int64* StatScanningCompleted = StatsCollector->CreateStat(TEXT("BuildB: Progress"), EStatFormat::Percentage);

				// Start the ManifestB stream.
				FManifestBuildStreamerConfig ManifestBStreamConfig({Configuration.CloudDirectory, ManifestBDesiredBytes});
				FManifestBuildStreamerDependencies ManifestBStreamDependencies({ChunkReferenceTrackerFactory.Get(), CloudChunkSourceFactoryB.Get(), StatsCollector.Get(), ManifestB.Get()});
				TUniquePtr<IManifestBuildStreamer> ManifestBStream(FBuildStreamerFactory::Create(MoveTemp(ManifestBStreamConfig), MoveTemp(ManifestBStreamDependencies)));

				// Our second loop which finds matching chunks in the new build.
				ScanningTimer.Start();
				FChunkSearcher FileListSearcher(*ManifestB.Get());
				TUniquePtr<FChunkMatchStomper> ChunkMatchStomper(new FChunkMatchStomper(*ManifestA.Get(), *ManifestB.Get()));
				const uint32 ScannerOverlapSize = Configuration.ScanWindowSize - 1;
				TUniquePtr<IChunkMatchProcessor> ChunkMatchProcessor(FChunkMatchProcessorFactory::Create());
				TArray<TUniquePtr<FDeltaScannerEntry>> DataScanners;
				int32 NumScannersCreated = 0;
				int32 NumScannersRequired = ManifestBStreamSize / (ScannerDataSize - ScannerOverlapSize);
				FMeanValue MeanScannerTime(5);
				int32 ConsumedBufferData = 0;
				uint64 StreamStartPosition = 0;
				StreamBuffer.SetNumUninitialized(0, AllowShrinking);
				const TMap<FDeltaChunkId, FChunkBuildReference>& ChunkBuildReferences = DeltaChunkEnumeration->GetChunkBuildReferences();
				uint64 BuildBScanTimer;
				FStatsCollector::AccumulateTimeBegin(BuildBScanTimer);
				while (ManifestBStream->IsEndOfData() == false || DataScanners.Num() > 0)
				{
					// Grab new stream data.
					check(StreamBuffer.Num() >= ConsumedBufferData);
					uint32 BufferDataSize = StreamBuffer.Num() - ConsumedBufferData;
					if (!ManifestBStream->IsEndOfData() && (BufferDataSize < ScannerDataSize))
					{
						// Move unconsumed data to the beginning.
						if (BufferDataSize > 0)
						{
							uint8* const CopyTo = StreamBuffer.GetData();
							const uint8* const CopyFrom = &StreamBuffer[ConsumedBufferData];
							FMemory::Memcpy(CopyTo, CopyFrom, BufferDataSize);
						}
						StreamStartPosition += ConsumedBufferData;
						ConsumedBufferData = 0;

						// Fill the rest of the buffer.
						StreamBuffer.SetNumUninitialized(BufferDataSize + StreamBufferReadSize, AllowShrinking);
						const uint32 SizeRead = ManifestBStream->DequeueData(StreamBuffer.GetData() + BufferDataSize, StreamBufferReadSize);
						StreamBuffer.SetNumUninitialized(BufferDataSize + SizeRead, AllowShrinking);
						BufferDataSize = StreamBuffer.Num();
					}

					// Grab a scanner result.
					if (DataScanners.Num() > 0 && DataScanners[0]->Scanner->IsComplete())
					{
						FDeltaScannerEntry& ScannerDetails = *DataScanners[0];
						if (!ScannerDetails.bWasFork)
						{
							MeanScannerTime.AddSample(ScannerDetails.Scanner->GetTimeRunning());
						}
						TArray<FChunkMatch> ChunkMatches = ScannerDetails.Scanner->GetResultWhenComplete();
						for (FChunkMatch& ChunkMatch : ChunkMatches)
						{
							FBlockStructure ChunkCBuildBStructure;
							ChunkMatch.DataOffset += ScannerDetails.Offset;
							ManifestBDesiredBytes.SelectSerialBytes(ChunkMatch.DataOffset, ChunkMatch.WindowSize, ChunkCBuildBStructure);
							ChunkMatchProcessor->ProcessMatch(0, ChunkMatch, MoveTemp(ChunkCBuildBStructure));
						}
						const FBlockRange ScannerRange = FBlockRange::FromFirstAndSize(ScannerDetails.Offset, ScannerDetails.Data.Num());
						const uint64 SafeFlushSize = ScannerDetails.bIsFinalScanner ? ScannerRange.GetLast() + 1 : ScannerRange.GetFirst();
						if (SafeFlushSize > 0)
						{
							ChunkMatchProcessor->FlushLayer(0, SafeFlushSize);
						}
						DataScanners.RemoveAt(0);
					}

					// Handle extra matches accepted.
					TArray<FMatchEntry> AcceptedChunkMatches;
					const FBlockRange CollectionRange = ChunkMatchProcessor->CollectLayer(0, AcceptedChunkMatches);
					if (CollectionRange.GetSize() > 0)
					{
						for (FMatchEntry& AcceptedChunkMatch : AcceptedChunkMatches)
						{
							const FChunkMatch& ChunkCMatch = AcceptedChunkMatch.ChunkMatch;
							const TArray<FChunkPart>& NewChunkReferences = ChunkBuildReferences[ChunkCMatch.ChunkGuid].Get<0>();
							const FBlockStructure& ChunkCBuildBStructure = AcceptedChunkMatch.BlockStructure;
							ChunkMatchStomper->ReplaceChunkReferences(NewChunkReferences, ChunkCBuildBStructure);
						}
					}

					// Create new scanner.
					uint32 SizeToScan = FMath::Min(ScannerDataSize, BufferDataSize);
					const bool bHasData = SizeToScan == ScannerDataSize || (ManifestBStream->IsEndOfData() && BufferDataSize > 0);
					if (bHasData && !DeltaOptimiseHelpers::ScannerArrayFull(DataScanners))
					{
						FDeltaScannerEntry* NewScanner = new FDeltaScannerEntry();
						NewScanner->Data.Append(StreamBuffer.GetData() + ConsumedBufferData, SizeToScan);
						NewScanner->Offset = StreamStartPosition + ConsumedBufferData;

						FBlockStructure ScannerBuildStructure;
						ManifestBDesiredBytes.SelectSerialBytes(NewScanner->Offset, SizeToScan, ScannerBuildStructure);
						DeltaOptimiseHelpers::MakeScannerLocalList(FileListSearcher, DeltaChunkEnumeration.Get(), ScannerBuildStructure, NewScanner->FilesList);

						NewScanner->Scanner.Reset(FDeltaScannerFactory::Create(Configuration.ScanWindowSize, NewScanner->Data, NewScanner->FilesList, DeltaChunkEnumeration.Get(), StatsCollector.Get()));
						ConsumedBufferData += SizeToScan;
						NewScanner->bIsFinalScanner = ManifestBStream->IsEndOfData() && ConsumedBufferData >= StreamBuffer.Num();
						if (!NewScanner->bIsFinalScanner)
						{
							ConsumedBufferData -= ScannerOverlapSize;
						}
						DataScanners.Emplace(NewScanner);
						++NumScannersCreated;
					}

					// Fork a scanner with too much work?
					if (DataScanners.Num() > 0 && MeanScannerTime.IsReliable() && DeltaOptimiseHelpers::HasUnusedCpu())
					{
						FDeltaScannerEntry& DataScannerEntry = *DataScanners[0];
						const double TopScannerTime = DataScannerEntry.Scanner->GetTimeRunning();
						double DownloadTimeMean;
						double DownloadTimeStd;
						MeanScannerTime.GetValues(DownloadTimeMean, DownloadTimeStd);
						const double BreakingPoint = FMath::Max<double>(0.25, DownloadTimeMean + DownloadTimeStd);
						if (TopScannerTime > BreakingPoint && DataScannerEntry.Scanner->SupportsFork())
						{
							DataScannerEntry.bWasFork = true;
							FStatsCollector::Accumulate(StatScannerForks, 1);
							const FBlockRange UnscannedRange = DataScannerEntry.Scanner->Fork();
							const uint64 ForkSize = (UnscannedRange.GetSize() / 2) + 1;
							if (ForkSize < UnscannedRange.GetSize())
							{
								// Insert the right fork first.
								const FBlockRange RightFork = FBlockRange::FromFirstAndLast(UnscannedRange.GetLast() - ForkSize, UnscannedRange.GetLast());
								FDeltaScannerEntry* NewScanner = new FDeltaScannerEntry();
								NewScanner->Data.Append(DataScannerEntry.Data.GetData() + RightFork.GetFirst(), RightFork.GetSize());
								NewScanner->Offset = DataScannerEntry.Offset + RightFork.GetFirst();

								FBlockStructure ScannerBuildStructure;
								ManifestBDesiredBytes.SelectSerialBytes(NewScanner->Offset, RightFork.GetSize(), ScannerBuildStructure);
								DeltaOptimiseHelpers::MakeScannerLocalList(FileListSearcher, DeltaChunkEnumeration.Get(), ScannerBuildStructure, NewScanner->FilesList);

								NewScanner->Scanner.Reset(FDeltaScannerFactory::Create(Configuration.ScanWindowSize, NewScanner->Data, NewScanner->FilesList, DeltaChunkEnumeration.Get(), StatsCollector.Get()));
								NewScanner->bIsFinalScanner = DataScannerEntry.bIsFinalScanner;
								NewScanner->bWasFork = true;
								DataScanners.EmplaceAt(1, NewScanner);

								// Insert the left fork.
								const FBlockRange LeftFork = FBlockRange::FromFirstAndLast(UnscannedRange.GetFirst(), UnscannedRange.GetFirst() + ForkSize);
								NewScanner = new FDeltaScannerEntry();
								NewScanner->Data.Append(DataScannerEntry.Data.GetData() + LeftFork.GetFirst(), LeftFork.GetSize());
								NewScanner->Offset = DataScannerEntry.Offset + LeftFork.GetFirst();

								ScannerBuildStructure.Empty();
								ManifestBDesiredBytes.SelectSerialBytes(NewScanner->Offset, LeftFork.GetSize(), ScannerBuildStructure);
								DeltaOptimiseHelpers::MakeScannerLocalList(FileListSearcher, DeltaChunkEnumeration.Get(), ScannerBuildStructure, NewScanner->FilesList);

								NewScanner->Scanner.Reset(FDeltaScannerFactory::Create(Configuration.ScanWindowSize, NewScanner->Data, NewScanner->FilesList, DeltaChunkEnumeration.Get(), StatsCollector.Get()));
								NewScanner->bIsFinalScanner = false;
								NewScanner->bWasFork = true;
								DataScanners.EmplaceAt(1, NewScanner);

								// Adjust original meta.
								DataScannerEntry.bIsFinalScanner = false;
								DataScannerEntry.Data.SetNumUninitialized(UnscannedRange.GetFirst(), EAllowShrinking::No);
							}
							else
							{
								// Something has gone wrong with the size calculation, this is fatal.
								check(ForkSize < UnscannedRange.GetSize());
							}
						}
					}

					const double PercentScanned = (double)(NumScannersCreated - DataScanners.Num()) / (double)NumScannersRequired;
					FStatsCollector::SetAsPercentage(StatScanningCompleted, PercentScanned);
					FStatsCollector::Set(StatScannerBacklog, DataScanners.Num());
					FStatsCollector::AccumulateTimeEnd(StatScanningTime, BuildBScanTimer);
					FStatsCollector::AccumulateTimeBegin(BuildBScanTimer);
				}
				FStatsCollector::AccumulateTimeEnd(StatScanningTime, BuildBScanTimer);
				FStatsCollector::SetAsPercentage(StatScanningCompleted, 1.0);
				ScanningTimer.Stop();

				// Grab the new manifest data.
				FFileManifestList FileManifestList = ChunkMatchStomper->GetNewFileManifests();

				// For all unknown data we need to re-chunk it out and fill in the gaps we have.
				FBlockStructure NewStreamBlocks;
				TArray<TTuple<FBlockStructure, FChunkPart>> NewChunks;
				uint64 ByteLocation = 0;
				for (const FFileManifest& FileManifest : FileManifestList.FileList)
				{
					for (const FChunkPart& ChunkPart : FileManifest.ChunkParts)
					{
						if (ChunkPart.Guid.IsValid() == false)
						{
							uint64 PartByteLocation = ByteLocation;
							uint32 PartSizeRemaining = ChunkPart.Size;
							while (PartSizeRemaining > 0)
							{
								// Start new chunk?
								if (NewChunks.Num() == 0 || NewChunks.Last().Get<1>().Size >= OutputChunkSize)
								{
									NewChunks.AddDefaulted_GetRef().Get<1>().Guid = FGuid::NewGuid();
								}

								TTuple<FBlockStructure, FChunkPart>* LastChunkDetail = &NewChunks.Last();
								const uint32 NewTotalSize = LastChunkDetail->Get<1>().Size + PartSizeRemaining;
								const uint32 ChunkPartConsume = NewTotalSize > OutputChunkSize ? PartSizeRemaining - (NewTotalSize - OutputChunkSize) : PartSizeRemaining;
								check(PartSizeRemaining >= ChunkPartConsume);

								NewStreamBlocks.Add(PartByteLocation, ChunkPartConsume, ESearchDir::FromEnd);
								LastChunkDetail->Get<0>().Add(PartByteLocation, ChunkPartConsume, ESearchDir::FromEnd);
								LastChunkDetail->Get<1>().Size += ChunkPartConsume;
								PartByteLocation += ChunkPartConsume;
								PartSizeRemaining -= ChunkPartConsume;
							}
						}
						ByteLocation += ChunkPart.Size;
					}
				}

				// Save out all new chunk data.
				TMap<FGuid, uint32> NewChunkWindowSizes;
				TSet<FChunkSearcher::FFileNode*> UpdatedFiles;
				FChunkSearcher ManifestSearcher(FileManifestList);
				FManifestBuildStreamerConfig UnknownDataStreamConfig({Configuration.CloudDirectory, NewStreamBlocks});
				FManifestBuildStreamerDependencies UnknownDataStreamDependencies({ChunkReferenceTrackerFactory.Get(), CloudChunkSourceFactoryB.Get(), StatsCollector.Get(), ManifestB.Get()});
				TUniquePtr<IManifestBuildStreamer> UnknownDataStream(FBuildStreamerFactory::Create(MoveTemp(UnknownDataStreamConfig), MoveTemp(UnknownDataStreamDependencies)));
				TUniquePtr<IChunkDataSerialization> ChunkDataSerializationWriter(FChunkDataSerializationFactory::Create(FileSystem.Get(), ManifestB->ManifestMeta.FeatureLevel));
				FParallelChunkWriterConfig ChunkWriterConfig = FParallelChunkWriterConfig({5, 5, 50, 8, Configuration.CloudDirectory, ManifestB->ManifestMeta.FeatureLevel});
				TUniquePtr<IParallelChunkWriter> ChunkWriter(FParallelChunkWriterFactory::Create(ChunkWriterConfig, FileSystem.Get(), ChunkDataSerializationWriter.Get(), StatsCollector.Get()));
				StreamBuffer.SetNumUninitialized(0, AllowShrinking);
				for (const TTuple<FBlockStructure, FChunkPart>& NewChunk : NewChunks)
				{
					const FBlockStructure& NewChunkStructure = NewChunk.Get<0>();
					const FChunkPart& NewChunkPart = NewChunk.Get<1>();
					NewChunkWindowSizes.Add(NewChunkPart.Guid, NewChunkPart.Size);

					DeltaOptimiseHelpers::StompChunkPart(NewChunkPart, NewChunkStructure, ManifestSearcher, UpdatedFiles);

					// Collect all the chunk data.
					const FBlockEntry* NewChunkBlock = NewChunkStructure.GetHead();
					StreamBuffer.SetNumUninitialized(NewChunkPart.Size, AllowShrinking);
					uint32 ChunkLocationOffset = 0;
					while (NewChunkBlock)
					{
						const uint32 SizeRead = UnknownDataStream->DequeueData(StreamBuffer.GetData() + ChunkLocationOffset, NewChunkBlock->GetSize());
						check(SizeRead == NewChunkBlock->GetSize());
						ChunkLocationOffset += NewChunkBlock->GetSize();
						NewChunkBlock = NewChunkBlock->GetNext();
					}
					check(ChunkLocationOffset == StreamBuffer.Num());

					// Ensure padding if necessary.
					StreamBuffer.SetNumZeroed(OutputChunkSize, AllowShrinking);

					// Save out new chunk.
					const uint64 NewChunkHash = FRollingHash::GetHashForDataSet(StreamBuffer.GetData(), StreamBuffer.Num());
					const FSHAHash NewChunkSha = DeltaOptimiseHelpers::GetShaForDataSet(StreamBuffer.GetData(), StreamBuffer.Num());

					// Save it out.
					ChunkWriter->AddChunkData(StreamBuffer, NewChunkPart.Guid, NewChunkHash, NewChunkSha);
				}

				// We also need to potentially upgrade chunks from ManifestA into ManifestB feature level.
				if (bUsingDifferentChunkSubdir)
				{
					// Enumerate all chunks referenced from ManifestA.
					TSet<FGuid> SourceChunkReferences;
					FChunkSearcher::FFileDListNode* FileNode = ManifestSearcher.GetHead();
					while (FileNode)
					{
						FChunkSearcher::FChunkDListNode* ChunkNode = FileNode->GetValue().ChunkParts.GetHead();
						while (ChunkNode)
						{
							if (ManifestA->GetChunkInfo(ChunkNode->GetValue().ChunkPart.Guid) != nullptr)
							{
								SourceChunkReferences.Add(ChunkNode->GetValue().ChunkPart.Guid);
							}
							ChunkNode = ChunkNode->GetNextNode();
						}
						FileNode = FileNode->GetNextNode();
					}

					// Load these chunks and save them out in the new format.
					TUniquePtr<IChunkReferenceTracker> UpgradeChunkReferenceTracker(ChunkReferenceTrackerFactory->Create(SourceChunkReferences.Array()));
					TUniquePtr<ICloudChunkSource> UpgradeCloudChunkSource(CloudChunkSourceFactoryA->Create(UpgradeChunkReferenceTracker.Get()));
					for (const FGuid& UpgradeChunk : SourceChunkReferences)
					{
						const FChunkInfo* UpgradeChunkInfo = ManifestA->GetChunkInfo(UpgradeChunk);
						IChunkDataAccess* UpgradeChunkDataAccess = UpgradeCloudChunkSource->Get(UpgradeChunk);
						checkf(UpgradeChunkDataAccess != nullptr, TEXT("Failed to download chunk from source %s."), *FBuildPatchUtils::GetDataFilename(*ManifestA.Get(), UpgradeChunk));
						FScopeLockedChunkData LockedChunkData(UpgradeChunkDataAccess);
						TArray<uint8> ChunkDataArray(LockedChunkData.GetData(), LockedChunkData.GetHeader()->DataSizeUncompressed);
						ChunkWriter->AddChunkData(MoveTemp(ChunkDataArray), UpgradeChunk, UpgradeChunkInfo->Hash, UpgradeChunkInfo->ShaHash);
						UpgradeChunkReferenceTracker->PopReference(UpgradeChunk);
					}
				}

				// We always make sure padding chunks are saved out, so a legacy client could actually grab it.
				FSHAHash PaddingChunkSha;
				FGuid PaddingChunkId = PaddingChunk::MakePaddingGuid(0);
				NewChunkWindowSizes.Add(PaddingChunkId, PaddingChunk::ChunkSize);
				StreamBuffer.SetNumUninitialized(PaddingChunk::ChunkSize);
				FMemory::Memset(StreamBuffer.GetData(), PaddingChunkId.D, PaddingChunk::ChunkSize);
				FSHA1::HashBuffer(StreamBuffer.GetData(), PaddingChunk::ChunkSize, PaddingChunkSha.Hash);
				ChunkWriter->AddChunkData(StreamBuffer, PaddingChunkId, FRollingHash::GetHashForDataSet(StreamBuffer.GetData(), PaddingChunk::ChunkSize), PaddingChunkSha);
				for (uint32 LoopIdx = 1; LoopIdx <= 255; ++LoopIdx)
				{
					const uint8 Byte = LoopIdx & 0xFF;
					PaddingChunkId.D = Byte;
					NewChunkWindowSizes.Add(PaddingChunkId, PaddingChunk::ChunkSize);
					FMemory::Memset(StreamBuffer.GetData(), PaddingChunkId.D, PaddingChunk::ChunkSize);
					FSHA1::HashBuffer(StreamBuffer.GetData(), PaddingChunk::ChunkSize, PaddingChunkSha.Hash);
					ChunkWriter->AddChunkData(StreamBuffer, PaddingChunkId, FRollingHash::GetHashForDataSet(StreamBuffer.GetData(), PaddingChunk::ChunkSize), PaddingChunkSha);
				}

				// Complete chunk writer.
				FParallelChunkWriterSummaries ChunkWriterSummaries = ChunkWriter->OnProcessComplete();

				// Produce the new stomped file manifests, but remove any that we no longer need if they don't actually change with the delta.
				FileManifestList = ManifestSearcher.BuildNewFileManifestList();
				for (auto FileListIterator = FileManifestList.FileList.CreateIterator(); FileListIterator; ++FileListIterator)
				{
					if (ManifestB->IsFileOutdated(ManifestA.ToSharedRef(), (*FileListIterator).Filename) == false)
					{
						FileListIterator.RemoveCurrent();
					}
				}

				// Save out the delta file.
				DeltaManifest.ManifestMeta = ManifestB->ManifestMeta;
				DeltaManifest.CustomFields = ManifestB->CustomFields;
				DeltaManifest.FileManifestList = MoveTemp(FileManifestList);
				TSet<FGuid> AddedChunkInfos;
				for (const FFileManifest& FileManifest : DeltaManifest.FileManifestList.FileList)
				{
					for (const FChunkPart& ChunkPart : FileManifest.ChunkParts)
					{
						bool bWasAlreadyInSet = false;
						AddedChunkInfos.Add(ChunkPart.Guid, &bWasAlreadyInSet);
						if (!bWasAlreadyInSet)
						{
							const FChunkInfo* OldChunkInfo = ManifestB->GetChunkInfo(ChunkPart.Guid);
							if (OldChunkInfo != nullptr)
							{
								DeltaManifest.ChunkDataList.ChunkList.Add(*OldChunkInfo);
							}
							else
							{
								OldChunkInfo = ManifestA->GetChunkInfo(ChunkPart.Guid);
								if (OldChunkInfo != nullptr)
								{
									DeltaManifest.ChunkDataList.ChunkList.Add(*OldChunkInfo);
								}
								else
								{
									FChunkInfo& NewChunkInfo = DeltaManifest.ChunkDataList.ChunkList.AddDefaulted_GetRef();
									NewChunkInfo.Guid = ChunkPart.Guid;
									NewChunkInfo.Hash = ChunkWriterSummaries.ChunkOutputHashes[ChunkPart.Guid];
									NewChunkInfo.ShaHash = ChunkWriterSummaries.ChunkOutputShas[ChunkPart.Guid];
									NewChunkInfo.GroupNumber = FCrc::MemCrc32(&ChunkPart.Guid, sizeof(FGuid)) % 100;
									NewChunkInfo.WindowSize = NewChunkWindowSizes[ChunkPart.Guid];
									NewChunkInfo.FileSize = ChunkWriterSummaries.ChunkOutputSizes[ChunkPart.Guid];
								}
							}
						}
					}
				}
				DeltaManifest.InitLookups();
				// For save format, we'll pick the newest of two input manifests, or EFeatureLevel::FirstOptimisedDelta if manifests are older.
				EFeatureLevel DeltaOutputFormat = FMath::Max3(EFeatureLevel::FirstOptimisedDelta, ManifestA->GetFeatureLevel(), ManifestB->GetFeatureLevel());
				const FString TmpOutputDeltaFilename = OutputDeltaFilename + TEXT("tmp");
				DeltaManifest.SaveToFile(TmpOutputDeltaFilename, DeltaOutputFormat);
				FileSystem->MoveFile(*OutputDeltaFilename, *TmpOutputDeltaFilename);
				FinalStatLogs.Add(FString::Printf(TEXT("Saved new optimised delta file %s"), *OutputDeltaFilename));
			}

			if (bSuccess)
			{
				// Count stats?
				TSet<FGuid> ChunksUnknown = ChunksB.Difference(ChunksA);
				uint64 OriginalUnknownBytes = 0;
				for (const FString& ManifestBFile : ListHelpers::GetFileList(*ManifestB))
				{
					for (const FChunkPart& ChunkPart : ManifestB->GetFileManifest(ManifestBFile)->ChunkParts)
					{
						if (ChunksUnknown.Contains(ChunkPart.Guid))
						{
							OriginalUnknownBytes += ChunkPart.Size;
						}
					}
				}
				uint64 FinalUnknownBytes = 0;
				for (const FFileManifest& FileManifest : DeltaManifest.FileManifestList.FileList)
				{
					for (const FChunkPart& ChunkPart : FileManifest.ChunkParts)
					{
						const bool bDeltaUniqueChunk = ManifestA->GetChunkInfo(ChunkPart.Guid) == nullptr && ManifestB->GetChunkInfo(ChunkPart.Guid) == nullptr;
						if (bDeltaUniqueChunk)
						{
							FinalUnknownBytes += ChunkPart.Size;
						}
					}
				}
				uint64 FinalUnknownCompressedBytes = 0;
				TSet<FGuid> TempTest;
				for (const FChunkInfo& DeltaChunkInfo : DeltaManifest.ChunkDataList.ChunkList)
				{
					const bool bDeltaUniqueChunk = ManifestA->GetChunkInfo(DeltaChunkInfo.Guid) == nullptr && ManifestB->GetChunkInfo(DeltaChunkInfo.Guid) == nullptr;
					if (bDeltaUniqueChunk)
					{
						FinalUnknownCompressedBytes += DeltaChunkInfo.FileSize;
						check(TempTest.Contains(DeltaChunkInfo.Guid) == false);
						TempTest.Add(DeltaChunkInfo.Guid);
					}
				}
				int64 DeltaFileSize = 0;
				if (bRunProcess && !FileSystem->GetFileSize(*OutputDeltaFilename, DeltaFileSize))
				{
					UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Could not save output to %s"), *OutputDeltaFilename);
					bSuccess = false;
					DeltaFileSize = 0;
				}
				ProcessTimer.Stop();

				// Final improvement stat logs.
				if (bRunProcess || bDeltaPreviouslyCompleted)
				{
					FinalUnknownCompressedBytes += DeltaFileSize;
					FinalStatLogs.Add(FString::Printf(TEXT("Final unknown compressed bytes, plus meta %llu"), FinalUnknownCompressedBytes));
					FinalStatLogs.Add(FString::Printf(TEXT("Original unknown compressed bytes         %llu"), OriginalUnknownCompressedBytes));
					if (OriginalUnknownCompressedBytes > FinalUnknownCompressedBytes)
					{
						FinalStatLogs.Add(FString::Printf(TEXT("Improvement: %s"), *FText::AsPercent(1.0 - ((double)FinalUnknownCompressedBytes / (double)OriginalUnknownCompressedBytes), &PercentFormat).ToString()));
					}
				}

				if (bRunProcess)
				{
					const FString TempMetaFilename = OutputDeltaFilename.Replace(TEXT("Deltas/"), TEXT("DeltaMetas/")).Replace(TEXT(".delta"), TEXT(".json"));
					FString JsonOutput;
					TSharedRef<FDeltaJsonWriter> Writer = FDeltaJsonWriterFactory::Create(&JsonOutput);
					Writer->WriteObjectStart();
					{
						Writer->WriteValue(TEXT("SourceBuildVersion"), ManifestA->GetVersionString());
						Writer->WriteValue(TEXT("DestinationBuildVersion"), ManifestB->GetVersionString());
						Writer->WriteValue(TEXT("OriginalUnknownBuildBytes"), (int64)OriginalUnknownBytes);
						Writer->WriteValue(TEXT("FinalUnknownBuildBytes"), (int64)FinalUnknownBytes);
						Writer->WriteValue(TEXT("OriginalUnknownCompressedBytes"), (int64)OriginalUnknownCompressedBytes);
						Writer->WriteValue(TEXT("FinalUnknownCompressedBytes"), (int64)FinalUnknownCompressedBytes);
						Writer->WriteValue(TEXT("ChunkBuildATime"), ChunkingTimer.GetSeconds());
						Writer->WriteValue(TEXT("ScanBuildBTime"), ScanningTimer.GetSeconds());
						Writer->WriteValue(TEXT("TotalProcessTime"), ProcessTimer.GetSeconds());
					}
					Writer->WriteObjectEnd();
					Writer->Close();
					if (!FFileHelper::SaveStringToFile(JsonOutput, *TempMetaFilename))
					{
						UE_LOG(LogChunkDeltaOptimiser, Error, TEXT("Could not save output to %s"), *TempMetaFilename);
						bSuccess = false;
					}
				}
			}
		}

		bShouldRun = false;
		return FinalStatLogs;
	}

	void FChunkDeltaOptimiser::HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		TPromise<FBuildPatchAppManifestPtr>* RelevantPromisePtr = RequestId == RequestIdManifestA ? &PromiseManifestA : RequestId == RequestIdManifestB ? &PromiseManifestB : nullptr;
		if (RelevantPromisePtr != nullptr)
		{
			if (Download->ResponseSuccessful())
			{
				Async(EAsyncExecution::ThreadPool, [Download, RelevantPromisePtr]()
				{
					FBuildPatchAppManifestPtr Manifest = MakeShareable(new FBuildPatchAppManifest());
					if (!Manifest->DeserializeFromData(Download->GetData()))
					{
						Manifest.Reset();
					}
					RelevantPromisePtr->SetValue(Manifest);
				});
			}
			else
			{
				RelevantPromisePtr->SetValue(FBuildPatchAppManifestPtr());
			}
		}
	}

	FBlockStructure FChunkDeltaOptimiser::GetDesiredBytes(const FBuildPatchAppManifestPtr& Manifest, const TSet<FGuid>& UnknownChunks)
	{
		uint64 UnknownCount = 0;
		FBlockStructure DesiredBytes;
		uint64 ChunkPartCount = 0;
		for (const FString& BuildFile : ListHelpers::GetFileList(*Manifest.Get()))
		{
			const FFileManifest* FileManifest = Manifest->GetFileManifest(BuildFile);
			for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
			{
				if (UnknownChunks.Contains(ChunkPart.Guid))
				{
					DesiredBytes.Add(ChunkPartCount, ChunkPart.Size, ESearchDir::FromEnd);
					UnknownCount += ChunkPart.Size;
				}
				ChunkPartCount += ChunkPart.Size;
			}
		}
		check(UnknownCount == BlockStructureHelpers::CountSize(DesiredBytes));
		return DesiredBytes;
	}

	IChunkDeltaOptimiser* FChunkDeltaOptimiserFactory::Create(const FChunkDeltaOptimiserConfiguration& Configuration)
	{
		return new FChunkDeltaOptimiser(Configuration);
	}
}
