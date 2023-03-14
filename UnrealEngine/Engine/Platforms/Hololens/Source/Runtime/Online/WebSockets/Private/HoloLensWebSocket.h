// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"

#if WITH_WEBSOCKETS

#include "IWebSocket.h"

class FHoloLensWebSocket :
	public IWebSocket,
	public FTSTickerObjectBase,
	public TSharedFromThis<FHoloLensWebSocket>
{
public:
	FHoloLensWebSocket(const FString& Uri, const TArray<FString>& Protocols, const TMap<FString, FString>& Headers);

	virtual void Connect() override;
	virtual void Close(int32 Code = 1000, const FString& Reason = FString()) override;
	virtual bool IsConnected() override;
	virtual void Send(const FString& Data) override;
	virtual void Send(const void* Utf8Data, SIZE_T Size, bool bIsBinary) override;

	virtual FWebSocketConnectedEvent& OnConnected() override				{ return ConnectedEvent; }
	virtual FWebSocketConnectionErrorEvent& OnConnectionError() override	{ return ConnectionErrorEvent; }
	virtual FWebSocketClosedEvent& OnClosed() override						{ return ClosedEvent; }
	virtual FWebSocketMessageEvent& OnMessage() override					{ return MessageEvent; }
	virtual FWebSocketRawMessageEvent& OnRawMessage() override				{ return RawMessageEvent; }
	virtual FWebSocketMessageSentEvent& OnMessageSent() override { return OnMessageSentEvent; }

public:
	virtual bool Tick(float DeltaTime) override;

private:
	void FinishClose(int32 Code, const FString& Reason);

	TQueue<TFunction<void()>, EQueueMode::Mpsc> GameThreadWork;

	Windows::Networking::Sockets::MessageWebSocket^ Socket;
	Windows::Storage::Streams::DataWriter^ Writer;

	Windows::Foundation::IAsyncAction^ ConnectAction;
	TArray<Windows::Storage::Streams::DataWriterStoreOperation^> SendOperations;
	FString Uri;

	FWebSocketConnectedEvent ConnectedEvent;
	FWebSocketConnectionErrorEvent ConnectionErrorEvent;
	FWebSocketClosedEvent ClosedEvent;
	FWebSocketMessageEvent MessageEvent;
	FWebSocketRawMessageEvent RawMessageEvent;
	FWebSocketMessageSentEvent OnMessageSentEvent;

	bool bUserClose;
};

#endif