// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "HAL/Runnable.h"
#include "BuildPatchProgress.h"
#include "BuildPatchManifest.h"
#include "Installer/Controllable.h"
#include "Common/SpeedRecorder.h"
#include "BuildPatchInstall.h"

// Forward declarations
class FBuildPatchAppManifest;
enum class EConstructionError : uint8;

namespace BuildPatchServices
{
	struct FChunkPart;
	class IFileSystem;
	class IChunkSource;
	class IChunkReferenceTracker;
	class IInstallerError;
	class IInstallerAnalytics;
	class IFileConstructorStat;
	class IBuildManifestSet;

	/**
	 * A struct containing the configuration values for a file constructor.
	 */
	struct FFileConstructorConfig
	{
		// The manifest set class for details on the installation files.
		IBuildManifestSet* ManifestSet;

		// The location for the installation.
		FString InstallDirectory;

		// The location where new installation files will be constructed.
		FString StagingDirectory;

		// The location where temporary files for tracking can be stored.
		FString MetaDirectory;

		// The list of files to be constructed, filename paths should match those contained in manifest.
		TArray<FString> ConstructList;

		// The install mode used for this installation.
		EInstallMode InstallMode;
	};

	/**
	 * FBuildPatchFileConstructor
	 * This class controls a thread that constructs files from a file list, given install details, and chunk availability notifications
	 */
	class FBuildPatchFileConstructor
		: public FRunnable
		, public IControllable
	{
	public:

		/**
		 * Constructor
		 * @param Configuration             The configuration for the constructor.
		 * @param FileSystem                The service used to open files.
		 * @param ChunkSource               Pointer to the chunk source.
		 * @param ChunkReferenceTracker     Pointer to the chunk reference tracker.
		 * @param InstallerError            Pointer to the installer error class for reporting fatal errors.
		 * @param InstallerAnalytics        Pointer to the installer analytics handler for reporting events.
		 * @param FileConstructorStat       Pointer to the stat class for receiving updates.
		 */
		FBuildPatchFileConstructor(FFileConstructorConfig Configuration, IFileSystem* FileSystem, IChunkSource* ChunkSource, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallerAnalytics* InstallerAnalytics, IFileConstructorStat* FileConstructorStat);

		/**
		 * Default Destructor, will delete the allocated Thread
		 */
		~FBuildPatchFileConstructor();

		// FRunnable interface begin.
		virtual uint32 Run() override;
		// FRunnable interface end.

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override
		{
			bIsPaused = bInIsPaused;
		}

		virtual void Abort() override
		{
			bShouldAbort = true;
		}
		// IControllable interface end.

		/**
		 * Blocks the calling thread until this one has completed
		 */
		void Wait();

		/**
		 * Get whether the thread has finished working
		 * @return	true if the thread completed
		 */
		bool IsComplete();

		/**
		 * Get the disk space that was required to perform the installation
		 * @return	the disk space required to perform the installation in bytes
		 */
		uint64 GetRequiredDiskSpace();

		/**
		 * Get the disk space that was available at the time of checking for the required disk space
		 * @return	the disk space that was available in bytes
		 */
		uint64 GetAvailableDiskSpace();

		/**
		 * Broadcasts with full filepath to file that the constructor is about to delete in order to free up space.
		 * @return	Reference to the event object.
		 */
		DECLARE_EVENT_OneParam(FBuildPatchFileConstructor, FOnBeforeDeleteFile, const FString& /*BuildFile*/);
		FOnBeforeDeleteFile& OnBeforeDeleteFile();

	private:

		/**
		 * Sets the bIsRunning flag
		 * @param bRunning	Whether the thread is running
		 */
		void SetRunning(bool bRunning);

		/**
		 * Sets the bIsInited flag
		 * @param bInited	Whether the thread successfully initialized
		 */
		void SetInited(bool bInited);

		/**
		 * Sets the bInitFailed flag
		 * @param bFailed	Whether the thread failed on init
		 */
		void SetInitFailed(bool bFailed);

		/**
		 * Count additional bytes processed, and set new install progress value
		 * @param ByteCount		Number of bytes to increment by
		 */
		void CountBytesProcessed(const int64& ByteCount);

		/**
		 * Function to fetch a chunk from the download list
		 * @param Filename		Receives the filename for the file to construct from the manifest
		 * @return true if there was a chunk guid in the list
		 */
		bool GetFileToConstruct(FString& Filename);

		/**
		 * @return the total bytes size of files not yet started construction
		 */
		int64 GetRemainingBytes();

		/**
		 * Calculates the minimum required disk space for the remaining work to be completed, based on a current file, and the list of files left in ConstructionStack.
		 * @param InProgressFileManifest	The manifest for the file currently being constructed.
		 * @param InProgressFileSize		The remaining size required for the file currently being constructed.
		 * @return the number of bytes required on disk to complete the installation.
		 */
		uint64 CalculateRequiredDiskSpace(const FFileManifest& InProgressFileManifest, uint64 InProgressFileSize);

		/**
		 * Constructs a particular file referenced by the given BuildManifest. The function takes an interface to a class that can provide availability information of chunks so that this
		 * file construction process can be ran alongside chunk acquisition threads. It will Sleep while waiting for chunks that it needs.
		 * @param FileManifest		The FFileManifest for the file to construct.
		 * @param bResumeExisting	Whether we should resume from an existing file
		 * @return	true if no file errors occurred
		 */
		bool ConstructFileFromChunks(const FFileManifest& FileManifest, bool bResumeExisting);

		/**
		 * Inserts the data data from a chunk into the destination file according to the chunk part info
		 * @param ChunkPart          The chunk part details.
		 * @param DestinationFile    The Filename for the file being constructed.
		 * @param HashState          An FSHA1 hash state to update with the data going into the destination file.
		 * @param ConstructionError  Will be set to the error type that ocurred or EConstructionError::None.
		 * @return true if no errors were detected
		 */
		bool InsertChunkData(const FChunkPart& ChunkPart, FArchive& DestinationFile, FSHA1& HashState, EConstructionError& ConstructionError);

		/**
		 * Delete all contents of a directory
		 * @param RootDirectory	 	Directory to make empty
		 */
		void DeleteDirectoryContents(const FString& RootDirectory);

	private:
		// The configuration for the constructor.
		const FFileConstructorConfig Configuration;

		// Hold a pointer to my thread for easier deleting.
		FRunnableThread* Thread;

		// A flag marking that we a running.
		bool bIsRunning;

		// A flag marking that we initialized correctly.
		bool bIsInited;

		// A flag marking that our init returned a failure (true means failed).
		bool bInitFailed;

		// A flag marking that we told the chunk cache to queue required downloads.
		bool bIsDownloadStarted;

		// A flag marking that we have made the initial disk space check following resume logic complete.
		bool bInitialDiskSizeCheck;

		// A flag marking whether we should be paused.
		FThreadSafeBool bIsPaused;

		// A flag marking whether we should abort operations and exit.
		FThreadSafeBool bShouldAbort;

		// A critical section to protect the flags and variables.
		FCriticalSection ThreadLock;

		// A stack of filenames for files that need to be constructed.
		TArray<FString> ConstructionStack;

		// Pointer to the file system.
		IFileSystem* FileSystem;

		// Pointer to chunk source.
		IChunkSource* ChunkSource;

		// Pointer to the chunk reference tracker.
		IChunkReferenceTracker* ChunkReferenceTracker;

		// Pointer to the installer error class.
		IInstallerError* InstallerError;

		// Pointer to the installer analytics handler.
		IInstallerAnalytics* InstallerAnalytics;

		// Pointer to the stat class.
		IFileConstructorStat* FileConstructorStat;

		// Total job size for tracking progress.
		int64 TotalJobSize;

		// Byte processed so far for tracking progress.
		int64 ByteProcessed;

		// The amount of disk space requirement that was calculated when beginning the process. 0 if the install process was not started, or no additional space was needed.
		uint64 RequiredDiskSpace;

		// The amount of disk space available when beginning the process. 0 if the install process was not started.
		uint64 AvailableDiskSpace;

		// Event executed before deleting an old installation file.
		FOnBeforeDeleteFile BeforeDeleteFileEvent;
	};

