// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "IMessageTransport.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

struct FWebSocketMessageConnection
{
	FWebSocketMessageConnection() = delete;
	FWebSocketMessageConnection(const FString& InUrl, FGuid InGuid, TSharedRef<class IWebSocket, ESPMode::ThreadSafe> InWebSocketConnection) :
		Url(InUrl),
		Guid(InGuid),
		WebSocketConnection(InWebSocketConnection),
		WebSocketServerConnection(nullptr),
		bIsConnecting(true),
		bDestroyed(false)
	{

	}

	FWebSocketMessageConnection(const FString& InUrl, FGuid InGuid, class INetworkingWebSocket* InWebSocketServerConnection) :
		Url(InUrl),
		Guid(InGuid),
		WebSocketConnection(nullptr),
		WebSocketServerConnection(InWebSocketServerConnection),
		bIsConnecting(true),
		bDestroyed(false)
	{

	}

	bool IsConnected() const;

	void Close();

	/** The WebSocket url */
	const FString Url;

	/** The message transport Guid */
	const FGuid Guid;

	/** Reference to the client websocket connection */
	TSharedPtr<class IWebSocket, ESPMode::ThreadSafe> WebSocketConnection;

	/** Reference to the server websocket connection */
	class INetworkingWebSocket* WebSocketServerConnection;

	/** Is the socket still trying to connect? */
	bool bIsConnecting;

	/** The socket is about to be destroyed */
	bool bDestroyed;

	/** Retry timer */
	FTSTicker::FDelegateHandle RetryHandle;
};

using FWebSocketMessageConnectionRef = TSharedRef<FWebSocketMessageConnection, ESPMode::ThreadSafe>;

class FWebSocketMessageTransport : public IMessageTransport, public TSharedFromThis<FWebSocketMessageTransport>
{
public:
	FWebSocketMessageTransport();
	virtual ~FWebSocketMessageTransport() override;

	virtual FName GetDebugName() const override
	{
		return "WebSocketMessageTransport";
	}

	virtual bool StartTransport(IMessageTransportHandler& Handler) override;

	virtual void StopTransport() override;

	virtual bool TransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const TArray<FGuid>& Recipients) override;

	virtual void OnJsonMessage(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection);
	virtual void OnConnected(FWebSocketMessageConnectionRef WebSocketMessageConnection);
	virtual void OnClosed(int32 Code, const FString& Reason, bool bUserClose, FWebSocketMessageConnectionRef WebSocketMessageConnection);
	virtual void OnConnectionError(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection);

	virtual void RetryConnection(FWebSocketMessageConnectionRef WebSocketMessageConnection);

	virtual void ClientConnected(class INetworkingWebSocket* NetworkingWebSocket);

	virtual bool ServerTick(float DeltaTime);

	virtual void OnServerJsonMessage(void* Data, int32 DataSize, FWebSocketMessageConnectionRef WebSocketMessageConnection);
	virtual void OnServerConnectionClosed(FWebSocketMessageConnectionRef WebSocketMessageConnection);

protected:
	void ForgetTransportNode(FWebSocketMessageConnectionRef WebSocketMessageConnection);
	
	IMessageTransportHandler* TransportHandler;
	TMap<FGuid, FWebSocketMessageConnectionRef> WebSocketMessageConnections;

	TUniquePtr<class IWebSocketServer> Server;
	FTSTicker::FDelegateHandle ServerTickerHandle;
};
