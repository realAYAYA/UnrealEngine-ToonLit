// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchSettings.h"

#include "Misc/App.h"
#include "HAL/PlatformProcess.h"

namespace BuildPatchServices
{
	FBuildPatchServicesInitSettings::FBuildPatchServicesInitSettings()
		: ApplicationSettingsDir(FPlatformProcess::ApplicationSettingsDir())
		, ProjectName(FApp::GetProjectName())
		, LocalMachineConfigFileName(TEXT("BuildPatchServicesLocal.ini"))
	{
	}

	enum class EInstallActionIntent : int32
	{
		Install,
		Update,
		Repair,
		Uninstall,

		Invalid
	};

	BuildPatchServices::FInstallerAction FInstallerAction::MakeInstall(const IBuildManifestRef& Manifest, TSet<FString> InstallTags /*= TSet<FString>()*/)
	{
		FInstallerAction InstallerAction;
		InstallerAction.InstallManifest = Manifest;
		InstallerAction.InstallTags = MoveTemp(InstallTags);
		InstallerAction.ActionIntent = EInstallActionIntent::Install;
		return InstallerAction;
	}

	BuildPatchServices::FInstallerAction FInstallerAction::MakeUpdate(const IBuildManifestRef& CurrentManifest, const IBuildManifestRef& InstallManifest, TSet<FString> InstallTags /*= TSet<FString>()*/)
	{
		FInstallerAction InstallerAction;
		InstallerAction.CurrentManifest = CurrentManifest;
		InstallerAction.InstallManifest = InstallManifest;
		InstallerAction.InstallTags = MoveTemp(InstallTags);
		InstallerAction.ActionIntent = EInstallActionIntent::Update;
		return InstallerAction;
	}

	BuildPatchServices::FInstallerAction FInstallerAction::MakeRepair(const IBuildManifestRef& Manifest, TSet<FString> InstallTags /*= TSet<FString>()*/)
	{
		FInstallerAction InstallerAction;
		InstallerAction.CurrentManifest = Manifest;
		InstallerAction.InstallManifest = Manifest;
		InstallerAction.InstallTags = MoveTemp(InstallTags);
		InstallerAction.ActionIntent = EInstallActionIntent::Repair;
		return InstallerAction;
	}

	BuildPatchServices::FInstallerAction FInstallerAction::MakeUninstall(const IBuildManifestRef& Manifest)
	{
		FInstallerAction InstallerAction;
		InstallerAction.CurrentManifest = Manifest;
		InstallerAction.ActionIntent = EInstallActionIntent::Uninstall;
		return InstallerAction;
	}

	FInstallerAction::FInstallerAction(const FInstallerAction& CopyFrom)
		: CurrentManifest(CopyFrom.CurrentManifest)
		, InstallManifest(CopyFrom.InstallManifest)
		, InstallTags(CopyFrom.InstallTags)
		, ActionIntent(CopyFrom.ActionIntent)
	{
	}

	FInstallerAction::FInstallerAction(FInstallerAction&& MoveFrom)
		: CurrentManifest(MoveTemp(MoveFrom.CurrentManifest))
		, InstallManifest(MoveTemp(MoveFrom.InstallManifest))
		, InstallTags(MoveTemp(MoveFrom.InstallTags))
		, ActionIntent(MoveFrom.ActionIntent)
	{
	}

	FInstallerAction::FInstallerAction()
		: ActionIntent(EInstallActionIntent::Invalid)
	{
	}

	bool FInstallerAction::IsInstall() const
	{
		return ActionIntent == EInstallActionIntent::Install;
	}

	bool FInstallerAction::IsUpdate() const
	{
		return ActionIntent == EInstallActionIntent::Update;
	}

	bool FInstallerAction::IsRepair() const
	{
		return ActionIntent == EInstallActionIntent::Repair;
	}

	bool FInstallerAction::IsUninstall() const
	{
		return ActionIntent == EInstallActionIntent::Uninstall;
	}

	const TSet<FString>& FInstallerAction::GetInstallTags() const
	{
		return InstallTags;
	}

	IBuildManifestRef FInstallerAction::GetInstallManifest() const
	{
		return InstallManifest.ToSharedRef();
	}

	IBuildManifestRef FInstallerAction::GetCurrentManifest() const
	{
		return CurrentManifest.ToSharedRef();
	}

	FBuildInstallerConfiguration::FBuildInstallerConfiguration(TArray<FInstallerAction> InInstallerActions)
		: InstallerActions(MoveTemp(InInstallerActions))
		, InstallDirectory()
		, StagingDirectory()
		, BackupDirectory()
		, ChunkDatabaseFiles()
		, CloudDirectories()
		, InstallMode(EInstallMode::NonDestructiveInstall)
		, VerifyMode(EVerifyMode::ShaVerifyAllFiles)
		, DeltaPolicy(EDeltaPolicy::Skip)
		, bRunRequiredPrereqs(true)
		, bAllowConcurrentExecution(false)
	{
	}