	/**
	 * This interface defines the statistics class required by the file constructor. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IFileConstructorStat
	{
	public:
		virtual ~IFileConstructorStat() {}

		/**
		 * Called when the resume process begins.
		 */
		virtual void OnResumeStarted() = 0;

		/**
		 * Called when the resume process completes.
		 */
		virtual void OnResumeCompleted() = 0;

		/**
		 * Called for each Get made to the chunk source.
		 * @param ChunkId       The id for the chunk required.
		 */
		virtual void OnChunkGet(const FGuid& ChunkId) = 0;

		/**
		 * Called when a file construction has started.
		 * @param Filename      The filename of the file.
		 * @param FileSize      The size of the file being constructed.
		 */
		virtual void OnFileStarted(const FString& Filename, int64 FileSize) = 0;

		/**
		 * Called during a file construction with the current progress.
		 * @param Filename      The filename of the file.
		 * @param TotalBytes    The number of bytes processed so far.
		 */
		virtual void OnFileProgress(const FString& Filename, int64 TotalBytes) = 0;

		/**
		 * Called when a file construction has completed.
		 * @param Filename      The filename of the file.
		 * @param bSuccess      True if the file construction succeeded.
		 */
		virtual void OnFileCompleted(const FString& Filename, bool bSuccess) = 0;

		/**
		 * Called when the construction process completes.
		 */
		virtual void OnConstructionCompleted() = 0;

		/**
		 * Called to update the total amount of bytes which have been constructed.
		 * @param TotalBytes    The number of bytes constructed so far.
		 */
		virtual void OnProcessedDataUpdated(int64 TotalBytes) = 0;

		/**
		 * Called to update the total number of bytes to be constructed.
		 * @param TotalBytes    The total number of bytes to be constructed.
		 */
		virtual void OnTotalRequiredUpdated(int64 TotalBytes) = 0;

		/**
		 * Called when we are beginning a file administration, such as open, close, seek.
		 */
		virtual void OnBeforeAdminister() = 0;

		/**
		 * Called upon completing an admin operation, with activity recording.
		 * @param Record        The activity record.
		 */
		virtual void OnAfterAdminister(const ISpeedRecorder::FRecord& Record) = 0;

		/**
		 * Called when we are beginning a read operation.
		 */
		virtual void OnBeforeRead() = 0;

		/**
		 * Called upon completing a read operation, with activity recording.
		 * @param Record        The activity record.
		 */
		virtual void OnAfterRead(const ISpeedRecorder::FRecord& Record) = 0;

		/**
		 * Called when we are beginning a write operation.
		 */
		virtual void OnBeforeWrite() = 0;

		/**
		 * Called upon completing a write operation, with activity recording.
		 * @param Record        The activity record.
		 */
		virtual void OnAfterWrite(const ISpeedRecorder::FRecord& Record) = 0;
	};
}

/**
 * Helpers for calculations that are useful for other classes or operations.
 */
namespace FileConstructorHelpers
{
	uint64 CalculateRequiredDiskSpace(const FBuildPatchAppManifestPtr& CurrentManifest, const FBuildPatchAppManifestRef& BuildManifest, const BuildPatchServices::EInstallMode& InstallMode, const TSet<FString>& InstallTags);
}
