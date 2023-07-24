// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterSync/DisplayClusterClusterSyncService.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncStrings.h"
#include "Network/Barrier/DisplayClusterBarrierFactory.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"
#include "Network/Listener/DisplayClusterTcpListener.h"
#include "Network/Session/DisplayClusterSession.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"
#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Game/IPDisplayClusterGameManager.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/QualifiedFrameTime.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterEnums.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"


FDisplayClusterClusterSyncService::FDisplayClusterClusterSyncService()
	: FDisplayClusterService(FString("SRV_CS"))
{
	// Get list of cluster node IDs
	TArray<FString> NodeIds;
	GDisplayCluster->GetPrivateClusterMgr()->GetNodeIds(NodeIds);

	// Network settings from config data
	const FDisplayClusterConfigurationNetworkSettings& NetworkSettings = GDisplayCluster->GetConfigMgr()->GetConfig()->Cluster->Network;

	// Instantiate service barriers
	BarrierGameStart  = FDisplayClusterBarrierFactory::CreateBarrier(NodeIds, NetworkSettings.GameStartBarrierTimeout,  TEXT("GameStart_barrier"));
	BarrierFrameStart = FDisplayClusterBarrierFactory::CreateBarrier(NodeIds, NetworkSettings.FrameStartBarrierTimeout, TEXT("FrameStart_barrier"));
	BarrierFrameEnd   = FDisplayClusterBarrierFactory::CreateBarrier(NodeIds, NetworkSettings.FrameEndBarrierTimeout,   TEXT("FrameEnd_barrier"));

	// Put the barriers into an aux container
	ServiceBarriers.Emplace(BarrierGameStart->GetName(),  &BarrierGameStart);
	ServiceBarriers.Emplace(BarrierFrameStart->GetName(), &BarrierFrameStart);
	ServiceBarriers.Emplace(BarrierFrameEnd->GetName(),   &BarrierFrameEnd);

	// Subscribe for barrier timeout events
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->OnBarrierTimeout().AddRaw(this, &FDisplayClusterClusterSyncService::ProcessBarrierTimeout);
	}

	// Subscribe for SessionClosed events
	OnSessionClosed().AddRaw(this, &FDisplayClusterClusterSyncService::ProcessSessionClosed);
}

FDisplayClusterClusterSyncService::~FDisplayClusterClusterSyncService()
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


bool FDisplayClusterClusterSyncService::Start(const FString& Address, const uint16 Port)
{
	// Activate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Activate();
	}

	return FDisplayClusterServer::Start(Address, Port);
}

bool FDisplayClusterClusterSyncService::Start(TSharedPtr<FDisplayClusterTcpListener>& ExternalListener)
{
	// Activate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Activate();
	}

	return FDisplayClusterServer::Start(ExternalListener);
}

void FDisplayClusterClusterSyncService::Shutdown()
{
	// Deactivate all barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->Deactivate();
	}

	return FDisplayClusterServer::Shutdown();
}

FString FDisplayClusterClusterSyncService::GetProtocolName() const
{
	static const FString ProtocolName(DisplayClusterClusterSyncStrings::ProtocolName);
	return ProtocolName;
}

void FDisplayClusterClusterSyncService::KillSession(const FString& NodeId)
{
	// Before killing the session on the parent level, we need to unregister this node from the barriers
	for (TPair<FString, TUniquePtr<IDisplayClusterBarrier>*>& BarrierIt : ServiceBarriers)
	{
		(*BarrierIt.Value)->UnregisterSyncNode(NodeId);
	}

	// Now do the session related job
	FDisplayClusterServer::KillSession(NodeId);
}

TSharedPtr<IDisplayClusterSession> FDisplayClusterClusterSyncService::CreateSession(FDisplayClusterSessionInfo& SessionInfo)
{
	SessionInfo.SessionName = FString::Printf(TEXT("%s_session_%lu_%s"), *GetName(), SessionInfo.SessionId, *SessionInfo.Endpoint.ToString());
	return MakeShared<FDisplayClusterSession<FDisplayClusterPacketInternal, true>>(SessionInfo, *this, *this, FDisplayClusterService::GetThreadPriority());
}

void FDisplayClusterClusterSyncService::ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo)
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

