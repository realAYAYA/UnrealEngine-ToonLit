// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlBase.h"


/**
 * Failover controller (secondary node)
 */
class FDisplayClusterFailoverNodeCtrlSecondary
	: public FDisplayClusterFailoverNodeCtrlBase
{
public:
	FDisplayClusterFailoverNodeCtrlSecondary()
		: FDisplayClusterFailoverNodeCtrlBase()
	{ }

protected:
	// EDisplayClusterConfigurationFailoverPolicy::Disabled
	virtual void HandleCommResult_Disabled(EDisplayClusterCommResult CommResult) override;
	virtual void HandleNodeFailed_Disabled(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) override;

	// EDisplayClusterConfigurationFailoverPolicy::DropSecondaryNodesOnly
	virtual void HandleCommResult_DropSecondaryNodesOnly(EDisplayClusterCommResult CommResult) override;
	virtual void HandleNodeFailed_DropSecondaryNodesOnly(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) override;
};
