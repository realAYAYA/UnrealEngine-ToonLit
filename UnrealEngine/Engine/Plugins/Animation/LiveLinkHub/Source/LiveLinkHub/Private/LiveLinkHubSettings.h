// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LiveLinkHubSettings.generated.h"

/**
 * Settings for LiveLinkHub.
 */
UCLASS(config=Game, defaultconfig)
class LIVELINKHUB_API ULiveLinkHubSettings : public UObject
{
	GENERATED_BODY()

public:
	/** If enabled, discovered clients will be automatically added to the current session. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub")
	bool bAutoAddDiscoveredClients = true;
};
