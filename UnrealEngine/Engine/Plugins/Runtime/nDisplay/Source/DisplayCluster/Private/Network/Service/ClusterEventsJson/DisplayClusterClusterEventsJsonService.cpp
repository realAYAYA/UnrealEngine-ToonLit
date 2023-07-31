// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonService.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonStrings.h"
#include "Network/Session/DisplayClusterSession.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Dom/JsonObject.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"


FDisplayClusterClusterEventsJsonService::FDisplayClusterClusterEventsJsonService()
	: FDisplayClusterService(FString("SRV_CEJ"))
{
	// Subscribe for SessionClosed events
	OnSessionClosed().AddRaw(this, &FDisplayClusterClusterEventsJsonService::ProcessSessionClosed);
}

FDisplayClusterClusterEventsJsonService::~FDisplayClusterClusterEventsJsonService()
{
	// Unsubscribe from SessionClosed notifications
	OnSessionClosed().RemoveAll(this);

	Shutdown();
}


FString FDisplayClusterClusterEventsJsonService::GetProtocolName() const
{
	static const FString ProtocolName("ClusterEventsJson");
	return ProtocolName;
}

TSharedPtr<IDisplayClusterSession> FDisplayClusterClusterEventsJsonService::CreateSession(FDisplayClusterSessionInfo& SessionInfo)
{
	SessionInfo.SessionName = FString::Printf(TEXT("%s_session_%lu_%s"), *GetName(), SessionInfo.SessionId, *SessionInfo.Endpoint.ToString());
	return MakeShared<FDisplayClusterSession<FDisplayClusterPacketJson, false>>(SessionInfo, *this, *this, FDisplayClusterService::GetThreadPriority());
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
typename IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType FDisplayClusterClusterEventsJsonService::ProcessPacket(const TSharedPtr<FDisplayClusterPacketJson>& Request, const FDisplayClusterSessionInfo& SessionInfo)
{
	// Check the pointer
	if (!Request.IsValid())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - Invalid request data (nullptr)"), *GetName());
		return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
	}

	bool MandatoryFieldsExist = true;

	if (!Request->GetJsonData()->HasField(FString(DisplayClusterClusterEventsJsonStrings::ArgName)))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Json packet doesn't have a mandatory field: %s"), DisplayClusterClusterEventsJsonStrings::ArgName);
		return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
	}

	if (!Request->GetJsonData()->HasField(FString(DisplayClusterClusterEventsJsonStrings::ArgType)))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Json packet doesn't have a mandatory field: %s"), DisplayClusterClusterEventsJsonStrings::ArgType);
		return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
	}

	if (!Request->GetJsonData()->HasField(FString(DisplayClusterClusterEventsJsonStrings::ArgCategory)))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Json packet doesn't have a mandatory field: %s"), DisplayClusterClusterEventsJsonStrings::ArgCategory);
		return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
	}

	// Convert net packet to the internal event data type
	FDisplayClusterClusterEventJson ClusterEvent;
	if(!DisplayClusterNetworkDataConversion::JsonPacketToJsonEvent(Request, ClusterEvent))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - couldn't translate net packet data to json event"), *GetName());
		return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
	}

	// Emit the event
	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - re-emitting cluster event for internal replication..."), *GetName());
	EmitClusterEventJson(ClusterEvent);

	return IDisplayClusterSessionPacketHandler<FDisplayClusterPacketJson, false>::ReturnType();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsJson
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterEventsJsonService::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(nD SRV_CEJ::EmitClusterEventJson);

	GDisplayCluster->GetPrivateClusterMgr()->EmitClusterEventJson(Event, true);
	return EDisplayClusterCommResult::Ok;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterEventsJsonService
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterEventsJsonService::ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo)
{
	if (!SessionInfo.IsTerminatedByServer())
	{
		// Get node ID
		const FString NodeId = SessionInfo.NodeId.Get(FString());
		if (!NodeId.IsEmpty())
		{
			// Notify others about node fail
			OnNodeFailed().Broadcast(NodeId, ENodeFailType::ConnectionLost);
		}
	}
}
