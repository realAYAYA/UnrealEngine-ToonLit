// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkSource.h"
#include "Installer/Controllable.h"
#include "Containers/Set.h"
#include "Common/SpeedRecorder.h"

class IBuildInstallerSharedContext;

namespace BuildPatchServices
{
	class IPlatform;
	class IFileSystem;
	class IChunkStore;
	class IChunkReferenceTracker;
	class IChunkDataSerialization;
	class IMessagePump;
	class IInstallerError;
	class IChunkDbChunkSourceStat;

	/**
	 * The interface for a chunkdb chunk source, which provides access to chunk data retrieved from chunkdb files.
	 */
	class IChunkDbChunkSource
		: public IChunkSource
		, public IControllable
	{
	public:
		virtual ~IChunkDbChunkSource() {}

		/**
		 * Get the set of chunks available in the chunkdbs which were provided to the source.
		 * @return the set of chunks available.
		 */
		virtual const TSet<FGuid>& GetAvailableChunks() const = 0;
	};

	/**
	 * A struct containing the configuration values for a chunkdb chunk source.
	 */
	struct FChunkDbSourceConfig
	{
		// An array of chunkdb full file paths.
		TArray<FString> ChunkDbFiles;
		// The minimum number of chunks to load ahead of what is required, depending on store slack.
		int32 PreFetchMinimum = 10;
		// The maximum number of chunks to load ahead of what is required, depending on store slack.
		int32 PreFetchMaximum = 40;
		// The time in seconds to wait until trying to open a chunkdb file again after we lost the file handle (e.g. due to device eject or network error).
		float ChunkDbOpenRetryTime = 5.0f;
		// If true, the loading will not begin until the first Get request is made. It is fairly fundamental to stop loading of chunks until resume
		// data is processed, but can be special case disabled.
		bool bBeginLoadsOnFirstGet = true;
		// The context for allocating shared resources.
		IBuildInstallerSharedContext* SharedContext = nullptr;

		/**
		 * Constructor which sets usual defaults, and takes params for values that cannot use a default.
		 * @param InChunkDbFiles    The chunkdb filename array.
		 */
		FChunkDbSourceConfig(TArray<FString> InChunkDbFiles)
			: ChunkDbFiles(MoveTemp(InChunkDbFiles))
		{}
	};

	/**
	 * A factory for creating an IChunkDbChunkSource instance.
	 */
	class FChunkDbChunkSourceFactory
	{
	public:
		/**
		 * This implementation will read chunks from provided chunkdbs if they are available. During initialization the provided chunkdb files
		 * are enumerated to find each available chunk.
		 * @param Configuration             The configuration struct for this source.
		 * @param Platform                  The platform access interface.
		 * @param FileSystem                The service used to open files.
		 * @param ChunkStore                The chunk store where loaded chunks will be put.
		 * @param ChunkReferenceTracker     The reference tracker for the installation, used to decide which chunks to fetch when loading.
		 * @param ChunkDataSerialization    Chunk data serialization implementation.
		 * @param MessagePump               The message pump to receive messages about source events.
		 * @param InstallerError            Error tracker where fatal errors will be reported.
		 * @param ChunkDbChunkSourceStat    The class to receive statistics information.
		 * @return the new IChunkDbChunkSource instance created.
		 */
		static IChunkDbChunkSource* Create(FChunkDbSourceConfig Configuration, IPlatform* Platform, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, IMessagePump* MessagePump, IInstallerError* InstallerError, IChunkDbChunkSourceStat* ChunkDbChunkSourceStat);
	};

	/**
	 * This interface defines the statistics class required by the chunkdb chunk source. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IChunkDbChunkSourceStat
	{
	public:
		/**
		 * Enum which describes success, or the reason for failure when loading a chunk.
		 */
		enum class ELoadResult : uint8
		{
			Success = 0,

			// The hash information was missing.
			MissingHashInfo,

			// The expected data hash for the chunk did not match.
			HashCheckFailed,

			// The chunkdb header specified an invalid chunk location offset or size.
			LocationOutOfBounds,

			// An unexpected error during serialization.
			SerializationError
		};

	public:
		virtual ~IChunkDbChunkSourceStat() {}

		/**
		 * Called when a batch of chunks are going to be loaded.
		 * @param ChunkIds  The ids of each chunk.
		 */
		virtual void OnBatchStarted(const TArray<FGuid>& ChunkIds) = 0;

		/**
		 * Called each time a chunk load begins.
		 * @param ChunkId   The id of the chunk.
		 */
		virtual void OnLoadStarted(const FGuid& ChunkId) = 0;

		/**
		 * Called each time a chunk load completes.
		 * @param ChunkId   The id of the chunk.
		 * @param Result    The load result.
		 * @param Record    The recording for the received data.
		 */
		virtual void OnLoadComplete(const FGuid& ChunkId, ELoadResult Result, const ISpeedRecorder::FRecord& Record) = 0;
	};
	
	/**
	 * A ToString implementation for IChunkDbChunkSourceStat::ELoadResult.
	 */
	const TCHAR* ToString(const IChunkDbChunkSourceStat::ELoadResult& LoadResult);
}