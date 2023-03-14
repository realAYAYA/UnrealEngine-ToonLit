// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "DisplayClusterConfigurationTypes_Enums.h"

struct FDisplayClusterSessionInfo;


/**
 * Base failover node controller class
 */
class FDisplayClusterFailoverNodeCtrlBase
	: public IDisplayClusterFailoverNodeController
{
public:
	FDisplayClusterFailoverNodeCtrlBase();

public:
	virtual EDisplayClusterConfigurationFailoverPolicy GetFailoverPolicy() const override final
	{
		return FailoverPolicy;
	}

public:
	// Handles in-cluster communication results
	virtual void HandleCommResult(EDisplayClusterCommResult CommResult) override final;

	// Handles node failure
	virtual void HandleNodeFailed(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) override final;

protected:
	// EDisplayClusterConfigurationFailoverPolicy::Disabled
	virtual void HandleCommResult_Disabled(EDisplayClusterCommResult CommResult) = 0;
	virtual void HandleNodeFailed_Disabled(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) = 0;

	// EDisplayClusterConfigurationFailoverPolicy::DropSecondaryNodesOnly
	virtual void HandleCommResult_DropSecondaryNodesOnly(EDisplayClusterCommResult CommResult) = 0;
	virtual void HandleNodeFailed_DropSecondaryNodesOnly(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType) = 0;

private:
	// Converts failover policy from CVar integer into enum
	static EDisplayClusterConfigurationFailoverPolicy GetFailoverPolicyFromConfig();

private:
	// Current failover policy
	const EDisplayClusterConfigurationFailoverPolicy FailoverPolicy;

	mutable FCriticalSection InternalsCS;
};
