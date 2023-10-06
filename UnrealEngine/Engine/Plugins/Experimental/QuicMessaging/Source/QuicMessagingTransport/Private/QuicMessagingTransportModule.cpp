// Copyright Epic Games, Inc. All Rights Reserved.

#include "IQuicMessagingTransportModule.h"

class FQuicMessagingTransportModule : public IQuicMessagingTransportModule
{
public:
	FQuicMessagingTransportModule() = default;
	virtual ~FQuicMessagingTransportModule() {}

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


IMPLEMENT_MODULE(FQuicMessagingTransportModule, QuicMessagingTransport);
