// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlBase.h"


/**
 * Failover controller (primary node)
 */
class FDisplayClusterFailoverNodeCtrlPrimary
	: public FDisplayClusterFailoverNodeCtrlBase
{
public:
	FDisplayClusterFailoverNodeCtrlPrimary()
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
