// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertSyncServerModule.h"
#include "ConcertSyncServer.h"
#include "ConcertSettings.h"
#include "Logging/LogVerbosity.h"
#include "Misc/EngineVersion.h"
#include "ConcertLogGlobal.h"
/**
 * 
 */
class FConcertSyncServerModule : public IConcertSyncServerModule
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual UConcertServerConfig* ParseServerSettings(const TCHAR* CommandLine) override
	{
		UConcertServerConfig* ServerConfig = NewObject<UConcertServerConfig>();

		if (CommandLine)
		{
			// Parse value overrides (if present)
			FParse::Value(CommandLine, TEXT("-CONCERTSERVER="), ServerConfig->ServerName);
			FParse::Value(CommandLine, TEXT("-CONCERTSESSION="), ServerConfig->DefaultSessionName);
			FParse::Value(CommandLine, TEXT("-CONCERTSESSIONTORESTORE="), ServerConfig->DefaultSessionToRestore);
			FParse::Value(CommandLine, TEXT("-CONCERTSAVESESSIONAS="), ServerConfig->DefaultSessionSettings.ArchiveNameOverride);
			FParse::Value(CommandLine, TEXT("-CONCERTPROJECT="), ServerConfig->DefaultSessionSettings.ProjectName);
			FParse::Value(CommandLine, TEXT("-CONCERTREVISION="), ServerConfig->DefaultSessionSettings.BaseRevision);
			FParse::Value(CommandLine, TEXT("-CONCERTWORKINGDIR="), ServerConfig->WorkingDir);
			FParse::Value(CommandLine, TEXT("-CONCERTSAVEDDIR="), ServerConfig->ArchiveDir);
			FParse::Value(CommandLine, TEXT("-CONCERTENDPOINTTIMEOUT="), ServerConfig->EndpointSettings.RemoteEndpointTimeoutSeconds);

			FString VersionString;
			if (FParse::Value(CommandLine, TEXT("-CONCERTVERSION="), VersionString))
			{
				FEngineVersion EngineVersion;
				UE_LOG(LogConcert, Warning, TEXT("CONCERTVERSION command line flag is deprecated and will be removed in a future version."));
				if (FEngineVersion::Parse(VersionString, EngineVersion))
				{
					ServerConfig->DefaultVersionInfo.EngineVersion.Initialize(EngineVersion);
					UE_LOG(LogConcert, Display, TEXT("Override for engine version set to '%s'."), *VersionString);
				}
				else
				{
					UE_LOG(LogConcert, Warning, TEXT("Failed to parse version string '%s'."),*VersionString);
				}
			}

			ServerConfig->ServerSettings.bIgnoreSessionSettingsRestriction |= FParse::Param(CommandLine, TEXT("CONCERTIGNORE"));
			FParse::Bool(CommandLine, TEXT("-CONCERTIGNORE="), ServerConfig->ServerSettings.bIgnoreSessionSettingsRestriction);

			ServerConfig->bCleanWorkingDir |= FParse::Param(CommandLine, TEXT("CONCERTCLEAN"));
			FParse::Bool(CommandLine, TEXT("-CONCERTCLEAN="), ServerConfig->bCleanWorkingDir);

			ServerConfig->EndpointSettings.bEnableLogging |= FParse::Param(CommandLine, TEXT("CONCERTLOGGING"));
			FParse::Bool(CommandLine, TEXT("-CONCERTLOGGING="), ServerConfig->EndpointSettings.bEnableLogging);
		}

		return ServerConfig;
	}

	virtual TSharedRef<IConcertSyncServer> CreateServer(const FString& InRole, const FConcertSessionFilter& InAutoArchiveSessionFilter) override
	{
		return MakeShared<FConcertSyncServer>(InRole, InAutoArchiveSessionFilter);
	}
};

IMPLEMENT_MODULE(FConcertSyncServerModule, ConcertSyncServer);
