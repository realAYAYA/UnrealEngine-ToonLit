// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Containers/ObservableArray.h"

template <typename OptionalType> struct TOptional;
struct FLiveLinkHubClientId;
struct FLiveLinkClientInfoMessage;
struct FLiveLinkHubUEClientInfo;
struct FLiveLinkSubjectKey;

/** Decouples the UI from the livelink hub functions. */
class ILiveLinkHubClientsModel
{
public:
	/** Types of client updates. */
	enum class EClientEventType
	{
		Discovered, // UE client was discovered by the hub.
		Disconnected, // Connection was lost with a client.
		Reestablished, // Connection was restored with a previously disconnected client.
		Modified // Information about a given client has changed (ie. level has changed).
	};

	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnClientEvent, FLiveLinkHubClientId, EClientEventType);

	virtual ~ILiveLinkHubClientsModel() = default;

	/** Delegate used to get noticed about changes to the client list. */
	virtual FOnClientEvent& OnClientEvent() = 0;

	/** Get the status text of a client. */
	virtual FText GetClientStatus(FLiveLinkHubClientId Client) const = 0;

	/** Get the list of clients that should be displayed in the clients panel according to the current session config. */
	virtual TArray<FLiveLinkHubClientId> GetSessionClients() const = 0;

	/** Get the list of clients discovered by the hub. */
	virtual TArray<FLiveLinkHubClientId> GetDiscoveredClients() const = 0;

	/** Get information about a given client given its address. */
	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(FLiveLinkHubClientId InClient) const = 0;

	/** Get the name of a client. */
	virtual FText GetClientDisplayName(FLiveLinkHubClientId InClient) const = 0;

	/** Get whether a client should receive livelink data. */
	virtual bool IsClientEnabled(FLiveLinkHubClientId InClient) const = 0;

	/** Get whether a client is connected to the hub. */
	virtual bool IsClientConnected(FLiveLinkHubClientId InClient) const = 0;

	/** Set whether a client should receive livelink data. */
	virtual void SetClientEnabled(FLiveLinkHubClientId InClient, bool bInEnable) = 0;

	/** Get whether a subject is enabled on a given client. */
	virtual bool IsSubjectEnabled(FLiveLinkHubClientId InClient, const FLiveLinkSubjectKey& Subject) const = 0;

	/** Set whether a subject should receive livelink data. */
	virtual void SetSubjectEnabled(FLiveLinkHubClientId InClient, const FLiveLinkSubjectKey& Subject, bool bInEnable) = 0;
};
