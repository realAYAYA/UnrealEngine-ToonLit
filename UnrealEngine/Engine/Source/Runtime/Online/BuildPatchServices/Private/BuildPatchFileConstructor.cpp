// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchFileConstructor.h"
#include "IBuildManifestSet.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceRedirector.h"
#include "BuildPatchServicesPrivate.h"
#include "Interfaces/IBuildInstaller.h"
#include "Data/ChunkData.h"
#include "Common/StatsCollector.h"
#include "Common/SpeedRecorder.h"
#include "Common/FileSystem.h"
#include "Installer/ChunkSource.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerAnalytics.h"
#include "BuildPatchUtil.h"

using namespace BuildPatchServices;

// This define the number of bytes on a half-finished file that we ignore from the end
// incase of previous partial write.
#define NUM_BYTES_RESUME_IGNORE     1024

static int32 SleepTimeWhenFileSystemThrottledSeconds = 15;
static FAutoConsoleVariableRef CVarSleepTimeWhenFileSystemThrottledSeconds(
	TEXT("BuildPatchFileConstructor.SleepTimeWhenFileSystemThrottledSeconds"),
	SleepTimeWhenFileSystemThrottledSeconds,
	TEXT("The amount of time to sleep if the destination filesystem is throttled."),
	ECVF_Default);

static bool bStallWhenFileSystemThrottled = false;
static FAutoConsoleVariableRef CVarStallWhenFileSystemThrottled(
	TEXT("BuildPatchFileConstructor.bStallWhenFileSystemThrottled"),
	bStallWhenFileSystemThrottled,
	TEXT("Whether to stall if the file system is throttled"),
	ECVF_Default);


// Helper functions wrapping common code.
namespace FileConstructorHelpers
{
	void WaitWhilePaused(FThreadSafeBool& bIsPaused, FThreadSafeBool& bShouldAbort)
	{
		// Wait while paused
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.5f);
		}
	}

	bool CheckRemainingDiskSpace(const FString& InstallDirectory, uint64 RemainingBytesRequired, uint64& OutAvailableDiskSpace)
	{
		bool bContinueConstruction = true;
		uint64 TotalSize = 0;
		OutAvailableDiskSpace = 0;
		if (FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalSize, OutAvailableDiskSpace))
		{
			if (OutAvailableDiskSpace < RemainingBytesRequired)
			{
				bContinueConstruction = false;
			}
		}
		return bContinueConstruction;
	}

	uint64 CalculateRequiredDiskSpace(const FBuildPatchAppManifestPtr& CurrentManifest, const FBuildPatchAppManifestRef& BuildManifest, const EInstallMode& InstallMode, const TSet<FString>& InInstallTags)
	{
		// Make tags expected
		TSet<FString> InstallTags = InInstallTags;
		if (InstallTags.Num() == 0)
		{
			BuildManifest->GetFileTagList(InstallTags);
		}
		InstallTags.Add(TEXT(""));
		// Calculate the files that need constructing.
		TSet<FString> TaggedFiles;
		BuildManifest->GetTaggedFileList(InstallTags, TaggedFiles);
		FString DummyString;
		TSet<FString> FilesToConstruct;
		BuildManifest->GetOutdatedFiles(CurrentManifest.Get(), DummyString, TaggedFiles, FilesToConstruct);
		// Count disk space needed by each operation.
		int64 DiskSpaceDeltaPeak = 0;
		if (InstallMode == EInstallMode::DestructiveInstall && CurrentManifest.IsValid())
		{
			// The simplest method will be to run through each high level file operation, tracking peak disk usage delta.
			int64 DiskSpaceDelta = 0;

			// Loop through all files to be made next, in order.
			FilesToConstruct.Sort(TLess<FString>());
			for (const FString& FileToConstruct : FilesToConstruct)
			{
				// First we would need to make the new file.
				DiskSpaceDelta += BuildManifest->GetFileSize(FileToConstruct);
				if (DiskSpaceDeltaPeak < DiskSpaceDelta)
				{
					DiskSpaceDeltaPeak = DiskSpaceDelta;
				}
				// Then we can remove the current existing file.
				DiskSpaceDelta -= CurrentManifest->GetFileSize(FileToConstruct);
			}
		}
		else
		{
			// When not destructive, or no CurrentManifest, we always stage all new and changed files.
			DiskSpaceDeltaPeak = BuildManifest->GetFileSize(FilesToConstruct);
		}
		return FMath::Max<int64>(DiskSpaceDeltaPeak, 0);
	}
}

