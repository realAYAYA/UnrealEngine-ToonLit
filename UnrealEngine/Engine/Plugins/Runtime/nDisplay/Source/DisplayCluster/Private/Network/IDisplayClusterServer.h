// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/DisplayClusterNetworkTypes.h"

class FDisplayClusterTcpListener;


/**
 * DisplayCluster TCP server interface
 */
class IDisplayClusterServer
{
public:
	virtual ~IDisplayClusterServer() = default;

public:
	// Start server on a specific socket
	virtual bool Start(const FString& Address, const uint16 Port) = 0;
	// Start server with a specified listener
	virtual bool Start(TSharedPtr<FDisplayClusterTcpListener>& Listener) = 0;
	// Stop server
	virtual void Shutdown() = 0;

	// Returns current server state
	virtual bool IsRunning() const = 0;

	// Returns server name
	virtual FString GetName() const = 0;
	// Returns server address
	virtual FString GetAddress() const = 0;
	// Returns server port
	virtual uint16 GetPort() const = 0;
	// Returns server protocol name
	virtual FString GetProtocolName() const = 0;

	// Kill all sessions of a specific node
	virtual void KillSession(const FString& NodeId) = 0;

	// Returns connection validation delegate
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsConnectionAllowedDelegate, const FDisplayClusterSessionInfo&);
	virtual FIsConnectionAllowedDelegate& OnIsConnectionAllowed() = 0;

	// Session opened event
	DECLARE_EVENT_OneParam(IDisplayClusterServer, FSessionOpenedEvent, const FDisplayClusterSessionInfo&);
	virtual FSessionOpenedEvent& OnSessionOpened() = 0;

	// Session closed event
	DECLARE_EVENT_OneParam(IDisplayClusterServer, FSessionClosedEvent, const FDisplayClusterSessionInfo&);
	virtual FSessionClosedEvent& OnSessionClosed() = 0;


	// Cluster node lost
	enum class ENodeFailType : uint8
	{
		BarrierTimeOut,
		ConnectionLost,
	};

	DECLARE_EVENT_TwoParams(IDisplayClusterServer, FNodeFailedEvent, const FString&, ENodeFailType);
	virtual FNodeFailedEvent& OnNodeFailed() = 0;
};
