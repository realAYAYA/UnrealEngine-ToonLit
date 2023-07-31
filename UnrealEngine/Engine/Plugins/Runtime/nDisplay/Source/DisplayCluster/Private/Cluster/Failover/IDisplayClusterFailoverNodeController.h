// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/DisplayClusterNetworkTypes.h"
#include "Network/IDisplayClusterServer.h"
#include "DisplayClusterConfigurationTypes_Enums.h"


/**
 * Failover controller interface
 */
class IDisplayClusterFailoverNodeController
{
public:
	virtual ~IDisplayClusterFailoverNodeController() = default;

public:
	// Returns current failover policy
	virtual EDisplayClusterConfigurationFailoverPolicy GetFailoverPolicy() const = 0;

public:
	// Handles in-cluster communication results
	virtual void HandleCommResult(EDisplayClusterCommResult CommResult) = 0;

	// Handles node failure
	virtual void HandleNodeFailed(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) = 0;
};
