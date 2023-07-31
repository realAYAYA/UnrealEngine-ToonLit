// Copyright Epic Games, Inc. All Rights Reserved.

#include "IWebSocketNetworkingModule.h"
#include "WebSocketNetworkingPrivate.h"
#include "WebSocketServer.h"
#include "WebSocket.h"



class FWebSocketNetworkingPlugin : public IWebSocketNetworkingModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}


public:
	TUniquePtr<IWebSocketServer> CreateServer() override
	{
		return MakeUnique<FWebSocketServer>();
	}

	TSharedPtr<INetworkingWebSocket> CreateConnection(const FInternetAddr& ServerAddress) override
	{
		return MakeShared<FWebSocket>(ServerAddress);
	}

};

IMPLEMENT_MODULE(FWebSocketNetworkingPlugin, WebSocketNetworking)

DEFINE_LOG_CATEGORY(LogWebSocketNetworking);


