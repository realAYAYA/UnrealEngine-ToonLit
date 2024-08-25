// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "WebSocketMessagingSettings.generated.h"

UENUM()
enum class EWebSocketMessagingTransportFormat : uint8
{
	Json,
	Cbor
};

UCLASS(config=Engine)
class UWebSocketMessagingSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Whether the WebSocket transport channel is enabled */
	UPROPERTY(config, EditAnywhere, Category = Transport)
	bool EnableTransport = false;

	/** Bind the WebSocket server on the specified port (0 disables it) */
	UPROPERTY(config, EditAnywhere, Category = Transport)
	int32 ServerPort = 0;

	/** Format used to serialize the messages on the server's WebSockets.*/
	UPROPERTY(Config, EditAnywhere, Category = Transport)
	EWebSocketMessagingTransportFormat ServerTransportFormat = EWebSocketMessagingTransportFormat::Cbor;
	
    /** The WebSocket Urls to connect to (Eg. ws://example.com/xyz) */
    UPROPERTY(config, EditAnywhere, Category = Transport)
    TArray<FString> ConnectToEndpoints;

	/** Additional HTTP headers to set when connecting to endpoints */
	UPROPERTY(config, EditAnywhere, Category = Transport)
	TMap<FString, FString> HttpHeaders;
};
