// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AppleARKitLiveLinkConnectionSettings.generated.h"

USTRUCT()
struct APPLEARKITFACESUPPORT_API FAppleARKitLiveLinkConnectionSettings
{
	GENERATED_BODY()

	/** The port to use when listening/sending LiveLink face blend shapes via the network */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	int32 Port = 11111;
};
