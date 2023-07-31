// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"


/**
 * Failover node controller (nullptr replacement)
 */
class FDisplayClusterFailoverNodeCtrlNull
	: public IDisplayClusterFailoverNodeController
{
public:
	// Returns current failover policy
	virtual EDisplayClusterConfigurationFailoverPolicy GetFailoverPolicy() const
	{
		return EDisplayClusterConfigurationFailoverPolicy::Disabled;
	}

public:
	// Handles in-cluster communication results
	virtual void HandleCommResult(EDisplayClusterCommResult CommResult) override
	{ }

	// Handles node failure
	virtual void HandleNodeFailed(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) override
	{ }
};
