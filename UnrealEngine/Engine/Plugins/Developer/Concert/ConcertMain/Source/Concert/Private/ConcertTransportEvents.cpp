// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertTransportEvents.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConcertTransportEvents)

namespace ConcertTransportEvents
{
	FConcertTransportLoggingEnabledChanged& OnConcertTransportLoggingEnabledChangedEvent()
	{
		static FConcertTransportLoggingEnabledChanged Instance;
		return Instance;
	}
	
	FConcertClientLogEvent& OnConcertClientLogEvent()
	{
		static FConcertClientLogEvent Instance;
		return Instance;
	}

	FConcertServerLogEvent& OnConcertServerLogEvent()
	{
		static FConcertServerLogEvent Instance;
		return Instance;
	}
}

