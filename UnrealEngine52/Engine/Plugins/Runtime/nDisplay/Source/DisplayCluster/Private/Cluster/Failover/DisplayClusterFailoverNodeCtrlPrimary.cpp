// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlPrimary.h"
#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterTypesConverterPrivate.h"

#include "Network/DisplayClusterNetworkTypes.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// EDisplayClusterConfigurationFailoverPolicy::Disabled
////////////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterFailoverNodeCtrlPrimary::HandleCommResult_Disabled(EDisplayClusterCommResult CommResult)
{
	// Any comm error on the primary node results in node termination
	if (CommResult != EDisplayClusterCommResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FString::Printf(TEXT("Failover: requesting application exit. CommError=%u (%s)"), static_cast<uint32>(CommResult), *DisplayClusterTypesConverter::ToString(CommResult)));
	}
}

void FDisplayClusterFailoverNodeCtrlPrimary::HandleNodeFailed_Disabled(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType)
{
	// Any error on the primary node results in node termination
	switch (NodeFailType)
	{
	case IDisplayClusterServer::ENodeFailType::ConnectionLost:
		FDisplayClusterAppExit::ExitApplication(FString::Printf(TEXT("Failover: requesting application exit. Node [%s] has disconnected"), *NodeId));
		break;

	case IDisplayClusterServer::ENodeFailType::BarrierTimeOut:
		FDisplayClusterAppExit::ExitApplication(FString::Printf(TEXT("Failover: requesting application exit. Node [%s] timed out on a barrier"), *NodeId));
		break;

	default:
		FDisplayClusterAppExit::ExitApplication(FString("Unknown error type (processing not implemented)"));
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// EDisplayClusterConfigurationFailoverPolicy::DropSecondaryNodesOnly
////////////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterFailoverNodeCtrlPrimary::HandleCommResult_DropSecondaryNodesOnly(EDisplayClusterCommResult CommResult)
{
	// Any comm error on the primary node results in node termination
	if (CommResult != EDisplayClusterCommResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FString::Printf(TEXT("Failover: requesting application exit. CommError=%u (%s)"), static_cast<uint32>(CommResult), *DisplayClusterTypesConverter::ToString(CommResult)));
	}
}

void FDisplayClusterFailoverNodeCtrlPrimary::HandleNodeFailed_DropSecondaryNodesOnly(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType)
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Failover: node [%s] failed, fail_type=%u, disconnecting the node and continue working..."), *NodeId, static_cast<uint32>(NodeFailType));

	// Just allow a secondary node to go away, but not terminate the whole cluster.
	switch (NodeFailType)
	{
	case IDisplayClusterServer::ENodeFailType::ConnectionLost:
	case IDisplayClusterServer::ENodeFailType::BarrierTimeOut:
	default:
		// @note
		// The node should have already been dropped on the cluster controller level. However,
		// the logic might be changed in the future. Anyway, it's safe to call DropClusterNode twice.
		GDisplayCluster->GetPrivateClusterMgr()->DropClusterNode(NodeId);
	}

	// Notify listeners
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterFailoverNodeDown().Broadcast(NodeId);
}
