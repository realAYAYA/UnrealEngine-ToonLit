// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "NetworkDelegates.h"
#include "PendingNetGame.generated.h"

class UEngine;
class UNetConnection;
class UNetDriver;
struct FWorldContext;

UCLASS(customConstructor, transient)
class ENGINE_API UPendingNetGame :
	public UObject,
	public FNetworkNotify
{
	GENERATED_BODY()

public:

	/** 
	 * Net driver created for contacting the new server
	 * Transferred to world on successful connection
	 */
	UPROPERTY()
	TObjectPtr<class UNetDriver>		NetDriver;

private:
	/** 
	 * Demo Net driver created for loading demos, but we need to go through pending net game
	 * Transferred to world on successful connection
	 */
	UPROPERTY()
	TObjectPtr<class UDemoNetDriver>	DemoNetDriver;

public:
	/** Gets the demo net driver for this pending world. */
	UDemoNetDriver* GetDemoNetDriver() const { return DemoNetDriver; }

	/** Sets the demo net driver for this pending world. */
	void SetDemoNetDriver(UDemoNetDriver* const InDemoNetDriver) { DemoNetDriver = InDemoNetDriver; }

	/**
	 * Setup the connection for encryption with a given key
	 * All future packets are expected to be encrypted
	 *
	 * @param Response response from the game containing its encryption key or an error message
	 * @param WeakConnection the connection related to the encryption request
	 */
	void FinalizeEncryptedConnection(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection);

	/**
	 * Set the encryption key for the connection. This doesn't cause outgoing packets to be encrypted,
	 * but it allows the connection to decrypt any incoming packets if needed.
	 *
	 * @param Response response from the game containing its encryption key or an error message
	 */
	void SetEncryptionKey(const FEncryptionKeyResponse& Response);

	bool HasFailedTravel() const {return bFailedTravel; }
	void SetFailedTravel(bool bInFailedTravel) { bFailedTravel = bInFailedTravel; }

public:
	/** URL associated with this level. */
	FURL					URL;

	/** @todo document */
	bool					bSuccessfullyConnected;

	/** @todo document */
	bool					bSentJoinRequest;

	/** set when we call LoadMapCompleted */
	bool					bLoadedMapSuccessfully;
private:
	/** initialized to true, delaytravel steps can set this to false to indicate error during pendingnetgame travel */
	bool					bFailedTravel;
public:
	/** @todo document */
	FString					ConnectionError;

	// Constructor.
	void Initialize(const FURL& InURL);

	// Constructor.
	UPendingNetGame(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void	InitNetDriver();

	/**
	 * Begin initial handshake if needed, or call SendInitialJoin.
	 */
	void BeginHandshake();

	/**
	 * Send the packet for triggering the initial join
	 */
	void SendInitialJoin();

	//~ Begin FNetworkNotify Interface.
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection ) override;
	virtual bool NotifyAcceptingChannel( class UChannel* Channel ) override;
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) override;
	//~ End FNetworkNotify Interface.

	/**  Update the pending level's status. */
	virtual void Tick( float DeltaTime );

	/** @todo document */
	virtual UNetDriver* GetNetDriver() { return NetDriver; }

	/** Send JOIN to other end */
	virtual void SendJoin();

	//~ Begin UObject Interface.
	virtual void Serialize( FArchive& Ar ) override;

	virtual void FinishDestroy() override
	{
		NetDriver = NULL;
		
		Super::FinishDestroy();
	}
	
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.
	

	/** Create the peer net driver and a socket to listen for new client peer connections. */
	void InitPeerListen();

	/** Called by the engine after it calls LoadMap for this PendingNetGame. */
	virtual bool LoadMapCompleted(UEngine* Engine, FWorldContext& Context, bool bLoadedMapSuccessfully, const FString& LoadMapError);

	/** Called by the engine after loadmapCompleted and the GameInstance has finished delaying */
	virtual void TravelCompleted(UEngine* Engine, FWorldContext& Context);
};
