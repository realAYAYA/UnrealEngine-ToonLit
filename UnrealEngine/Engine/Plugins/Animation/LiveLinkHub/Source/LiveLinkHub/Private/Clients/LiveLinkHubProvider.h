// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Clients/GameThreadMessageHandler.h"
#include "Clients/LiveLinkHubClientsModel.h"
#include "Clients/LiveLinkHubUEClientInfo.h"
#include "Engine/TimerHandle.h"
#include "IMessageContext.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkProviderImpl.h"
#include "Templates/Function.h"


class ILiveLinkHubSessionManager;
struct FLiveLinkHubClientId;
struct FLiveLinkHubConnectMessage;
struct FLiveLinkHubUEClientInfo;


/** 
 * LiveLink Provider that allows getting more information about a UE client by communicating with a LiveLinkHub MessageBus Source.
 */
class FLiveLinkHubProvider : public FLiveLinkProvider, public ILiveLinkHubClientsModel, public TSharedFromThis<FLiveLinkHubProvider>
{
public:
	using FLiveLinkProvider::SendClearSubjectToConnections;
	using FLiveLinkProvider::GetLastSubjectStaticDataStruct;

	/**
	 * Create a message bus handler that will dispatch messages on the game thread. 
	 * This is useful to receive some messages on AnyThread and delegate others on the game thread (ie. for methods that will trigger UI updates which need to happen on game thread. )
	 */
	template <typename MessageType>
	TSharedRef<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>> MakeHandler(typename TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>::FuncType Func)
	{
		return MakeShared<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>>(this, Func);
	}

	FLiveLinkHubProvider(const TSharedRef<ILiveLinkHubSessionManager>& InSessionManager);

	virtual ~FLiveLinkHubProvider() override;

	//~ Begin LiveLinkProvider interface
	virtual bool ShouldTransmitToSubject_AnyThread(FName SubjectName, FMessageAddress Address) const override;
	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(FLiveLinkHubClientId InClient) const override;
	//~ End LiveLinkProvider interface

	/**
	 * Restore a client, calling this will modify the client ID if it matches an existing connection.
	 */
	void AddRestoredClient(FLiveLinkHubUEClientInfo& InOutRestoredClientInfo);

	/** Retrieve the existing client map. */
	const TMap<FLiveLinkHubClientId, FLiveLinkHubUEClientInfo>& GetClientsMap() const { return ClientsMap; }

	/** Timecode settings that should be shared to connected editors. */
	void SetTimecodeSettings(FLiveLinkHubTimecodeSettings InSettings);

private:
	/** Handle a connection message resulting from a livelink hub message bus source connecting to this provider. */
	void HandleHubConnectMessage(const FLiveLinkHubConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	
	/** Handle a client info message being received. Happens when new information about a client is received (ie. Client has changed map) */
	void HandleClientInfoMessage(const FLiveLinkClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Send timecode settings to connected Live Link Hub provider. */
	void SendTimecodeSettings();

	/** Send a message to clients that are connected and enabled through the hub clients list. */
	template<typename MessageType>
	void SendMessageToEnabledClients(MessageType* Message)
	{
		TArray<FMessageAddress> AllAddresses;
		GetConnectedAddresses(AllAddresses);

		TArray<FMessageAddress> EnabledAddresses = AllAddresses.FilterByPredicate([this](const FMessageAddress& Address)
		{
			return ShouldTransmitToClient_AnyThread(Address);
		});

		SendMessage(Message, EnabledAddresses);
	}

	/**
	 * Whether a message should be transmitted to a particular client, identified by a message address.
	 * You can specify an additional filter method if you want to filter based on the client info.
	 **/
	bool ShouldTransmitToClient_AnyThread(FMessageAddress Address, TFunctionRef<bool(const FLiveLinkHubUEClientInfo* ClientInfoPtr)> AdditionalFilter = [](const FLiveLinkHubUEClientInfo* ClientInfoPtr){ return true; }) const;

protected:
	//~ Begin ILiveLinkHubClientsModel interface
	virtual void OnConnectionsClosed(const TArray<FMessageAddress>& ClosedAddresses) override;
	virtual TArray<FLiveLinkHubClientId> GetSessionClients() const override;
	virtual TMap<FName, FString> GetAnnotations() const override;
	virtual TArray<FLiveLinkHubClientId> GetDiscoveredClients() const override;
	virtual FText GetClientDisplayName(FLiveLinkHubClientId InAddress) const override;
	virtual FOnClientEvent& OnClientEvent() override
	{
		return OnClientEventDelegate;
	}
	virtual FText GetClientStatus(FLiveLinkHubClientId Client) const override;
	virtual bool IsClientEnabled(FLiveLinkHubClientId Client) const override;
	virtual bool IsClientConnected(FLiveLinkHubClientId Client) const override;
	virtual void SetClientEnabled(FLiveLinkHubClientId Client, bool bInEnable) override;
	virtual bool IsSubjectEnabled(FLiveLinkHubClientId Client, const FLiveLinkSubjectKey& Subject) const override;
	virtual void SetSubjectEnabled(FLiveLinkHubClientId Client, const FLiveLinkSubjectKey& Subject, bool bInEnable) override;
	//~ End ILiveLinkHubClientsModel interface

private:
	/** Handle to the timer responsible for validating the livelinkprovider's connections.*/
	FTimerHandle ValidateConnectionsTimer;
	/** List of information we have on clients we have discovered. */
	TMap<FLiveLinkHubClientId, FLiveLinkHubUEClientInfo> ClientsMap;
	/** Delegate called when the provider receives a client change. */
	FOnClientEvent OnClientEventDelegate;
	/** Annotations sent with every message from this provider. In our case it's use to disambiguate a livelink hub provider from other livelink providers.*/
	TMap<FName, FString> Annotations;
	/** LiveLinkHub session manager. */
	TWeakPtr<ILiveLinkHubSessionManager> SessionManager;
	/** Cache used to retrieve the client id from a message bus address. */
	TMap<FMessageAddress, FLiveLinkHubClientId> AddressToIdCache;
	/** Cached value of timecode connection settings. */
	FLiveLinkHubTimecodeSettings TimecodeSettings;

	/** Lock used to access the clients map from different threads. */
	mutable FRWLock ClientsMapLock;
};
