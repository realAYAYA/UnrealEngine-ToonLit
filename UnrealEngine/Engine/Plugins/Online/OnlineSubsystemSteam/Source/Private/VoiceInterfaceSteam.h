// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceDataCommon.h"
#include "VoicePacketImpl.h"
#include "OnlineSubsystemSteamTypes.h"
#include "OnlineSubsystemSteamPackage.h"
#include "OnlineSubsystemUtilsPackage.h"
#include "VoiceInterfaceImpl.h"
#include "VoiceEngineSteam.h"

/**
 * The Steam implementation of the voice interface 
 */

class ONLINESUBSYSTEMSTEAM_API FOnlineVoiceSteam : public FOnlineVoiceImpl
{
PACKAGE_SCOPE:
	FOnlineVoiceSteam() : FOnlineVoiceImpl()
	{};

public:

	/** Constructor */
	FOnlineVoiceSteam(class IOnlineSubsystem* InOnlineSubsystem) :
		FOnlineVoiceImpl(InOnlineSubsystem)
	{
		check(InOnlineSubsystem);
	};

	virtual IVoiceEnginePtr CreateVoiceEngine() override
	{
		return MakeShareable(new FVoiceEngineSteam(OnlineSubsystem));
	}

	/** Virtual destructor to force proper child cleanup */
	virtual ~FOnlineVoiceSteam() override {};
};

typedef TSharedPtr<FOnlineVoiceSteam, ESPMode::ThreadSafe> FOnlineVoiceSteamPtr;
