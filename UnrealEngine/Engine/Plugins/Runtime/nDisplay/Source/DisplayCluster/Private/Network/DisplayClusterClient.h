// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/IDisplayClusterClient.h"
#include "Network/Transport/DisplayClusterSocketOperations.h"
#include "Network/Transport/DisplayClusterSocketOperationsHelper.h"

#include "Misc/DisplayClusterConstants.h"


/**
 * Base DisplayCluster TCP client
 */
class FDisplayClusterClientBase
	: public IDisplayClusterClient
	, public FDisplayClusterSocketOperations
{
public:
	FDisplayClusterClientBase(const FString& InName)
		: FDisplayClusterSocketOperations(CreateSocket(InName), DisplayClusterConstants::net::PacketBufferSize, InName)
	{ }

	virtual ~FDisplayClusterClientBase()
	{
		Disconnect();
	}

public:
	// Connects to a server
	virtual bool Connect(const FString& Address, const uint16 Port, const uint32 ConnectRetriesAmount, const uint32 ConnectRetryDelay) override;
	// Terminates current connection
	virtual void Disconnect() override final;

	// Provides with net unit name
	virtual FString GetName() const override
	{
		return GetConnectionName();
	}

	virtual bool IsConnected() const override
	{
		return IsOpen();
	}

protected:
	// Creates client socket
	FSocket* CreateSocket(const FString& InName);
};


template <typename TPacketType>
class FDisplayClusterClient
	: public    FDisplayClusterClientBase
	, protected FDisplayClusterSocketOperationsHelper<TPacketType>
{
public:
	FDisplayClusterClient(const FString& InName)
		: FDisplayClusterClientBase(InName)
		, FDisplayClusterSocketOperationsHelper<TPacketType>(*this, InName)
	{ }
};
