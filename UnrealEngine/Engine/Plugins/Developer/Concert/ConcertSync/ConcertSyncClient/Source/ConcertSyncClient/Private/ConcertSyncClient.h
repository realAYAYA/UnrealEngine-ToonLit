// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IConcertSyncClient.h"

class IConcertClientSession;
class IConcertClientPackageBridge;
class IConcertClientTransactionBridge;
class FConcertSyncClientLiveSession;
class FConcertClientWorkspace;
class FConcertClientPresenceManager;
class FConcertClientSequencerManager;
class FConcertSourceControlProxy;

/**
 * Implementation for a Concert Sync Client.
 */
class FConcertSyncClient : public IConcertSyncClient
{
public:
	FConcertSyncClient(const FString& InRole, IConcertClientPackageBridge* InPackageBridge, IConcertClientTransactionBridge* InTransactionBridge);
	virtual ~FConcertSyncClient();

	//~ IConcertSyncClient interface
	virtual void Startup(const UConcertClientConfig* InClientConfig, const EConcertSyncSessionFlags InSessionFlags) override;
	virtual void Shutdown() override;
	virtual IConcertClientRef GetConcertClient() const override;
	virtual TSharedPtr<IConcertClientWorkspace> GetWorkspace() const override;
	virtual IConcertClientPresenceManager* GetPresenceManager() const override;
	virtual IConcertClientSequencerManager* GetSequencerManager() const override;

	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceStartup() override;
	virtual FOnConcertClientWorkspaceStartupOrShutdown& OnWorkspaceShutdown() override;

	virtual FOnConcertClientSyncSessionStartupOrShutdown& OnSyncSessionStartup() override;
	virtual FOnConcertClientSyncSessionStartupOrShutdown& OnSyncSessionShutdown() override;

	virtual void PersistAllSessionChanges() override;
	virtual void GetSessionClientActions(const FConcertSessionClientInfo& InClientInfo, TArray<FConcertActionDefinition>& OutActions) const override;

	virtual void SetFileSharingService(TSharedPtr<IConcertFileSharingService> InFileSharingService) override;
	virtual IConcertClientTransactionBridge* GetTransactionBridge() const override;

private:
	void CreateWorkspace(const TSharedRef<FConcertSyncClientLiveSession>& InLiveSession);
	void DestroyWorkspace();

	void RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);
	void UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);

	/** Client for Concert */
	IConcertClientRef ConcertClient;

	/** Flags controlling what features are enabled for sessions within this client */
	EConcertSyncSessionFlags SessionFlags;

	/** Package bridge used by sessions of this client */
	IConcertClientPackageBridge* PackageBridge;

	/** Transaction bridge used by sessions of this client */
	IConcertClientTransactionBridge* TransactionBridge;

	/** Live session data for the current session. */
	TSharedPtr<FConcertSyncClientLiveSession> LiveSession;

	/** Client workspace for the current session. */
	TSharedPtr<FConcertClientWorkspace> Workspace;

	/** Delegate called on every workspace startup. */
	FOnConcertClientWorkspaceStartupOrShutdown OnWorkspaceStartupDelegate;

	/** Delegate called on every workspace shutdown. */
	FOnConcertClientWorkspaceStartupOrShutdown OnWorkspaceShutdownDelegate;

	/** Delegate called just before the workspace get created. */
	FOnConcertClientSyncSessionStartupOrShutdown OnSyncSessionStartupDelegate;
	
	/** Delegate called just after the workspace was deleted. */
	FOnConcertClientSyncSessionStartupOrShutdown OnSyncSessionShutdownDelegate;

	/** Optional side channel to exchange large blobs (package data) with the server in a scalable way (ex. the request/response transport layer is not designed and doesn't support exchanging 3GB packages). */
	TSharedPtr<IConcertFileSharingService> FileSharingService;

#if WITH_EDITOR
	/** Presence manager for the current session. */
	TSharedPtr<FConcertClientPresenceManager> PresenceManager;

	/** Sequencer event manager for Concert session. */
	TUniquePtr<FConcertClientSequencerManager> SequencerManager;

	/** Source Control Provider Proxy for Concert session. */
	TUniquePtr<FConcertSourceControlProxy> SourceControlProxy;
#endif
};
