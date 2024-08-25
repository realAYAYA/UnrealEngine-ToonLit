// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMessageBusSource.h"

class UWorld;
enum class EMapChangeType : uint8;
struct FLiveLinkClientInfoMessage;
struct FMessageAddress;
struct FLiveLinkHubTimecodeSettings;

/** LiveLink Message bus source that is connected to a livelink hub. */
class FLiveLinkHubMessageBusSource : public FLiveLinkMessageBusSource
{
public:
	FLiveLinkHubMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset);
	virtual ~FLiveLinkHubMessageBusSource();

protected:
	//~ Begin FLiveLinkMessageBusSource interface
	virtual void InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder);
	virtual double GetDeadSourceTimeout() const override;
	virtual void SendConnectMessage() override;
	//~ End FLiveLinkMessageBusSource

private:
	void HandleTimecodeSettings(const FLiveLinkHubTimecodeSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	/** Send information about this UE client to the connected provider */
	void SendClientInfoMessage();
	/** Handler called on map changed to update the livelink hub. */
	void OnMapChanged(UWorld* World, EMapChangeType ChangeType);
	/** Gather information about this client to put in a client info struct. */
	FLiveLinkClientInfoMessage CreateLiveLinkClientInfo() const;
};
