// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/Protocol/IDisplayClusterProtocolEventsJson.h"
#include "Network/Protocol/IDisplayClusterProtocolEventsBinary.h"
#include "Network/Protocol/IDisplayClusterProtocolClusterSync.h"
#include "Network/Protocol/IDisplayClusterProtocolRenderSync.h"

#include "DisplayClusterEnums.h"


/**
 * Node controller interface
 */
class IDisplayClusterClusterNodeController
	: public IDisplayClusterProtocolEventsJson
	, public IDisplayClusterProtocolEventsBinary
	, public IDisplayClusterProtocolClusterSync
	, public IDisplayClusterProtocolRenderSync
{
public:
	virtual ~IDisplayClusterClusterNodeController() = default;

public:
	/** Initialize controller instance */
	virtual bool Initialize() = 0;

	/** Stop  clients/servers/etc */
	virtual void Shutdown() = 0;

	/** Returns cluster role of the controller */
	virtual EDisplayClusterNodeRole GetClusterRole() const = 0;

	/** Return node ID */
	virtual FString GetNodeId() const = 0;

	/** Return controller name */
	virtual FString GetControllerName() const = 0;

	/** Drop specific cluster node */
	virtual bool DropClusterNode(const FString& NodeId)
	{
		return false;
	}

	/** Send binary event to a specific target outside of the cluster */
	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
	{ }

	/** Send JSON event to a specific target outside of the cluster */
	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
	{ }
};
