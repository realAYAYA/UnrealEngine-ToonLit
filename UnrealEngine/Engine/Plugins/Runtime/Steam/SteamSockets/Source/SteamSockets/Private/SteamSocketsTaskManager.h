// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamSocketsTaskManagerInterface.h"
#include "SteamSocketsPrivate.h"
#include "Containers/Queue.h"

// A glorified event manager for handling status updates about SteamSockets we hold.
class FSteamSocketsTaskManager : public FSteamSocketsTaskManagerInterface
{
public:
	FSteamSocketsTaskManager(FSteamSocketsSubsystem* SocketSub) :
		FSteamSocketsTaskManagerInterface(SocketSub),
	    OnConnectionStatusChangedCallback(this, &FSteamSocketsTaskManager::OnConnectionStatusChanged),
		OnConnectionStatusChangedGSCallback(this, &FSteamSocketsTaskManager::OnConnectionStatusChangedGS)
	{
		
	}

	virtual ~FSteamSocketsTaskManager()
	{
		MessageQueue.Empty();
	}

	virtual void Tick() override;


private:
	STEAM_CALLBACK(FSteamSocketsTaskManager, OnConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t, OnConnectionStatusChangedCallback);
	STEAM_GAMESERVER_CALLBACK(FSteamSocketsTaskManager, OnConnectionStatusChangedGS, SteamNetConnectionStatusChangedCallback_t, OnConnectionStatusChangedGSCallback);

	// A copied queue of our messages we get from the SteamAPI. There's no difference between client and server messages
	// just how they are registered.
	TQueue<SteamNetConnectionStatusChangedCallback_t> MessageQueue;
};