void FDisplayClusterClusterSyncService::ProcessBarrierTimeout(const FString& BarrierName, const TArray<FString>& NodesTimedOut)
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
TSharedPtr<FDisplayClusterPacketInternal> FDisplayClusterClusterSyncService::ProcessPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Request, const FDisplayClusterSessionInfo& SessionInfo)
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
	if (Request->GetProtocol() != DisplayClusterClusterSyncStrings::ProtocolName || Request->GetType() != DisplayClusterClusterSyncStrings::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Unsupported packet type: %s"), *GetName(), *Request->ToLogString());
		return nullptr;
	}

	// Initialize response packet
	TSharedPtr<FDisplayClusterPacketInternal> Response = MakeShared<FDisplayClusterPacketInternal>(Request->GetName(), DisplayClusterClusterSyncStrings::TypeResponse, Request->GetProtocol());

	// Dispatch the packet
	const FString& ReqName = Request->GetName();
	if (ReqName.Equals(DisplayClusterClusterSyncStrings::WaitForGameStart::Name, ESearchCase::IgnoreCase))
	{
		const EDisplayClusterCommResult CommResult = WaitForGameStart();
		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::WaitForFrameStart::Name, ESearchCase::IgnoreCase))
	{
		const EDisplayClusterCommResult CommResult = WaitForFrameStart();
		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::WaitForFrameEnd::Name, ESearchCase::IgnoreCase))
	{
		const EDisplayClusterCommResult CommResult = WaitForFrameEnd();
		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::GetTimeData::Name, ESearchCase::IgnoreCase))
	{
		double DeltaTime = 0.0f;
		double GameTime  = 0.0f;
		TOptional<FQualifiedFrameTime> FrameTime;

		const EDisplayClusterCommResult CommResult = GetTimeData(DeltaTime, GameTime, FrameTime);

		// Convert to hex strings
		const FString StrDeltaTime = DisplayClusterTypesConverter::template ToHexString<double>(DeltaTime);
		const FString StrGameTime  = DisplayClusterTypesConverter::template ToHexString<double>(GameTime);

		Response->SetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgDeltaTime,        StrDeltaTime);
		Response->SetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgGameTime,         StrGameTime);
		Response->SetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgIsFrameTimeValid, FrameTime.IsSet());
		
		if (FrameTime.IsSet())
		{
			Response->SetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgFrameTime, FrameTime.GetValue());
		}
		else
		{
			Response->RemoveTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetTimeData::ArgFrameTime);
		}

		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::GetObjectsData::Name, ESearchCase::IgnoreCase))
	{
		TMap<FString, FString> ObjectsData;
		uint8 SyncGroupNum = 0;
		Request->GetTextArg(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, DisplayClusterClusterSyncStrings::GetObjectsData::ArgSyncGroup, SyncGroupNum);
		EDisplayClusterSyncGroup SyncGroup = (EDisplayClusterSyncGroup)SyncGroupNum;

		const EDisplayClusterCommResult CommResult = GetObjectsData(SyncGroup, ObjectsData);

		Response->SetTextArgs(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, ObjectsData);

		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::GetEventsData::Name, ESearchCase::IgnoreCase))
	{
		TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>   JsonEvents;
		TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>> BinaryEvents;
		
		const EDisplayClusterCommResult CommResult = GetEventsData(JsonEvents, BinaryEvents);

		DisplayClusterNetworkDataConversion::JsonEventsToInternalPacket(JsonEvents, Response);
		DisplayClusterNetworkDataConversion::BinaryEventsToInternalPacket(BinaryEvents, Response);

		Response->SetCommResult(CommResult);

		return Response;
	}
	else if (ReqName.Equals(DisplayClusterClusterSyncStrings::GetNativeInputData::Name, ESearchCase::IgnoreCase))
	{
		TMap<FString, FString> NativeInputData;

		const EDisplayClusterCommResult CommResult = GetNativeInputData(NativeInputData);

		Response->SetTextArgs(DisplayClusterClusterSyncStrings::ArgumentsDefaultCategory, NativeInputData);

		Response->SetCommResult(CommResult);

		return Response;
	}

	// Being here means that we have no appropriate dispatch logic for this packet
	UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s - No dispatcher found for packet '%s'"), *GetName(), *Request->GetName());

	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolClusterSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterSyncService::WaitForGameStart()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::WaitForGameStart);

	const FString CachedNodeId = GetSessionInfoCache().NodeId.Get("");
	BarrierGameStart->Wait(CachedNodeId);
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::WaitForFrameStart()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::WaitForFrameStart);

	const FString CachedNodeId = GetSessionInfoCache().NodeId.Get("");
	BarrierFrameStart->Wait(CachedNodeId);
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::WaitForFrameEnd()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::WaitForFrameEnd);

	const FString CachedNodeId = GetSessionInfoCache().NodeId.Get("");
	BarrierFrameEnd->Wait(CachedNodeId);
	return EDisplayClusterCommResult::Ok;
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::GetTimeData);
	return GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->GetTimeData(OutDeltaTime, OutGameTime, OutFrameTime);
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::GetObjectsData(EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::GetObjectsData);
	return GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->GetObjectsData(InSyncGroup, OutObjectsData);
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::GetEventsData);
	return GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->GetEventsData(OutJsonEvents, OutBinaryEvents);
}

EDisplayClusterCommResult FDisplayClusterClusterSyncService::GetNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CS::GetNativeInputData);
	return GDisplayCluster->GetPrivateClusterMgr()->GetClusterNodeController()->GetNativeInputData(OutNativeInputData);
}
