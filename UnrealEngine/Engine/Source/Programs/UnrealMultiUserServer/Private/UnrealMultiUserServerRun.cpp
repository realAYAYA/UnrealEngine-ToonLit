// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealMultiUserServerRun.h"

#include "ConcertSettings.h"
#include "ConcertSyncServerLoop.h"

#include "Misc/CommandLine.h"

int32 RunUnrealMultiUserServer(int ArgC, TCHAR* ArgV[])
{
	FString Role(TEXT("MultiUser"));
	FConcertSyncServerLoopInitArgs ServerLoopInitArgs;
	ServerLoopInitArgs.SessionFlags = EConcertSyncSessionFlags::Default_MultiUserSession;
	ServerLoopInitArgs.ServiceRole = Role;
	ServerLoopInitArgs.ServiceFriendlyName = TEXT("Multi-User Editing Server");

	ServerLoopInitArgs.GetServerConfigFunc = [Role]() -> const UConcertServerConfig*
	{
		UConcertServerConfig* ServerConfig = IConcertSyncServerModule::Get().ParseServerSettings(FCommandLine::Get());
		if (ServerConfig->WorkingDir.IsEmpty())
		{
			ServerConfig->WorkingDir = FPaths::ProjectIntermediateDir() / Role;
		}
		if (ServerConfig->ArchiveDir.IsEmpty())
		{
			ServerConfig->ArchiveDir = FPaths::ProjectSavedDir() / Role;
		}
		return ServerConfig;
	};

	return ConcertSyncServerLoop(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr), ServerLoopInitArgs);
}
