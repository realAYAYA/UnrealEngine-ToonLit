// Copyright Epic Games, Inc. All Rights Reserved.

#include "Diffing/DiffManifests.h"

#include "Algo/Transform.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"

#include "Common/ChunkDataSizeProvider.h"
#include "Common/FileSystem.h"
#include "Common/HttpManager.h"
#include "Common/SpeedRecorder.h"
#include "Common/StatsCollector.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/DownloadService.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/OptimisedDelta.h"
#include "BuildPatchFileConstructor.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_CLASS(LogDiffManifests, Log, All);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace BuildPatchServices
{
	namespace DiffHelpers
	{
		struct FSimConfig
		{
		public:
			FSimConfig()
				: InstallMode(EInstallMode::DestructiveInstall)
				, DownloadSpeed(0)
				, DiskReadSpeed(0)
				, DiskWriteSpeed(0)
				, BackupSerialisationSpeed(0)
				, FileCreateTime(0)
			{ }

			BuildPatchServices::EInstallMode InstallMode;
			double DownloadSpeed;
			double DiskReadSpeed;
			double DiskWriteSpeed;
			double BackupSerialisationSpeed;
			double FileCreateTime;
		};

		TArray<double> CalculateInstallTimeCoefficient(const FBuildPatchAppManifestRef& CurrentManifest, const TSet<FString>& InCurrentTags, const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& InInstallTags, const TArray<FSimConfig>& SimConfigs)
		{
			// Process tag setup.
			TSet<FString> CurrentTags = InCurrentTags;
			TSet<FString> InstallTags = InInstallTags;
			if (CurrentTags.Num() == 0)
			{
				CurrentManifest->GetFileTagList(CurrentTags);
			}
			if (InstallTags.Num() == 0)
			{
				InstallManifest->GetFileTagList(InstallTags);
			}
			CurrentTags.Add(TEXT(""));
			InstallTags.Add(TEXT(""));

			// Enumerate what is available in the current install.
			TSet<FString> FilesInstalled;
			TSet<FGuid>   ChunksInstalled;
			CurrentManifest->GetTaggedFileList(CurrentTags, FilesInstalled);
			{
				TSet<FGuid> ChunksReferenced;
				CurrentManifest->GetChunksRequiredForFiles(FilesInstalled, ChunksReferenced);
				CurrentManifest->EnumerateProducibleChunks(CurrentTags, ChunksReferenced, ChunksInstalled);
			}

			// Enumerate what is needed for the update.
			TSet<FString> FilesToBuild;
			TSet<FGuid>   ChunksNeeded;
			{
				TSet<FString> TaggedFiles;
				InstallManifest->GetTaggedFileList(InstallTags, TaggedFiles);
				for (FString& TaggedFile : TaggedFiles)
				{
					const FFileManifest* const OldFile = CurrentManifest->GetFileManifest(TaggedFile);
					const FFileManifest* const NewFile = InstallManifest->GetFileManifest(TaggedFile);
					if (!OldFile || OldFile->FileHash != NewFile->FileHash || !FilesInstalled.Contains(TaggedFile))
					{
						FilesToBuild.Add(MoveTemp(TaggedFile));
					}
				}
			}
			FilesToBuild.Sort(TLess<FString>());
			CurrentManifest->GetChunksRequiredForFiles(FilesToBuild, ChunksNeeded);

			// Setup a chunk reference tracker.
			TArray<FGuid> ChunkReferences;
			for (const FString& FileToBuild : FilesToBuild)
			{
				const FFileManifest* const FileManifest = InstallManifest->GetFileManifest(FileToBuild);
				for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
				{
					ChunkReferences.Add(ChunkPart.Guid);
				}
			}
			TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker(FChunkReferenceTrackerFactory::Create(ChunkReferences));

			// A private struct to simulate based on statistics configuration SimConfigs.
			struct FInstallTimeSim
			{
			public:
				FInstallTimeSim(const IChunkReferenceTracker& InChunkReferenceTracker, const FBuildPatchAppManifest& InInstallManifest, const TSet<FGuid>& InChunksInstalled, const FSimConfig& InConfig)
					: ChunkReferenceTracker(InChunkReferenceTracker)
					, InstallManifest(InInstallManifest)
					, ChunksInstalled(InChunksInstalled)
					, Config(InConfig)
					, Timer(0.0)
				{ }

				void CreateFile()
				{
					Timer += Config.FileCreateTime;
				}

				void TickDownloads()
				{
					// Complete downloads.
					while (DownloadChunks.Num() > 0 && DownloadChunks[0].Get<0>() <= Timer)
					{
						LoadedChunks.Add(DownloadChunks[0].Get<1>());
						DownloadChunks.RemoveAt(0);
					}
					// Queue up some more downloads once our in-flight list is getting emptied.
					if (DownloadChunks.Num() < 50)
					{
						TSet<FGuid> DownloadingChunks;
						Algo::Transform(DownloadChunks, DownloadingChunks, [](const TTuple<double, FGuid>& Elem) { return Elem.Get<1>(); });
						TFunction<bool(const FGuid&)> SelectPredicate = [&](const FGuid& ChunkId) { return !ChunksInstalled.Contains(ChunkId); };
						TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker.SelectFromNextReferences(100, SelectPredicate);
						TFunction<bool(const FGuid&)> RemovePredicate = [&](const FGuid& ChunkId) { return DownloadingChunks.Contains(ChunkId) || LoadedChunks.Contains(ChunkId) || BackupChunks.Contains(ChunkId); };
						BatchLoadChunks.RemoveAll(RemovePredicate);
						double DownloadTime = FMath::Max(DownloadChunks.Num() > 0 ? DownloadChunks.Last().Get<0>() : 0, Timer);
						for (const FGuid& BatchLoadChunk : BatchLoadChunks)
						{
							DownloadTime += (double)InstallManifest.GetChunkInfo(BatchLoadChunk)->FileSize / Config.DownloadSpeed;
							DownloadChunks.Emplace(DownloadTime, BatchLoadChunk);
						}
					}
				}

				void GetChunk(const FChunkInfo& ChunkInfo)
				{
					if (!LoadedChunks.Contains(ChunkInfo.Guid))
					{
						if (BackupChunks.Contains(ChunkInfo.Guid))
						{
							Timer += (double)ChunkInfo.FileSize / Config.DiskReadSpeed;
							LoadedChunks.Add(ChunkInfo.Guid);
						}
						else if (ChunksInstalled.Contains(ChunkInfo.Guid))
						{
							Timer += (double)ChunkInfo.WindowSize / Config.DiskReadSpeed;
							LoadedChunks.Add(ChunkInfo.Guid);
						}
						else
						{
							// figure out when this chunk will finish downloading
							for (int32 DownloadChunkIdx = 0; DownloadChunkIdx < DownloadChunks.Num(); ++DownloadChunkIdx)
							{
								const double& TimeDownloaded = DownloadChunks[DownloadChunkIdx].Get<0>();
								const FGuid& ChunkId = DownloadChunks[DownloadChunkIdx].Get<1>();
								if (ChunkInfo.Guid == ChunkId)
								{
									Timer = TimeDownloaded;
									LoadedChunks.Add(ChunkId);
									DownloadChunks.RemoveAt(DownloadChunkIdx);
									break;
								}
							}
						}
						checkf(LoadedChunks.Contains(ChunkInfo.Guid), TEXT("Logic error with timer simulation."));
					}
				}

				void WriteData(uint32 Size)
				{
					Timer += (double)Size / Config.DiskWriteSpeed;
				}

				void EvaluateDestruction(const FFileManifest& OldFile)
				{
					if (Config.InstallMode == EInstallMode::DestructiveInstall)
					{
						// Collect all chunks in this file.
						TSet<FGuid> FileManifestChunks;
						Algo::Transform(OldFile.ChunkParts, FileManifestChunks, &FChunkPart::Guid);
						FileManifestChunks = FileManifestChunks.Intersect(ChunksInstalled);
						// Select all chunks still required from this file.
						TFunction<bool(const FGuid&)> SelectPredicate = [&](const FGuid& ChunkId) { return !LoadedChunks.Contains(ChunkId) && FileManifestChunks.Contains(ChunkId); };
						TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker.GetNextReferences(TNumericLimits<int32>::Max(), SelectPredicate);
						for (const FGuid& BatchLoadChunk : BatchLoadChunks)
						{
							// Load it from disk.
							Timer += (double)InstallManifest.GetChunkInfo(BatchLoadChunk)->WindowSize / Config.BackupSerialisationSpeed;
							// Save it to backup.
							Timer += (double)InstallManifest.GetChunkInfo(BatchLoadChunk)->FileSize / Config.BackupSerialisationSpeed;
							BackupChunks.Add(BatchLoadChunk);
						}
					}
				}

				double GetTimer() const
				{
					return Timer;
				}

				// Dependencies
				const IChunkReferenceTracker& ChunkReferenceTracker;
				const FBuildPatchAppManifest& InstallManifest;
				const TSet<FGuid>& ChunksInstalled;
				const FSimConfig Config;

				// Tracking
				double Timer;
				TSet<FGuid> LoadedChunks;
				TSet<FGuid> BackupChunks;
				TArray<TTuple<double, FGuid>> DownloadChunks;
			};

			// Setup the simulators and run the process through them.
			TArray<FInstallTimeSim> TimeSims;
			for (const FSimConfig& SimConfig : SimConfigs)
			{
				TimeSims.Emplace(*ChunkReferenceTracker.Get(), InstallManifest.Get(), ChunksInstalled, SimConfig);
			}
			for (const FString& FileToBuild : FilesToBuild)
			{
				// Create a new file.
				for (FInstallTimeSim& TimeSim : TimeSims) { TimeSim.CreateFile(); }

				// For each required chunk.
				const FFileManifest& NewFileManifest = *InstallManifest->GetFileManifest(FileToBuild);
				for (const FChunkPart& ChunkPart : NewFileManifest.ChunkParts)
				{
					// Process completed downloads.
					for (FInstallTimeSim& TimeSim : TimeSims) { TimeSim.TickDownloads(); }

					// Get the chunk.
					const FChunkInfo& ChunkInfo = *InstallManifest->GetChunkInfo(ChunkPart.Guid);
					for (FInstallTimeSim& TimeSim : TimeSims) { TimeSim.GetChunk(ChunkInfo); }

					// Write the chunk to file.
					for (FInstallTimeSim& TimeSim : TimeSims) { TimeSim.WriteData(ChunkPart.Size); }
					ChunkReferenceTracker->PopReference(ChunkPart.Guid);
				}
				// If there's an old file to delete, add time for backing up all of the still referenced chunks.
				if (FilesInstalled.Contains(FileToBuild))
				{
					const FFileManifest& OldFileManifest = *CurrentManifest->GetFileManifest(FileToBuild);
					for (FInstallTimeSim& TimeSim : TimeSims) { TimeSim.EvaluateDestruction(OldFileManifest); }
				}
			}

			// Return the simulation results.
			TArray<double> Results;
			for (FInstallTimeSim& TimeSim : TimeSims) { Results.Add(TimeSim.GetTimer()); }
			return Results;
		}
	}

	class FDiffManifests
		: public IDiffManifests
	{
	public:
		FDiffManifests(const FDiffManifestsConfiguration& InConfiguration);
		~FDiffManifests();

		// IChunkDeltaOptimiser interface begin.
		virtual	bool Run() override;
		// IChunkDeltaOptimiser interface end.

	private:
		bool AsyncRun();
		void HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download);

	private:
		const FDiffManifestsConfiguration Configuration;
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
		TUniquePtr<FStatsCollector> StatsCollector;
		FThreadSafeBool bShouldRun;

		// Manifest downloading
		int32 RequestIdManifestA;
		int32 RequestIdManifestB;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestA;
		TPromise<FBuildPatchAppManifestPtr> PromiseManifestB;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestA;
		TFuture<FBuildPatchAppManifestPtr> FutureManifestB;
	};

	FDiffManifests::FDiffManifests(const FDiffManifestsConfiguration& InConfiguration)
		: Configuration(InConfiguration)
		, CoreTicker(FTSTicker::GetCoreTicker())
		, DownloadCompleteDelegate(FDownloadCompleteDelegate::CreateRaw(this, &FDiffManifests::HandleDownloadComplete))
		, DownloadProgressDelegate()
		, FileSystem(FFileSystemFactory::Create())
		, HttpManager(FHttpManagerFactory::Create())
		, ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create())
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(nullptr))
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), ChunkDataSizeProvider.Get(), InstallerAnalytics.Get()))
		, DownloadService(FDownloadServiceFactory::Create(HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
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

	FDiffManifests::~FDiffManifests()
	{
	}

	bool FDiffManifests::Run()
	{
		// Run any core initialisation required.
		FHttpModule::Get();

		// Kick off Manifest downloads.
		RequestIdManifestA = DownloadService->RequestFile(Configuration.ManifestAUri, DownloadCompleteDelegate, DownloadProgressDelegate);
		RequestIdManifestB = DownloadService->RequestFile(Configuration.ManifestBUri, DownloadCompleteDelegate, DownloadProgressDelegate);

		// Start the generation thread.
		TFuture<bool> Thread = Async(EAsyncExecution::Thread, [this]() { return AsyncRun(); });

		// Main timers.
		double DeltaTime = 0.0;
		double LastTime = FPlatformTime::Seconds();

		// Setup desired frame times.
		float MainsFramerate = 100.0f;
		const float MainsFrameTime = 1.0f / MainsFramerate;

		// Run the main loop.
		while (bShouldRun)
		{
			// Increment global frame counter once for each app tick.
			GFrameCounter++;

			// Application tick.
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTSTicker::GetCoreTicker().Tick(DeltaTime);
			GLog->FlushThreadedLogs();

			// Control frame rate.
			FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

			// Calculate deltas.
			const double AppTime = FPlatformTime::Seconds();
			DeltaTime = AppTime - LastTime;
			LastTime = AppTime;
		}
		GLog->FlushThreadedLogs();

		// Return thread success.
		return Thread.Get();
	}

	bool FDiffManifests::AsyncRun()
	{
		FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
		FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();
		bool bSuccess = true;
		if (ManifestA.IsValid() == false)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not download ManifestA from %s."), *Configuration.ManifestAUri);
			bSuccess = false;
		}
		if (ManifestB.IsValid() == false)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not download ManifestB from %s."), *Configuration.ManifestBUri);
			bSuccess = false;
		}
		if (bSuccess)
		{
			// Check for delta file, replacing ManifestB if we find one
			FOptimisedDeltaConfiguration OptimisedDeltaConfiguration(ManifestB.ToSharedRef());
			OptimisedDeltaConfiguration.SourceManifest = ManifestA;
			OptimisedDeltaConfiguration.DeltaPolicy = EDeltaPolicy::TryFetchContinueWithout;
			OptimisedDeltaConfiguration.CloudDirectories = { FPaths::GetPath(Configuration.ManifestBUri) };
			FOptimisedDeltaDependencies OptimisedDeltaDependencies;
			OptimisedDeltaDependencies.DownloadService = DownloadService.Get();
			TUniquePtr<IOptimisedDelta> OptimisedDelta(FOptimisedDeltaFactory::Create(OptimisedDeltaConfiguration, MoveTemp(OptimisedDeltaDependencies)));
			ManifestB = OptimisedDelta->GetResult().GetValue();
			const int32 MetaDownloadBytes = OptimisedDelta->GetMetaDownloadSize();

			TSet<FString> TagsA, TagsB;
			ManifestA->GetFileTagList(TagsA);
			if (Configuration.TagSetA.Num() > 0)
			{
				TagsA = TagsA.Intersect(Configuration.TagSetA);
			}
			ManifestB->GetFileTagList(TagsB);
			if (Configuration.TagSetB.Num() > 0)
			{
				TagsB = TagsB.Intersect(Configuration.TagSetB);
			}

			int64 NewChunksCount = 0;
			int64 TotalChunkSize = 0;
			TSet<FString> TaggedFileSetA;
			TSet<FString> TaggedFileSetB;
			TSet<FGuid> ChunkSetA;
			TSet<FGuid> ChunkSetB;
			ManifestA->GetTaggedFileList(TagsA, TaggedFileSetA);
			ManifestA->GetChunksRequiredForFiles(TaggedFileSetA, ChunkSetA);
			ManifestB->GetTaggedFileList(TagsB, TaggedFileSetB);
			ManifestB->GetChunksRequiredForFiles(TaggedFileSetB, ChunkSetB);
			TArray<FString> NewChunkPaths;
			for (FGuid& ChunkB : ChunkSetB)
			{
				if (ChunkSetA.Contains(ChunkB) == false)
				{
					++NewChunksCount;
					int32 ChunkFileSize = ManifestB->GetDataSize(ChunkB);
					TotalChunkSize += ChunkFileSize;
					NewChunkPaths.Add(FBuildPatchUtils::GetDataFilename(ManifestB.ToSharedRef(), ChunkB));
					UE_LOG(LogDiffManifests, Verbose, TEXT("New chunk discovered: Size: %10lld, Path: %s"), ChunkFileSize, *NewChunkPaths.Last());
				}
			}

			UE_LOG(LogDiffManifests, Display, TEXT("New chunks:  %lld"), NewChunksCount);
			UE_LOG(LogDiffManifests, Display, TEXT("Total bytes: %lld"), TotalChunkSize);

			TSet<FString> NewFilePaths = TaggedFileSetB.Difference(TaggedFileSetA);
			TSet<FString> RemovedFilePaths = TaggedFileSetA.Difference(TaggedFileSetB);
			TSet<FString> ChangedFilePaths;
			TSet<FString> UnchangedFilePaths;

			const TSet<FString>& SetToIterate = TaggedFileSetB.Num() > TaggedFileSetA.Num() ? TaggedFileSetA : TaggedFileSetB;
			for (const FString& TaggedFile : SetToIterate)
			{
				FSHAHash FileHashA;
				FSHAHash FileHashB;
				if (ManifestA->GetFileHash(TaggedFile, FileHashA) && ManifestB->GetFileHash(TaggedFile, FileHashB))
				{
					if (FileHashA == FileHashB)
					{
						UnchangedFilePaths.Add(TaggedFile);
					}
					else
					{
						ChangedFilePaths.Add(TaggedFile);
					}
				}
			}

			// Log download details.
			FNumberFormattingOptions SizeFormattingOptions;
			SizeFormattingOptions.MaximumFractionalDigits = 3;
			SizeFormattingOptions.MinimumFractionalDigits = 3;

			int64 DownloadSizeA = ManifestA->GetDownloadSize(TagsA);
			int64 BuildSizeA = ManifestA->GetBuildSize(TagsA);
			int64 DownloadSizeB = ManifestB->GetDownloadSize(TagsB);
			int64 BuildSizeB = ManifestB->GetBuildSize(TagsB);
			int64 DeltaDownloadSize = ManifestB->GetDeltaDownloadSize(TagsB, ManifestA.ToSharedRef(), TagsA) + MetaDownloadBytes;
			int64 TempDiskSpaceReq = FileConstructorHelpers::CalculateRequiredDiskSpace(ManifestA, ManifestB.ToSharedRef(), EInstallMode::DestructiveInstall, TagsB);

			// Break down the sizes and delta into new chunks per tag.
			TMap<FString, int64> TagDownloadImpactA;
			TMap<FString, int64> TagBuildImpactA;
			TMap<FString, int64> TagDownloadImpactB;
			TMap<FString, int64> TagBuildImpactB;
			TMap<FString, int64> TagDeltaImpact;
			for (const FString& Tag : TagsA)
			{
				TSet<FString> TagSet;
				TagSet.Add(Tag);
				TagDownloadImpactA.Add(Tag, ManifestA->GetDownloadSize(TagSet));
				TagBuildImpactA.Add(Tag, ManifestA->GetBuildSize(TagSet));
			}
			for (const FString& Tag : TagsB)
			{
				TSet<FString> TagSet;
				TagSet.Add(Tag);
				TagDownloadImpactB.Add(Tag, ManifestB->GetDownloadSize(TagSet));
				TagBuildImpactB.Add(Tag, ManifestB->GetBuildSize(TagSet));
				TagDeltaImpact.Add(Tag, ManifestB->GetDeltaDownloadSize(TagSet, ManifestA.ToSharedRef(), TagsA));
			}
			if (MetaDownloadBytes > 0)
			{
				TagDeltaImpact.FindOrAdd(TEXT("")) += MetaDownloadBytes;
			}

			// Compare tag sets
			TMap<FString, int64> CompareTagSetDeltaImpact;
			TMap<FString, int64> CompareTagSetBuildImpactA;
			TMap<FString, int64> CompareTagSetDownloadSizeA;
			TMap<FString, int64> CompareTagSetBuildImpactB;
			TMap<FString, int64> CompareTagSetDownloadSizeB;
			TMap<FString, int64> CompareTagSetTempDiskSpaceReqs;
			TSet<FString> CompareTagSetKeys;
			for (const TSet<FString>& TagSet : Configuration.CompareTagSets)
			{
				TArray<FString> TagArrayCompare = TagSet.Array();
				Algo::Sort(TagArrayCompare);
				FString TagSetString = FString::Join(TagArrayCompare, TEXT(", "));
				CompareTagSetKeys.Add(TagSetString);
				CompareTagSetDeltaImpact.Add(TagSetString, ManifestB->GetDeltaDownloadSize(TagSet, ManifestA.ToSharedRef(), TagSet) + MetaDownloadBytes);
				CompareTagSetBuildImpactB.Add(TagSetString, ManifestB->GetBuildSize(TagSet));
				CompareTagSetDownloadSizeB.Add(TagSetString, ManifestB->GetDownloadSize(TagSet));
				CompareTagSetBuildImpactA.Add(TagSetString, ManifestA->GetBuildSize(TagSet));
				CompareTagSetDownloadSizeA.Add(TagSetString, ManifestA->GetDownloadSize(TagSet));
				CompareTagSetTempDiskSpaceReqs.Add(TagSetString, FileConstructorHelpers::CalculateRequiredDiskSpace(ManifestA, ManifestB.ToSharedRef(), EInstallMode::DestructiveInstall, TagSet));
			}

			// Log the information.
			TArray<FString> TagArrayB = TagsB.Array();
			Algo::Sort(TagArrayB);
			FString UntaggedLog(TEXT("(untagged)"));
			FString TagLogList = FString::Join(TagArrayB, TEXT(", "));
			if (TagLogList.IsEmpty() || TagLogList.StartsWith(TEXT(", ")))
			{
				TagLogList.InsertAt(0, UntaggedLog);
			}
			UE_LOG(LogDiffManifests, Display, TEXT("TagSet: %s"), *TagLogList);
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:   %20s bytes (%10s, %11s)"), *FText::AsNumber(DownloadSizeA).ToString(), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:      %20s bytes (%10s, %11s)"), *FText::AsNumber(BuildSizeA).ToString(), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s:"), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:   %20s bytes (%10s, %11s)"), *FText::AsNumber(DownloadSizeB).ToString(), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:      %20s bytes (%10s, %11s)"), *FText::AsNumber(BuildSizeB).ToString(), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("%s %s -> %s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString(), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Delta Size:      %20s bytes (%10s, %11s)"), *FText::AsNumber(DeltaDownloadSize).ToString(), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT("    Temp Disk Space: %20s bytes (%10s, %11s)"), *FText::AsNumber(TempDiskSpaceReq).ToString(), *FText::AsMemory(TempDiskSpaceReq, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TempDiskSpaceReq, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			UE_LOG(LogDiffManifests, Display, TEXT(""));

			for (const FString& Tag : TagArrayB)
			{
				UE_LOG(LogDiffManifests, Display, TEXT("%s Impact:"), *(Tag.IsEmpty() ? UntaggedLog : Tag));
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Download Size:  %20s bytes (%10s, %11s)"), *FText::AsNumber(TagDownloadImpactB[Tag]).ToString(), *FText::AsMemory(TagDownloadImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagDownloadImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Build Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(TagBuildImpactB[Tag]).ToString(), *FText::AsMemory(TagBuildImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagBuildImpactB[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Individual Delta Size:     %20s bytes (%10s, %11s)"), *FText::AsNumber(TagDeltaImpact[Tag]).ToString(), *FText::AsMemory(TagDeltaImpact[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(TagDeltaImpact[Tag], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			}

			for (const FString& TagSet : CompareTagSetKeys)
			{
				const FString& TagSetDisplay = TagSet.IsEmpty() || TagSet.StartsWith(TEXT(",")) ? UntaggedLog + TagSet : TagSet;
				UE_LOG(LogDiffManifests, Display, TEXT("Impact of TagSet: %s"), *TagSetDisplay);
				UE_LOG(LogDiffManifests, Display, TEXT("    Download Size:    %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetDownloadSizeB[TagSet]).ToString(), *FText::AsMemory(CompareTagSetDownloadSizeB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetDownloadSizeB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Build Size:       %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetBuildImpactB[TagSet]).ToString(), *FText::AsMemory(CompareTagSetBuildImpactB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetBuildImpactB[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Delta Size:       %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetDeltaImpact[TagSet]).ToString(), *FText::AsMemory(CompareTagSetDeltaImpact[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetDeltaImpact[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
				UE_LOG(LogDiffManifests, Display, TEXT("    Temp Disk Space:  %20s bytes (%10s, %11s)"), *FText::AsNumber(CompareTagSetTempDiskSpaceReqs[TagSet]).ToString(), *FText::AsMemory(CompareTagSetTempDiskSpaceReqs[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(CompareTagSetTempDiskSpaceReqs[TagSet], &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
			}

			// Hit a destructive and nondestructive simulation for a few different specs.
			TArray<DiffHelpers::FSimConfig> SimConfigs;
			// Add some lower spec values, taken from around 25 percentile of stats at the time of writing [July 2019].
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::DestructiveInstall;
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::NonDestructiveInstall;
			SimConfigs[0].DownloadSpeed  = SimConfigs[1].DownloadSpeed  =   1200000.0; // 1.2 MB/s
			SimConfigs[0].DiskReadSpeed  = SimConfigs[1].DiskReadSpeed  =  30000000.0; // 30 MB/s
			SimConfigs[0].DiskWriteSpeed = SimConfigs[1].DiskWriteSpeed =  25000000.0; // 25 MB/s
			// We didn't have stats for BackupSerialisationSpeed, but it runs much slower than disk speed and so we tune it to be sure destructive is relatively penalizing.
			SimConfigs[0].BackupSerialisationSpeed = SimConfigs[1].BackupSerialisationSpeed = 10000000.0; // 10 MB/s
			// Add some lower spec values, taken from around 50 percentile of stats at the time of writing [July 2019].
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::DestructiveInstall;
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::NonDestructiveInstall;
			SimConfigs[2].DownloadSpeed  = SimConfigs[3].DownloadSpeed  =   3500000.0; // 3.5 MB/s
			SimConfigs[2].DiskReadSpeed  = SimConfigs[3].DiskReadSpeed  = 145000000.0; // 145 MB/s
			SimConfigs[2].DiskWriteSpeed = SimConfigs[3].DiskWriteSpeed =  75000000.0; // 75 MB/s
			// We didn't have stats for BackupSerialisationSpeed, but it runs much slower than disk speed and so we tune it to be sure destructive is relatively penalizing.
			SimConfigs[2].BackupSerialisationSpeed = SimConfigs[3].BackupSerialisationSpeed = 20000000.0; // 20 MB/s
			// Add some higher spec values, taken from around 75 percentile of stats at the time of writing [July 2019].
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::DestructiveInstall;
			SimConfigs.AddDefaulted_GetRef().InstallMode = EInstallMode::NonDestructiveInstall;
			SimConfigs[4].DownloadSpeed  = SimConfigs[5].DownloadSpeed  =  13000000.0; // 13 MB/s
			SimConfigs[4].DiskReadSpeed  = SimConfigs[5].DiskReadSpeed  = 295000000.0; // 295 MB/s
			SimConfigs[4].DiskWriteSpeed = SimConfigs[5].DiskWriteSpeed = 125000000.0; // 125 MB/s
			// We didn't have stats for BackupSerialisationSpeed, but it runs much slower than disk speed and so we tune it to be sure destructive is relatively penalizing.
			SimConfigs[4].BackupSerialisationSpeed = SimConfigs[5].BackupSerialisationSpeed = 40000000.0; // 40 MB/s

			// Run the calculations and log.
			TArray<double> InstallTimeCoefficients = DiffHelpers::CalculateInstallTimeCoefficient(ManifestA.ToSharedRef(), TagsA, ManifestB.ToSharedRef(), TagsB, SimConfigs);
			checkf(6 == InstallTimeCoefficients.Num() && 6 == SimConfigs.Num(), TEXT("Unexpected result size from CalculateInstallTimeCoefficient."));
			UE_LOG(LogDiffManifests, Display, TEXT(""));
			UE_LOG(LogDiffManifests, Display, TEXT("Install time coefficients are not accurate timing representations, but are comparable from patch to patch."));
			UE_LOG(LogDiffManifests, Display, TEXT("They can be used to spot out of the ordinary time requirements for installing an update."));
			UE_LOG(LogDiffManifests, Display, TEXT("Install Time Coefficients:"));
			UE_LOG(LogDiffManifests, Display, TEXT("    Low-Spec  DestructiveInstall:    %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[0]));
			UE_LOG(LogDiffManifests, Display, TEXT("    Low-Spec  NonDestructiveInstall: %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[1]));
			UE_LOG(LogDiffManifests, Display, TEXT("    Mid-Spec  DestructiveInstall:    %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[2]));
			UE_LOG(LogDiffManifests, Display, TEXT("    Mid-Spec  NonDestructiveInstall: %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[3]));
			UE_LOG(LogDiffManifests, Display, TEXT("    High-Spec DestructiveInstall:    %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[4]));
			UE_LOG(LogDiffManifests, Display, TEXT("    High-Spec NonDestructiveInstall: %s"), *FPlatformTime::PrettyTime(InstallTimeCoefficients[5]));

			// Save the output.
			if (bSuccess && Configuration.OutputFilePath.IsEmpty() == false)
			{
				FString JsonOutput;
				TSharedRef<FDiffJsonWriter> Writer = FDiffJsonWriterFactory::Create(&JsonOutput);
				Writer->WriteObjectStart();
				{
					Writer->WriteObjectStart(TEXT("ManifestA"));
					{
						Writer->WriteValue(TEXT("AppName"), ManifestA->GetAppName());
						Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestA->GetAppID()));
						Writer->WriteValue(TEXT("VersionString"), ManifestA->GetVersionString());
						Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeA);
						Writer->WriteValue(TEXT("BuildSize"), BuildSizeA);
						Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
						for (const TPair<FString, int64>& Pair : TagDownloadImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetDownloadSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetDownloadSizeA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
						for (const TPair<FString, int64>& Pair : TagBuildImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetBuildSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetBuildImpactA)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
					Writer->WriteObjectStart(TEXT("ManifestB"));
					{
						Writer->WriteValue(TEXT("AppName"), ManifestB->GetAppName());
						Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestB->GetAppID()));
						Writer->WriteValue(TEXT("VersionString"), ManifestB->GetVersionString());
						Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeB);
						Writer->WriteValue(TEXT("BuildSize"), BuildSizeB);
						Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
						for (const TPair<FString, int64>& Pair : TagDownloadImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetDownloadSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetDownloadSizeB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
						for (const TPair<FString, int64>& Pair : TagBuildImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart("CompareTagSetBuildSizes");
						for (const TPair<FString, int64>& Pair : CompareTagSetBuildImpactB)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
					}
					Writer->WriteObjectEnd();
					Writer->WriteObjectStart(TEXT("Differential"));
					{
						Writer->WriteArrayStart(TEXT("NewFilePaths"));
						for (const FString& NewFilePath : NewFilePaths)
						{
							Writer->WriteValue(NewFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("RemovedFilePaths"));
						for (const FString& RemovedFilePath : RemovedFilePaths)
						{
							Writer->WriteValue(RemovedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("ChangedFilePaths"));
						for (const FString& ChangedFilePath : ChangedFilePaths)
						{
							Writer->WriteValue(ChangedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("UnchangedFilePaths"));
						for (const FString& UnchangedFilePath : UnchangedFilePaths)
						{
							Writer->WriteValue(UnchangedFilePath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteArrayStart(TEXT("NewChunkPaths"));
						for (const FString& NewChunkPath : NewChunkPaths)
						{
							Writer->WriteValue(NewChunkPath);
						}
						Writer->WriteArrayEnd();
						Writer->WriteValue(TEXT("TotalChunkSize"), TotalChunkSize);
						Writer->WriteValue(TEXT("DeltaDownloadSize"), DeltaDownloadSize);
						Writer->WriteValue(TEXT("TempDiskSpaceReq"), TempDiskSpaceReq);
						Writer->WriteObjectStart(TEXT("IndividualTagDeltaSizes"));
						for (const TPair<FString, int64>& Pair : TagDeltaImpact)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("CompareTagSetDeltaSizes"));
						for (const TPair<FString, int64>& Pair : CompareTagSetDeltaImpact)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteObjectStart(TEXT("CompareTagSetTempDiskSpaceReqs"));
						for (const TPair<FString, int64>& Pair : CompareTagSetTempDiskSpaceReqs)
						{
							Writer->WriteValue(Pair.Key, Pair.Value);
						}
						Writer->WriteObjectEnd();
						Writer->WriteArrayStart(TEXT("InstallTimeCoefficients"));
						for (const double& InstallTimeCoefficient : InstallTimeCoefficients)
						{
							Writer->WriteValue(InstallTimeCoefficient);
						}
						Writer->WriteArrayEnd();
					}
					Writer->WriteObjectEnd();
				}
				Writer->WriteObjectEnd();
				Writer->Close();
				bSuccess = FFileHelper::SaveStringToFile(JsonOutput, *Configuration.OutputFilePath);
				if (!bSuccess)
				{
					UE_LOG(LogDiffManifests, Error, TEXT("Could not save output to %s"), *Configuration.OutputFilePath);
				}
			}
		}
		bShouldRun = false;
		return bSuccess;
	}

	void FDiffManifests::HandleDownloadComplete(int32 RequestId, const FDownloadRef& Download)
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

	IDiffManifests* FDiffManifestsFactory::Create(const FDiffManifestsConfiguration& Configuration)
	{
		return new FDiffManifests(Configuration);
	}
}