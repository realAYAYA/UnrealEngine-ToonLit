// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/CriticalSection.h"
#include "Templates/Atomic.h"

#include "MessageEndpoint.h"
#include "LiveLinkMessages.h"
#include "LiveLinkMessageBusFinder.h"

class FLiveLinkMessageBusSource;

/** A class to asynchronously discover message bus sources. */
class FLiveLinkMessageBusDiscoveryManager : FRunnable
{
public:
	FLiveLinkMessageBusDiscoveryManager();
	~FLiveLinkMessageBusDiscoveryManager();

	//~ Begin FRunnable interface

	virtual uint32 Run() override;

	virtual void Stop() override;

	//~ End FRunnable interface

	void AddDiscoveryMessageRequest();
	void RemoveDiscoveryMessageRequest();
	TArray<FProviderPollResultPtr> GetDiscoveryResults() const;

	bool IsRunning() const;

private:
	void HandlePongMessage(const FLiveLinkPongMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

private:
	// Counter of item that request discovery message
	TAtomic<int32> PingRequestCounter;

	// Last ping Request id
	FGuid LastPingRequest;

	// Time of the last ping request
	double LastPingRequestTime;

	// Ping request timeout
	double PingRequestFrequency;

	// Result from the last ping request
	TArray<FProviderPollResultPtr> LastProviderPoolResults;

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	// Thread safe bool for stopping the thread
	FThreadSafeBool bRunning;

	// Thread the heartbeats are sent on
	FRunnableThread* Thread;

	// Critical section for accessing the Source Set
	mutable FCriticalSection SourcesCriticalSection;
};