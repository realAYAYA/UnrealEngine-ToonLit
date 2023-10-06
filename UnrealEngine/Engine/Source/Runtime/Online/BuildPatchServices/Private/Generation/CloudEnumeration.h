// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Common/StatsCollector.h"
#include "BuildPatchFeatureLevel.h"

namespace BuildPatchServices
{
	class ICloudEnumeration
	{
	public:
		virtual ~ICloudEnumeration(){}
		virtual bool IsComplete() const = 0;
		virtual const TSet<uint32>& GetUniqueWindowSizes() const = 0;
		virtual const TMap<uint64, TSet<FGuid>>& GetChunkInventory() const = 0;
		virtual const TMap<FGuid, int64>& GetChunkFileSizes() const = 0;
		virtual const TMap<FGuid, FSHAHash>& GetChunkShaHashes() const = 0;
		virtual const TMap<FGuid, uint32>& GetChunkWindowSizes() const = 0;
		virtual bool IsChunkFeatureLevelMatch(const FGuid& ChunkId) const = 0;
		virtual const uint64& GetChunkHash(const FGuid& ChunkId) const = 0;
		virtual const FSHAHash& GetChunkShaHash(const FGuid& ChunkId) const = 0;
		virtual const TMap<FSHAHash, TSet<FGuid>>& GetIdenticalChunks() const = 0;
	};

	class FCloudEnumerationFactory
	{
	public:
		static ICloudEnumeration* Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const EFeatureLevel& OutputFeatureLevel, FStatsCollector* StatsCollector);
	};
}