enum class EConstructionError : uint8
{
	None = 0,
	CannotCreateFile,
	OutOfDiskSpace,
	MissingChunk,
	SerializeError,
	TrackingError,
	OutboundDataError
};

/**
 * This struct handles loading and saving of simple resume information, that will allow us to decide which
 * files should be resumed from. It will also check that we are creating the same version and app as we expect to be.
 */
struct FResumeData
{
public:
	// File system dependency
	const IFileSystem* const FileSystem;

	// The manifests for the app we are installing
	const IBuildManifestSet* const ManifestSet;

	// Save the staging directory
	const FString StagingDir;

	// The filename to the resume data information
	const FString ResumeDataFilename;

	// The resume ids that we loaded from disk
	TSet<FString> LoadedResumeIds;

	// The set of files that were started
	TSet<FString> FilesStarted;

	// The set of files that were completed, determined by expected file size
	TSet<FString> FilesCompleted;

	// The set of files that exist but are not able to assume resumable
	TSet<FString> FilesIncompatible;

	// Whether we have any resume data for this install
	bool bHasResumeData;

public:

	FResumeData(IFileSystem* InFileSystem, IBuildManifestSet* InManifestSet, const FString& InStagingDir, const FString& InResumeDataFilename)
		: FileSystem(InFileSystem)
		, ManifestSet(InManifestSet)
		, StagingDir(InStagingDir)
		, ResumeDataFilename(InResumeDataFilename)
		, bHasResumeData(false)
	{
		// Load data from previous resume file
		bHasResumeData = FileSystem->FileExists(*ResumeDataFilename);
		GLog->Logf(TEXT("BuildPatchResumeData file found: %s"), bHasResumeData ? TEXT("true") : TEXT("false"));
		if (bHasResumeData)
		{
			// Grab existing resume metadata.
			const bool bCullEmptyLines = true;
			FString PrevResumeData;
			TArray<FString> PrevResumeDataLines;
			FileSystem->LoadFileToString(*ResumeDataFilename, PrevResumeData);
			PrevResumeData.ParseIntoArrayLines(PrevResumeDataLines, bCullEmptyLines);
			// Grab current resume ids
			const bool bCheckLegacyIds = true;
			TSet<FString> NewResumeIds;
			ManifestSet->GetInstallResumeIds(NewResumeIds, bCheckLegacyIds);
			LoadedResumeIds.Reserve(PrevResumeDataLines.Num());
			// Check if any builds we are installing are a resume from previous run.
			for (FString& PrevResumeDataLine : PrevResumeDataLines)
			{
				PrevResumeDataLine.TrimStartAndEndInline();
				LoadedResumeIds.Add(PrevResumeDataLine);
				if (NewResumeIds.Contains(PrevResumeDataLine))
				{
					bHasResumeData = true;
					GLog->Logf(TEXT("BuildPatchResumeData version matched %s"), *PrevResumeDataLine);
				}
			}
		}
	}

	/**
	 * Saves out the resume data
	 */
	void SaveOut(const TSet<FString>& ResumeIds)
	{
		// Save out the patch versions
		FileSystem->SaveStringToFile(*ResumeDataFilename, FString::Join(ResumeIds, TEXT("\n")));
	}