	FBuildInstallerConfiguration::FBuildInstallerConfiguration(FBuildInstallerConfiguration&& MoveFrom)
		: InstallerActions(MoveTemp(MoveFrom.InstallerActions))
		, InstallDirectory(MoveTemp(MoveFrom.InstallDirectory))
		, StagingDirectory(MoveTemp(MoveFrom.StagingDirectory))
		, BackupDirectory(MoveTemp(MoveFrom.BackupDirectory))
		, ChunkDatabaseFiles(MoveTemp(MoveFrom.ChunkDatabaseFiles))
		, CloudDirectories(MoveTemp(MoveFrom.CloudDirectories))
		, InstallMode(MoveFrom.InstallMode)
		, VerifyMode(MoveFrom.VerifyMode)
		, DeltaPolicy(MoveFrom.DeltaPolicy)
		, bRunRequiredPrereqs(MoveFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(MoveFrom.bAllowConcurrentExecution)
	{
	}

	FBuildInstallerConfiguration::FBuildInstallerConfiguration(const FBuildInstallerConfiguration& CopyFrom)
		: InstallerActions(CopyFrom.InstallerActions)
		, InstallDirectory(CopyFrom.InstallDirectory)
		, StagingDirectory(CopyFrom.StagingDirectory)
		, BackupDirectory(CopyFrom.BackupDirectory)
		, ChunkDatabaseFiles(CopyFrom.ChunkDatabaseFiles)
		, CloudDirectories(CopyFrom.CloudDirectories)
		, InstallMode(CopyFrom.InstallMode)
		, VerifyMode(CopyFrom.VerifyMode)
		, DeltaPolicy(CopyFrom.DeltaPolicy)
		, bRunRequiredPrereqs(CopyFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(CopyFrom.bAllowConcurrentExecution)
	{
	}

	FChunkBuildConfiguration::FChunkBuildConfiguration()
		: FeatureLevel(EFeatureLevel::Latest)
		, AppId(0)
		, DataAgeThreshold(TNumericLimits<float>::Max())
		, bShouldHonorReuseThreshold(false)
		, OutputChunkWindowSize(1024 * 1024)
		, bShouldMatchAnyWindowSize(true)
		, bAllowEmptyBuild(false)
	{
	}

	FChunkDeltaOptimiserConfiguration::FChunkDeltaOptimiserConfiguration()
		: ScanWindowSize(8191)
		, OutputChunkSize(1024 * 1024)
		, DiffAbortThreshold(TNumericLimits<int64>::Max())
	{
	}

	FPatchDataEnumerationConfiguration::FPatchDataEnumerationConfiguration()
		: bIncludeSizes(false)
	{
	}

	FDiffManifestsConfiguration::FDiffManifestsConfiguration()
	{
	}

	FCompactifyConfiguration::FCompactifyConfiguration()
		: DataAgeThreshold(7.0f)
		, bRunPreview(true)
	{
	}

	FPackageChunksConfiguration::FPackageChunksConfiguration()
		: FeatureLevel(EFeatureLevel::Latest)
		, MaxOutputFileSize(TNumericLimits<uint64>::Max())
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(const IBuildManifestRef& InInstallManifest)
		: CurrentManifest(nullptr)
		, InstallManifest(InInstallManifest)
		, InstallDirectory()
		, StagingDirectory()
		, BackupDirectory()
		, ChunkDatabaseFiles()
		, CloudDirectories()
		, InstallTags()
		, InstallMode(EInstallMode::NonDestructiveInstall)
		, VerifyMode(EVerifyMode::ShaVerifyAllFiles)
		, DeltaPolicy(EDeltaPolicy::Skip)
		, bIsRepair(false)
		, bRunRequiredPrereqs(true)
		, bAllowConcurrentExecution(false)
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(FInstallerConfiguration&& MoveFrom)
		: CurrentManifest(MoveTemp(MoveFrom.CurrentManifest))
		, InstallManifest(MoveTemp(MoveFrom.InstallManifest))
		, InstallDirectory(MoveTemp(MoveFrom.InstallDirectory))
		, StagingDirectory(MoveTemp(MoveFrom.StagingDirectory))
		, BackupDirectory(MoveTemp(MoveFrom.BackupDirectory))
		, ChunkDatabaseFiles(MoveTemp(MoveFrom.ChunkDatabaseFiles))
		, CloudDirectories(MoveTemp(MoveFrom.CloudDirectories))
		, InstallTags(MoveTemp(MoveFrom.InstallTags))
		, InstallMode(MoveFrom.InstallMode)
		, VerifyMode(MoveFrom.VerifyMode)
		, DeltaPolicy(MoveFrom.DeltaPolicy)
		, bIsRepair(MoveFrom.bIsRepair)
		, bRunRequiredPrereqs(MoveFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(MoveFrom.bAllowConcurrentExecution)
	{
	}

	FInstallerConfiguration::FInstallerConfiguration(const FInstallerConfiguration& CopyFrom)
		: CurrentManifest(CopyFrom.CurrentManifest)
		, InstallManifest(CopyFrom.InstallManifest)
		, InstallDirectory(CopyFrom.InstallDirectory)
		, StagingDirectory(CopyFrom.StagingDirectory)
		, BackupDirectory(CopyFrom.BackupDirectory)
		, ChunkDatabaseFiles(CopyFrom.ChunkDatabaseFiles)
		, CloudDirectories(CopyFrom.CloudDirectories)
		, InstallTags(CopyFrom.InstallTags)
		, InstallMode(CopyFrom.InstallMode)
		, VerifyMode(CopyFrom.VerifyMode)
		, DeltaPolicy(CopyFrom.DeltaPolicy)
		, bIsRepair(CopyFrom.bIsRepair)
		, bRunRequiredPrereqs(CopyFrom.bRunRequiredPrereqs)
		, bAllowConcurrentExecution(CopyFrom.bAllowConcurrentExecution)
	{
	}
}

