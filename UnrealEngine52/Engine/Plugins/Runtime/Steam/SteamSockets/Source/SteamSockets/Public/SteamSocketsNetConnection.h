// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/NetConnection.h"
#include "SteamSocketsNetConnection.generated.h"

// Forward declare some types for functional type arguments
class FSocket;
class UNetDriver;
class FInternetAddr;
class FSteamSocket;

UCLASS(transient, config=Engine)
class STEAMSOCKETS_API USteamSocketsNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:

	USteamSocketsNetConnection() :
		ConnectionSocket(nullptr),
		bInConnectionlessHandshake(false)
	{
	}

	//~ Begin NetConnection Interface
	virtual void CleanUp() override;
	virtual void InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, const FInternetAddr& InRemoteAddr, 
		EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	FString LowLevelGetRemoteAddress(bool bAppendPort=false) override;
	FString LowLevelDescribe() override;
	//~ End NetConnection Interface

private:
	const FSteamSocket* GetRawSocket() const { return ConnectionSocket; }

	void HandleRecvMessage(void* InData, int32 SizeOfData, const FInternetAddr* InFormattedAddress);

	void FlagForHandshake() { bInConnectionlessHandshake = true; }

	void ClearSocket() { ConnectionSocket = nullptr; }

	FSteamSocket* ConnectionSocket;
	bool bInConnectionlessHandshake;

	friend class USteamSocketsNetDriver;
};
