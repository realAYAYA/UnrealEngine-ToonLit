// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetConnection.h"
#include "ChildConnection.generated.h"

/**
 * Represents a secondary split screen connection that reroutes calls to the parent connection.
 */
UCLASS(MinimalAPI,transient,config=Engine)
class UChildConnection
	: public UNetConnection
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(transient)
	TObjectPtr<class UNetConnection> Parent;

public:

	UNetConnection* GetParentConnection() { return Parent; }

	// UNetConnection interface.

	virtual UChildConnection* GetUChildConnection() override
	{
		return this;
	}

	virtual FString LowLevelGetRemoteAddress(bool bAppendPort=false) override
	{
		return Parent->LowLevelGetRemoteAddress(bAppendPort);
	}

	virtual FString LowLevelDescribe() override
	{
		return Parent->LowLevelDescribe();
	}

	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override
	{
	}

	virtual void InitSendBuffer() override
	{
		Parent->InitSendBuffer();
	}

	virtual void AssertValid() override
	{
		Parent->AssertValid();
	}

	virtual void FlushNet(bool bIgnoreSimulation = false) override
	{
		Parent->FlushNet(bIgnoreSimulation);
	}

	virtual int32 IsNetReady(bool Saturate) override
	{
		return Parent->IsNetReady(Saturate);
	}

	virtual bool IsEncryptionEnabled() const override
	{
		return Parent->IsEncryptionEnabled();
	}

	virtual void Tick(float DeltaSeconds) override
	{
		SetConnectionState(Parent->GetConnectionState());
	}

	virtual void HandleClientPlayer(class APlayerController* PC, class UNetConnection* NetConnection) override;
	virtual void CleanUp() override;

	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}

	virtual void InitChildConnection(UNetDriver* InDriver, UNetConnection* InParent);
};
