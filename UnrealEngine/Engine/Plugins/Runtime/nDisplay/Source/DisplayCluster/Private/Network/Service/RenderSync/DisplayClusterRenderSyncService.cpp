// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/RenderSync/DisplayClusterRenderSyncService.h"
#include "Network/Service/RenderSync/DisplayClusterRenderSyncStrings.h"
#include "Network/Barrier/DisplayClusterBarrierFactory.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "Network/Listener/DisplayClusterTcpListener.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Session/DisplayClusterSession.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterRenderSyncService::FDisplayClusterRenderSyncService()
	: FDisplayClusterService(FString("SRV_RS"))
{
	// Perform barriers initialization depending on current circumstances
	InitializeBarriers();
	// Subscribe for SessionClosed events
	OnSessionClosed().AddRaw(this, &FDisplayClusterRenderSyncService::ProcessSessionClosed);
}

FDisplayClusterRenderSyncService::~FDisplayClusterRenderSyncService()
{
	// Unsubscribe from barrier timeout events
	UnsubscribeFromAllBarrierEvents();
	// Unsubscribe from SessionClosed notifications
	OnSessionClosed().RemoveAll(this);

	Shutdown();
}


bool FDisplayClusterRenderSyncService::Start(const FString& Address, const uint16 Port)
{
	// Activate all barriers
	ActivateAllBarriers();

	return FDisplayClusterServer::Start(Address, Port);
}

bool FDisplayClusterRenderSyncService::Start(TSharedPtr<FDisplayClusterTcpListener>& ExternalListener)
{
	// Activate all barriers
	ActivateAllBarriers();

	return FDisplayClusterServer::Start(ExternalListener);
}

void FDisplayClusterRenderSyncService::Shutdown()
{
	// Deactivate all barriers
	DeactivateAllBarriers();

	return FDisplayClusterServer::Shutdown();
}

FString FDisplayClusterRenderSyncService::GetProtocolName() const
{
	static const FString ProtocolName(DisplayClusterRenderSyncStrings::ProtocolName);
	return ProtocolName;
}

void FDisplayClusterRenderSyncService::KillSession(const FString& NodeId)
{
	// Before killing the session on the parent level, we need to unregister this node from the barriers
	UnregisterClusterNode(NodeId);

	// Now do the session related job
	FDisplayClusterServer::KillSession(NodeId);
}

TSharedPtr<IDisplayClusterSession> FDisplayClusterRenderSyncService::CreateSession(FDisplayClusterSessionInfo& SessionInfo)
{
	SessionInfo.SessionName = FString::Printf(TEXT("%s_%lu_%s_%s"),
		*GetName(),
		SessionInfo.SessionId,
		*SessionInfo.Endpoint.ToString(),
		*SessionInfo.NodeId.Get(TEXT("(na)"))
	);

	return MakeShared<FDisplayClusterSession<FDisplayClusterPacketInternal, true>>(SessionInfo, *this, *this, FDisplayClusterService::GetThreadPriority());
}

void FDisplayClusterRenderSyncService::ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo)
{
	if (!SessionInfo.IsTerminatedByServer())
	{
		// Get node ID
		const FString NodeId = SessionInfo.NodeId.Get(FString());

		if (!NodeId.IsEmpty())
		{
			// We have to unregister the node that just disconnected from all barriers
			UnregisterClusterNode(NodeId);

			// Notify others about node fail
			OnNodeFailed().Broadcast(NodeId, ENodeFailType::ConnectionLost);
		}
	}
}

// Callbck on barrier timeout
void FDisplayClusterRenderSyncService::ProcessBarrierTimeout(const FString& BarrierName, const TArray<FString>& NodesTimedOut)
{
	for (const FString& NodeId : NodesTimedOut)
	{
		// We have to unregister the node that just timed out from all barriers
		UnregisterClusterNode(NodeId);

		// Notify others about timeout
		OnNodeFailed().Broadcast(NodeId, ENodeFailType::BarrierTimeOut);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionPacketHandler
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<FDisplayClusterPacketInternal> FDisplayClusterRenderSyncService::ProcessPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Request, const FDisplayClusterSessionInfo& SessionInfo)
{
	// Check the pointer
	if (!Request.IsValid())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Invalid request data (nullptr)"), *GetName());
		return nullptr;
	}

	// Cache session info
	SetSessionInfoCache(SessionInfo);

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing packet: %s"), *GetName(), *Request->ToLogString());

	// Check protocol and type
	if (Request->GetProtocol() != DisplayClusterRenderSyncStrings::ProtocolName || Request->GetType() != DisplayClusterRenderSyncStrings::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Unsupported packet type: %s"), *GetName(), *Request->ToLogString());
		return nullptr;
	}

	// Initialize response packet
	TSharedPtr<FDisplayClusterPacketInternal> Response = MakeShared<FDisplayClusterPacketInternal>(Request->GetName(), DisplayClusterRenderSyncStrings::TypeResponse, Request->GetProtocol());

	// Dispatch the packet
	if (Request->GetName().Equals(DisplayClusterRenderSyncStrings::SyncOnBarrier::Name, ESearchCase::IgnoreCase))
	{
		// Process command
		const EDisplayClusterCommResult CommResult = SyncOnBarrier();
		Response->SetCommResult(CommResult);

		return Response;
	}

	// Being here means that we have no appropriate dispatch logic for this packet
	UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s - No dispatcher found for packet '%s'"), *GetName(), *Request->GetName());

	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolRenderSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterRenderSyncService::SyncOnBarrier()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_RS::SyncOnBarrier);
	
	const FString NodeId = GetSessionInfoCache().NodeId.Get("");

	IDisplayClusterBarrier* Barrier = GetBarrierForNode(NodeId);
	if (ensureMsgf(Barrier, TEXT("%s could not find a barrier for node '%s'"), *GetName(), *NodeId))
	{
		Barrier->Wait(NodeId);
	}

	return EDisplayClusterCommResult::Ok;
}

