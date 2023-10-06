// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"

class DMXPROTOCOL_API IDMXProtocolFactory
{
public:
	virtual ~IDMXProtocolFactory() {}

	/**
	 * It creates only one instance of Protocol. There is the condition to check in DMXProtocolModule
	 * @return Return the pointer to protocol
	 */
	virtual IDMXProtocolPtr CreateProtocol(const FName& ProtocolName) = 0;
};
