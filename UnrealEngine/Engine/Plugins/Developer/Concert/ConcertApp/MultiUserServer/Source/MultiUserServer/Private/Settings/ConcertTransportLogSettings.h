// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertFrontendUtils.h"
#include "UObject/Object.h"
#include "ConcertTransportLogSettings.generated.h"

/**
 * 
 */
UCLASS(config = MultiUserServerUserSettings, DefaultConfig)
class MULTIUSERSERVER_API UConcertTransportLogSettings : public UObject
{
	GENERATED_BODY()
public:

	static UConcertTransportLogSettings* GetSettings();

	/** How to displayed FConcertLog::Timestamp */
	UPROPERTY(Config)
	ETimeFormat TimestampTimeFormat = ETimeFormat::Absolute;
};
