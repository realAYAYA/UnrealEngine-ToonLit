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

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"


FDisplayClusterRenderSyncService::FDisplayClusterRenderSyncService()
	: FDisplayClusterService(FString("SRV_RS"))
{
	// Get list of cluster node IDs
	TArray<FString> NodeIds;
	GDisplayCluster->GetPrivateClusterMgr()->GetNodeIds(NodeIds);

	// Network settings from config data
	const FDisplayClusterConfigurationNetworkSettings& NetworkSettings = GDisplayCluster->GetConfigMgr()->GetConfig()->Cluster->Network;

	// Instantiate service barriers
	BarrierSwap = FDisplayClusterBarrierFactory::CreateBarrier(NodeIds, NetworkSettings.RenderSyncBarrierTimeout, TEXT("RenderSync_barrier"));

	// Put the barriers into an aux container
	ServiceBarriers.Emplace(BarrierSwap->GetName(), &BarrierSwap);

	// Subscribe for barrier timeout events
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->OnBarrierTimeout().AddRaw(this, &FDisplayClusterRenderSyncService::ProcessBarrierTimeout);
	}

	// Subscribe for SessionClosed events
	OnSessionClosed().AddRaw(this, &FDisplayClusterRenderSyncService::ProcessSessionClosed);
}

FDisplayClusterRenderSyncService::~FDisplayClusterRenderSyncService()
{
	// Unsubscribe from barrier timeout events
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->OnBarrierTimeout().RemoveAll(this);
	}

	// Unsubscribe from SessionClosed notifications
	OnSessionClosed().RemoveAll(this);

	Shutdown();
}


bool FDisplayClusterRenderSyncService::Start(const FString& Address, const uint16 Port)
{
	// Activate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Activate();
	}

	return FDisplayClusterServer::Start(Address, Port);
}

bool FDisplayClusterRenderSyncService::Start(TSharedPtr<FDisplayClusterTcpListener>& ExternalListener)
{
	// Activate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Activate();
	}

	return FDisplayClusterServer::Start(ExternalListener);
}

void FDisplayClusterRenderSyncService::Shutdown()
{
	// Deactivate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Deactivate();
	}

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
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->UnregisterSyncNode(NodeId);
	}

	// Now do the session related job
	FDisplayClusterServer::KillSession(NodeId);
}

TSharedPtr<IDisplayClusterSession> FDisplayClusterRenderSyncService::CreateSession(FDisplayClusterSessionInfo& SessionInfo)
{
	SessionInfo.SessionName = FString::Printf(TEXT("%s_session_%lu_%s"), *GetName(), SessionInfo.SessionId, *SessionInfo.Endpoint.ToString());
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
			for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
			{
				(*BarrierIt.Value)->UnregisterSyncNode(NodeId);
			}

			// Notify others about node fail
			OnNodeFailed().Broadcast(NodeId, ENodeFailType::ConnectionLost);
		}
	}
}

// Callbck on barrier timeout
void FDisplayClusterRenderSyncService::ProcessBarrierTimeout(const FString& BarrierName, const TArray<FString>& NodesTimedOut)
{
	// We have to unregister the node that just timed out from all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		for (const FString& NodeId : NodesTimedOut)
		{
			(*BarrierIt.Value)->UnregisterSyncNode(NodeId);
		}
	}

	// Notify others about timeout
	for (const FString& NodeId : NodesTimedOut)
	{
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
	if (Request->GetName().Equals(DisplayClusterRenderSyncStrings::WaitForSwapSync::Name, ESearchCase::IgnoreCase))
	{
		const EDisplayClusterCommResult CommResult = WaitForSwapSync();
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
EDisplayClusterCommResult FDisplayClusterRenderSyncService::WaitForSwapSync()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_RS::WaitForSwapSync);

	const FString CachedNodeId = GetSessionInfoCache().NodeId.Get("");
	BarrierSwap->Wait(CachedNodeId);
	return EDisplayClusterCommResult::Ok;
}
