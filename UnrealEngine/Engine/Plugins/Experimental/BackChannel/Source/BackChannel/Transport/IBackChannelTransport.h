// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IBackChannelSocketConnection.h"
#include "Modules/ModuleManager.h"

/**
 *	Main module and factory interface for Backchannel connections
 */
class BACKCHANNEL_API IBackChannelTransport : public IModuleInterface
{
public:

	static inline bool IsAvailable(void)
	{
		return Get() != nullptr;
	}

	static inline IBackChannelTransport* Get(void)
	{
		return FModuleManager::LoadModulePtr<IBackChannelTransport>("BackChannel");
	}

	virtual TSharedPtr<IBackChannelSocketConnection> CreateConnection(const int32 Type) = 0;

public:

	static const int TCP;

protected:

	IBackChannelTransport() {}
	virtual ~IBackChannelTransport() {}
};
