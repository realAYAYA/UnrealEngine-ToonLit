// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/DisplayClusterNetworkTypes.h"


/**
 * Generic barriers protocol.
 * Provides generic barrier sync mechanism.
 */
class IDisplayClusterProtocolGenericBarrier
{
public:
	virtual ~IDisplayClusterProtocolGenericBarrier() = default;

public:
	// Just a union of different barrier control operation results to simplify management API
	enum class EBarrierControlResult : uint8
	{
		CreatedSuccessfully = 0,
		AlreadyExists,
		NotFound,
		ReleasedSuccessfully,
		SynchronizedSuccessfully,
		UnknownError
	};

public:
	// Creates new barrier
	virtual EDisplayClusterCommResult CreateBarrier(const FString& BarrierId, const TArray<FString>& UniqueThreadMarkers, uint32 Timeout, EBarrierControlResult& Result) = 0;

	// Wait until a barrier with specific ID is created and ready to go
	virtual EDisplayClusterCommResult WaitUntilBarrierIsCreated(const FString& BarrierId, EBarrierControlResult& Result) = 0;

	// Checks if a specific barrier exists
	virtual EDisplayClusterCommResult IsBarrierAvailable(const FString& BarrierId, EBarrierControlResult& Result) = 0;

	// Releases specific barrier
	virtual EDisplayClusterCommResult ReleaseBarrier(const FString& BarrierId, EBarrierControlResult& Result) = 0;

	// Synchronize calling thread on a specific barrier
	virtual EDisplayClusterCommResult SyncOnBarrier(const FString& BarrierId, const FString& UniqueThreadMarker, EBarrierControlResult& Result) = 0;

	// Synchronize calling thread on a specific barrier (with custom data)
	virtual EDisplayClusterCommResult SyncOnBarrierWithData(const FString& BarrierId, const FString& UniqueThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData, EBarrierControlResult& Result) = 0;
};