void FDisplayClusterRenderSyncService::InitializeBarriers()
{
	// Get configuration data
	const UDisplayClusterConfigurationData* const Config = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!Config)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - No configuartion data, can't initialize barriers"), *GetName());
		return;
	}

	// Get list of cluster node IDs (runtime nodes)
	TArray<FString> RuntimeNodeIds;
	GDisplayCluster->GetPrivateClusterMgr()->GetNodeIds(RuntimeNodeIds);

	// Filter nodes that are expected to present in runtime
	const TMap<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>> RuntimeNodeCfgs = Config->Cluster->Nodes.FilterByPredicate(
		[RuntimeNodeIds](const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Pair)
		{
			return RuntimeNodeIds.Contains(Pair.Key);
		});

	// Build sync groups (this one is an aux mapping of policy IDs to their node IDs)
	TMap<FString, TArray<FString>> SyncGroups;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& RuntimeNodeCfg : RuntimeNodeCfgs)
	{
		FString SyncPolicyId;

		// Headless nodes always use the same policy
		if (RuntimeNodeCfg.Value->bRenderHeadless)
		{
			SyncPolicyId = DisplayClusterConfigurationStrings::config::cluster::render_sync::HeadlessRenderingSyncPolicy;
		}
#if 0 // Not implemented yet
		// Individual sync policy
		else if(NodeCfgIt.Value->bOverrideSyncPolicy)
		{
			SyncPolicyId = NodeCfgIt.Value->Sync.RenderSyncPolicy.Type;
		}
#endif
		// Then default sync policy
		else
		{
			SyncPolicyId = Config->Cluster->Sync.RenderSyncPolicy.Type;
		}

		// To prevent any case related issues caused by manual .ndisplay file editing. We won't
		// need it once we refactor all the IDs to be FName's.
		SyncPolicyId.ToLowerInline();

		// Create new sync group if not yet available
		TArray<FString>* NodesInSyncGroup = SyncGroups.Find(SyncPolicyId);
		if (!NodesInSyncGroup)
		{
			NodesInSyncGroup = &SyncGroups.Emplace(SyncPolicyId);
		}

		// Assign this cluster node to the specific sync group
		NodesInSyncGroup->Add(RuntimeNodeCfg.Key);
		// Update node-policy mapping
		NodeToPolicyMap.Emplace(RuntimeNodeCfg.Key, SyncPolicyId);
	}

	// Finally, initialize barriers for every sync group
	for (const TPair<FString, TArray<FString>>& SyncGroup : SyncGroups)
	{
		const FString BarrierName = SyncGroup.Key + TEXT("_barrier");

		// Instantiate the barrier
		if (IDisplayClusterBarrier* const Barrier = FDisplayClusterBarrierFactory::CreateBarrier(BarrierName, SyncGroup.Value, Config->Cluster->Network.RenderSyncBarrierTimeout))
		{
			// Subscribe for barrier timeout events
			Barrier->OnBarrierTimeout().AddRaw(this, &FDisplayClusterRenderSyncService::ProcessBarrierTimeout);
			// Store barrier instance
			PolicyToBarrierMap.Emplace(SyncGroup.Key, Barrier);
		}
	}
}

void FDisplayClusterRenderSyncService::ActivateAllBarriers()
{
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>>& PolicyToBarrierIt : PolicyToBarrierMap)
	{
		PolicyToBarrierIt.Value->Activate();
	}
}

void FDisplayClusterRenderSyncService::DeactivateAllBarriers()
{
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>>& PolicyToBarrierIt : PolicyToBarrierMap)
	{
		PolicyToBarrierIt.Value->Deactivate();
	}
}

void FDisplayClusterRenderSyncService::UnsubscribeFromAllBarrierEvents()
{
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>>& PolicyToBarrierIt : PolicyToBarrierMap)
	{
		PolicyToBarrierIt.Value->OnBarrierTimeout().RemoveAll(this);
	}
}

IDisplayClusterBarrier* FDisplayClusterRenderSyncService::GetBarrierForNode(const FString& NodeId) const
{
	if (const FString* const Policy = NodeToPolicyMap.Find(NodeId))
	{
		if (const TUniquePtr<IDisplayClusterBarrier>* const Barrier = PolicyToBarrierMap.Find(*Policy))
		{
			return Barrier->Get();
		}
	}

	return nullptr;
}

void FDisplayClusterRenderSyncService::UnregisterClusterNode(const FString& NodeId)
{
	if (IDisplayClusterBarrier* Barrier = GetBarrierForNode(NodeId))
	{
		Barrier->UnregisterSyncCaller(NodeId);
	}
}
