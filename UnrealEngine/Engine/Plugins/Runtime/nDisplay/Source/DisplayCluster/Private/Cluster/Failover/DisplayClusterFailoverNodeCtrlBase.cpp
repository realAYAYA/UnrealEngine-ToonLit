// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlBase.h"

#include "DisplayClusterConfigurationTypes.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"

#include "Network/DisplayClusterNetworkTypes.h"

#include "HAL/IConsoleManager.h"


FDisplayClusterFailoverNodeCtrlBase::FDisplayClusterFailoverNodeCtrlBase()
	: FailoverPolicy( FDisplayClusterFailoverNodeCtrlBase::GetFailoverPolicyFromConfig() )
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Failover: policy %u"), static_cast<uint8>(FailoverPolicy));
}


void FDisplayClusterFailoverNodeCtrlBase::HandleCommResult(EDisplayClusterCommResult CommResult)
{
	UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Failover: CommResult - %u"), (uint32)CommResult);

	FScopeLock Lock(&InternalsCS);

	switch (GetFailoverPolicy())
	{
	case EDisplayClusterConfigurationFailoverPolicy::Disabled:
		return HandleCommResult_Disabled(CommResult);

	case EDisplayClusterConfigurationFailoverPolicy::DropSecondaryNodesOnly:
		return HandleCommResult_DropSecondaryNodesOnly(CommResult);
	}
}

void FDisplayClusterFailoverNodeCtrlBase::HandleNodeFailed(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType)
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Failover: Cluster node failed - node=%s, fail_type=%u"), *NodeId, static_cast<uint32>(NodeFailType));

	FScopeLock Lock(&InternalsCS);

	switch (GetFailoverPolicy())
	{
	case EDisplayClusterConfigurationFailoverPolicy::Disabled:
		return HandleNodeFailed_Disabled(NodeId, NodeFailType);

	case EDisplayClusterConfigurationFailoverPolicy::DropSecondaryNodesOnly:
		return HandleNodeFailed_DropSecondaryNodesOnly(NodeId, NodeFailType);
	}
}

EDisplayClusterConfigurationFailoverPolicy FDisplayClusterFailoverNodeCtrlBase::GetFailoverPolicyFromConfig()
{
	if (const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr())
	{
		if (const UDisplayClusterConfigurationData* const ConfigData = ConfigMgr->GetConfig())
		{
			return ConfigData->Cluster->Failover.FailoverPolicy;
		}
	}

	return EDisplayClusterConfigurationFailoverPolicy::Disabled;
}
