// Copyright Epic Games, Inc. All Rights Reserved.

#include "LobbyModule.h"

IMPLEMENT_MODULE(FLobbyModule, Lobby);

DEFINE_LOG_CATEGORY(LogLobby);

#if STATS
//DEFINE_STAT(STAT_LobbyStat1);
#endif

void FLobbyModule::StartupModule()
{	
}

void FLobbyModule::ShutdownModule()
{
}

