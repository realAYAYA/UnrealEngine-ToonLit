// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWebSocket.h"
#include "HAL/ThreadSafeCounter.h"
#include "WebSocketsModule.h"
#include "Templates/SharedPointer.h"
#include "PixelStreamingServersLog.h"
#include "Engine/EngineBaseTypes.h"
#include "ServerUtils.h"

namespace UE::PixelStreamingServers
{

	/* 
	* A utility class that tries to establish a websocket connection. 
	* Useful for testing whether servers have come online yet.
	*/
	class FWebSocketProbe
	{
	private:
		TSharedRef<IWebSocket> WebSocket;
		FThreadSafeBool bShouldAttemptReconnect;

	public:
		FWebSocketProbe(FURL Url, TArray<FString> Protocols = TArray<FString>())
			: WebSocket(FWebSocketsModule::Get().CreateWebSocket(Utils::ToString(Url), Protocols))
			, bShouldAttemptReconnect(true)
		{
			WebSocket->OnConnectionError().AddLambda([Url, &bShouldAttemptReconnect = bShouldAttemptReconnect](const FString& Error) {
				UE_LOG(LogPixelStreamingServers, Log, TEXT("Probing websocket %s | Msg= \"%s\" | Retrying..."), *Utils::ToString(Url),  *Error);
				bShouldAttemptReconnect = true;
			});
		}

		bool Probe()
		{
			bool bIsConnected = WebSocket->IsConnected();

			if (!bIsConnected && bShouldAttemptReconnect)
			{
				WebSocket->Connect();
				bShouldAttemptReconnect = false;
			}

			return bIsConnected;
		}
	};

} // UE::PixelStreamingServers