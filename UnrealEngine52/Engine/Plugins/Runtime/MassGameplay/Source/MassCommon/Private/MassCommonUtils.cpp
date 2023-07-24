// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCommonUtils.h"

namespace UE::Mass::Utils
{
	TArray<FMassEntityHandle> EntityQueueToArray(TQueue<FMassEntityHandle, EQueueMode::Mpsc>& EntitiesQueue, const int32 EntitiesCount)
	{
		check(EntitiesCount > 0);
		TArray<FMassEntityHandle> EntitiesArray;
		EntitiesArray.AddUninitialized(EntitiesCount);

		FMassEntityHandle TempEntity;
		uint32 CurrentIndex = 0;
		while (EntitiesQueue.Dequeue(TempEntity))
		{
			EntitiesArray[CurrentIndex++] = TempEntity;
		}
		ensure(CurrentIndex == EntitiesCount);

		return MoveTemp(EntitiesArray);
	}
} // namespace UE::Mass::Utils