// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertFrontendLogFilter_Client.h"

#include "ConcertTransportEvents.h"
#include "Widgets/Clients/Logging/ConcertLogEntry.h"
#include "Widgets/Clients/Util/EndpointToUserNameCache.h"

namespace UE::MultiUserServer
{
	bool FConcertLogFilter_Client::PassesFilter(const FConcertLogEntry& InItem) const
	{
		return AllowedClientMessagingNodeId == EndpointCache->TranslateEndpointIdToNodeId(InItem.Log.OriginEndpointId)
			|| AllowedClientMessagingNodeId == EndpointCache->TranslateEndpointIdToNodeId(InItem.Log.DestinationEndpointId);
	}
}