	/**
	 * Checks whether the file was completed during last install attempt and adds it to FilesCompleted if so
	 * @param Filename    The filename to check
	 */
	void CheckFile(const FString& Filename)
	{
		// If we had resume data, check if this file might have been resumable
		if (bHasResumeData)
		{
			int64 DiskFileSize;
			const FString FullFilename = StagingDir / Filename;
			const bool bFileExists = FileSystem->GetFileSize(*FullFilename, DiskFileSize);
			const bool bCheckLegacyIds = true;
			TSet<FString> FileResumeIds;
			ManifestSet->GetInstallResumeIdsForFile(Filename, FileResumeIds, bCheckLegacyIds);
			if (LoadedResumeIds.Intersect(FileResumeIds).Num() > 0)
			{
				const FFileManifest* NewFileManifest = ManifestSet->GetNewFileManifest(Filename);
				if (NewFileManifest && bFileExists)
				{
					const uint64 UnsignedDiskFileSize = DiskFileSize;
					if (UnsignedDiskFileSize > 0 && UnsignedDiskFileSize <= NewFileManifest->FileSize)
					{
						FilesStarted.Add(Filename);
					}
					if (UnsignedDiskFileSize == NewFileManifest->FileSize)
					{
						FilesCompleted.Add(Filename);
					}
					if (UnsignedDiskFileSize > NewFileManifest->FileSize)
					{
						FilesIncompatible.Add(Filename);
					}
				}
			}
			else if (bFileExists)
			{
				FilesIncompatible.Add(Filename);
			}
		}
	}
};

/* FBuildPatchFileConstructor implementation
 *****************************************************************************/
FBuildPatchFileConstructor::FBuildPatchFileConstructor(FFileConstructorConfig InConfiguration, IFileSystem* InFileSystem, IChunkSource* InChunkSource, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallerAnalytics* InInstallerAnalytics, IFileConstructorStat* InFileConstructorStat)
	: Configuration(MoveTemp(InConfiguration))
	, bIsDownloadStarted(false)
	, bInitialDiskSizeCheck(false)
	, bIsPaused(false)
	, bShouldAbort(false)
	, ThreadLock()
	, ConstructionStack()
	, FileSystem(InFileSystem)
	, ChunkSource(InChunkSource)
	, ChunkReferenceTracker(InChunkReferenceTracker)
	, InstallerError(InInstallerError)
	, InstallerAnalytics(InInstallerAnalytics)
	, FileConstructorStat(InFileConstructorStat)
	, TotalJobSize(0)
	, ByteProcessed(0)
	, RequiredDiskSpace(0)
	, AvailableDiskSpace(0)
{
	// Count initial job size
	const int32 ConstructListNum = Configuration.ConstructList.Num();
	ConstructionStack.Reserve(ConstructListNum);
	ConstructionStack.AddDefaulted(ConstructListNum);
	for (int32 ConstructListIdx = 0; ConstructListIdx < ConstructListNum ; ++ConstructListIdx)
	{
		const FString& ConstructListElem = Configuration.ConstructList[ConstructListIdx];
		const FFileManifest* FileManifest = Configuration.ManifestSet->GetNewFileManifest(ConstructListElem);
		if (FileManifest)
		{
			TotalJobSize += FileManifest->FileSize;
		}
		ConstructionStack[(ConstructListNum - 1) - ConstructListIdx] = ConstructListElem;
	}
}

FBuildPatchFileConstructor::~FBuildPatchFileConstructor()
{
}

