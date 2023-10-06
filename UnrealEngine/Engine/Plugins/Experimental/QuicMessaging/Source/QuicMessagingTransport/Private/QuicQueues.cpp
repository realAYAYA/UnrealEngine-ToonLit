// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuicQueues.h"


void FQuicQueues::ConsumeMessages()
{
	const double CurrentTime = FPlatformTime::Seconds();

	if (CurrentTime < (LastConsumed + ConsumeInterval.GetTotalSeconds()))
	{
		return;
	}

	ConsumeInboundMessages();
	ProcessInboundBuffers();
	ConsumeOutboundMessages();

	LastConsumed = FPlatformTime::Seconds();
}
