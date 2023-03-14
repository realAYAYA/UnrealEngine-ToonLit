// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"

#ifndef STEAM_SOCKETS_SUBSYSTEM
#define STEAM_SOCKETS_SUBSYSTEM FName(TEXT("SteamSockets"))
#endif

// Add our two new protocol types
namespace FNetworkProtocolTypes
{
	STEAMSOCKETS_API extern const FLazyName SteamSocketsP2P;
	STEAMSOCKETS_API extern const FLazyName SteamSocketsIP;
}

typedef uint32 SteamSocketHandles;