void FBuildPatchFileConstructor::Run()
{
	FileConstructorStat->OnTotalRequiredUpdated(TotalJobSize);

	// Check for resume data, we need to also look for a legacy resume file to use instead in case we are resuming from an install of previous code version.
	const FString LegacyResumeDataFilename = Configuration.StagingDirectory / TEXT("$resumeData");
	const FString ResumeDataFilename = Configuration.MetaDirectory / TEXT("$resumeData");
	const bool bHasLegacyResumeData = FileSystem->FileExists(*LegacyResumeDataFilename);
	// If we find a legacy resume data file, lets move it first.
	if (bHasLegacyResumeData)
	{
		FileSystem->MoveFile(*ResumeDataFilename, *LegacyResumeDataFilename);
	}
	FResumeData ResumeData(FileSystem, Configuration.ManifestSet, Configuration.StagingDirectory, ResumeDataFilename);

	// Remove incompatible files
	if (ResumeData.bHasResumeData)
	{
		for (const FString& FileToConstruct : Configuration.ConstructList)
		{
			ResumeData.CheckFile(FileToConstruct);
			const bool bFileIncompatible = ResumeData.FilesIncompatible.Contains(FileToConstruct);
			if (bFileIncompatible)
			{
				GLog->Logf(TEXT("FBuildPatchFileConstructor: Deleting incompatible stage file %s"), *FileToConstruct);
				FileSystem->DeleteFile(*(Configuration.StagingDirectory / FileToConstruct));
			}
		}
	}

	// Save for started versions
	TSet<FString> ResumeIds;
	const bool bCheckLegacyIds = false;

	Configuration.ManifestSet->GetInstallResumeIds(ResumeIds, bCheckLegacyIds);
	ResumeData.SaveOut(ResumeIds);

	// Start resume progress at zero or one.
	FileConstructorStat->OnResumeStarted();

	// While we have files to construct, run.
	FString FileToConstruct;
	while (GetFileToConstruct(FileToConstruct) && !bShouldAbort)
	{
		// Get the file manifest.
		const FFileManifest* FileManifest = Configuration.ManifestSet->GetNewFileManifest(FileToConstruct);
		bool bFileSuccess = FileManifest != nullptr;
		if (bFileSuccess)
		{
			const int64 FileSize = FileManifest->FileSize;
			FileConstructorStat->OnFileStarted(FileToConstruct, FileSize);

			// Check resume status for this file.
			const bool bFilePreviouslyComplete = ResumeData.FilesCompleted.Contains(FileToConstruct);
			const bool bFilePreviouslyStarted = ResumeData.FilesStarted.Contains(FileToConstruct);

			// Construct or skip the file.
			if (bFilePreviouslyComplete)
			{
				bFileSuccess = true;
				CountBytesProcessed(FileSize);
				GLog->Logf(TEXT("FBuildPatchFileConstructor: Skipping completed file %s"), *FileToConstruct);
				// Go through each chunk part, and dereference it from the reference tracker.
				for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
				{
					bFileSuccess = ChunkReferenceTracker->PopReference(ChunkPart.Guid) && bFileSuccess;
				}
			}
			else
			{
				bFileSuccess = ConstructFileFromChunks(FileToConstruct, *FileManifest, bFilePreviouslyStarted);
			}
		}
		else
		{
			// Only report or log if the first error
			if (InstallerError->HasError() == false)
			{
				InstallerAnalytics->RecordConstructionError(FileToConstruct, INDEX_NONE, TEXT("Missing File Manifest"));
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Missing file manifest for %s"), *FileToConstruct);
			}
			// Always set
			InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
		}

		if (bFileSuccess)
		{
			// If we are destructive, remove the old file.
			if (Configuration.InstallMode == EInstallMode::DestructiveInstall)
			{
				const bool bRequireExists = false;
				const bool bEvenReadOnly = true;
				FString FileToDelete = Configuration.InstallDirectory / FileToConstruct;
				FPaths::NormalizeFilename(FileToDelete);
				FPaths::CollapseRelativeDirectories(FileToDelete);
				if (FileSystem->FileExists(*FileToDelete))
				{
					OnBeforeDeleteFile().Broadcast(FileToDelete);
					IFileManager::Get().Delete(*FileToDelete, bRequireExists, bEvenReadOnly);
				}
			}
		}
		else
		{
			// This will only record and log if a failure was not already registered.
			bShouldAbort = true;
			InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::UnknownFail);
			UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed to build %s "), *FileToConstruct);
		}
		FileConstructorStat->OnFileCompleted(FileToConstruct, bFileSuccess);

		// Wait while paused.
		FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
	}

	// Mark resume complete if we didn't have work to do.
	if (!bIsDownloadStarted)
	{
		FileConstructorStat->OnResumeCompleted();
	}
	FileConstructorStat->OnConstructionCompleted();
}

uint64 FBuildPatchFileConstructor::GetRequiredDiskSpace()
{
	FScopeLock Lock(&ThreadLock);
	return RequiredDiskSpace;
}

uint64 FBuildPatchFileConstructor::GetAvailableDiskSpace()
{
	FScopeLock Lock(&ThreadLock);
	return AvailableDiskSpace;
}

FBuildPatchFileConstructor::FOnBeforeDeleteFile& FBuildPatchFileConstructor::OnBeforeDeleteFile()
{
	return BeforeDeleteFileEvent;
}

void FBuildPatchFileConstructor::CountBytesProcessed( const int64& ByteCount )
{
	ByteProcessed += ByteCount;
	FileConstructorStat->OnProcessedDataUpdated(ByteProcessed);
}

