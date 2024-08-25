// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "VCamCoreUserSettings.generated.h"

/**
 * Virtual Camera Core User Settings
 */
UCLASS(config = VirtualCameraCore)
class VCAMCORE_API UVirtualCameraCoreUserSettings : public UObject
{
	GENERATED_BODY()

public:

	/** The IP endpoint to listen to for app discovery messages. */
	UPROPERTY(config, EditAnywhere, Category = "Virtual Camera Core|Remote Discovery")
	FString DiscoveryEndpoint = "230.0.0.3";

	/** The port to listen to for app discovery messages. */
	UPROPERTY(config, EditAnywhere, Category = "Virtual Camera Core|Remote Discovery")
	int32 DiscoveryPort = 6667;
};