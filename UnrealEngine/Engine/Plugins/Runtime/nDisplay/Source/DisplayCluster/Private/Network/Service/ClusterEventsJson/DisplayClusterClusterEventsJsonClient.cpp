// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonClient.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonStrings.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"

#include "Cluster/DisplayClusterClusterEvent.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterEventsJsonClient::FDisplayClusterClusterEventsJsonClient()
	: FDisplayClusterClusterEventsJsonClient(FString("CLN_CEJ"))
{
}

FDisplayClusterClusterEventsJsonClient::FDisplayClusterClusterEventsJsonClient(const FString& InName)
	: FDisplayClusterClient(InName)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsJson
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterEventsJsonClient::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event)
{
	// Convert internal json event type to json net packet
	TSharedPtr<FDisplayClusterPacketJson> Request = DisplayClusterNetworkDataConversion::JsonEventToJsonPacket(Event);
	if (!Request)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't convert json cluster event data to net packet"));
		return EDisplayClusterCommResult::WrongRequestData;
	}

	bool bResult = false;

	{
		// Send event
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CEJ::EmitClusterEventJson);
		bResult = SendPacket(Request);
	}

	if (!bResult)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't send json cluster event"));
		return EDisplayClusterCommResult::NetworkError;
	}

	return EDisplayClusterCommResult::Ok;
}