bool FBuildPatchFileConstructor::GetFileToConstruct(FString& Filename)
{
	FScopeLock Lock(&ThreadLock);
	const bool bFileAvailable = ConstructionStack.Num() > 0;
	if (bFileAvailable)
	{
		Filename = ConstructionStack.Pop(EAllowShrinking::No);
	}
	return bFileAvailable;
}

int64 FBuildPatchFileConstructor::GetRemainingBytes()
{
	FScopeLock Lock(&ThreadLock);
	return Configuration.ManifestSet->GetTotalNewFileSize(ConstructionStack);
}

uint64 FBuildPatchFileConstructor::CalculateRequiredDiskSpace(const FFileManifest& InProgressFileManifest, uint64 InProgressFileSize)
{
	int64 DiskSpaceDeltaPeak = InProgressFileSize;
	if (Configuration.InstallMode == EInstallMode::DestructiveInstall)
	{
		// The simplest method will be to run through each high level file operation, tracking peak disk usage delta.
		int64 DiskSpaceDelta = InProgressFileSize;

		// Can remove old in progress file.
		DiskSpaceDelta -= InProgressFileManifest.FileSize;

		// Loop through all files to be made next, in order.
		for (int32 ConstructionStackIdx = ConstructionStack.Num() - 1; ConstructionStackIdx >= 0; --ConstructionStackIdx)
		{
			const FString& FileToConstruct = ConstructionStack[ConstructionStackIdx];
			const FFileManifest* NewFileManifest = Configuration.ManifestSet->GetNewFileManifest(FileToConstruct);
			const FFileManifest* OldFileManifest = Configuration.ManifestSet->GetCurrentFileManifest(FileToConstruct);
			// First we would need to make the new file.
			DiskSpaceDelta += NewFileManifest->FileSize;
			if (DiskSpaceDeltaPeak < DiskSpaceDelta)
			{
				DiskSpaceDeltaPeak = DiskSpaceDelta;
			}
			// Then we can remove the current existing file.
			if (OldFileManifest)
			{
				DiskSpaceDelta -= OldFileManifest->FileSize;
			}
		}
	}
	else
	{
		// When not destructive, we always stage all new and changed files.
		DiskSpaceDeltaPeak += Configuration.ManifestSet->GetTotalNewFileSize(ConstructionStack);
	}
	return FMath::Max<int64>(DiskSpaceDeltaPeak, 0);
}

