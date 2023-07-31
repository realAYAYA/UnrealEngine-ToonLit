// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/IDisplayClusterServer.h"
#include "Network/DisplayClusterNetworkTypes.h"

#include "Containers/Queue.h"

struct FIPv4Endpoint;
class FSocket;
class IDisplayClusterSession;
class FDisplayClusterTcpListener;


/**
 * Base DisplayCluster TCP server
 */
class FDisplayClusterServer
	: public IDisplayClusterServer
{
public:
	// Minimal time (seconds) before cleaning resources of the 'pending kill' sessions
	static const double CleanSessionResourcesSafePeriod;

public:
	FDisplayClusterServer(const FString& Name);
	virtual ~FDisplayClusterServer() = default;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterServer
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	// Start server on a specific socket
	virtual bool Start(const FString& Address, const uint16 Port) override;

	// Start server with a specified listener
	virtual bool Start(TSharedPtr<FDisplayClusterTcpListener>& ExternalListener) override;

	// Stop server
	virtual void Shutdown() override;

	// Returns current server state
	virtual bool IsRunning() const override;

	// Server name
	virtual FString GetName() const override
	{
		return Name;
	}

	// Server address
	virtual FString GetAddress() const override;

	// Server port
	virtual uint16 GetPort() const override;

	// Kills all sessions of a specified cluster node
	virtual void KillSession(const FString& NodeId) override;

	// Connection validation delegate
	virtual FIsConnectionAllowedDelegate& OnIsConnectionAllowed() override
	{
		return IsConnectionAllowedDelegate;
	}

	// Session opened event
	virtual FSessionOpenedEvent& OnSessionOpened() override
	{
		return SessionOpenedEvent;
	}

	// Session closed event
	virtual FSessionClosedEvent& OnSessionClosed() override
	{
		return SessionClosedEvent;
	}

	// Node failed event
	virtual FNodeFailedEvent& OnNodeFailed() override
	{
		return NodeFailedEvent;
	}

protected:
	// Allow to specify custom session class
	virtual TSharedPtr<IDisplayClusterSession> CreateSession(FDisplayClusterSessionInfo& SessionInfo) = 0;

	// Handle incoming connections
	bool ConnectionHandler(FDisplayClusterSessionInfo& SessionInfo);

	// Callback on session opened
	void ProcessSessionOpened(const FDisplayClusterSessionInfo& SessionInfo);
	// Callback on session closed
	void ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo);

private:
	// Free resources of the sessions that already finished their job
	void CleanPendingKillSessions();

private:
	// Server data
	const FString Name;
	
	// Server running state
	bool bIsRunning = false;

	// Socket listeners
	TSharedPtr<FDisplayClusterTcpListener> Listener;

	// Server events/delegates
	FIsConnectionAllowedDelegate IsConnectionAllowedDelegate;
	FSessionOpenedEvent SessionOpenedEvent;
	FSessionClosedEvent SessionClosedEvent;
	FNodeFailedEvent    NodeFailedEvent;

private:
	// Session counter used for session ID generation
	uint64 IncrementalSessionId = 0;

	// Pending sessions
	TMap<uint64, TSharedPtr<IDisplayClusterSession>> PendingSessions;
	// Active sessions
	TMap<uint64, TSharedPtr<IDisplayClusterSession>> ActiveSessions;
	// Closed sessions, awaiting for cleaning
	TMap<uint64, TSharedPtr<IDisplayClusterSession>> PendingKillSessions;

	// Sync access
	mutable FCriticalSection ServerStateCritSec;
	mutable FCriticalSection SessionsCritSec;
};
