// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterEnums.h"
#include "Misc/Optional.h"
#include "Misc/QualifiedFrameTime.h"

#include "Cluster/DisplayClusterClusterEvent.h"
#include "Network/DisplayClusterNetworkTypes.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"


/**
 * Cluster synchronization protocol. Used to synchronize/replicate any
 * DisplayCluster data on the game thread.
 */
class IDisplayClusterProtocolClusterSync
{
public:
	virtual ~IDisplayClusterProtocolClusterSync() = default;

public:
	// Game start barrier
	virtual EDisplayClusterCommResult WaitForGameStart() = 0;

	// Frame start barrier
	virtual EDisplayClusterCommResult WaitForFrameStart() = 0;

	// Frame end barrier
	virtual EDisplayClusterCommResult WaitForFrameEnd() = 0;

	// Engine time
	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) = 0;

	// Sync objects
	virtual EDisplayClusterCommResult GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData) = 0;

	// Sync events
	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) = 0;

	// Sync native UE input
	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) = 0;
};
