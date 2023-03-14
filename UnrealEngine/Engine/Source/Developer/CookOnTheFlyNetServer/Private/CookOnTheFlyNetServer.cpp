// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookOnTheFlyNetServer.h"
#include "Modules/ModuleManager.h"
#include "TCPServer.h"
#include "PlatformProtocolServer.h"

DEFINE_LOG_CATEGORY(LogCookOnTheFlyNetworkServer);

class FCookOnTheFlyNetworkServerModule final
	: public UE::Cook::ICookOnTheFlyNetworkServerModule
{
public:
	virtual TSharedPtr<UE::Cook::ICookOnTheFlyNetworkServer> CreateServer(const UE::Cook::FCookOnTheFlyNetworkServerOptions& Options) override
	{
		using namespace UE::Cook;

		switch (Options.Protocol)
		{
		case ECookOnTheFlyNetworkServerProtocol::Tcp:
			return MakeShared<FCookOnTheFlyServerTCP>(Options.Port, Options.TargetPlatforms, Options.ZenProjectName);
		case ECookOnTheFlyNetworkServerProtocol::Platform:
			return MakeShared<FCookOnTheFlyServerPlatformProtocol>(Options.TargetPlatforms, Options.ZenProjectName);
		default:
			UE_LOG(LogCookOnTheFlyNetworkServer, Fatal, TEXT("Unsupported protocol: %d"), Options.Protocol);
			return nullptr;
		}
	}
};

IMPLEMENT_MODULE(FCookOnTheFlyNetworkServerModule, CookOnTheFlyNetServer);
