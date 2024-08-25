// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaSessionCreateInfo.h"

namespace uba
{
	class NetworkServer;

	struct SessionServerCreateInfo : SessionCreateInfo
	{
		SessionServerCreateInfo(Storage& s, NetworkServer& c, LogWriter& writer = g_consoleLogWriter) : SessionCreateInfo(s, writer), server(c) {}

		NetworkServer& server;
		u8 memWaitLoadPercent = 85; // When memory usage goes above this percent, no new processes will be spawned until back below
		u8 memKillLoadPercent = 95; // When memory usage goes above this percent, newest processes will be killed to bring it back below
		bool resetCas = false;
		bool remoteExecutionEnabled = true;
		bool nameToHashTableEnabled = true;
		bool checkMemory = true;
		bool allowWaitOnMem = false;
		bool allowKillOnMem = false;
		bool remoteLogEnabled = false; // If Uba is built in debug, then the logs will be sent back to server
		bool remoteTraceEnabled = false; // If this is true, the agents will run trace and send the .uba file back to server
	};
}
