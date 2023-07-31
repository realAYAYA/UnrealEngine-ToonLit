// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/DisplayClusterNetworkTypes.h"

#include "Sockets.h"
#include "HAL/Runnable.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "Misc/DisplayClusterConstants.h"



/**
 * TCP connection listener.
 *
 * Listens for incoming connections and redirects the requests to specific server implementations.
 * Can be shared by nDisplay services that use internal communication protocol.
 */
class FDisplayClusterTcpListener
	: protected FRunnable
{
public:
	FDisplayClusterTcpListener(bool bIsShared, const FString& InName);
	virtual ~FDisplayClusterTcpListener();

public:
	// Start listening to a specified socket
	bool StartListening(const FString& InAddr, const uint16 InPort);
	// Start listening to a specified endpoint
	bool StartListening(const FIPv4Endpoint& Endpoint);
	// Stop listening
	void StopListening(bool bWaitForCompletion);
	// Wait unless working thread is finished
	void WaitForCompletion();

	// Is currently listening
	bool IsListening() const
	{
		return bIsListening;
	}

	// Returns listenting parameters
	bool GetListeningParams(FString& OutAddr, uint16& OutPort)
	{
		if (IsListening())
		{
			OutAddr = GetListeningHost();
			OutPort = GetListeningPort();
		}

		return false;
	}

	// Returns listening host
	FString GetListeningHost() const
	{
		return IsListening() ? Endpoint.Address.ToString() : FString();
	}

	// Returns listening port
	uint16 GetListeningPort() const
	{
		return IsListening() ? Endpoint.Port : 0;
	}

	// Delegate for processing incoming connections
	DECLARE_DELEGATE_RetVal_OneParam(bool, FConnectionAcceptedDelegate, FDisplayClusterSessionInfo&);

	// Returns connection validation delegate
	FConnectionAcceptedDelegate& OnConnectionAccepted()
	{
		return ConnectionAcceptedDelegate;
	}

	// Returns protocol-bound delegate
	FConnectionAcceptedDelegate& OnConnectionAccepted(const FString& ProtocolName);

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRunnable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

protected:
	bool GenIPv4Endpoint(const FString& Addr, const uint16 Port, FIPv4Endpoint& EP) const;

private:
	// Socket name
	FString Name;
	// Listening socket
	FSocket* SocketObj = nullptr;
	// Listening endpoint
	FIPv4Endpoint Endpoint;
	// Holds the thread object
	TUniquePtr<FRunnableThread> ThreadObj;
	// Listening state
	bool bIsListening = false;

	// Holds a delegate to be invoked when an incoming connection has been accepted.
	FConnectionAcceptedDelegate ConnectionAcceptedDelegate;

private:
	// Handles incoming connections
	bool ProcessIncomingConnection(FDisplayClusterSessionInfo& SessionInfo);

	// ProtocolName-to-ServiceDelegate map for transferring connection ownership to an appropriate server
	TMap<FString, FConnectionAcceptedDelegate> ProtocolDispatchingMap;

	// Critical section for access control
	FCriticalSection InternalsCS;
};
