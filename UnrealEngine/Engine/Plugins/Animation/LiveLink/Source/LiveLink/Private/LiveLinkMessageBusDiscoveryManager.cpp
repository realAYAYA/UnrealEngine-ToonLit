// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusDiscoveryManager.h"

#include "Async/Async.h"
#include "ILiveLinkClient.h"
#include "LiveLinkMessageBusSource.h"
#include "LiveLinkSettings.h"
#include "HAL/PlatformTime.h"
#include "MessageEndpointBuilder.h"
#include "Misc/ScopeLock.h"

LLM_DEFINE_TAG(LiveLink_LiveLinkMessageBusDiscoveryManager);

#define LL_HEARTBEAT_SLEEP_TIME 1.0f

FLiveLinkMessageBusDiscoveryManager::FLiveLinkMessageBusDiscoveryManager()
	: bRunning(true)
	, Thread(nullptr)
{
	LLM_SCOPE_BYTAG(LiveLink_LiveLinkMessageBusDiscoveryManager);

	PingRequestCounter = 0;
	PingRequestFrequency = GetDefault<ULiveLinkSettings>()->GetMessageBusPingRequestFrequency();

	MessageEndpoint = FMessageEndpoint::Builder(TEXT("LiveLinkMessageHeartbeatManager"))
		.Handling<FLiveLinkPongMessage>(this, &FLiveLinkMessageBusDiscoveryManager::HandlePongMessage);

	bRunning = MessageEndpoint.IsValid();
	if (bRunning)
	{
		Thread = FRunnableThread::Create(this, TEXT("LiveLinkMessageBusDiscoveryManager"));
	}
}

FLiveLinkMessageBusDiscoveryManager::~FLiveLinkMessageBusDiscoveryManager()
{
	{
		FScopeLock Lock(&SourcesCriticalSection);
		
		// Disable the Endpoint message handling since the message could keep it alive a bit.
		if (MessageEndpoint)
		{
			MessageEndpoint->Disable();
			MessageEndpoint.Reset();
		}
	}

	Stop();

	if (Thread)
	{
		Thread->Kill(true);
		Thread = nullptr;
	}
}

uint32 FLiveLinkMessageBusDiscoveryManager::Run()
{
	while (bRunning)
	{
		{
			FScopeLock Lock(&SourcesCriticalSection);

			if (PingRequestCounter > 0)
			{
				LastProviderPoolResults.Reset();
				LastPingRequest = FGuid::NewGuid();
				const int32 Version = ILiveLinkClient::LIVELINK_VERSION;
				MessageEndpoint->Publish(FMessageEndpoint::MakeMessage<FLiveLinkPingMessage>(LastPingRequest, Version));
			}
		}

		FPlatformProcess::Sleep(PingRequestFrequency);
	}
	return 0;
}

void FLiveLinkMessageBusDiscoveryManager::Stop()
{
	bRunning = false;
}

void FLiveLinkMessageBusDiscoveryManager::AddDiscoveryMessageRequest()
{
	FScopeLock Lock(&SourcesCriticalSection);
	if (++PingRequestCounter == 1)
	{
		LastProviderPoolResults.Reset();
	}
}

void FLiveLinkMessageBusDiscoveryManager::RemoveDiscoveryMessageRequest()
{
	--PingRequestCounter;
}

TArray<FProviderPollResultPtr> FLiveLinkMessageBusDiscoveryManager::GetDiscoveryResults() const
{
	FScopeLock Lock(&SourcesCriticalSection);

	return LastProviderPoolResults;
}

bool FLiveLinkMessageBusDiscoveryManager::IsRunning() const
{
	return bRunning;
}

void FLiveLinkMessageBusDiscoveryManager::HandlePongMessage(const FLiveLinkPongMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FScopeLock Lock(&SourcesCriticalSection);

	if (Message.PollRequest == LastPingRequest)
	{
		// Verify Message.LiveLinkVersion to consider validity of discovered provider. Older UE always sends 1
		constexpr bool bIsValidProvider = true;
		const double MachineTimeOffset = LiveLinkMessageBusHelper::CalculateProviderMachineOffset(Message.CreationPlatformTime, Context);
		LastProviderPoolResults.Emplace(MakeShared<FProviderPollResult, ESPMode::ThreadSafe>(Context->GetSender(), Message.ProviderName, Message.MachineName, MachineTimeOffset, bIsValidProvider));
	}
}