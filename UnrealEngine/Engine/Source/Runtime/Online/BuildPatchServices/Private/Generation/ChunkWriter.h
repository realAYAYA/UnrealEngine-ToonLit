// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	class IFileSystem;
	class IChunkDataSerialization;
	class FStatsCollector;

	struct FParallelChunkWriterConfig
	{
		int32 SaveRetryCount;
		float SaveRetryTime;
		int32 MaxQueueSize;
		int32 NumberOfThreads;
		FString ChunkDirectory;
		EFeatureLevel FeatureLevel;
	};

	struct FParallelChunkWriterSummaries
	{
		EFeatureLevel FeatureLevel;
		TMap<FGuid, int64> ChunkOutputSizes;
		TMap<FGuid, uint64> ChunkOutputHashes;
		TMap<FGuid, FSHAHash> ChunkOutputShas;
	};

	class IParallelChunkWriter
	{
	public:
		virtual ~IParallelChunkWriter() { }

		/**
		 * ...
		 */
		virtual void AddChunkData(TArray<uint8> ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash, const FSHAHash& ChunkSha) = 0;

		/**
		 * ...
		 */
		virtual FParallelChunkWriterSummaries OnProcessComplete() = 0;
	};

	class FParallelChunkWriterFactory
	{
	public:
		static IParallelChunkWriter* Create(FParallelChunkWriterConfig Config, IFileSystem* FileSystem, IChunkDataSerialization* ChunkDataSerialization, FStatsCollector* StatsCollector);
	};
}
