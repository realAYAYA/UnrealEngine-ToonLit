// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkVRPNConnectionSettings.generated.h"

UENUM(BlueprintType)
enum class EVRPNDeviceType : uint8
{
	Analog,
	Dial,
	Button,
	Tracker,
};

USTRUCT()
struct LIVELINKVRPN_API FLiveLinkVRPNConnectionSettings
{
	GENERATED_BODY()

	/** IP address of the VRPN server */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	FString IPAddress = TEXT("127.0.0.1");

	/** Maximum rate (in Hz) at which to ask the VRPN server to update */
	UPROPERTY(EditAnywhere, Category = "Connection Settings", meta = (ClampMin = 1, ClampMax = 1000))
	uint32 LocalUpdateRateInHz = 120;

	/** VRPN device name */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	FString DeviceName = TEXT("Mouse0");

	/** LiveLink subject name */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	FString SubjectName = TEXT("MouseAxes");

	/** Device type */
	UPROPERTY(EditAnywhere, Category = "Connection Settings")
	EVRPNDeviceType Type = EVRPNDeviceType::Analog;
};