bool FBuildPatchFileConstructor::ConstructFileFromChunks(const FString& BuildFilename, const FFileManifest& FileManifest, bool bResumeExisting)
{
	bool bSuccess = true;
	EConstructionError ConstructionError = EConstructionError::None;
	uint32 LastError = 0;
	FString NewFilename = Configuration.StagingDirectory / BuildFilename;

	// Calculate the hash as we write the data
	FSHA1 HashState;
	FSHAHash HashValue;

	if (!FileManifest.SymlinkTarget.IsEmpty())
	{
#if PLATFORM_MAC
		bSuccess = symlink(TCHAR_TO_UTF8(*FileManifest.SymlinkTarget), TCHAR_TO_UTF8(*NewFilename)) == 0;
#else
		const bool bSymlinkNotImplemented = false;
		check(bSymlinkNotImplemented);
		bSuccess = false;
#endif
		return bSuccess;
	}

	// Check for resuming of existing file
	int64 StartPosition = 0;
	int32 StartChunkPart = 0;
	if (bResumeExisting)
	{
		// We have to read in the existing file so that the hash check can still be done.
		TUniquePtr<FArchive> NewFileReader(IFileManager::Get().CreateFileReader(*NewFilename));
		if (NewFileReader.IsValid())
		{
			// Start with a sensible buffer size for reading. 4 MiB.
			const int32 ReadBufferSize = 4 * 1024 * 1024;
			// Read buffer
			TArray<uint8> ReadBuffer;
			ReadBuffer.Empty(ReadBufferSize);
			ReadBuffer.SetNumUninitialized(ReadBufferSize);
			// Reuse a certain amount of the file
			StartPosition = FMath::Max<int64>(0, NewFileReader->TotalSize() - NUM_BYTES_RESUME_IGNORE);
			// We'll also find the correct chunkpart to start writing from
			int64 ByteCounter = 0;
			for (int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest.ChunkParts.Num() && !bShouldAbort; ++ChunkPartIdx)
			{
				const FChunkPart& ChunkPart = FileManifest.ChunkParts[ChunkPartIdx];
				const int64 NextBytePosition = ByteCounter + ChunkPart.Size;
				if (NextBytePosition <= StartPosition)
				{
					// Ensure buffer is large enough
					ReadBuffer.SetNumUninitialized(ChunkPart.Size, EAllowShrinking::No);
					ISpeedRecorder::FRecord ActivityRecord;
					// Read data for hash check
					FileConstructorStat->OnBeforeRead();
					ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
					NewFileReader->Serialize(ReadBuffer.GetData(), ChunkPart.Size);
					ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
					ActivityRecord.Size = ChunkPart.Size;
					HashState.Update(ReadBuffer.GetData(), ChunkPart.Size);
					FileConstructorStat->OnAfterRead(ActivityRecord);
					// Count bytes read from file
					ByteCounter = NextBytePosition;
					// Set to resume from next chunk part
					StartChunkPart = ChunkPartIdx + 1;
					// Inform the reference tracker of the chunk part skip
					bSuccess = ChunkReferenceTracker->PopReference(ChunkPart.Guid) && bSuccess;
					CountBytesProcessed(ChunkPart.Size);
					FileConstructorStat->OnFileProgress(BuildFilename, NewFileReader->Tell());
					// Wait if paused
					FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
				}
				else
				{
					// No more parts on disk
					break;
				}
			}
			// Set start position to the byte we got up to
			StartPosition = ByteCounter;
			// Close file
			NewFileReader->Close();
		}
	}

	// If we haven't done so yet, make the initial disk space check
	if (!bInitialDiskSizeCheck)
	{
		bInitialDiskSizeCheck = true;
		const uint64 RequiredSpace = CalculateRequiredDiskSpace(FileManifest, FileManifest.FileSize - StartPosition);
		// ThreadLock protects access to members RequiredDiskSpace and AvailableDiskSpace;
		FScopeLock Lock(&ThreadLock);
		RequiredDiskSpace = RequiredSpace;
		if (!FileConstructorHelpers::CheckRemainingDiskSpace(Configuration.InstallDirectory, RequiredSpace, AvailableDiskSpace))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Out of HDD space. Needs %llu bytes, Free %llu bytes"), RequiredSpace, AvailableDiskSpace);
			InstallerError->SetError(
				EBuildPatchInstallError::OutOfDiskSpace,
				DiskSpaceErrorCodes::InitialSpaceCheck,
				0,
				BuildPatchServices::GetDiskSpaceMessage(Configuration.InstallDirectory, RequiredSpace, AvailableDiskSpace));
			return false;
		}
	}

	// Now we can make sure the chunk cache knows to start downloading chunks
	if (!bIsDownloadStarted)
	{
		bIsDownloadStarted = true;
		FileConstructorStat->OnResumeCompleted();
	}

	// Attempt to create the file
	ISpeedRecorder::FRecord ActivityRecord;
	FileConstructorStat->OnBeforeAdminister();
	ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
	TUniquePtr<FArchive> NewFile = FileSystem->CreateFileWriter(*NewFilename, bResumeExisting ? EWriteFlags::Append : EWriteFlags::None);
	LastError = FPlatformMisc::GetLastError();
	ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
	ActivityRecord.Size = 0;
	FileConstructorStat->OnAfterAdminister(ActivityRecord);
	bSuccess = NewFile != nullptr;
	if (bSuccess)
	{
		// Seek to file write position
		if (NewFile->Tell() != StartPosition)
		{
			FileConstructorStat->OnBeforeAdminister();
			ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
			NewFile->Seek(StartPosition);
			ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
			ActivityRecord.Size = 0;
			FileConstructorStat->OnAfterAdminister(ActivityRecord);
		}

		// For each chunk, load it, and place it's data into the file
		for (int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest.ChunkParts.Num() && bSuccess && !bShouldAbort; ++ChunkPartIdx)
		{
			const FChunkPart& ChunkPart = FileManifest.ChunkParts[ChunkPartIdx];
			bSuccess = InsertChunkData(ChunkPart, *NewFile, HashState, ConstructionError);
			FileConstructorStat->OnFileProgress(BuildFilename, NewFile->Tell());
			if (bSuccess)
			{
				CountBytesProcessed(ChunkPart.Size);
				// Wait while paused
				FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
			}
			// Only report or log if this is the first error
			else if (InstallerError->HasError() == false)
			{
				if (ConstructionError == EConstructionError::MissingChunk)
				{
					InstallerAnalytics->RecordConstructionError(BuildFilename, INDEX_NONE, TEXT("Missing Chunk"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to missing chunk %s"), *BuildFilename, *ChunkPart.Guid.ToString());
				}
				else if (ConstructionError == EConstructionError::TrackingError)
				{
					InstallerAnalytics->RecordConstructionError(BuildFilename, INDEX_NONE, TEXT("Tracking Error"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to untracked chunk %s"), *BuildFilename, *ChunkPart.Guid.ToString());
				}
			}
		}

		// Close the file writer
		FileConstructorStat->OnBeforeAdminister();
		ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
		const bool bArchiveSuccess = NewFile->Close();
		NewFile.Reset();
		ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
		ActivityRecord.Size = 0;
		FileConstructorStat->OnAfterAdminister(ActivityRecord);

		// Check for final success
		if (ConstructionError == EConstructionError::None && !bArchiveSuccess)
		{
			ConstructionError = EConstructionError::SerializeError;
			bSuccess = false;
		}
	}
	else
	{
		ConstructionError = EConstructionError::CannotCreateFile;
	}

	// Check for error state
	if (!bSuccess)
	{
		// Recalculate disk space first
		int64 InProgressFileSize = FileManifest.FileSize;
		FileSystem->GetFileSize(*NewFilename, InProgressFileSize);
		const uint64 RemainingRequiredSpace = CalculateRequiredDiskSpace(FileManifest, FileManifest.FileSize - InProgressFileSize);
		uint64 RemainingAvailableDiskSpace = 0;
		if (!FileConstructorHelpers::CheckRemainingDiskSpace(Configuration.InstallDirectory, RemainingRequiredSpace, RemainingAvailableDiskSpace))
		{
			// ThreadLock protects access to members RequiredDiskSpace and AvailableDiskSpace
			ThreadLock.Lock();
			RequiredDiskSpace = RemainingRequiredSpace;
			AvailableDiskSpace = RemainingAvailableDiskSpace;
			ThreadLock.Unlock();
			// Convert error to disk space rather than reported.
			UE_LOG(LogBuildPatchServices, Error, TEXT("Out of HDD space. Needs %llu bytes, Free %llu bytes"), RemainingRequiredSpace, RemainingAvailableDiskSpace);
			InstallerError->SetError(
				EBuildPatchInstallError::OutOfDiskSpace,
				DiskSpaceErrorCodes::DuringInstallation,
				0,
				BuildPatchServices::GetDiskSpaceMessage(Configuration.InstallDirectory, RemainingRequiredSpace, RemainingAvailableDiskSpace));
			ConstructionError = EConstructionError::OutOfDiskSpace;
		}
		else
		{
			const bool bReportAnalytic = InstallerError->HasError() == false;
			switch (ConstructionError)
			{
			case EConstructionError::CannotCreateFile:
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(BuildFilename, LastError, TEXT("Could Not Create File"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Could not create %s"), *BuildFilename);
				}
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::FileCreateFail, LastError);
				break;
			case EConstructionError::MissingChunk:
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingChunkData);
				break;
			case EConstructionError::SerializeError:
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::SerializationError);
				break;
			case EConstructionError::TrackingError:
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::TrackingError);
				break;
			}
		}
	}

	// Verify the hash for the file that we created
	if (bSuccess)
	{
		HashState.Final();
		HashState.GetHash(HashValue.Hash);
		bSuccess = HashValue == FileManifest.FileHash;
		if (!bSuccess)
		{
			ConstructionError = EConstructionError::OutboundDataError;
			// Only report or log if the first error
			if (InstallerError->HasError() == false)
			{
				InstallerAnalytics->RecordConstructionError(BuildFilename, INDEX_NONE, TEXT("Serialised Verify Fail"));
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Verify failed after constructing %s"), *BuildFilename);
			}
			// Always set
			InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::OutboundCorrupt);
		}
	}

