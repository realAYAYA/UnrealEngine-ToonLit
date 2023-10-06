// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "StageAppBeaconReceiver.generated.h"

class FArrayReader;
class FInternetAddr;
class FSocket;

USTRUCT()
struct FRemoteControl_StageAppBeaconMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FRemoteControl_StageAppConnectionInfoMessage
{
	GENERATED_BODY()

	/** Version of the stage app beacon protocol to use. */
	uint8 Protocol;

	/** ID of the sender. */
	FGuid SenderId;

	/** The websocket port the app should connect to. */
	uint32 WebsocketPort;

	/** The websocket address the app should connect to. */
	FString WebsocketAddress;

	/** The user-friendly name of the connection to show to the user. */
	FString FriendlyName;
};

/**
 * Receives beacon messages from the Unreal Stage App and replies with connection information.
 * This allows the app to detect compatible Unreal instances on the local network and list them for the user.
 */
class FStageAppBeaconReceiver
	: public FRunnable
{
public:
	/** Creates and initializes a new beacon receiver. */
	FStageAppBeaconReceiver();

	/** Destroy the beacon receiver. */
	virtual ~FStageAppBeaconReceiver() {};

	/** Open a socket and start a thread listening for beacon messages. */
	void Startup();

	/** Close the socket and kill the listening thread. */
	void Shutdown();

protected:
	//~ Begin FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	//~ End FRunnable implementation

private:
	/** Called regularly to receive beacon messages. */
	void ReceiveBeaconMessages();

	/** Handle a message received on the beacon endpoint. */
	void HandleBeaconMessage(FArrayReader& MessageData, TSharedRef<FInternetAddr> Source);

	/** Get the name to report to apps searching for the engine. */
	FString GetFriendlyName() const;

	/** Identifier for this engine instance. Used by the app to differentiate instances even if they're on the same machine. */
	FGuid Guid;

	/** The websocket port to reply with. */
	uint32 WebsocketPort;

	/** Socket used to listen for and reply to beacon messages from the app. */
	FSocket* Socket = nullptr;

	/** Flag indicating that the thread is stopping. */
	TAtomic<bool> bStopping;

	/** The thread to receive beacon messages on. */
	TUniquePtr<FRunnableThread> Thread = nullptr;
};