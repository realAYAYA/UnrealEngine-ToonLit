// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "IOSMessageProtocol.generated.h"


USTRUCT()
struct FIOSLaunchDaemonPing
{
	GENERATED_USTRUCT_BODY()

	//FIOSLaunchDaemonPing() {}
};


USTRUCT()
struct FIOSLaunchDaemonPong
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceID;

	UPROPERTY()
	FString DeviceName;

	UPROPERTY()
	FString DeviceStatus;

	UPROPERTY()
	FString DeviceType;

	UPROPERTY()
	bool bCanPowerOff;

	UPROPERTY()
	bool bCanPowerOn;

	UPROPERTY()
	bool bIsAuthorized;

	UPROPERTY()
	bool bCanReboot;

	FIOSLaunchDaemonPong()
		: bCanPowerOff(false)
		, bCanPowerOn(false)
		, bIsAuthorized(false)
		, bCanReboot(false)
	{}

	FIOSLaunchDaemonPong(FString InDeviceID, FString InDeviceName, FString InDeviceStatus, FString InDeviceType, bool bInCanPowerOff, bool bInCanPowerOn, bool bInCanReboot, bool bIsAuthorized)
		: DeviceID(InDeviceID)
		, DeviceName(InDeviceName)
		, DeviceStatus(InDeviceStatus)
		, DeviceType(InDeviceType)
		, bCanPowerOff(bInCanPowerOff)
		, bCanPowerOn(bInCanPowerOn)
		, bIsAuthorized(bIsAuthorized)
		, bCanReboot(bInCanReboot)
	{}
};


USTRUCT()
struct FIOSLaunchDaemonLaunchApp
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString AppID;

	UPROPERTY()
	FString Parameters;

	FIOSLaunchDaemonLaunchApp() {}

	FIOSLaunchDaemonLaunchApp(FString InAppID, FString Params)
		: AppID(InAppID)
		, Parameters(Params)
	{}
};
