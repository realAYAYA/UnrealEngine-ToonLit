// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryClient.h"
#include "Network/Conversion/DisplayClusterNetworkDataConversion.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterEventsBinaryClient::FDisplayClusterClusterEventsBinaryClient()
	: FDisplayClusterClusterEventsBinaryClient(FString("CLN_CEB"))
{
}

FDisplayClusterClusterEventsBinaryClient::FDisplayClusterClusterEventsBinaryClient(const FString& InName)
	: FDisplayClusterClient(InName)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsBinary
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterEventsBinaryClient::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event)
{
	// Convert internal binary event type to binary net packet
	TSharedPtr<FDisplayClusterPacketBinary> Request = DisplayClusterNetworkDataConversion::BinaryEventToBinaryPacket(Event);
	if (!Request)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't convert binary cluster event data to net packet"));
		return EDisplayClusterCommResult::WrongRequestData;
	}

	bool bResult = false;

	{
		// Send event
		TRACE_CPUPROFILER_EVENT_SCOPE(nD CLN_CEB::EmitClusterEventBinary);
		bResult = SendPacket(Request);
	}

	if (!bResult)
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Couldn't send binary cluster event"));
		return EDisplayClusterCommResult::NetworkError;
	}

	return EDisplayClusterCommResult::Ok;
}
