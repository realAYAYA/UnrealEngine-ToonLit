// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpServerConfig.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogHttpServerConfig)

const FHttpServerListenerConfig FHttpServerConfig::GetListenerConfig(uint32 Port) 
{
	static const FString IniSectionName(TEXT("HTTPServer.Listeners"));

	// Code default values
	FHttpServerListenerConfig Config;

	// Apply default ini configuration
	GConfig->GetString(*IniSectionName, TEXT("DefaultBindAddress"), Config.BindAddress, GEngineIni);
	GConfig->GetInt(*IniSectionName, TEXT("DefaultBufferSize"), Config.BufferSize, GEngineIni);
	GConfig->GetInt(*IniSectionName, TEXT("DefaultConnectionsBacklogSize"), Config.ConnectionsBacklogSize, GEngineIni);
	GConfig->GetInt(*IniSectionName, TEXT("DefaultMaxConnectionsAcceptPerFrame"), Config.MaxConnectionsAcceptPerFrame, GEngineIni);
	GConfig->GetBool(*IniSectionName, TEXT("DefaultReuseAddressAndPort"), Config.bReuseAddressAndPort, GEngineIni);

	// Apply per-port ini overrides
	TArray<FString> ListenerConfigs;
	if (GConfig->GetArray(*IniSectionName, TEXT("ListenerOverrides"), ListenerConfigs, GEngineIni))
	{
		for (FString ListenerConfigStr : ListenerConfigs)
		{
			ListenerConfigStr.TrimStartAndEndInline();
			ListenerConfigStr.ReplaceInline(TEXT("("), TEXT(""));
			ListenerConfigStr.ReplaceInline(TEXT(")"), TEXT(""));

			// Listener config overrides must specify a port
			uint32 ConfiguredPort = 0;
			if (!FParse::Value(*ListenerConfigStr, TEXT("Port="), ConfiguredPort))
			{
				UE_LOG(LogHttpServerConfig, Error,
					TEXT("ListenerOverride: %s does not specify required Port parameter"),
					*ListenerConfigStr);
				continue;
			}

			if (Port == ConfiguredPort)
			{
				// override defaults with config values
				FParse::Value(*ListenerConfigStr, TEXT("BindAddress="), Config.BindAddress);
				FParse::Value(*ListenerConfigStr, TEXT("BufferSize="), Config.BufferSize);
				FParse::Value(*ListenerConfigStr, TEXT("ConnectionsBacklogSize="), Config.ConnectionsBacklogSize);
				FParse::Value(*ListenerConfigStr, TEXT("MaxConnectionsAcceptPerFrame="), Config.MaxConnectionsAcceptPerFrame);
				FParse::Bool(*ListenerConfigStr, TEXT("ReuseAddressAndPort="), Config.bReuseAddressAndPort);
				break;
			}
		}
	}
	return Config;
}

const FHttpServerConnectionConfig FHttpServerConfig::GetConnectionConfig()
{
	static const FString IniSectionName(TEXT("HTTPServer.Connections"));

	// Code default values
	FHttpServerConnectionConfig Config;

	// Apply default ini configuration
	GConfig->GetFloat(*IniSectionName, TEXT("BeginReadWaitTimeMS"), Config.BeginReadWaitTimeMS, GEngineIni);

	return Config;
}
