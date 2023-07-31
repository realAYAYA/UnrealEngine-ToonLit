// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterClient.h"
#include "Network/Protocol/IDisplayClusterProtocolEventsBinary.h"
#include "Network/Packet/DisplayClusterPacketBinary.h"

struct FDisplayClusterClusterEventBinary;


/**
 * Binary cluster events TCP client
 */
class FDisplayClusterClusterEventsBinaryClient
	: public FDisplayClusterClient<FDisplayClusterPacketBinary>
	, public IDisplayClusterProtocolEventsBinary
{
public:
	FDisplayClusterClusterEventsBinaryClient();
	FDisplayClusterClusterEventsBinaryClient(const FString& InName);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsBinary
	//////////////////////////////////////////////////////////////////////////////////////////////
	EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) override;
};
