// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

namespace BuildPatchServices
{
	struct FBuildInstallerConfiguration;
	enum class EVerifyError : uint32;

	/**
	 * An enum describing the current state of the data for a file operation that has or will be processed.
	 */
	enum class EFileOperationState : int32
	{
		// Not yet evaluated.
		Unknown = 0,

		// The data for this operation has not yet been requested (from a local chunkdb source).
		PendingLocalChunkDbData,

		// The data is being acquired (from a local chunkdb source).
		RetrievingLocalChunkDbData,

		// The data for this operation has not yet been requested (from a local install source).
		PendingLocalInstallData,

		// The data is being acquired (from a local install source).
		RetrievingLocalInstallData,

		// The data for this operation has not yet been requested (from a remote cloud source).
		PendingRemoteCloudData,

		// The data is being acquired (from a remote cloud source).
		RetrievingRemoteCloudData,

		// The data is in a local store to be loaded later.
		PendingLocalDataStore,

		// The data is being acquired (from a local store).
		RetrievingLocalDataStore,

		// The data is in a memory chunk store ready for use.
		DataInMemoryStore,

		// The file operation has been written to the staging location.
		Staged,

		// The file operation has been written to the destination install location.
		Installed,

		// The file operation is being verified.
		Verifying,

		// The file operation has been verified and is corrupt.
		VerifiedFail,

		// The file operation has been verified and successful.
		VerifiedSuccess,

		// Helpers.
		NUM_States,
		Complete = NUM_States - 1
	};

	/**
	 * A struct representing a file operation to be completed.
	 */
	struct FFileOperation
	{
	public:
		FFileOperation(FString Filename, const FGuid& DataId, uint64 Offest, uint64 Size, EFileOperationState CurrentState);
		~FFileOperation();

	public:
		// The build filename where this data section goes.
		FString Filename;
		// The idea of the chunk where the data is taken from.
		FGuid DataId;
		// The offset into the file.
		uint64 Offest;
		// The size of the data to write.
		uint64 Size;
		// The current state of this operation.
		EFileOperationState CurrentState;
	};

	/**
	 * A struct representing a download.
	 */
	struct FDownload
	{
		// The uri for the download.
		FString Data;
		// The size of the download.
		int64 Size;
		// The amount of data received so far.
		int64 Received;
	};

	/**
	 * An interface for accessing runtime statistical information about an installer.
	 */
	class IBuildStatistics
	{
	public:
		/**
		 * Virtual destructor.
		 */
		virtual ~IBuildStatistics() { }

		/**
		 * @return the configuration used when constructing the installer.
		 */
		virtual const FBuildInstallerConfiguration& GetConfiguration() const = 0;

		/**
		 * @return the total download size for the installation.
		 */
		virtual int64 GetDownloadSize() const = 0;

		/**
		 * @return the total install size for the installation.
		 */
		virtual int64 GetBuildSize() const = 0;

		/**
		 * @return the size in chunks of the memory store.
		 */
		virtual int32 GetMemoryChunkStoreSize() const = 0;

		/**
		 * @return the number of chunks currently in the memory store.
		 */
		virtual int32 GetMemoryChunksInStore() const = 0;

		/**
		 * @return the number of chunks that have been booted from the memory store.
		 */
		virtual int32 GetMemoryChunksBooted() const = 0;

		/**
		 * @return the number of chunks currently in the memory store which are held due to multiple referencing.
		 */
		virtual int32 GetMemoryChunksRetained() const = 0;

		/**
		 * @return the filename of the file currently being worked on.
		 */
		virtual FString GetCurrentWorkingFileName() const = 0;

		/**
		 * @return the progress of the file currently being worked on.
		 */
		virtual float GetCurrentWorkingFileProgress() const = 0;

		/**
		 * @return the chunk ID currently being used to write the current file.
		 */
		virtual FGuid GetCurrentWorkingData() const = 0;

		/**
		 * @return an array of currently active downloads.
		 */
		virtual TArray<FDownload> GetCurrentDownloads() const = 0;

		/**
		 * @return true if there are downloads currently active.
		 */
		virtual bool IsDownloadActive() const = 0;

		/**
		 * @return true if the current operation is creating or opening files.
		 */
		virtual bool IsHardDiskActiveAdministering() const = 0;

		/**
		 * @return true if currently writing data to disk.
		 */
		virtual bool IsHardDiskActiveWrite() const = 0;

		/**
		 * @return true if currently reading data from disk.
		 */
		virtual bool IsHardDiskActiveRead() const = 0;

		/**
		 * @return an array containing the states for all file write operations being performed by this installation.
		 */
		virtual const TArray<FFileOperation>& GetFileOperationStates() const = 0;

		/**
		 * @return the current download speed in bytes per second.
		 */
		virtual double GetDownloadByteSpeed() const = 0;

		/**
		 * @return the current disk read speed in bytes per second.
		 */
		virtual double GetDiskReadByteSpeed() const = 0;

		/**
		 * @return the current chunkdb read speed in bytes per second.
		 */
		virtual double GetChunkDbReadByteSpeed() const = 0;

		/**
		 * @return the current disk write speed in bytes per second.
		 */
		virtual double GetDiskWriteByteSpeed() const = 0;

		/**
		 * @return the total number of verify errors experienced during this installation.
		 */
		virtual int32 GetNumVerifyErrors() const = 0;
		
		/**
		 * @return the verify error counts per error type experienced during this installation.
		 */
		virtual TMap<EVerifyError, int32> GetVerifyErrorCounts() const = 0;

		UE_DEPRECATED(4.21, "Please use GetMemoryChunkStoreSize instead.")
		virtual int32 GetInstallMemoryChunkStoreSize() const { return GetMemoryChunkStoreSize(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksInStore instead.")
		virtual int32 GetInstallMemoryChunksInStore() const { return GetMemoryChunksInStore(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksBooted instead.")
		virtual int32 GetInstallMemoryChunksBooted() const { return GetMemoryChunksBooted(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksRetained instead.")
		virtual int32 GetInstallMemoryChunksRetained() const { return GetMemoryChunksRetained(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunkStoreSize instead.")
		virtual int32 GetCloudMemoryChunkStoreSize() const { return GetMemoryChunkStoreSize(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksInStore instead.")
		virtual int32 GetCloudMemoryChunksInStore() const { return GetMemoryChunksInStore(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksBooted instead.")
		virtual int32 GetCloudMemoryChunksBooted() const { return GetMemoryChunksBooted(); }
		UE_DEPRECATED(4.21, "Please use GetMemoryChunksRetained instead.")
		virtual int32 GetCloudMemoryChunksRetained() const { return GetMemoryChunksRetained(); }
	};

	typedef TSharedPtr<IBuildStatistics> IBuildStatisticsPtr;
	typedef TSharedRef<IBuildStatistics> IBuildStatisticsRef;
}