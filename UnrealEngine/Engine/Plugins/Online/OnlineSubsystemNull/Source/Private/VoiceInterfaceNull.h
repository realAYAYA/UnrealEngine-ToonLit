// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VoiceInterfaceImpl.h"
#include "Online/CoreOnline.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceDataCommon.h"
#include "OnlineSubsystemUtilsPackage.h"

class ONLINESUBSYSTEMNULL_API FOnlineVoiceImplNull : public FOnlineVoiceImpl {
PACKAGE_SCOPE:
	FOnlineVoiceImplNull() : FOnlineVoiceImpl()
	{};

public:
	/** Constructor */
	FOnlineVoiceImplNull(class IOnlineSubsystem* InOnlineSubsystem) :
		FOnlineVoiceImpl(InOnlineSubsystem)
	{
	};

	virtual ~FOnlineVoiceImplNull() override {}
};

typedef TSharedPtr<FOnlineVoiceImpl, ESPMode::ThreadSafe> FOnlineVoiceImplPtr;
