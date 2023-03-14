// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonTypes.h"
#include "Containers/Array.h"
#include "Containers/Queue.h"

namespace UE::Mass::Utils
{
	/** 
	 * Creates a TArray of entities based on the given EntitiesQueue. Note that it's the caller's responsibility to 
	 * ensure EntitiesCount > 0, otherwise the function will fail a check (with explosive results).
	 */
	MASSCOMMON_API TArray<FMassEntityHandle> EntityQueueToArray(TQueue<FMassEntityHandle, EQueueMode::Mpsc>& EntitiesQueue, const int32 EntitiesCount);
} // namespace UE::Mass::Utils