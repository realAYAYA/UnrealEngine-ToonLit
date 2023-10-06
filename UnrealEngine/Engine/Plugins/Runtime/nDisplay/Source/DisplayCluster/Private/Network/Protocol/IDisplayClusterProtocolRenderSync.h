// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/DisplayClusterNetworkTypes.h"


/**
 * Rendering synchronization protocol.
 * Used to synchronize frame presentation on RHI thread.
 */
class IDisplayClusterProtocolRenderSync
{
public:
	virtual ~IDisplayClusterProtocolRenderSync() = default;

public:
	// Synchronize RHI threads on a network barrier
	virtual EDisplayClusterCommResult SyncOnBarrier() = 0;
};
