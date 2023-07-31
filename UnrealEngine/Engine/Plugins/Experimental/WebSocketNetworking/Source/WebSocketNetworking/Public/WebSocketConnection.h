// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/NetConnection.h"
#include "WebSocketConnection.generated.h"


UCLASS(transient, config = Engine)
class WEBSOCKETNETWORKING_API UWebSocketConnection : public UNetConnection
{
	GENERATED_UCLASS_BODY()

	class INetworkingWebSocket* WebSocket;

	//~ Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	FString LowLevelDescribe() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FinishDestroy();
	virtual void ReceivedRawPacket(void* Data,int32 Count);
	//~ End NetConnection Interface


	void SetWebSocket(INetworkingWebSocket* InWebSocket);
	INetworkingWebSocket* GetWebSocket();

	bool bChallengeHandshake = false;
};
