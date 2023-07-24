// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensTargetDevice.h: Declares the HoloLensTargetDevice class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetDevice.h"
#include "Interfaces/ITargetPlatform.h"
#include "IHoloLensDeviceDetector.h"

#if WITH_ENGINE
#include "Engine/EngineTypes.h"
#endif

#include "Windows/AllowWindowsPlatformTypes.h"


/**
 * Implements a HoloLens target device.
 */
class FHoloLensTargetDevice
	: public ITargetDevice
{
public:

	/**
	* Creates and initializes a new device for the specified target platform.
	*
	* @param InTargetPlatform - The target platform.
	*/
	FHoloLensTargetDevice(const ITargetPlatform& InTargetPlatform, const FHoloLensDeviceInfo& InInfo);

	~FHoloLensTargetDevice();

	virtual bool Connect() override;

	virtual void Disconnect() override;

	virtual ETargetDeviceTypes GetDeviceType() const override;

	virtual FTargetDeviceId GetId() const override;

	virtual FString GetName() const override;

	virtual FString GetOperatingSystemName() override;

	virtual bool GetProcessSnapshotAsync(TFunction<void(const TArray<FTargetDeviceProcessInfo>&)> CompleteHandler) override;

	virtual int32 GetProcessSnapshot(TArray<FTargetDeviceProcessInfo>& OutProcessInfos) override { return 0; }

	virtual const class ITargetPlatform& GetTargetPlatform() const override;

	virtual bool GetUserCredentials(FString& OutUserName, FString& OutUserPassword) override;

	virtual bool IsConnected();

	virtual bool IsDefault() const override;

	virtual bool PowerOff(bool Force) override;

	virtual bool PowerOn() override;

	virtual bool Reboot(bool bReconnect = false) override;

	virtual void SetUserCredentials(const FString& UserName, const FString& UserPassword) override;

	virtual bool SupportsFeature(ETargetDeviceFeatures Feature) const override;

	virtual bool TerminateProcess(const int64 ProcessId) override;

	virtual bool TerminateLaunchedProcess(const FString & ProcessIdentifier) override;

private:

	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> GenerateRequest() const;

	void StartHeartBeat();

#if WITH_ENGINE
	FTimerHandle TimerHandle;
#endif

	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HeartBeatRequest;
	FCriticalSection CriticalSection;

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	FHoloLensDeviceInfo Info;

	volatile bool IsDeviceConnected;
};

typedef TSharedPtr<FHoloLensTargetDevice, ESPMode::ThreadSafe> FHoloLensDevicePtr;

#include "Windows/HideWindowsPlatformTypes.h"
