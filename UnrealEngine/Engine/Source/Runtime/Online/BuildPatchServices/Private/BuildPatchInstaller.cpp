// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.cpp: Implements the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

#include "BuildPatchInstaller.h"
#include "IBuildManifestSet.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Math/UnitConversion.h"
#include "Containers/Ticker.h"
#include "HttpModule.h"
#include "BuildPatchFileConstructor.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchUtil.h"
#include "BuildPatchServicesPrivate.h"
#include "BuildPatchSettings.h"
#include "Common/HttpManager.h"
#include "Common/FileSystem.h"
#include "Common/ChunkDataSizeProvider.h"
#include "Installer/InstallerError.h"
#include "Installer/DiskChunkStore.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/DownloadConnectionCount.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/DownloadService.h"
#include "Installer/ChunkDbChunkSource.h"
#include "Installer/InstallChunkSource.h"
#include "Installer/ChainedChunkSource.h"
#include "Installer/Verifier.h"
#include "Installer/FileAttribution.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/InstallerSharedContext.h"
#include "Installer/Prerequisites.h"
#include "Installer/MachineConfig.h"
#include "Installer/MessagePump.h"
#include "Installer/OptimisedDelta.h"
#include "Installer/Statistics/MemoryChunkStoreStatistics.h"
#include "Installer/Statistics/DiskChunkStoreStatistics.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/Statistics/ChunkDbChunkSourceStatistics.h"
#include "Installer/Statistics/InstallChunkSourceStatistics.h"
#include "Installer/Statistics/CloudChunkSourceStatistics.h"
#include "Installer/Statistics/FileConstructorStatistics.h"
#include "Installer/Statistics/VerifierStatistics.h"
#include "Installer/Statistics/FileOperationTracker.h"

#if !defined(ENABLE_PATCH_DISK_OVERFLOW_STORE)
#	define ENABLE_PATCH_DISK_OVERFLOW_STORE 1
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBPSInstallerConfig, Log, All);

namespace ConfigHelpers
{
	using namespace BuildPatchServices;

	int32 LoadNumFileMoveRetries()
	{
		int32 MoveRetries = 5;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("NumFileMoveRetries"), MoveRetries, GEngineIni);
		return FMath::Clamp<int32>(MoveRetries, 1, 50);
	}

	int32 LoadNumInstallerRetries()
	{
		int32 InstallerRetries = 5;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("NumInstallerRetries"), InstallerRetries, GEngineIni);
		return FMath::Clamp<int32>(InstallerRetries, 1, 50);
	}

	float LoadDownloadSpeedAverageTime()
	{
		float AverageTime = 10.0;
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("DownloadSpeedAverageTime"), AverageTime, GEngineIni);
		return FMath::Clamp<float>(AverageTime, 1.0f, 30.0f);
	}

	float DownloadSpeedAverageTime()
	{
		static float AverageTime = LoadDownloadSpeedAverageTime();
		return AverageTime;
	}

	int32 NumFileMoveRetries()
	{
		static int32 MoveRetries = LoadNumFileMoveRetries();
		return MoveRetries;
	}

	int32 NumInstallerRetries()
	{
		static int32 InstallerRetries = LoadNumInstallerRetries();
		return InstallerRetries;
	}

	FOptimisedDeltaConfiguration BuildOptimisedDeltaConfig(const FBuildInstallerConfiguration& Config, const FBuildPatchInstallerAction& InstallerAction)
	{
		check(InstallerAction.IsUpdate());
		// The optimised delta can deal with getting dupe manifests so lets just allow that for easy config.
		FOptimisedDeltaConfiguration OptimisedDeltaConfiguration(InstallerAction.GetSharedInstallOrCurrentManifest());
		OptimisedDeltaConfiguration.SourceManifest = InstallerAction.TryGetSharedCurrentManifest();
		OptimisedDeltaConfiguration.CloudDirectories = Config.CloudDirectories;
		if (!InstallerAction.GetCloudSubdirectory().IsEmpty())
		{
			for (FString& CloudDirectory : OptimisedDeltaConfiguration.CloudDirectories)
			{
				CloudDirectory /= InstallerAction.GetCloudSubdirectory();
			}
		}
		OptimisedDeltaConfiguration.DeltaPolicy = Config.DeltaPolicy;
		OptimisedDeltaConfiguration.InstallMode = Config.InstallMode;
		return OptimisedDeltaConfiguration;
	}
}

namespace InstallerHelpers
{
	using namespace BuildPatchServices;

	void LogBuildStatInfo(const FBuildInstallStats& BuildStats, const FGuid& InstallerId)
	{
		using namespace BuildPatchServices;
		static FCriticalSection DoNotInterleaveLogs;
		FScopeLock ScopeLock(&DoNotInterleaveLogs);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: InstallerId: %s"), *InstallerId.ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesInBuild: %u"), BuildStats.NumFilesInBuild);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesOutdated: %u"), BuildStats.NumFilesOutdated);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesToRemove: %u"), BuildStats.NumFilesToRemove);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksRequired: %u"), BuildStats.NumChunksRequired);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksQueuedForDownload: %u"), BuildStats.ChunksQueuedForDownload);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksLocallyAvailable: %u"), BuildStats.ChunksLocallyAvailable);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ChunksInChunkDbs: %u"), BuildStats.ChunksInChunkDbs);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksDownloaded: %u"), BuildStats.NumChunksDownloaded);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksRecycled: %u"), BuildStats.NumChunksRecycled);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksReadFromChunkDbs: %u"), BuildStats.NumChunksReadFromChunkDbs);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFailedDownloads: %u"), BuildStats.NumFailedDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumBadDownloads: %u"), BuildStats.NumBadDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumAbortedDownloads: %u"), BuildStats.NumAbortedDownloads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumRecycleFailures: %u"), BuildStats.NumRecycleFailures);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunksStoreBooted: %u"), BuildStats.NumChunksStoreBooted);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumDriveStoreChunkLoads: %u"), BuildStats.NumDriveStoreChunkLoads);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumDriveStoreLoadFailures: %u"), BuildStats.NumDriveStoreLoadFailures);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumChunkDbChunksFailed: %u"), BuildStats.NumChunkDbChunksFailed);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalDownloadedData: %llu"), BuildStats.TotalDownloadedData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ActiveRequestCountPeak : %u"), BuildStats.ActiveRequestCountPeak);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDownloadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDownloadSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDownloadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDownloadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDownloadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDownloadSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDownloadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDownloadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalReadData: %llu"), BuildStats.TotalReadData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDiskReadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDiskReadSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDiskReadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDiskReadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDiskReadSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDiskReadSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDiskReadSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDiskReadSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TotalWrittenData: %llu"), BuildStats.TotalWrittenData);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageDiskWriteSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.AverageDiskWriteSpeed).ToString(), *FText::AsMemory(BuildStats.AverageDiskWriteSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.AverageDiskWriteSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakDiskWriteSpeed: %s bytes (%s, %s) /sec"), *FText::AsNumber(BuildStats.PeakDiskWriteSpeed).ToString(), *FText::AsMemory(BuildStats.PeakDiskWriteSpeed, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(BuildStats.PeakDiskWriteSpeed, EMemoryUnitStandard::IEC).ToString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumFilesConstructed: %u"), BuildStats.NumFilesConstructed);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: InitializeTime: %s"), *FPlatformTime::PrettyTime(BuildStats.InitializeTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: TheoreticalDownloadTime: %s"), *FPlatformTime::PrettyTime(BuildStats.TheoreticalDownloadTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ConstructTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ConstructTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: MoveFromStageTime: %s"), *FPlatformTime::PrettyTime(BuildStats.MoveFromStageTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FileAttributesTime: %s"), *FPlatformTime::PrettyTime(BuildStats.FileAttributesTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: VerifyTime: %s"), *FPlatformTime::PrettyTime(BuildStats.VerifyTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: CleanUpTime: %s"), *FPlatformTime::PrettyTime(BuildStats.CleanUpTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PrereqTime: %s"), *FPlatformTime::PrettyTime(BuildStats.PrereqTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessPausedTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessPausedTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessActiveTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessActiveTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessExecuteTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessExecuteTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessSuccess: %s"), BuildStats.ProcessSuccess ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ErrorCode: %s"), *BuildStats.ErrorCode);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FailureReasonText: %s"), *BuildStats.FailureReasonText.BuildSourceString());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FailureType: %s"), *EnumToString(BuildStats.FailureType));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: NumInstallRetries: %u"), BuildStats.NumInstallRetries);
		check(BuildStats.NumInstallRetries == BuildStats.RetryFailureTypes.Num() && BuildStats.NumInstallRetries == BuildStats.RetryErrorCodes.Num());
		for (uint32 RetryIdx = 0; RetryIdx < BuildStats.NumInstallRetries; ++RetryIdx)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: RetryFailureType %u: %s"), RetryIdx, *EnumToString(BuildStats.RetryFailureTypes[RetryIdx]));
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: RetryErrorCodes %u: %s"), RetryIdx, *BuildStats.RetryErrorCodes[RetryIdx]);
		}
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: FinalProgressValue: %f"), BuildStats.FinalProgress);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: OverallRequestSuccessRate: %f"), BuildStats.OverallRequestSuccessRate);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ExcellentDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ExcellentDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: GoodDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.GoodDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: OkDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.OkDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PoorDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.PoorDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: DisconnectedDownloadHealthTime: %s"), *FPlatformTime::PrettyTime(BuildStats.DisconnectedDownloadHealthTime));
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageMemoryStoreUse: %.1f"), BuildStats.AverageMemoryStoreUse);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakMemoryStoreUse: %u"), BuildStats.PeakMemoryStoreUse);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: AverageMemoryStoreRetained: %.1f"), BuildStats.AverageMemoryStoreRetained);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: PeakMemoryStoreRetained: %u"), BuildStats.PeakMemoryStoreRetained);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: MemoryStoreSize: %u"), BuildStats.MemoryStoreSize);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessRequiredDiskSpace: %llu"), BuildStats.ProcessRequiredDiskSpace);
		UE_LOG(LogBuildPatchServices, Log, TEXT("Build Stat: ProcessAvailableDiskSpace: %llu"), BuildStats.ProcessAvailableDiskSpace);
	}

	const TCHAR* GetActionTypeLog(const FInstallerAction& InstallerAction)
	{
		if (InstallerAction.IsInstall())
		{
			return TEXT("Install");
		}
		else if (InstallerAction.IsUpdate())
		{
			return TEXT("Update");
		}
		else if (InstallerAction.IsRepair())
		{
			return TEXT("Repair");
		}
		else if (InstallerAction.IsUninstall())
		{
			return TEXT("Uninstall");
		}
		return TEXT("Invalid");
	}

	FString GetManifestLog(const FBuildPatchAppManifestPtr& Manifest)
	{
		if (Manifest.IsValid())
		{
			return FString::Printf(TEXT("%s %s (%s)"), *Manifest->GetAppName(), *Manifest->GetVersionString(), *Manifest->GetBuildId());
		}
		else
		{
			return TEXT("NULL");
		}
	}

	void LogBuildConfiguration(const BuildPatchServices::FBuildInstallerConfiguration& InstallerConfiguration, const FGuid& InstallerId)
	{
		if (!UE_LOG_ACTIVE(LogBPSInstallerConfig, Log)) // Just skip all this iteration if we aren't going to log anything
			return;

		using namespace BuildPatchServices;
		static FCriticalSection DoNotInterleaveLogs;
		FScopeLock ScopeLock(&DoNotInterleaveLogs);
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: InstallerId: %s"), *InstallerId.ToString());
		for (const FInstallerAction& InstallerAction : InstallerConfiguration.InstallerActions)
		{
			FBuildPatchAppManifestPtr CurrentManifest = StaticCastSharedPtr<FBuildPatchAppManifest>(InstallerAction.TryGetCurrentManifest());
			FBuildPatchAppManifestPtr InstallManifest = StaticCastSharedPtr<FBuildPatchAppManifest>(InstallerAction.TryGetInstallManifest());
			UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: ActionType: %s"), GetActionTypeLog(InstallerAction));
			UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: CurrentManifest: %s"), *GetManifestLog(CurrentManifest));
			UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: InstallManifest: %s"), *GetManifestLog(InstallManifest));
			for (const FString& Tag : InstallerAction.GetInstallTags())
			{
				UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: InstallTags: %s"), *Tag);
			}
			TSet<FString> ValidTags;
			InstallerAction.GetInstallOrCurrentManifest()->GetFileTagList(ValidTags);
			for (const FString& Tag : ValidTags)
			{
				UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: ValidTags: %s"), *Tag);
			}
			if (!InstallerAction.GetInstallSubdirectory().IsEmpty())
			{
				UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: InstallSubdirectory: %s"), *InstallerAction.GetInstallSubdirectory());
			}
			if (!InstallerAction.GetCloudSubdirectory().IsEmpty())
			{
				UE_LOG(LogBuildPatchServices, Log, TEXT("Build Config: CloudSubdirectory: %s"), *InstallerAction.GetCloudSubdirectory());
			}
		}

		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: InstallDirectory: %s"), *InstallerConfiguration.InstallDirectory);
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: StagingDirectory: %s"), *InstallerConfiguration.StagingDirectory);
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: BackupDirectory: %s"), *InstallerConfiguration.BackupDirectory);

		for (const FString& DatabaseFile : InstallerConfiguration.ChunkDatabaseFiles)
		{
			UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: ChunkDatabaseFile: %s"), *DatabaseFile);
		}
		
		for (const FString& CloudDirectory : InstallerConfiguration.CloudDirectories)
		{
			UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: CloudDirectories: %s"), *CloudDirectory);
		}

		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: InstallMode: %s"), LexToString(InstallerConfiguration.InstallMode));
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: VerifyMode: %s"), LexToString(InstallerConfiguration.VerifyMode));
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: DeltaPolicy: %s"), LexToString(InstallerConfiguration.DeltaPolicy));
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: bRunRequiredPrereqs: %s"), (InstallerConfiguration.bRunRequiredPrereqs) ? TEXT("true") : TEXT("false"));
		UE_LOG(LogBPSInstallerConfig, Log, TEXT("Build Config: bAllowConcurrentExecution: %s"), (InstallerConfiguration.bAllowConcurrentExecution) ? TEXT("true") : TEXT("false"));
	}

	TSet<FGuid> GetMultipleReferencedChunks(IBuildManifestSet* ManifestSet)
	{
		using namespace BuildPatchServices;
		TSet<FGuid> MultipleReferencedChunks;
		TSet<FGuid> AllReferencedChunks;
		TSet<FString> ExpectedFiles;
		ManifestSet->GetExpectedFiles(ExpectedFiles);
		for (const FString& File : ExpectedFiles)
		{
			const FFileManifest* NewFileManifest = ManifestSet->GetNewFileManifest(File);
			if (NewFileManifest != nullptr)
			{
				for (const FChunkPart& ChunkPart : NewFileManifest->ChunkParts)
				{
					if (AllReferencedChunks.Contains(ChunkPart.Guid))
					{
						MultipleReferencedChunks.Add(ChunkPart.Guid);
					}
					else
					{
						AllReferencedChunks.Add(ChunkPart.Guid);
					}
				}
			}
		}
		return MultipleReferencedChunks;
	}

	const TCHAR* GetVerifyErrorCode(const BuildPatchServices::EVerifyResult& VerifyResult)
	{
		using namespace BuildPatchServices;
		switch (VerifyResult)
		{
			case EVerifyResult::FileMissing: return VerifyErrorCodes::FileMissing;
			case EVerifyResult::OpenFileFailed: return VerifyErrorCodes::OpenFileFailed;
			case EVerifyResult::HashCheckFailed: return VerifyErrorCodes::HashCheckFailed;
			case EVerifyResult::FileSizeFailed: return VerifyErrorCodes::FileSizeFailed;
		}

		return VerifyErrorCodes::UnknownFail;
	}

	void LogAdditionalVerifyErrors(BuildPatchServices::EVerifyError Error, int32 Count)
	{
		using namespace BuildPatchServices;
		EVerifyResult VerifyResult;
		if (TryConvertToVerifyResult(Error, VerifyResult))
		{
			FString Prefix = InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(EBuildPatchInstallError::BuildVerifyFail)];
			FString Suffix = InstallerHelpers::GetVerifyErrorCode(VerifyResult);
			UE_LOG(LogBuildPatchServices, Log, TEXT("Build verification error encountered: %s: %d"), *(Prefix + Suffix), Count);
		}
	}

	FOptimisedDeltaDependencies BuildOptimisedDeltaDependencies(const TUniquePtr<IDownloadService>& DownloadService)
	{
		FOptimisedDeltaDependencies OptimisedDeltaDependencies;
		OptimisedDeltaDependencies.DownloadService = DownloadService.Get();
		return OptimisedDeltaDependencies;
	}
}

