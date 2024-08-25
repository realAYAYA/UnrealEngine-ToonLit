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

#if !UE_BUILD_SHIPPING

	/**
	 * If this is true, then the mass systems should strive to be as deterministic as possible, this will also enable the fixed random seed
	 * Currently maps to FApp::bUseFixedSeed
	 */
	MASSCOMMON_API bool IsDeterministic();

	/**
	 * If IsDeterministic() returns true, then this function will return the value of ai.massrepresentation.OverrideRandomSeed in place of InSeed
	 */
	MASSCOMMON_API int32 OverrideRandomSeedForTesting(int32 InSeed);

	/**
	 * If IsDeterministic() returns true, then this function will return the value of ai.massrepresentation.OverrideRandomSeed in place of FMath::Rand()
	 */
	MASSCOMMON_API int32 GenerateRandomSeed();

#else

	FORCEINLINE constexpr bool IsDeterministic() { return false; }

	FORCEINLINE int32 OverrideRandomSeedForTesting(int32 InSeed) { return InSeed; }

	FORCEINLINE int32 GenerateRandomSeed() { return FMath::Rand(); }
#endif

} // namespace UE::Mass::Utils