#if PLATFORM_MAC
	if (bSuccess && EnumHasAllFlags(FileManifest.FileMetaFlags, EFileMetaFlags::UnixExecutable))
	{
		// Enable executable permission bit
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NewFilename), &FileInfo) == 0)
		{
			bSuccess = chmod(TCHAR_TO_UTF8(*NewFilename), FileInfo.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
		}
	}
#endif

#if PLATFORM_ANDROID
	if (bSuccess)
	{
		IFileManager::Get().SetTimeStamp(*NewFilename, FDateTime::UtcNow());
	}
#endif

	// Delete the staging file if unsuccessful by means of any failure that could leave the file in unknown state.
	if (!bSuccess)
	{
		switch (ConstructionError)
		{
		case EConstructionError::CannotCreateFile:
		case EConstructionError::SerializeError:
		case EConstructionError::TrackingError:
		case EConstructionError::OutboundDataError:
			if (!FileSystem->DeleteFile(*NewFilename))
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("FBuildPatchFileConstructor: Error deleting file: %s (Error Code %i)"), *NewFilename, FPlatformMisc::GetLastError());
			}
			break;
		}
	}

	return bSuccess;
}

bool FBuildPatchFileConstructor::InsertChunkData(const FChunkPart& ChunkPart, FArchive& DestinationFile, FSHA1& HashState, EConstructionError& ConstructionError)
{
	if (bStallWhenFileSystemThrottled)
	{
		int64 AvailableBytes = FileSystem->GetAllowedBytesToWriteThrottledStorage(*DestinationFile.GetArchiveName());
		while (ChunkPart.Size > AvailableBytes)
		{
			UE_LOG(LogBuildPatchServices, Display, TEXT("Avaliable write bytes to write throttled storage exhausted (%s).  Sleeping %ds.  Bytes needed: %u, bytes available: %lld")
				, *DestinationFile.GetArchiveName(), SleepTimeWhenFileSystemThrottledSeconds, ChunkPart.Size, AvailableBytes);
			FPlatformProcess::Sleep(SleepTimeWhenFileSystemThrottledSeconds);
			AvailableBytes = FileSystem->GetAllowedBytesToWriteThrottledStorage(*DestinationFile.GetArchiveName());
		}
	}

	uint8* Data;
	uint8* DataStart;
	ConstructionError = EConstructionError::None;
	ISpeedRecorder::FRecord ActivityRecord;
	FileConstructorStat->OnChunkGet(ChunkPart.Guid);
	IChunkDataAccess* ChunkDataAccess = ChunkSource->Get(ChunkPart.Guid);
	if (ChunkDataAccess != nullptr)
	{
		ChunkDataAccess->GetDataLock(&Data, nullptr);
		FileConstructorStat->OnBeforeWrite();
		ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
		DataStart = &Data[ChunkPart.Offset];
		HashState.Update(DataStart, ChunkPart.Size);
		DestinationFile.Serialize(DataStart, ChunkPart.Size);
		const bool bSerializeOk = !DestinationFile.IsError();
		ActivityRecord.Size = ChunkPart.Size;
		ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
		FileConstructorStat->OnAfterWrite(ActivityRecord);
		ChunkDataAccess->ReleaseDataLock();
		const bool bPopReferenceOk = ChunkReferenceTracker->PopReference(ChunkPart.Guid);
		if (!bSerializeOk)
		{
			ConstructionError = EConstructionError::SerializeError;
		}
		else if (!bPopReferenceOk)
		{
			ConstructionError = EConstructionError::TrackingError;
		}
	}
	else
	{
		ConstructionError = EConstructionError::MissingChunk;
	}
	return ConstructionError == EConstructionError::None;
}

void FBuildPatchFileConstructor::DeleteDirectoryContents(const FString& RootDirectory)
{
	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
	for (const FString& DirName : SubDirNames)
	{
		IFileManager::Get().DeleteDirectory(*(RootDirectory / DirName), false, true);
	}

	TArray<FString> SubFileNames;
	IFileManager::Get().FindFiles(SubFileNames, *(RootDirectory / TEXT("*")), true, false);
	for (const FString& FileName : SubFileNames)
	{
		IFileManager::Get().Delete(*(RootDirectory / FileName), false, true);
	}
}