namespace BuildPatchServices
{
	struct FScopedControllables
	{
	public:
		FScopedControllables(FCriticalSection* InSyncObject, TArray<IControllable*>& InRegistrationArray, bool& bInIsPaused, bool& bInShouldAbort)
			: SyncObject(InSyncObject)
			, RegistrationArray(InRegistrationArray)
			, bIsPaused(bInIsPaused)
			, bShouldAbort(bInShouldAbort)
		{
		}

		~FScopedControllables()
		{
			FScopeLock ScopeLock(SyncObject);
			for (IControllable* Controllable : RegisteredArray)
			{
				RegistrationArray.Remove(Controllable);
			}
		}

		void Register(IControllable* Controllable)
		{
			FScopeLock ScopeLock(SyncObject);
			RegistrationArray.Add(Controllable);
			RegisteredArray.Add(Controllable);
			if (bShouldAbort)
			{
				Controllable->Abort();
			}
			else
			{
				Controllable->SetPaused(bIsPaused);
			}
		}

	private:
		FCriticalSection* SyncObject;
		TArray<IControllable*>& RegistrationArray;
		TArray<IControllable*> RegisteredArray;
		bool& bIsPaused;
		bool& bShouldAbort;
	};

	struct FBuildPatchDownloadRecord
	{
		double StartTime;
		double EndTime;
		int64 DownloadSize;

		FBuildPatchDownloadRecord()
			: StartTime(0)
			, EndTime(0)
			, DownloadSize(0)
		{}

		friend bool operator<(const FBuildPatchDownloadRecord& Lhs, const FBuildPatchDownloadRecord& Rhs)
		{
			return Lhs.StartTime < Rhs.StartTime;
		}
	};

	enum class EMemoryStore : int32
	{
		Install = 0,
		Cloud
	};

