// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkFreeDConnectionSettings.generated.h"

USTRUCT()
struct LIVELINKFREED_API FLiveLinkFreeDConnectionSettings
{
	GENERATED_BODY()

public:
	/** IP address of the free-d tracking source */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	FString IPAddress = TEXT("127.0.0.1");

	/** UDP port number */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	uint16 UDPPortNumber = 40000;
};
