// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemSteamTypes.h"
#include "OnlineSubsystemUtilsPackage.h"
#include "VoiceEngineImpl.h"

class IOnlineSubsystem;
class FUniqueNetIdSteam;

#define INVALID_INDEX -1

/**
* Generic implementation of voice engine, using Voice module for capture/codec
*/
class FVoiceEngineSteam : public FVoiceEngineImpl
{
	/** Steam User interface */
	class ISteamUser* SteamUserPtr;
	/** Steam Friends interface */
	class ISteamFriends* SteamFriendsPtr;

	/** Start capturing voice data */
	virtual void StartRecording() const override;

	/** Called when "last half second" is over */
	virtual void StoppedRecording() const override;

PACKAGE_SCOPE:

	/** Constructor */
	FVoiceEngineSteam() :
		FVoiceEngineImpl(),
		SteamUserPtr(nullptr),
		SteamFriendsPtr(nullptr)
	{};

public:

	FVoiceEngineSteam(IOnlineSubsystem* InSubsystem);
	virtual ~FVoiceEngineSteam();
};
