// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkSource.h"
#include "Installer/Controllable.h"
#include "Common/SpeedRecorder.h"
#include "BuildPatchManifest.h"


namespace BuildPatchServices
{
	class IChunkStore;
	class IChunkReferenceTracker;
	class IInstallerError;
	class IInstallChunkSourceStat;
	class IFileSystem;
	class IBuildManifestSet;

	/**
	 * The interface for an installation chunk source, which provides access to chunk data retrieved from known local installations.
	 */
	class IInstallChunkSource
		: public IChunkSource
		, public IControllable
	{
	public:
		virtual ~IInstallChunkSource() {}

		/**
		 * Get the set of chunks available locally which are relevant to the installation being performed.
		 * @return the set of chunks that should be available locally.
		 */
		virtual const TSet<FGuid>& GetAvailableChunks() const = 0;

		/**
		 * Given an installation file path, harvest all remaining required chunks from the file immediately.
		 * NB: The filepath must match a file contained in one of the installation sources provided in order to load chunks.
		 * @param FilePath  The full filepath to the file to harvest.
		 */
		virtual void HarvestRemainingChunksFromFile(const FString& FilePath) = 0;
	};

	/**
	 * A struct containing the configuration values for an install chunk source.
	 */
	struct FInstallSourceConfig
	{
		// A set of chunks to not retrieve unless specifically asked for.
		TSet<FGuid> ChunkIgnoreSet;
		// The minimum number of chunks to load at a time when one is requested, depending on store slack.
		int32 BatchFetchMinimum;
		// The maximum number of chunks to load at a time when one is requested, depending on store slack.
		int32 BatchFetchMaximum;

		/**
		 * Constructor which sets usual defaults.
		 */
		FInstallSourceConfig()
			: ChunkIgnoreSet()
			, BatchFetchMinimum(10)
			, BatchFetchMaximum(40)
		{
		}
	};

	/**
	 * A factory for creating an IInstallChunkSource instance.
	 */
	class FInstallChunkSourceFactory
	{
	public:
		/**
		 * This implementation will read chunks from provided local installations if they are available.
		 * During initialization the local installations are enumerated to find each available chunk and expected local files
		 * are checked and skipped if missing or incorrect size.
		 * @param Configuration             The configuration struct for this cloud source.
		 * @param FileSystem                The service used to open files on disk.
		 * @param ChunkStore                The chunk store where loaded chunks will be put.
		 * @param ChunkReferenceTracker     The reference tracker for the installation, used to decide which chunks to fetch when loading.
		 * @param InstallerError            Error tracker where fatal errors will be reported.
		 * @param InstallChunkSourceStat    The class to receive statistics and event information.
		 * @param InstallationSources       The map of known local installation directories and the associated manifest.
		 * @param InstallManifest           The manifest that chunks are required for.
		 * @return the new IInstallChunkSource instance created.
		 */
		static IInstallChunkSource* Create(FInstallSourceConfig Configuration, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallChunkSourceStat* InstallChunkSourceStat, const TMultiMap<FString, FBuildPatchAppManifestRef>& InstallationSources, IBuildManifestSet* ManifestSet);
	};

	/**
	 * This interface defines the statistics class required by the install chunk source. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IInstallChunkSourceStat
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

			// Chunk part information was missing.
			MissingPartInfo,

			// Failed to open a source file.
			OpenFileFail,

			// The expected source file size was not matched.
			IncorrectFileSize,

			// The expected data hash for the chunk did not match.
			HashCheckFailed,

			// The process has been aborted.
			Aborted
		};

	public:
		virtual ~IInstallChunkSourceStat() {}

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
		virtual void OnLoadComplete(const FGuid& ChunkId, const ELoadResult& Result, const ISpeedRecorder::FRecord& Record) = 0;

		/**
		 * Called when a batch of chunks are added and accepted via IChunkSource::AddRuntimeRequirements.
		 * @param ChunkIds  The ids of each chunk.
		 */
		virtual void OnAcceptedNewRequirements(const TSet<FGuid>& ChunkIds) = 0;
	};
	
	/**
	 * A ToString implementation for IInstallChunkSourceStat::ELoadResult.
	 */
	const TCHAR* ToString(const IInstallChunkSourceStat::ELoadResult& LoadResult);
}