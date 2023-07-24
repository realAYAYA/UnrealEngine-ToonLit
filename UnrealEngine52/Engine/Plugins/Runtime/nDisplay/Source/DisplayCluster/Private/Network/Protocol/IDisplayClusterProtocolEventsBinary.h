// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterNetworkTypes.h"

struct FDisplayClusterClusterEventBinary;


/**
 * Binary cluster events protocol.
 */
class IDisplayClusterProtocolEventsBinary
{
public:
	virtual ~IDisplayClusterProtocolEventsBinary() = default;

public:
	virtual EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) = 0;
};