	/* FBuildPatchInstaller implementation
	*****************************************************************************/
	FBuildPatchInstaller::FBuildPatchInstaller(FBuildInstallerConfiguration InConfiguration, TMultiMap<FString, FBuildPatchAppManifestRef> InInstallationInfo, const FString& InLocalMachineConfigFile, TSharedPtr<IAnalyticsProvider> InAnalytics, FBuildPatchInstallerDelegate InStartDelegate, FBuildPatchInstallerDelegate InCompleteDelegate)
		: SessionId(FGuid::NewGuid())
		, Thread(nullptr)
		, StartDelegate(InStartDelegate)
		, CompleteDelegate(InCompleteDelegate)
		, Configuration(MoveTemp(InConfiguration))
		, DataStagingDir(Configuration.StagingDirectory / TEXT("PatchData"))
		, InstallStagingDir(Configuration.StagingDirectory / TEXT("Install"))
		, MetaStagingDir(Configuration.StagingDirectory / TEXT("Meta"))
		, PreviousMoveMarker(Configuration.InstallDirectory / TEXT("$movedMarker"))
		, ThreadLock()
		, bSuccess(false)
		, bIsRunning(false)
		, bIsInited(false)
		, bFirstInstallIteration(true)
		, PreviousTotalDownloadRequired(0)
		, BuildStats()
		, BuildProgress()
		, bIsPaused(false)
		, bShouldAbort(false)
		, FilesInstalled()
		, TaggedFiles()
		, FilesToConstruct()
		, InstallationInfo(MoveTemp(InInstallationInfo))
		, LocalMachineConfigFile(InLocalMachineConfigFile)
		, HttpManager(FHttpManagerFactory::Create())
		, FileSystem(FFileSystemFactory::Create())
		, Platform(FPlatformFactory::Create())
		, InstallerError(FInstallerErrorFactory::Create())
		, Analytics(MoveTemp(InAnalytics))
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(Analytics.Get()))
		, FileOperationTracker(Configuration.bTrackFileOperations ? FFileOperationTrackerFactory::Create(FTSTicker::GetCoreTicker()) : FFileOperationTrackerFactory::CreateNull())
		, MemoryChunkStoreStatistics(FMemoryChunkStoreStatisticsFactory::Create(FileOperationTracker.Get()))
		, DiskChunkStoreStatistics(FDiskChunkStoreStatisticsFactory::Create(InstallerAnalytics.Get(), FileOperationTracker.Get()))
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, DiskReadSpeedRecorder(FSpeedRecorderFactory::Create())
		, DiskWriteSpeedRecorder(FSpeedRecorderFactory::Create())
		, ChunkDbReadSpeedRecorder(FSpeedRecorderFactory::Create())
		, ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create())
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), ChunkDataSizeProvider.Get(), InstallerAnalytics.Get()))
		, ChunkDbChunkSourceStatistics(FChunkDbChunkSourceStatisticsFactory::Create(ChunkDbReadSpeedRecorder.Get(), FileOperationTracker.Get()))
		, InstallChunkSourceStatistics(FInstallChunkSourceStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), InstallerAnalytics.Get(), FileOperationTracker.Get()))
		, CloudChunkSourceStatistics(FCloudChunkSourceStatisticsFactory::Create(InstallerAnalytics.Get(), &BuildProgress, FileOperationTracker.Get()))
		, FileConstructorStatistics(FFileConstructorStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), DiskWriteSpeedRecorder.Get(), &BuildProgress, FileOperationTracker.Get()))
		, VerifierStatistics(FVerifierStatisticsFactory::Create(DiskReadSpeedRecorder.Get(), &BuildProgress, FileOperationTracker.Get()))
		, DownloadService(FDownloadServiceFactory::Create(HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
		, MessagePump(FMessagePumpFactory::Create())
		, Controllables()
	{
		InstallerError->RegisterForErrors([this]() { CancelInstall(); });
		Controllables.Add(&BuildProgress);

		// Convert the manifest collection to make other code cleaner.
		FString InstallDirectory = Configuration.InstallDirectory;
		FPaths::NormalizeDirectoryName(InstallDirectory);
		FPaths::CollapseRelativeDirectories(InstallDirectory);

		TArray<FBuildPatchAppManifestPtr> Manifests;
		Manifests.Reserve(Configuration.InstallerActions.Num() * 2);

		for (const FInstallerAction& InstallerAction : Configuration.InstallerActions)
		{
			FBuildPatchInstallerAction& BuildPatchInstallerAction = InstallerActions.Emplace_GetRef(InstallerAction);
			// Make sure existing manifests are added to installation info.
			if (BuildPatchInstallerAction.TryGetCurrentManifest())
			{
				InstallationInfo.Add(InstallDirectory / BuildPatchInstallerAction.GetInstallSubdirectory(), BuildPatchInstallerAction.GetSharedCurrentManifest());
			}
			// Cache chunk sizes too
			if (FBuildPatchAppManifestPtr SharedCurrentManifest = BuildPatchInstallerAction.TryGetSharedCurrentManifest())
			{
				Manifests.Emplace(MoveTemp(SharedCurrentManifest));
			}
			if (FBuildPatchAppManifestPtr SharedInstallManifest = BuildPatchInstallerAction.TryGetSharedInstallManifest())
			{
				Manifests.Emplace(MoveTemp(SharedInstallManifest));
			}
		}

		ChunkDataSizeProvider->AddManifestData(Manifests);

		if (!Configuration.SharedContext)
		{
			Configuration.SharedContext = FBuildInstallerSharedContextFactory::Create(TEXT("BuildPatchInstaller"));
			const bool bUseChunkDBs = !Configuration.ChunkDatabaseFiles.IsEmpty();
			const uint32 NumExpectedThreads = Configuration.SharedContext->NumThreadsPerInstaller(bUseChunkDBs);
			Configuration.SharedContext->PreallocateThreads(NumExpectedThreads);
		}
	}

	FBuildPatchInstaller::~FBuildPatchInstaller()
	{
		PreExit();
	}

	void FBuildPatchInstaller::PreExit()
	{
		// Set shutdown error so any running threads will exit if no error has already been set.
		if (bIsRunning)
		{
			InstallerError->SetError(EBuildPatchInstallError::ApplicationClosing, ApplicationClosedErrorCodes::ApplicationClosed);
		}

		CleanupThread();

		if (InstallerAnalytics.IsValid())
		{
			InstallerAnalytics->Flush();
		}
	}

	bool FBuildPatchInstaller::Tick()
	{
		bool bStillTicking = true;
		PumpMessages();
		if (IsComplete())
		{
			ExecuteCompleteDelegate();
			CleanupThread();
			bStillTicking = false;
		}
		return bStillTicking;
	}

	const IFileOperationTracker* FBuildPatchInstaller::GetFileOperationTracker() const
	{
		return FileOperationTracker.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDownloadSpeedRecorder() const
	{
		return DownloadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDiskReadSpeedRecorder() const
	{
		return DiskReadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetChunkDbReadSpeedRecorder() const
	{
		return ChunkDbReadSpeedRecorder.Get();
	}

	const ISpeedRecorder* FBuildPatchInstaller::GetDiskWriteSpeedRecorder() const
	{
		return DiskWriteSpeedRecorder.Get();
	}

	const IDownloadServiceStatistics* FBuildPatchInstaller::GetDownloadServiceStatistics() const
	{
		return DownloadServiceStatistics.Get();
	}

	const IInstallChunkSourceStatistics* FBuildPatchInstaller::GetInstallChunkSourceStatistics() const
	{
		return InstallChunkSourceStatistics.Get();
	}

	const ICloudChunkSourceStatistics* FBuildPatchInstaller::GetCloudChunkSourceStatistics() const
	{
		return CloudChunkSourceStatistics.Get();
	}

	const IFileConstructorStatistics* FBuildPatchInstaller::GetFileConstructorStatistics() const
	{
		return FileConstructorStatistics.Get();
	}

	const IVerifierStatistics* FBuildPatchInstaller::GetVerifierStatistics() const
	{
		return VerifierStatistics.Get();
	}

	const IMemoryChunkStoreStatistics* FBuildPatchInstaller::GetMemoryChunkStoreStatistics() const
	{
		return MemoryChunkStoreStatistics.Get();
	}

	const IDiskChunkStoreStatistics* FBuildPatchInstaller::GetDiskChunkStoreStatistics() const
	{
		return DiskChunkStoreStatistics.Get();
	}

	const FBuildInstallerConfiguration& FBuildPatchInstaller::GetConfiguration() const
	{
		return Configuration;
	}

	bool FBuildPatchInstaller::StartInstallation()
	{
		if (Thread == nullptr)
		{
			// Start thread!
			Thread = Configuration.SharedContext->CreateThread();
			Thread->RunTask([this]{ Run(); });

			StartDelegate.ExecuteIfBound(AsShared());
		}
		return Thread != nullptr;
	}

	bool FBuildPatchInstaller::Initialize()
	{
		bool bInstallerInitSuccess = true;
		InstallerHelpers::LogBuildConfiguration(Configuration, SessionId);

		// Check provided tags are all valid.
		for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
		{
			TSet<FString> ValidTags;
			ValidTags.Add(TEXT(""));
			InstallerAction.GetInstallOrCurrentManifest().GetFileTagList(ValidTags);
			ValidTags.Add(TEXT(""));
			if (InstallerAction.GetInstallTags().Difference(ValidTags).Num() > 0)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Installer configuration: Invalid InstallTags provided."));
				InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::InvalidInstallTags, 0,
					NSLOCTEXT("BuildPatchInstallError", "InvalidInstallTags", "This installation could not continue due to a configuration issue. Please contact support."));
				bInstallerInitSuccess = false;
				break;
			}
		}

		// Check that we were provided with a bound delegate.
		if (!CompleteDelegate.IsBound())
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer configuration: Completion delegate not provided."));
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingCompleteDelegate);
			bInstallerInitSuccess = false;
		}

		// Make sure we have install directory access.
		IFileManager::Get().MakeDirectory(*Configuration.InstallDirectory, true);
		if (!IFileManager::Get().DirectoryExists(*Configuration.InstallDirectory))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Inability to create InstallDirectory %s."), *Configuration.InstallDirectory);
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingInstallDirectory, 0,
				FText::Format(NSLOCTEXT("BuildPatchInstallError", "MissingInstallDirectory", "The installation directory could not be created.\n{0}"), FText::FromString(Configuration.InstallDirectory)));
			bInstallerInitSuccess = false;
		}

		// Make sure we have staging directory access.
		IFileManager::Get().MakeDirectory(*Configuration.StagingDirectory, true);
		if (!IFileManager::Get().DirectoryExists(*Configuration.StagingDirectory))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Inability to create StagingDirectory %s."), *Configuration.StagingDirectory);
			InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingStageDirectory, 0,
				FText::Format(NSLOCTEXT("BuildPatchInstallError", "MissingStageDirectory", "The following directory could not be created.\n{0}"), FText::FromString(Configuration.StagingDirectory)));
			bInstallerInitSuccess = false;
		}

		// Make sure that we have a prereq if we've specified a prereq only install.
		if (Configuration.InstallMode == EInstallMode::PrereqOnly)
		{
			bool bMissingPrereqPath = true;
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				if (!InstallerAction.GetInstallOrCurrentManifest().GetPrereqPath().IsEmpty())
				{
					bMissingPrereqPath = false;
					break;
				}
			}
			if (bMissingPrereqPath)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: PrereqOnly install selected for manifest with no prereq."));
				InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingPrereqForPrereqOnlyInstall, 0,
					NSLOCTEXT("BuildPatchInstallError", "MissingPrereqForPrereqOnlyInstall", "This installation could not continue due to a prerequisite configuration issue. Please contact support."));
				bInstallerInitSuccess = false;
			}
		}

		// do the delta optimization
		typedef TTuple<FBuildPatchInstallerAction&, TUniquePtr<IOptimisedDelta>> FManifestInfoDeltaPair;
		TArray<FManifestInfoDeltaPair> RunningOptimisedDeltas;
		for (FBuildPatchInstallerAction& InstallerAction : InstallerActions)
		{
			if (InstallerAction.IsUpdate())
			{
				TUniquePtr<IOptimisedDelta> OptimisedDelta(FOptimisedDeltaFactory::Create(
					ConfigHelpers::BuildOptimisedDeltaConfig(Configuration, InstallerAction), 
					InstallerHelpers::BuildOptimisedDeltaDependencies(DownloadService)));
				RunningOptimisedDeltas.Add(FManifestInfoDeltaPair{ InstallerAction, MoveTemp(OptimisedDelta) });
			}
		}
		for (FManifestInfoDeltaPair& RunningOptimisedDelta : RunningOptimisedDeltas)
		{
			FBuildPatchInstallerAction& InstallerAction = RunningOptimisedDelta.Get<0>();
			const TUniquePtr<IOptimisedDelta>& OptimisedDelta = RunningOptimisedDelta.Get<1>();
			const IOptimisedDelta::FResultValueOrError& OptimisedDeltaResult = OptimisedDelta->GetResult();
			PreviousTotalDownloadRequired.Add(OptimisedDelta->GetMetaDownloadSize());
			// The OptimiseDelta class handles policy, so if we get a nullptr back, that is a hard error.
			if (!OptimisedDeltaResult.IsValid())
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Destination manifest could not be obtained."));
				InstallerError->SetError(EBuildPatchInstallError::DownloadError, *OptimisedDeltaResult.GetError());
				bInstallerInitSuccess = false;
				break;
			}
			else
			{
				InstallerAction.SetDeltaManifest(OptimisedDeltaResult.GetValue().ToSharedRef());
			}
		}
		RunningOptimisedDeltas.Empty();

		// We can now build out any systems that need late construction but can survive between retries.
		ManifestSet.Reset(FBuildManifestSetFactory::Create(InstallerActions));
		Verifier.Reset(FVerifierFactory::Create(FileSystem.Get(), VerifierStatistics.Get(), Configuration.VerifyMode, ManifestSet.Get(), Configuration.InstallDirectory, Configuration.InstallMode == EInstallMode::StageFiles ? InstallStagingDir : FString()));

		// Add systems to controllables.
		{
			FScopeLock Lock(&ThreadLock);
			Controllables.Add(Verifier.Get());
		}

		// Update to chunk data size cache since we may have delta manifests now
		{
			TArray<FBuildPatchAppManifestPtr> ActionManifests;
			ActionManifests.Reserve(InstallerActions.Num());
			for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
			{
				ActionManifests.Emplace(InstallerAction.TryGetSharedInstallManifest());
			}
			ChunkDataSizeProvider->AddManifestData(ActionManifests);
		}

		// Init build statistics that are known.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.NumFilesInBuild = ManifestSet->GetNumExpectedFiles();
			BuildStats.ProcessSuccess = bInstallerInitSuccess;
			BuildStats.ErrorCode = InstallerError->GetErrorCode();
			BuildStats.FailureReasonText = InstallerError->GetErrorText();
			BuildStats.FailureType = InstallerError->GetErrorType();
		}

		// Check for any filepath violations.
		FString InstallDirectoryWithSlash = Configuration.InstallDirectory;
		FPaths::NormalizeDirectoryName(InstallDirectoryWithSlash);
		FPaths::CollapseRelativeDirectories(InstallDirectoryWithSlash);
		InstallDirectoryWithSlash /= TEXT("/");
		TSet<FString> ExpectedFiles;
		ManifestSet->GetExpectedFiles(ExpectedFiles);
		for (const FString& ExpectedFile : ExpectedFiles)
		{
			const FString InstallConstructionFile = FPaths::ConvertRelativePathToFull(Configuration.InstallDirectory, ExpectedFile);
			if (!InstallConstructionFile.StartsWith(InstallDirectoryWithSlash))
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Installer setup: Filepath in manifest escaped install directory. %s -> %s"), *ExpectedFile, *InstallConstructionFile);
				InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::InvalidDataInManifest);
				bInstallerInitSuccess = false;
			}
		}

		bIsInited = true;
		return bInstallerInitSuccess;
	}

	uint32 FBuildPatchInstaller::Run()
	{
		// Make sure this function can never be parallelized
		static FCriticalSection SingletonFunctionLockCS;
		const bool bShouldLock = !Configuration.bAllowConcurrentExecution;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Lock();
		}
		bIsRunning = true;
		ProcessExecuteTimer.Start();
		ProcessActiveTimer.Start();

		// No longer queued
		BuildProgress.SetStateProgress(EBuildPatchState::Queued, 1.0f);

		// Init prereqs progress value
		bool bHasPrereqPath = false;
		for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
		{
			if (!InstallerAction.GetInstallOrCurrentManifest().GetPrereqPath().IsEmpty())
			{
				bHasPrereqPath = true;
				break;
			}
		}
		const bool bInstallPrereqs = Configuration.bRunRequiredPrereqs && bHasPrereqPath;
		BuildProgress.SetStateProgress(EBuildPatchState::PrerequisitesInstall, bInstallPrereqs ? 0.0f : 1.0f);

		// Initialization
		InitializeTimer.Start();
		bool bProcessSuccess = Initialize();

		// Run if successful init
		if (bProcessSuccess)
		{
			// Keep track of files that failed verify
			TArray<FString> CorruptFiles;

			// Keep retrying the install while it is not canceled, or caused by download error
			bProcessSuccess = false;
			bool bCanRetry = true;
			int32 InstallRetries = ConfigHelpers::NumInstallerRetries();
			while (!bProcessSuccess && bCanRetry)
			{
				// Inform file operation tracker of the selected manifest.
				FileOperationTracker->OnManifestSelection(ManifestSet.Get());
				MemoryChunkStoreStatistics->SetMultipleReferencedChunk(InstallerHelpers::GetMultipleReferencedChunks(ManifestSet.Get()));

				// Run the install
				bool bInstallSuccess = RunInstallation(CorruptFiles);

				// Backup local changes then move generated files
				bInstallSuccess = bInstallSuccess && RunBackupAndMove();

				// Setup file attributes
				bInstallSuccess = bInstallSuccess && RunFileAttributes();

				// Run Verification
				CorruptFiles.Empty();
				bProcessSuccess = bInstallSuccess && RunVerification(CorruptFiles);

				// Clean staging if INSTALL success, we still do cleanup if we failed at the verify stage.
				if (bInstallSuccess)
				{
					CleanUpTimer.Start();
					if (Configuration.InstallMode == EInstallMode::StageFiles)
					{
						UE_LOG(LogBuildPatchServices, Log, TEXT("Deleting litter from staging area."));
						IFileManager::Get().DeleteDirectory(*DataStagingDir, false, true);
					}
					else
					{
						UE_LOG(LogBuildPatchServices, Log, TEXT("Deleting staging area."));
						IFileManager::Get().DeleteDirectory(*Configuration.StagingDirectory, false, true);
						CleanupEmptyDirectories(Configuration.InstallDirectory);
					}
					CleanUpTimer.Stop();
				}
				BuildProgress.SetStateProgress(EBuildPatchState::CleanUp, 1.0f);

				// Set if we can retry
				--InstallRetries;
				bCanRetry = InstallRetries > 0 && !InstallerError->IsCancelled() && InstallerError->CanRetry();
				const bool bWillRetry = !bProcessSuccess && bCanRetry;

				// If successful or we will retry, remove the moved files marker
				if (bProcessSuccess || bCanRetry)
				{
					UE_LOG(LogBuildPatchServices, Log, TEXT("Reset MM."));
					IFileManager::Get().Delete(*PreviousMoveMarker, false, true);
				}

				// Setup end of attempt stats
				bFirstInstallIteration = false;
				float TempFinalProgress = BuildProgress.GetProgressNoMarquee();
				{
					FScopeLock Lock(&ThreadLock);
					BuildStats.NumInstallRetries = ConfigHelpers::NumInstallerRetries() - (InstallRetries + 1);
					BuildStats.FinalProgress = TempFinalProgress;
					// If we failed, and will retry, record this failure type and reset the abort flag
					if (bWillRetry)
					{
						BuildStats.RetryFailureTypes.Add(InstallerError->GetErrorType());
						BuildStats.RetryErrorCodes.Add(InstallerError->GetErrorCode());
						bShouldAbort = false;
					}
				}

				// If we will retry the install, reset progress states.
				if (bWillRetry)
				{
					InitializeTimer.Start();
					BuildProgress.CancelAbort();
					BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::Resuming, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::Installing, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::SettingAttributes, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::BuildVerification, 0.0f);
					BuildProgress.SetStateProgress(EBuildPatchState::CleanUp, 0.0f);
				}
			}
		}

		if (bProcessSuccess)
		{
			// Run the prerequisites installer if this is our first install and the manifest has prerequisites info
			if (bInstallPrereqs)
			{
				PrereqTimer.Start();
				bProcessSuccess &= RunPrerequisites();
				PrereqTimer.Stop();
			}
		}

		// Make sure all timers are stopped
		InitializeTimer.Stop();
		ConstructTimer.Stop();
		MoveFromStageTimer.Stop();
		FileAttributesTimer.Stop();
		VerifyTimer.Stop();
		CleanUpTimer.Stop();
		PrereqTimer.Stop();
		ProcessPausedTimer.Stop();
		ProcessActiveTimer.Stop();
		ProcessExecuteTimer.Stop();

		// Set final stat values and log out results
		bSuccess = bProcessSuccess;
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.InitializeTime = InitializeTimer.GetSeconds();
			BuildStats.ConstructTime = ConstructTimer.GetSeconds();
			BuildStats.MoveFromStageTime = MoveFromStageTimer.GetSeconds();
			BuildStats.FileAttributesTime = FileAttributesTimer.GetSeconds();
			BuildStats.VerifyTime = VerifyTimer.GetSeconds();
			BuildStats.CleanUpTime = CleanUpTimer.GetSeconds();
			BuildStats.PrereqTime = PrereqTimer.GetSeconds();
			BuildStats.ProcessPausedTime = ProcessPausedTimer.GetSeconds();
			BuildStats.ProcessActiveTime = ProcessActiveTimer.GetSeconds();
			BuildStats.ProcessExecuteTime = ProcessExecuteTimer.GetSeconds();
			BuildStats.ProcessSuccess = bProcessSuccess;
			BuildStats.ErrorCode = InstallerError->GetErrorCode();
			BuildStats.FailureReasonText = InstallerError->GetErrorText();
			BuildStats.FailureType = InstallerError->GetErrorType();
		}

		// Mark that we are done
		bIsRunning = false;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Unlock();
		}
		return bSuccess ? 0 : 1;
	}

	bool FBuildPatchInstaller::CheckForExternallyInstalledFiles(const TSet<FString>& FilesToCheck)
	{
		// Check the marker file for a previous installation unfinished, if we find it, we'll continue to move files and then verify them.
		if (FileSystem->FileExists(*PreviousMoveMarker))
		{
			return true;
		}

		// This check is only valid for an installer that is performing no updates.
		if (ManifestSet->ContainsUpdate())
		{
			return false;
		}

		// Check if any of the provided files exist on disk. If they do, we'll be starting the installation with a verify to find out what work needs to be done.
		for (const FString& FileToCheck : FilesToCheck)
		{
			if (FileSystem->FileExists(*(Configuration.InstallDirectory / FileToCheck)))
			{
				return true;
			}
		}
		return false;
	}

	FChunkDbSourceConfig FBuildPatchInstaller::BuildChunkDbSourceConfig()
	{
		FChunkDbSourceConfig ChunkDbSourceConfig(Configuration.ChunkDatabaseFiles);

		ChunkDbSourceConfig.SharedContext = Configuration.SharedContext.Get();

		// Load batch fetch config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourcePreFetchMinimum"), ChunkDbSourceConfig.PreFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourcePreFetchMaximum"), ChunkDbSourceConfig.PreFetchMaximum, GEngineIni);
		ChunkDbSourceConfig.PreFetchMinimum = FMath::Clamp<int32>(ChunkDbSourceConfig.PreFetchMinimum, 1, 1000);
		ChunkDbSourceConfig.PreFetchMaximum = FMath::Clamp<int32>(ChunkDbSourceConfig.PreFetchMaximum, ChunkDbSourceConfig.PreFetchMinimum, 1000);

		// Load reopen retry time.
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("ChunkDbSourceChunkDbOpenRetryTime"), ChunkDbSourceConfig.ChunkDbOpenRetryTime, GEngineIni);
		ChunkDbSourceConfig.ChunkDbOpenRetryTime = FMath::Clamp<float>(ChunkDbSourceConfig.ChunkDbOpenRetryTime, 0.5f, 60.0f);

		return ChunkDbSourceConfig;
	}

	FInstallSourceConfig FBuildPatchInstaller::BuildInstallSourceConfig(TSet<FGuid> ChunkIgnoreSet)
	{
		FInstallSourceConfig InstallSourceConfig;
		InstallSourceConfig.ChunkIgnoreSet = MoveTemp(ChunkIgnoreSet);

		// Load batch fetch config
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallSourceBatchFetchMinimum"), InstallSourceConfig.BatchFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallSourceBatchFetchMaximum"), InstallSourceConfig.BatchFetchMaximum, GEngineIni);
		InstallSourceConfig.BatchFetchMinimum = FMath::Clamp<int32>(InstallSourceConfig.BatchFetchMinimum, 1, 1000);
		InstallSourceConfig.BatchFetchMaximum = FMath::Clamp<int32>(InstallSourceConfig.BatchFetchMaximum, InstallSourceConfig.BatchFetchMinimum, 1000);

		return InstallSourceConfig;
	}

	FCloudSourceConfig FBuildPatchInstaller::BuildCloudSourceConfig()
	{
		FCloudSourceConfig CloudSourceConfig(Configuration.CloudDirectories);

		CloudSourceConfig.SharedContext = Configuration.SharedContext.Get();

		// Load max download retry count from engine config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkRetries"), CloudSourceConfig.MaxRetryCount, GEngineIni);
		CloudSourceConfig.MaxRetryCount = FMath::Clamp<int32>(CloudSourceConfig.MaxRetryCount, -1, 1000);

		// Load prefetch config
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudSourcePreFetchMinimum"), CloudSourceConfig.PreFetchMinimum, GEngineIni);
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudSourcePreFetchMaximum"), CloudSourceConfig.PreFetchMaximum, GEngineIni);
		CloudSourceConfig.PreFetchMinimum = FMath::Clamp<int32>(CloudSourceConfig.PreFetchMinimum, 1, 1000);
		CloudSourceConfig.PreFetchMaximum = FMath::Clamp<int32>(CloudSourceConfig.PreFetchMaximum, CloudSourceConfig.PreFetchMinimum, 1000);

		// Load retry times from engine config.
		TArray<FString> ConfigStrings;
		GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("RetryTimes"), ConfigStrings, GEngineIni);
		bool bReadArraySuccess = ConfigStrings.Num() > 0;
		TArray<float> RetryDelayTimes;
		RetryDelayTimes.AddZeroed(ConfigStrings.Num());
		for (int32 TimeIdx = 0; TimeIdx < ConfigStrings.Num() && bReadArraySuccess; ++TimeIdx)
		{
			float TimeValue = FPlatformString::Atof(*ConfigStrings[TimeIdx]);
			// Atof will return 0.0 if failed to parse, and we don't expect a time of 0.0 so presume error
			if (TimeValue > 0.0f)
			{
				RetryDelayTimes[TimeIdx] = FMath::Clamp<float>(TimeValue, 0.5f, 300.0f);
			}
			else
			{
				bReadArraySuccess = false;
			}
		}
		// If the retry array was parsed successfully, set on config.
		if (bReadArraySuccess)
		{
			CloudSourceConfig.RetryDelayTimes = MoveTemp(RetryDelayTimes);
		}

		// Load percentiles for download health groupings from engine config.
		// If the enum was changed since writing, the config here needs updating.
		check((int32)EBuildPatchDownloadHealth::NUM_Values == 5);
		TArray<float> HealthPercentages;
		HealthPercentages.AddZeroed((int32)EBuildPatchDownloadHealth::NUM_Values);
		if (GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("OKHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::OK], GEngineIni)
		 && GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("GoodHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::Good], GEngineIni)
		 && GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("ExcellentHealth"), HealthPercentages[(int32)EBuildPatchDownloadHealth::Excellent], GEngineIni))
		{
			CloudSourceConfig.HealthPercentages = MoveTemp(HealthPercentages);
		}

		// Load the delay for how long we get no data for until determining the health as disconnected.
		GConfig->GetFloat(TEXT("Portal.BuildPatch"), TEXT("DisconnectedDelay"), CloudSourceConfig.DisconnectedDelay, GEngineIni);
		CloudSourceConfig.DisconnectedDelay = FMath::Clamp<float>(CloudSourceConfig.DisconnectedDelay, 1.0f, 30.0f);

		// We tell the cloud source to only start downloads once it receives the first get call.
		CloudSourceConfig.bBeginDownloadsOnFirstGet = true;

		return CloudSourceConfig;
	}

	FDownloadConnectionCountConfig FBuildPatchInstaller::BuildConnectionCountConfig()
	{
		FDownloadConnectionCountConfig ConnectionCountConfiguration;

		// Load simultaneous downloads from engine config.
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloads"), (int32&)ConnectionCountConfiguration.FallbackCount, GEngineIni);
		ConnectionCountConfiguration.FallbackCount = FMath::Clamp<uint32>(ConnectionCountConfiguration.FallbackCount, 1, 100);

		bool bDisableDownloadConnectionScaling = false;
		if (GConfig->GetBool(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsDisableConnectionScaling"), bDisableDownloadConnectionScaling, GEngineIni))
		{
			if (bDisableDownloadConnectionScaling)
			{
				ConnectionCountConfiguration.bDisableConnectionScaling = true;
			}
		}

		uint32 MaxLibCurlConnections = 0;
		uint32 MaxDownloadCount = ConnectionCountConfiguration.MaxLimit;
		if (!GConfig->GetInt(TEXT("HTTP"), TEXT("HttpMaxConnectionsPerServer"), (int32&)MaxLibCurlConnections, GEngineIni))
		{
			MaxDownloadCount = 16;
			UE_LOG(LogBuildPatchServices, Warning, TEXT("HttpMaxConnectionsPerServer=0 is not set in the Engine.ini [HTTP] section. Simultaneous downloads will be limited to %d"), MaxDownloadCount);
		}
		else
		{
			if (0 != MaxLibCurlConnections)
			{
				MaxDownloadCount = MaxLibCurlConnections;
				UE_LOG(LogBuildPatchServices, Warning, TEXT("HttpMaxConnectionsPerServer is set to a non-zero value in the Engine.ini [HTTP] section. Simultaneous downloads will be limited to %d"), MaxDownloadCount);
			}
		}

		uint32 MinLimit = ConnectionCountConfiguration.MinLimit;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsLowerLimit"), (int32&)ConnectionCountConfiguration.MinLimit, GEngineIni);
		ConnectionCountConfiguration.MinLimit = FMath::Clamp<uint32>(ConnectionCountConfiguration.MinLimit,
																		FMath::Min(MinLimit, MaxDownloadCount - 2),
																		FMath::Min(32U, MaxDownloadCount - 2));

		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsUpperLimit"), (int32&)ConnectionCountConfiguration.MaxLimit, GEngineIni);
		ConnectionCountConfiguration.MaxLimit = FMath::Clamp<uint32>(ConnectionCountConfiguration.MaxLimit, ConnectionCountConfiguration.MinLimit + 2, MaxDownloadCount);

		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsSlowdownHysteresis"), (int32&)ConnectionCountConfiguration.NegativeHysteresis, GEngineIni);
		ConnectionCountConfiguration.NegativeHysteresis = FMath::Clamp<uint32>(ConnectionCountConfiguration.NegativeHysteresis, 1, 256);

		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsSpeedupHysteresis"), (int32&)ConnectionCountConfiguration.PositiveHysteresis, GEngineIni);
		ConnectionCountConfiguration.PositiveHysteresis = FMath::Clamp<uint32>(ConnectionCountConfiguration.PositiveHysteresis, 1, 256);

		GConfig->GetDouble(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsLowBandwidthFactor"), ConnectionCountConfiguration.LowBandwidthFactor, GEngineIni);
		ConnectionCountConfiguration.LowBandwidthFactor = FMath::Clamp<double>(ConnectionCountConfiguration.LowBandwidthFactor, 0.2L, 0.8L);

		GConfig->GetDouble(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsHighBandwidthFactor"), ConnectionCountConfiguration.HighBandwidthFactor, GEngineIni);
		ConnectionCountConfiguration.HighBandwidthFactor = FMath::Clamp<double>(ConnectionCountConfiguration.HighBandwidthFactor, ConnectionCountConfiguration.LowBandwidthFactor + 0.1L, 1.0L);

		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsAverageMinCount"), (int32&)ConnectionCountConfiguration.AverageSpeedMinCount, GEngineIni);
		ConnectionCountConfiguration.AverageSpeedMinCount = FMath::Clamp<uint32>(ConnectionCountConfiguration.AverageSpeedMinCount, 1, 32);

		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkDownloadsHealthHysteresis"), (int32&)ConnectionCountConfiguration.HealthHysteresis, GEngineIni);
		ConnectionCountConfiguration.HealthHysteresis = FMath::Clamp<uint32>(ConnectionCountConfiguration.HealthHysteresis, 1, 256);

		return ConnectionCountConfiguration;
	}

	bool FBuildPatchInstaller::RunInstallation(TArray<FString>& CorruptFiles)
	{
		UE_LOG(LogBuildPatchServices, Log, TEXT("Starting Installation"));
		// Save the staging directories
		FPaths::NormalizeDirectoryName(DataStagingDir);
		FPaths::NormalizeDirectoryName(InstallStagingDir);
		FPaths::NormalizeDirectoryName(MetaStagingDir);

		// Make sure staging directories exist
		IFileManager::Get().MakeDirectory(*DataStagingDir, true);
		IFileManager::Get().MakeDirectory(*InstallStagingDir, true);
		IFileManager::Get().MakeDirectory(*MetaStagingDir, true);

		// Reset our error and build progress
		InstallerError.Reset(FInstallerErrorFactory::Create());
		InstallerError->RegisterForErrors([this]() { CancelInstall(); });

		// Get the list of required files, by the tags
		TaggedFiles.Empty();
		ManifestSet->GetExpectedFiles(TaggedFiles);
		OutdatedFiles.Empty();
		ManifestSet->GetOutdatedFiles(Configuration.InstallDirectory, OutdatedFiles);

		const bool bIsPrereqOnly = Configuration.InstallMode == EInstallMode::PrereqOnly;
		const bool bHasCorruptFiles = CorruptFiles.Num() > 0;

		// Get the list of files actually needing construction
		FilesToConstruct.Empty();
		if (bHasCorruptFiles)
		{
			FilesToConstruct.Append(CorruptFiles);
		}
		else if (bIsPrereqOnly)
		{
			TArray<FPreReqInfo> PreReqInfos;
			ManifestSet->GetPreReqInfo(PreReqInfos);
			for (FPreReqInfo PreReqInfo : PreReqInfos)
			{
				FilesToConstruct.Add(PreReqInfo.Path);
			}
		}
		else
		{
			FilesToConstruct = OutdatedFiles.Intersect(TaggedFiles);
		}
		FilesToConstruct.Sort(TLess<FString>());
		UE_LOG(LogBuildPatchServices, Log, TEXT("Requiring %d files"), FilesToConstruct.Num());

		// Check if we should skip out of this process due to existing installation,
		// that will mean we start with the verification stage
		if (!bHasCorruptFiles && (bIsPrereqOnly || CheckForExternallyInstalledFiles(FilesToConstruct)))
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Detected previous staging completed, or existing files in target directory"));
			// Add required files to the verifier as 'touched' since we do not know their state.
			Verifier->AddTouchedFiles(FilesToConstruct);
			// Set weights for verify only
			BuildProgress.SetStateWeight(EBuildPatchState::Downloading, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::Installing, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::MovingToInstall, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::SettingAttributes, 0.2f);
			BuildProgress.SetStateWeight(EBuildPatchState::BuildVerification, 1.0f);
			// Mark all installation steps complete
			BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Resuming, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
			// Stop relevant timers
			InitializeTimer.Stop();
			return true;
		}

		if (!bHasCorruptFiles)
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.NumFilesOutdated = FilesToConstruct.Num();
		}

		// Make sure all the files won't exceed the maximum path length
		for (const FString& FileToConstruct : FilesToConstruct)
		{
			const FString InstallConstructionFile = Configuration.InstallDirectory / FileToConstruct;
			const FString StagedConstructionFile = InstallStagingDir / FileToConstruct;
			if (InstallConstructionFile.Len() >= FPlatformMisc::GetMaxPathLength())
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Could not create new file due to exceeding maximum path length %s"), *InstallConstructionFile);
				InstallerError->SetError(EBuildPatchInstallError::PathLengthExceeded, PathLengthErrorCodes::InstallDirectory);
				return false;
			}
			if (StagedConstructionFile.Len() >= FPlatformMisc::GetMaxPathLength())
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Could not create new file due to exceeding maximum path length %s"), *StagedConstructionFile);
				InstallerError->SetError(EBuildPatchInstallError::PathLengthExceeded, PathLengthErrorCodes::StagingDirectory);
				return false;
			}
		}

		// Set initial states on IO state tracker.
		const bool bVerifyAllFiles = Configuration.VerifyMode == EVerifyMode::ShaVerifyAllFiles || Configuration.VerifyMode == EVerifyMode::FileSizeCheckAllFiles;
		const EFileOperationState UntouchedFileState = bVerifyAllFiles ? EFileOperationState::Installed : EFileOperationState::Complete;
		for (const FString& TaggedFile : TaggedFiles)
		{
			if (!FilesToConstruct.Contains(TaggedFile))
			{
				FileOperationTracker->OnFileStateUpdate(TaggedFile, UntouchedFileState);
			}
		}

		// Default chunk store size to tie in with the default prefetch maxes for source configs.
		const int32 DefaultChunkDbMaxRead = FChunkDbSourceConfig({}).PreFetchMaximum;
		const int32 DefaultInstallMaxRead = FInstallSourceConfig().BatchFetchMaximum;
		const int32 DefaultCloudMaxRead = FCloudSourceConfig({}).PreFetchMaximum;
		int32 ChunkStoreMemorySize = DefaultCloudMaxRead + DefaultChunkDbMaxRead + DefaultInstallMaxRead;
		// Load overridden size from config.
		if (!GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkStoreMemorySize"), ChunkStoreMemorySize, GEngineIni))
		{
			// If we didn't get the new value, check for legacy ones.
			int32 CloudChunkStoreMemorySize = DefaultCloudMaxRead + DefaultChunkDbMaxRead;
			int32 InstallChunkStoreMemorySize = DefaultInstallMaxRead;
			const bool bLoadedCloudSize = GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudChunkStoreMemorySize"), CloudChunkStoreMemorySize, GEngineIni);
			const bool bLoadedInstallSize = GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallChunkStoreMemorySize"), InstallChunkStoreMemorySize, GEngineIni);
			if (bLoadedCloudSize || bLoadedInstallSize)
			{
				ChunkStoreMemorySize = CloudChunkStoreMemorySize + InstallChunkStoreMemorySize;
			}
		}
		// Clamp to sensible limits.
		ChunkStoreMemorySize = FMath::Clamp<int32>(ChunkStoreMemorySize, 64, 2048);
		// Cache the last download requirement in case we are running a retry.
		PreviousTotalDownloadRequired.Add(CloudChunkSourceStatistics->GetRequiredDownloadSize());
		// Reset so that we don't double count data.
		CloudChunkSourceStatistics->OnRequiredDataUpdated(0);
		CloudChunkSourceStatistics->OnReceivedDataUpdated(0);

		// Scoped systems composition and execution.
		{
			TUniquePtr<IChunkDataSerialization> ChunkDataSerialization(FChunkDataSerializationFactory::Create(
				FileSystem.Get()));
			TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker(FChunkReferenceTrackerFactory::Create(
				ManifestSet.Get(),
				FilesToConstruct));
			TSet<FGuid> ReferencedChunks = ChunkReferenceTracker->GetReferencedChunks();
			TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy(FChunkEvictionPolicyFactory::Create(
				ChunkReferenceTracker.Get()));
			TUniquePtr<IDiskChunkStore> DiskOverflowStore;
#if ENABLE_PATCH_DISK_OVERFLOW_STORE
			FDiskChunkStoreConfig DiskChunkStoreConfig(DataStagingDir);
			DiskChunkStoreConfig.SharedContext = Configuration.SharedContext.Get();
			DiskOverflowStore.Reset(FDiskChunkStoreFactory::Create(
				FileSystem.Get(),
				ChunkDataSerialization.Get(),
				DiskChunkStoreStatistics.Get(),
				MoveTemp(DiskChunkStoreConfig)));
#endif // ENABLE_PATCH_DISK_OVERFLOW_STORE
			TUniquePtr<IMemoryChunkStore> CloudChunkStore(FMemoryChunkStoreFactory::Create(
				ChunkStoreMemorySize,
				MemoryEvictionPolicy.Get(),
				DiskOverflowStore.Get(),
				MemoryChunkStoreStatistics.Get()));
			TUniquePtr<IChunkDbChunkSource> ChunkDbChunkSource(FChunkDbChunkSourceFactory::Create(
				BuildChunkDbSourceConfig(),
				Platform.Get(),
				FileSystem.Get(),
				CloudChunkStore.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				ChunkDbChunkSourceStatistics.Get()));
			TUniquePtr<IInstallChunkSource> InstallChunkSource(FInstallChunkSourceFactory::Create(
				BuildInstallSourceConfig(ChunkDbChunkSource->GetAvailableChunks()),
				FileSystem.Get(),
				CloudChunkStore.Get(),
				ChunkReferenceTracker.Get(),
				InstallerError.Get(),
				InstallChunkSourceStatistics.Get(),
				InstallationInfo,
				ManifestSet.Get()));
			TSet<FGuid> InitialDownloadChunks = ReferencedChunks.Difference(InstallChunkSource->GetAvailableChunks()).Difference(ChunkDbChunkSource->GetAvailableChunks());
			FileOperationTracker->OnDataStateUpdate(ReferencedChunks.Intersect(ChunkDbChunkSource->GetAvailableChunks()), EFileOperationState::PendingLocalChunkDbData);
			FileOperationTracker->OnDataStateUpdate(ReferencedChunks.Intersect(InstallChunkSource->GetAvailableChunks()).Difference(ChunkDbChunkSource->GetAvailableChunks()), EFileOperationState::PendingLocalInstallData);
			FileOperationTracker->OnDataStateUpdate(InitialDownloadChunks, EFileOperationState::PendingRemoteCloudData);
			TUniquePtr<IDownloadConnectionCount> DownloadConnectionCount(FDownloadConnectionCountFactory::Create(BuildConnectionCountConfig(), DownloadServiceStatistics.Get()));
			TUniquePtr<ICloudChunkSource> CloudChunkSource(FCloudChunkSourceFactory::Create(
				BuildCloudSourceConfig(),
				Platform.Get(),
				CloudChunkStore.Get(),
				DownloadService.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				DownloadConnectionCount.Get(),
				CloudChunkSourceStatistics.Get(),
				ManifestSet.Get(),
				MoveTemp(InitialDownloadChunks)));
			TArray<IChunkSource*> ChunkSources;
			ChunkSources.Add(ChunkDbChunkSource.Get());
			ChunkSources.Add(InstallChunkSource.Get());
			ChunkSources.Add(CloudChunkSource.Get());
			TUniquePtr<IChainedChunkSource> ChainedChunkSource(FChainedChunkSourceFactory::Create(
				ChunkSources));
			TUniquePtr<FBuildPatchFileConstructor> FileConstructor(new FBuildPatchFileConstructor(
				FFileConstructorConfig({
					ManifestSet.Get(),
					Configuration.InstallDirectory,
					InstallStagingDir,
					MetaStagingDir,
					FilesToConstruct.Array(),
					Configuration.InstallMode}),
				FileSystem.Get(),
				ChainedChunkSource.Get(),
				ChunkReferenceTracker.Get(),
				InstallerError.Get(),
				InstallerAnalytics.Get(),
				FileConstructorStatistics.Get()));
			FDelegateHandle OnBeforeDeleteFileHandle = FileConstructor->OnBeforeDeleteFile().AddLambda([this, &InstallChunkSource](const FString& FilePath)
			{
				FString BuildRelativeFilename = FilePath;
				BuildRelativeFilename.RemoveFromStart(Configuration.InstallDirectory);
				BuildRelativeFilename.RemoveFromStart(TEXT("/"));
				OldFilesRemovedBySystem.Add(BuildRelativeFilename);
				InstallChunkSource->HarvestRemainingChunksFromFile(FilePath);
			});
			TFunction<void(const FGuid&)> LostChunkCallback = [&ChainedChunkSource](const FGuid& LostChunk)
			{
				ChainedChunkSource->AddRepeatRequirement(LostChunk);
			};
#if ENABLE_PATCH_DISK_OVERFLOW_STORE
			DiskOverflowStore->SetLostChunkCallback(LostChunkCallback);
#endif // ENABLE_PATCH_DISK_OVERFLOW_STORE
			CloudChunkStore->SetLostChunkCallback(LostChunkCallback);


			// Register controllables.
			FScopedControllables ScopedControllables(&ThreadLock, Controllables, bIsPaused, bShouldAbort);
			ScopedControllables.Register(InstallChunkSource.Get());
			ScopedControllables.Register(CloudChunkSource.Get());
			ScopedControllables.Register(FileConstructor.Get());

			// Set chunk counter stats.
			if(!bHasCorruptFiles)
			{
				FScopeLock Lock(&ThreadLock);
				BuildStats.NumChunksRequired = ReferencedChunks.Num();
				BuildStats.ChunksQueuedForDownload = InitialDownloadChunks.Num();
				BuildStats.ChunksLocallyAvailable = ReferencedChunks.Intersect(InstallChunkSource->GetAvailableChunks()).Num();
				BuildStats.ChunksInChunkDbs = ReferencedChunks.Intersect(ChunkDbChunkSource->GetAvailableChunks()).Num();
			}

			// Setup some weightings for the progress tracking
			const bool bRepairOnly = ManifestSet->IsRepairOnly() && FilesToConstruct.Num() == 0;
			const float NumRequiredChunksFloat = ReferencedChunks.Num();
			const bool bHasFileAttributes = ManifestSet->HasFileAttributes();
			const float AttributesWeight = bHasFileAttributes ? bRepairOnly ? 1.0f / 50.0f : 1.0f / 20.0f : 0.0f;
			const float VerifyWeight = Configuration.VerifyMode == EVerifyMode::ShaVerifyAllFiles || Configuration.VerifyMode == EVerifyMode::ShaVerifyTouchedFiles ? 1.1f / 9.0f : 0.3f / 9.0f;
			BuildProgress.SetStateWeight(EBuildPatchState::Downloading, 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::MovingToInstall, 0.05f);
			BuildProgress.SetStateWeight(EBuildPatchState::Installing, FilesToConstruct.Num() > 0 ? 1.0f : 0.0f);
			BuildProgress.SetStateWeight(EBuildPatchState::SettingAttributes, AttributesWeight);
			BuildProgress.SetStateWeight(EBuildPatchState::BuildVerification, VerifyWeight);

			// If this is a repair operation, start off with install and download complete
			if (bRepairOnly)
			{
				UE_LOG(LogBuildPatchServices, Log, TEXT("Performing a repair operation"));
				BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
				BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
				BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
			}

			// Initializing is now complete if we are constructing files
			if (FilesToConstruct.Num() > 0)
			{
				BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 1.0f);
				InitializeTimer.Stop();
			}

			// Wait for the file constructor to complete
			ConstructTimer.Start();
			FileConstructor->Run();
			ConstructTimer.Stop();
			FileConstructor->OnBeforeDeleteFile().Remove(OnBeforeDeleteFileHandle);
			UE_LOG(LogBuildPatchServices, Log, TEXT("File construction complete"));

			// Set any final stats before system destruction.
			{
				FScopeLock Lock(&ThreadLock);
				BuildStats.ProcessRequiredDiskSpace = FileConstructor->GetRequiredDiskSpace();
				BuildStats.ProcessAvailableDiskSpace = FileConstructor->GetAvailableDiskSpace();
			}

			// Let the verifier know which files we built.
			Verifier->AddTouchedFiles(FilesToConstruct);
		}

		// Process some final stats.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.NumChunksDownloaded = DownloadServiceStatistics->GetNumSuccessfulChunkDownloads();
			BuildStats.NumFailedDownloads = DownloadServiceStatistics->GetNumFailedChunkDownloads();
			BuildStats.NumBadDownloads = CloudChunkSourceStatistics->GetNumCorruptChunkDownloads();
			BuildStats.NumAbortedDownloads = CloudChunkSourceStatistics->GetNumAbortedChunkDownloads();
			BuildStats.OverallRequestSuccessRate = CloudChunkSourceStatistics->GetDownloadSuccessRate();
			BuildStats.NumChunksRecycled = InstallChunkSourceStatistics->GetNumSuccessfulChunkRecycles();
			BuildStats.NumChunksReadFromChunkDbs = ChunkDbChunkSourceStatistics->GetNumSuccessfulLoads();
			BuildStats.NumRecycleFailures = InstallChunkSourceStatistics->GetNumFailedChunkRecycles();
			BuildStats.NumChunksStoreBooted = MemoryChunkStoreStatistics->GetNumBooted();
			BuildStats.NumDriveStoreChunkLoads = DiskChunkStoreStatistics->GetNumSuccessfulLoads();
			BuildStats.NumDriveStoreLoadFailures = DiskChunkStoreStatistics->GetNumFailedLoads();
			BuildStats.NumChunkDbChunksFailed = ChunkDbChunkSourceStatistics->GetNumFailedLoads();
			TArray<float> HealthTimers = CloudChunkSourceStatistics->GetDownloadHealthTimers();
			BuildStats.ExcellentDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Excellent];
			BuildStats.GoodDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Good];
			BuildStats.OkDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::OK];
			BuildStats.PoorDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Poor];
			BuildStats.DisconnectedDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Disconnected];
			BuildStats.ActiveRequestCountPeak = CloudChunkSourceStatistics->GetPeakRequestCount();
		}

		UE_LOG(LogBuildPatchServices, Log, TEXT("Staged install complete"));
		const bool bReturnInstallationSuccess = !InstallerError->HasError();

		// Ensure all progress complete
		if (bReturnInstallationSuccess)
		{
			BuildProgress.SetStateProgress(EBuildPatchState::Initializing, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Resuming, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Downloading, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchState::Installing, 1.0f);
			InitializeTimer.Stop();
		}

		return bReturnInstallationSuccess;
	}

	bool FBuildPatchInstaller::RunPrerequisites()
	{
		TUniquePtr<IMachineConfig> MachineConfig(FMachineConfigFactory::Create(LocalMachineConfigFile, true));
		TUniquePtr<IPrerequisites> Prerequisites(FPrerequisitesFactory::Create(
			MachineConfig.Get(),
			InstallerAnalytics.Get(),
			InstallerError.Get(),
			FileSystem.Get(),
			Platform.Get()));

		return Prerequisites->RunPrereqs(ManifestSet.Get(), Configuration, InstallStagingDir, BuildProgress);
	}

	void FBuildPatchInstaller::CleanupEmptyDirectories(const FString& RootDirectory)
	{
		TArray<FString> SubDirNames;
		IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
		for(auto DirName : SubDirNames)
		{
			CleanupEmptyDirectories(*(RootDirectory / DirName));
		}

		TArray<FString> SubFileNames;
		IFileManager::Get().FindFilesRecursive(SubFileNames, *RootDirectory, TEXT("*.*"), true, false);
		if (SubFileNames.Num() == 0)
		{
	#if PLATFORM_MAC
			// On Mac we need to delete the .DS_Store file, but FindFiles() skips .DS_Store files.
			IFileManager::Get().Delete(*(RootDirectory / TEXT(".DS_Store")), false, true);
	#endif

			bool bDeleteSuccess = IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
			const uint32 LastError = FPlatformMisc::GetLastError();
			UE_LOG(LogBuildPatchServices, Log, TEXT("Deleted Empty Folder (%u,%u) %s"), bDeleteSuccess ? 1 : 0, LastError, *RootDirectory);
		}
	}

	void FBuildPatchInstaller::FilterToExistingFiles(const FString& RootDirectory, TSet<FString>& Files)
	{
		for (auto FilesIt = Files.CreateIterator(); FilesIt; ++FilesIt)
		{
			if (!FileSystem->FileExists(*(RootDirectory / *FilesIt)))
			{
				FilesIt.RemoveCurrent();
			}
		}
	}

	bool FBuildPatchInstaller::RemoveFileWithRetries(const FString& FullFilename, uint32& ErrorCode)
	{
		int32 DeleteRetries = ConfigHelpers::NumFileMoveRetries();
		bool bDeleteSuccess = false;
		ErrorCode = 0;
		while (DeleteRetries >= 0 && !bDeleteSuccess)
		{
			bDeleteSuccess = FileSystem->DeleteFile(*FullFilename);
			ErrorCode = FPlatformMisc::GetLastError();
			if (!bDeleteSuccess && (--DeleteRetries) >= 0)
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("Failed to delete file %s (%d), retying after 0.5 sec.."), *FullFilename, ErrorCode);
				FPlatformProcess::Sleep(0.5f);
			}
		}
		if (!bDeleteSuccess)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to delete file %s (%d), installer will exit."), *FullFilename, ErrorCode);
		}
		return bDeleteSuccess;
	}

	bool FBuildPatchInstaller::RelocateFileWithRetries(const FString& ToFullFilename, const FString& FromFullFilename, uint32& RenameErrorCode, uint32& CopyErrorCode)
	{
		int32 RelocateRetries = ConfigHelpers::NumFileMoveRetries();
		bool bRelocateSuccess = false;
		RenameErrorCode = 0;
		CopyErrorCode = 0;
		while (RelocateRetries >= 0 && !bRelocateSuccess)
		{
			bRelocateSuccess = FileSystem->MoveFile(*ToFullFilename, *FromFullFilename);
			RenameErrorCode = FPlatformMisc::GetLastError();
			if (!bRelocateSuccess)
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("Failed to move file %s (%d), trying copy.."), *FromFullFilename, RenameErrorCode);
				bRelocateSuccess = FileSystem->CopyFile(*ToFullFilename, *FromFullFilename);
				CopyErrorCode = FPlatformMisc::GetLastError();
				if (bRelocateSuccess)
				{
					FileSystem->DeleteFile(*FromFullFilename);
				}
				else if((--RelocateRetries) >= 0)
				{
					UE_LOG(LogBuildPatchServices, Warning, TEXT("Failed to copy too (%d), retying after 0.5 sec.."), CopyErrorCode);
					FPlatformProcess::Sleep(0.5f);
				}
			}
		}
		if (!bRelocateSuccess)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to relocated file %s (%d-%d), installer will exit."), *FromFullFilename, RenameErrorCode, CopyErrorCode);
		}
		return bRelocateSuccess;
	}

	bool FBuildPatchInstaller::RunBackupAndMove()
	{
		BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 0.0f);
		// We skip this step if performing stage only
		bool bRunBackupAndMoveSuccess = true;
		if (Configuration.InstallMode == EInstallMode::StageFiles || (bFirstInstallIteration && Configuration.InstallMode == EInstallMode::PrereqOnly))
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Skipping backup and stage relocation"));
			BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
		}
		else
		{
			MoveFromStageTimer.Start();
			UE_LOG(LogBuildPatchServices, Log, TEXT("Running backup and stage relocation"));
			// If there's no error, move all complete files
			bRunBackupAndMoveSuccess = InstallerError->HasError() == false;
			if (bRunBackupAndMoveSuccess)
			{
				// Get list of all expected files
				TSet<FString> FilesToRelocate;
				ManifestSet->GetExpectedFiles(FilesToRelocate);
				FilterToExistingFiles(InstallStagingDir, FilesToRelocate);
				FilesToRelocate.Sort(TLess<FString>());

				// First handle files that should be removed for patching
				TSet<FString> FilesToRemove;
				ManifestSet->GetRemovableFiles(FilesToRemove);
				FilterToExistingFiles(Configuration.InstallDirectory, FilesToRemove);
				FilesToRemove.Sort(TLess<FString>());

				// Counters for progress tracking.
				float NumOperationsFloat = FilesToRelocate.Num() + FilesToRemove.Num();
				float PerformedOperationsFloat = 0;

				// Add to build stats
				ThreadLock.Lock();
				BuildStats.NumFilesToRemove = FilesToRemove.Num();
				ThreadLock.Unlock();

				if (NumOperationsFloat > 0.0f)
				{
					UE_LOG(LogBuildPatchServices, Log, TEXT("Create MM"));
					TUniquePtr<FArchive> MoveMarkerFile = FileSystem->CreateFileWriter(*PreviousMoveMarker, EWriteFlags::EvenIfReadOnly);
				}

				// Perform all of the removals
				for (auto FileToRemove = FilesToRemove.CreateConstIterator(); FileToRemove && !InstallerError->HasError(); ++FileToRemove)
				{
					BackupFileIfNecessary(*FileToRemove);
					const FString FullFilename = Configuration.InstallDirectory / *FileToRemove;
					uint32 ErrorCode = 0;
					bool bDeleteSuccess = RemoveFileWithRetries(FullFilename, ErrorCode);
					if (bDeleteSuccess)
					{
						MessagePump->SendMessage(FInstallationFileAction{ FInstallationFileAction::EType::Removed, *FileToRemove });
					}
					else
					{
						InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, MoveErrorCodes::DeleteOldFileFailed, ErrorCode);
						bRunBackupAndMoveSuccess = false;
						break;
					}
					BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, PerformedOperationsFloat / NumOperationsFloat);
					PerformedOperationsFloat += 1.0f;
				}

				// Perform all of the relocations
				for (auto FileToRelocate = FilesToRelocate.CreateConstIterator(); FileToRelocate && !InstallerError->HasError(); ++FileToRelocate)
				{
					const FString SrcFilename = InstallStagingDir / *FileToRelocate;
					const FString DestFilename = Configuration.InstallDirectory / *FileToRelocate;
					const bool bOldFileExists = FileSystem->FileExists(*DestFilename);
					if (bOldFileExists)
					{
						BackupFileIfNecessary(*FileToRelocate);
						uint32 ErrorCode = 0;
						bool bDeleteSuccess = RemoveFileWithRetries(DestFilename, ErrorCode);
						if (!bDeleteSuccess)
						{
							InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, MoveErrorCodes::DeletePrevFileFailed, ErrorCode);
							bRunBackupAndMoveSuccess = false;
							break;
						}
					}
					uint32 RenameErrorCode = 0;
					uint32 CopyErrorCode = 0;
					bool bRelocateSuccess = RelocateFileWithRetries(DestFilename, SrcFilename, RenameErrorCode, CopyErrorCode);
					if (bRelocateSuccess)
					{
						FilesInstalled.Add(*FileToRelocate);
						FileOperationTracker->OnFileStateUpdate(*FileToRelocate, EFileOperationState::Installed);
						FInstallationFileAction::EType Action = OldFilesRemovedBySystem.Contains(*FileToRelocate) || bOldFileExists ? FInstallationFileAction::EType::Updated : FInstallationFileAction::EType::Added;
						MessagePump->SendMessage(FInstallationFileAction{ Action, *FileToRelocate });
					}
					else
					{
						FString ErrorString = FString::Printf(TEXT("%s-%u-%u"), MoveErrorCodes::StageToInstall, RenameErrorCode, CopyErrorCode);
						InstallerError->SetError(EBuildPatchInstallError::MoveFileToInstall, (RenameErrorCode != 0 || CopyErrorCode != 0) ? *ErrorString : MoveErrorCodes::StageToInstall);
						bRunBackupAndMoveSuccess = false;
						break;
					}
					BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, PerformedOperationsFloat / NumOperationsFloat);
					PerformedOperationsFloat += 1.0f;
				}

				bRunBackupAndMoveSuccess = bRunBackupAndMoveSuccess && (InstallerError->HasError() == false);
				if (bRunBackupAndMoveSuccess)
				{
					BuildProgress.SetStateProgress(EBuildPatchState::MovingToInstall, 1.0f);
				}
			}
			UE_LOG(LogBuildPatchServices, Log, TEXT("Relocation complete %d"), bRunBackupAndMoveSuccess ? 1 : 0);
			MoveFromStageTimer.Stop();
		}
		return bRunBackupAndMoveSuccess;
	}

	bool FBuildPatchInstaller::RunFileAttributes()
	{
		// Only provide stage directory if stage-only mode
		FString EmptyString;
		FString& OptionalStageDirectory = Configuration.InstallMode == EInstallMode::StageFiles ? InstallStagingDir : EmptyString;

		// Construct the attributes class
		FileAttributesTimer.Start();
		TUniquePtr<IFileAttribution> Attributes(FFileAttributionFactory::Create(FileSystem.Get(), ManifestSet.Get(), FilesToConstruct, Configuration.InstallDirectory, OptionalStageDirectory, &BuildProgress));
		FScopedControllables ScopedControllables(&ThreadLock, Controllables, bIsPaused, bShouldAbort);
		ScopedControllables.Register(Attributes.Get());
		Attributes->ApplyAttributes();
		FileAttributesTimer.Stop();

		// We don't fail on this step currently
		return true;
	}

	bool FBuildPatchInstaller::RunVerification(TArray< FString >& CorruptFiles)
	{
		// Make sure this function can never be parallelized
		static FCriticalSection SingletonFunctionLockCS;
		const bool bShouldLock = !Configuration.bAllowConcurrentExecution;
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Lock();
		}

		VerifyTimer.Start();
		BuildProgress.SetStateProgress(EBuildPatchState::BuildVerification, 0.0f);

		// Verify the installation
		UE_LOG(LogBuildPatchServices, Log, TEXT("Verifying install"));
		CorruptFiles.Empty();

		// Verify the build
		EVerifyResult VerifyResult = Verifier->Verify(CorruptFiles);
		const bool bVerifySuccessful = VerifyResult == EVerifyResult::Success;
		if (!bVerifySuccessful)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Build verification failed on %u file(s)"), CorruptFiles.Num());
			InstallerError->SetError(EBuildPatchInstallError::BuildVerifyFail, InstallerHelpers::GetVerifyErrorCode(VerifyResult));
		}
		TMap<EVerifyError, int32> VerifyErrorCounts = VerifierStatistics->GetVerifyErrorCounts();
		for (const TPair<EVerifyError, int32>& VerifyErrorCount : VerifyErrorCounts)
		{
			const int32 CachedCount = CachedVerifyErrorCounts.FindRef(VerifyErrorCount.Key);
			if (CachedCount < VerifyErrorCount.Value)
			{
				InstallerHelpers::LogAdditionalVerifyErrors(VerifyErrorCount.Key, VerifyErrorCount.Value - CachedCount);
			}
		}
		CachedVerifyErrorCounts = MoveTemp(VerifyErrorCounts);

		BuildProgress.SetStateProgress(EBuildPatchState::BuildVerification, 1.0f);

		// Delete/Backup any incorrect files if failure was not cancellation
		if (!InstallerError->IsCancelled())
		{
			for (const FString& CorruptFile : CorruptFiles)
			{
				BackupFileIfNecessary(CorruptFile, true);
				FString StagedFile = InstallStagingDir / CorruptFile;
				if (FileSystem->FileExists(*StagedFile))
				{
					FileSystem->DeleteFile(*StagedFile);
				}
				if (Configuration.InstallMode != EInstallMode::StageFiles)
				{
					FString InstalledFile = Configuration.InstallDirectory / CorruptFile;
					if (FileSystem->FileExists(*InstalledFile))
					{
						FileSystem->DeleteFile(*InstalledFile);
						OldFilesRemovedBySystem.Add(CorruptFile);
					}
				}
			}
		}

		UE_LOG(LogBuildPatchServices, Log, TEXT("Verify stage complete %d"), bVerifySuccessful ? 1 : 0);

		VerifyTimer.Stop();
		if (bShouldLock)
		{
			SingletonFunctionLockCS.Unlock();
		}
		return bVerifySuccessful;
	}

	bool FBuildPatchInstaller::BackupFileIfNecessary(const FString& Filename, bool bDiscoveredByVerification /*= false */)
	{
		const FString InstalledFilename = Configuration.InstallDirectory / Filename;
		const FString BackupFilename = Configuration.BackupDirectory / Filename;
		const bool bBackupOriginals = !Configuration.BackupDirectory.IsEmpty();
		// Skip if not doing backups
		if (!bBackupOriginals)
		{
			return true;
		}
		// Skip if no file to backup
		const bool bInstalledFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*InstalledFilename);
		if (!bInstalledFileExists)
		{
			return true;
		}
		// Skip if already backed up
		const bool bAlreadyBackedUp = FPlatformFileManager::Get().GetPlatformFile().FileExists(*BackupFilename);
		if (bAlreadyBackedUp)
		{
			return true;
		}
		// Skip if the target file was already copied to the installation
		const bool bAlreadyInstalled = FilesInstalled.Contains(Filename);
		if (bAlreadyInstalled)
		{
			return true;
		}
		// If discovered by verification, but the patching system did not touch the file, we know it must be backed up.
		// If patching system touched the file it would already have been backed up
		if (bDiscoveredByVerification && !OutdatedFiles.Contains(Filename))
		{
			return IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
		}
		bool bUserEditedFile = bDiscoveredByVerification;
		const bool bCheckFileChanges = !bDiscoveredByVerification;
		if (bCheckFileChanges)
		{
			const FFileManifest* OldFileManifest = ManifestSet->GetCurrentFileManifest(Filename);
			const FFileManifest* NewFileManifest = ManifestSet->GetNewFileManifest(Filename);
			const int64 InstalledFilesize = IFileManager::Get().FileSize(*InstalledFilename);
			const int64 OriginalFileSize = OldFileManifest ? OldFileManifest->FileSize : INDEX_NONE;
			const int64 NewFileSize = NewFileManifest ? NewFileManifest->FileSize : INDEX_NONE;
			const FSHAHash HashZero;
			const FSHAHash& HashOld = OldFileManifest ? OldFileManifest->FileHash : HashZero;
			const FSHAHash& HashNew = NewFileManifest ? NewFileManifest->FileHash : HashZero;
			const bool bFileSizeDiffers = OriginalFileSize != InstalledFilesize && NewFileSize != InstalledFilesize;
			bUserEditedFile = bFileSizeDiffers || FBuildPatchUtils::VerifyFile(FileSystem.Get(), InstalledFilename, HashOld, HashNew) == 0;
		}
		// Finally, use the above logic to determine if we must do the backup
		const bool bNeedBackup = bUserEditedFile;
		bool bBackupSuccess = true;
		if (bNeedBackup)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Backing up %s"), *Filename);
			bBackupSuccess = IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
		}
		return bBackupSuccess;
	}

	void FBuildPatchInstaller::CleanupThread()
	{
		Configuration.SharedContext->ReleaseThread(Thread);
		Thread = nullptr;
	}

	double FBuildPatchInstaller::GetDownloadSpeed() const
	{
		return DownloadSpeedRecorder->GetAverageSpeed(ConfigHelpers::DownloadSpeedAverageTime());
	}

	int64 FBuildPatchInstaller::GetTotalDownloadRequired() const
	{
		return CloudChunkSourceStatistics->GetRequiredDownloadSize() + PreviousTotalDownloadRequired.GetValue();
	}

	int64 FBuildPatchInstaller::GetTotalDownloaded() const
	{
		return DownloadServiceStatistics->GetBytesDownloaded();
	}

	bool FBuildPatchInstaller::IsComplete() const
	{
		return !bIsRunning && bIsInited;
	}

	bool FBuildPatchInstaller::IsCanceled() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.FailureType == EBuildPatchInstallError::UserCanceled;
	}

	bool FBuildPatchInstaller::IsPaused() const
	{
		FScopeLock Lock(&ThreadLock);
		return bIsPaused;
	}

	bool FBuildPatchInstaller::IsResumable() const
	{
		FScopeLock Lock(&ThreadLock);
		if (BuildStats.FailureType == EBuildPatchInstallError::PathLengthExceeded)
		{
			return false;
		}
		return !BuildStats.ProcessSuccess;
	}

	bool FBuildPatchInstaller::IsUpdate() const
	{
		for (const FBuildPatchInstallerAction& InstallerAction : InstallerActions)
		{
			if (InstallerAction.IsUpdate())
			{
				return true;
			}
		}
		return false;
	}

	bool FBuildPatchInstaller::CompletedSuccessfully() const
	{
		return IsComplete() && bSuccess;
	}

	bool FBuildPatchInstaller::HasError() const
	{
		FScopeLock Lock(&ThreadLock);
		if (BuildStats.FailureType == EBuildPatchInstallError::UserCanceled)
		{
			return false;
		}
		return !BuildStats.ProcessSuccess;
	}

	EBuildPatchInstallError FBuildPatchInstaller::GetErrorType() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.FailureType;
	}

	FString FBuildPatchInstaller::GetErrorCode() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats.ErrorCode;
	}

	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	FText FBuildPatchInstaller::GetPercentageText() const
	{
		static const FText PleaseWait = NSLOCTEXT("BuildPatchInstaller", "BuildPatchInstaller_GenericProgress", "Please Wait");

		FScopeLock Lock(&ThreadLock);

		float Progress = GetUpdateProgress() * 100.0f;
		if (Progress <= 0.0f)
		{
			return PleaseWait;
		}

		FNumberFormattingOptions PercentFormattingOptions;
		PercentFormattingOptions.MaximumFractionalDigits = 0;
		PercentFormattingOptions.MinimumFractionalDigits = 0;

		return FText::AsPercent(GetUpdateProgress(), &PercentFormattingOptions);
	}

	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	FText FBuildPatchInstaller::GetDownloadSpeedText() const
	{
		static const FText DownloadSpeedFormat = NSLOCTEXT("BuildPatchInstaller", "BuildPatchInstaller_DownloadSpeedFormat", "{Current} / {Total} ({Speed}/sec)");

		FScopeLock Lock(&ThreadLock);
		FText SpeedDisplayedText;
		double DownloadSpeed = GetDownloadSpeed();
		double InitialDownloadSize = GetTotalDownloadRequired();
		double TotalDownloaded = GetTotalDownloaded();
		if (DownloadSpeed >= 0)
		{
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 1;
			FormattingOptions.MinimumFractionalDigits = 1;

			FFormatNamedArguments Args;
			Args.Add(TEXT("Speed"), FText::AsMemory(DownloadSpeed, &FormattingOptions));
			Args.Add(TEXT("Total"), FText::AsMemory(InitialDownloadSize, &FormattingOptions));
			Args.Add(TEXT("Current"), FText::AsMemory(TotalDownloaded, &FormattingOptions));

			return FText::Format(DownloadSpeedFormat, Args);
		}

		return FText();
	}

	EBuildPatchState FBuildPatchInstaller::GetState() const
	{
		return BuildProgress.GetState();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FText FBuildPatchInstaller::GetStatusText() const
	{
		return BuildPatchServices::StateToText(GetState());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	float FBuildPatchInstaller::GetUpdateProgress() const
	{
		return BuildProgress.GetProgress();
	}

	FBuildInstallStats FBuildPatchInstaller::GetBuildStatistics() const
	{
		FScopeLock Lock(&ThreadLock);
		return BuildStats;
	}

	EBuildPatchDownloadHealth FBuildPatchInstaller::GetDownloadHealth() const
	{
		return CloudChunkSourceStatistics->GetDownloadHealth();
	}

	FText FBuildPatchInstaller::GetErrorText() const
	{
		return InstallerError->GetErrorText();
	}

	void FBuildPatchInstaller::CancelInstall()
	{
		InstallerError->SetError(EBuildPatchInstallError::UserCanceled, UserCancelErrorCodes::UserRequested);

		// Make sure we are not paused
		if (IsPaused())
		{
			TogglePauseInstall();
		}

		// Abort all controllable classes
		ThreadLock.Lock();
		bShouldAbort = true;
		for (IControllable* Controllable : Controllables)
		{
			Controllable->Abort();
		}
		ThreadLock.Unlock();
	}

	bool FBuildPatchInstaller::TogglePauseInstall()
	{
		FScopeLock Lock(&ThreadLock);
		// If there is an error, we don't allow pausing.
		const bool bShouldBePaused = !bIsPaused && !InstallerError->HasError();
		if (bIsPaused)
		{
			// Stop pause timer.
			ProcessPausedTimer.Stop();
		}
		else if (bShouldBePaused)
		{
			// Start pause timer.
			ProcessPausedTimer.Start();
		}
		bIsPaused = bShouldBePaused;
		// Set pause state on all controllable classes
		for (IControllable* Controllable : Controllables)
		{
			Controllable->SetPaused(bShouldBePaused);
		}
		// Set pause state on pausable process timers.
		ConstructTimer.SetPause(bIsPaused);
		MoveFromStageTimer.SetPause(bIsPaused);
		FileAttributesTimer.SetPause(bIsPaused);
		VerifyTimer.SetPause(bIsPaused);
		CleanUpTimer.SetPause(bIsPaused);
		ProcessActiveTimer.SetPause(bIsPaused);
		return bShouldBePaused;
	}

	void FBuildPatchInstaller::RegisterMessageHandler(FMessageHandler* MessageHandler)
	{
		check(IsInGameThread());
		check(MessageHandler != nullptr);
		MessagePump->RegisterMessageHandler(MessageHandler);
	}

	void FBuildPatchInstaller::UnregisterMessageHandler(FMessageHandler* MessageHandler)
	{
		check(IsInGameThread());
		MessagePump->UnregisterMessageHandler(MessageHandler);
	}

	void FBuildPatchInstaller::ExecuteCompleteDelegate()
	{
		// Should be executed in main thread, and already be complete.
		check(IsInGameThread());
		check(IsComplete());
		// Finish applying build statistics.
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.FinalDownloadSpeed = GetDownloadSpeed();
			BuildStats.AverageDownloadSpeed = DownloadSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDownloadSpeed = DownloadSpeedRecorder->GetPeakSpeed();
			BuildStats.AverageDiskReadSpeed = DiskReadSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDiskReadSpeed = DiskReadSpeedRecorder->GetPeakSpeed();
			BuildStats.AverageDiskWriteSpeed = DiskWriteSpeedRecorder->GetAverageSpeed(TNumericLimits<float>::Max());
			BuildStats.PeakDiskWriteSpeed = DiskWriteSpeedRecorder->GetPeakSpeed();
			BuildStats.TotalDownloadedData = DownloadServiceStatistics->GetBytesDownloaded();
			BuildStats.TotalReadData = InstallChunkSourceStatistics->GetBytesRead();
			BuildStats.TotalReadData += VerifierStatistics->GetBytesVerified();
			BuildStats.TotalWrittenData = FileConstructorStatistics->GetBytesConstructed();
			BuildStats.NumFilesConstructed = FileConstructorStatistics->GetFilesConstructed();
			BuildStats.TheoreticalDownloadTime = BuildStats.AverageDownloadSpeed > 0 ? BuildStats.TotalDownloadedData / BuildStats.AverageDownloadSpeed : 0;
			BuildStats.AverageMemoryStoreUse = MemoryChunkStoreStatistics->GetAverageStoreUse();
			BuildStats.PeakMemoryStoreUse = MemoryChunkStoreStatistics->GetPeakStoreUse();
			BuildStats.AverageMemoryStoreRetained = MemoryChunkStoreStatistics->GetAverageStoreRetained();
			BuildStats.PeakMemoryStoreRetained = MemoryChunkStoreStatistics->GetPeakStoreRetained();
			BuildStats.MemoryStoreSize = MemoryChunkStoreStatistics->GetStoreSize();
			InstallerHelpers::LogBuildStatInfo(BuildStats, SessionId);
		}
		// Call the complete delegate.
		CompleteDelegate.ExecuteIfBound(AsShared());
	}

	void FBuildPatchInstaller::PumpMessages()
	{
		check(IsInGameThread());
		MessagePump->PumpMessages();
	}
}
