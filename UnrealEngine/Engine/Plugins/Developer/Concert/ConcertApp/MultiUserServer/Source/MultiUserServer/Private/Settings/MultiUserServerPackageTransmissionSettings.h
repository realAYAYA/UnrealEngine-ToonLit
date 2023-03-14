// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertFrontendUtils.h"
#include "UObject/Object.h"
#include "MultiUserServerPackageTransmissionSettings.generated.h"

/**
 * 
 */
UCLASS(config = MultiUserServerUserSettings, DefaultConfig)
class MULTIUSERSERVER_API UMultiUserServerPackageTransmissionSettings : public UObject
{
	GENERATED_BODY()
public:

	static UMultiUserServerPackageTransmissionSettings* GetSettings();

	/** How to display FPackageTransmissionEntry::Time */
	UPROPERTY(Config)
	ETimeFormat TimestampTimeFormat = ETimeFormat::Absolute;
};
