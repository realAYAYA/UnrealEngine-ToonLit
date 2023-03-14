// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertModule.h"

#include "ConcertSettings.h"
#include "ConcertServer.h"
#include "ConcertClient.h"

/**
 * Implements the Concert module
 */
class FConcertModule : public IConcertModule
{
public:

	virtual void StartupModule() override
	{
		EndpointProvider = IConcertTransportModule::Get().CreateEndpointProvider();
	}

	virtual void ShutdownModule() override
	{
		EndpointProvider.Reset();
	}

	virtual IConcertServerRef CreateServer(const FString& InRole, const FConcertSessionFilter& InAutoArchiveSessionFilter, IConcertServerEventSink* InEventSink) override
	{
		return MakeShared<FConcertServer, ESPMode::ThreadSafe>(InRole, InAutoArchiveSessionFilter, InEventSink, EndpointProvider);
	}

	virtual IConcertClientRef CreateClient(const FString& InRole) override
	{
		return MakeShared<FConcertClient, ESPMode::ThreadSafe>(InRole, EndpointProvider);
	}

private:
	TSharedPtr<IConcertEndpointProvider> EndpointProvider;
};

IMPLEMENT_MODULE(FConcertModule, Concert);
