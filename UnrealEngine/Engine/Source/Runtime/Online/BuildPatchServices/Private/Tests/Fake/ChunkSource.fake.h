// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Installer/ChunkSource.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class IChunkDataAccess;

	class FFakeChunkSource
		: public IChunkSource
	{
	public:
		virtual IChunkDataAccess* Get(const FGuid& DataId) override
		{
			TUniquePtr<IChunkDataAccess>* FindResult = ChunkDatas.Find(DataId);
			return FindResult ? FindResult->Get() : nullptr;
		}

		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override
		{
			return MoveTemp(NewRequirements);
		}

		bool AddRepeatRequirement(const FGuid& RepeatRequirement)
		{
			return false;
		}

		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override
		{
		}

	public:
		TMap<FGuid, TUniquePtr<IChunkDataAccess>> ChunkDatas;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
