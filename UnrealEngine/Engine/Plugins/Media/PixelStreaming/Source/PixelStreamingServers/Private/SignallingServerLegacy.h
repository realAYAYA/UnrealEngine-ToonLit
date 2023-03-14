// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SignallingServer.h"

namespace UE::PixelStreamingServers
{

	/*
	 * A signalling server that matches the signalling protocol and features of the 4.26/4.27 Cirrus signalling server.
	 * While we don't want people to use this it is useful to keep around for tests to prevent breaking changes while
	 * those servers are still being used by some users.
	 */
	class FSignallingServerLegacy : public FSignallingServer
	{
	protected:
		virtual void SendPlayerMessage(uint16 PlayerConnectionId, FString MessageType, FString Message) override;
		virtual void SendStreamerMessage(uint16 StreamerConnectionId, FString MessageType, FString Message) override;
		virtual void OnPlayerConnected(uint16 ConnectionId) override;
	};

} // namespace UE::PixelStreamingServers