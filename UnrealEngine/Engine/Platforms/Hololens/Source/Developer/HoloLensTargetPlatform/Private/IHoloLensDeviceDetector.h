// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

class FJsonObject;

namespace HoloLensDeviceTypes
{
	extern const FName HoloLens;
	extern const FName HoloLensEmulation;
}

enum class EHoloLensArchitecture : uint8
{
	Unknown = 0, X86 = 1, X64 = 2, ARM32 = 3, ARM64 = 4
};


struct FHoloLensDeviceInfo
{
	FString HostName;
	FString WdpUrl;
	FString WindowsDeviceId;
	FName DeviceTypeName;
	EHoloLensArchitecture Architecture;
	FString Username;
	FString Password;
	uint8 RequiresCredentials : 1;
	uint8 IsLocal : 1;
};

typedef TSharedPtr<class IHoloLensDeviceDetector, ESPMode::ThreadSafe> IHoloLensDeviceDetectorPtr;

class IHoloLensDeviceDetector
{
public:
	// Virtual destructor.
	virtual ~IHoloLensDeviceDetector() {};

	static IHoloLensDeviceDetectorPtr Create();

	virtual void StartDeviceDetection() = 0;
	virtual void StopDeviceDetection() = 0;

	DECLARE_EVENT_OneParam(IHoloLensDeviceDetectorModule, FOnDeviceDetected, const FHoloLensDeviceInfo&);
	virtual FOnDeviceDetected& OnDeviceDetected() = 0;

	virtual const TArray<FHoloLensDeviceInfo> GetKnownDevices() = 0;

	virtual void TryAddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password) = 0;
};

bool GetJsonField(FString& OutVal, const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName);
bool GetJsonField(int64& OutVal, const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName);

class SslCertDisabler
{
	bool prevValue;
public:
	SslCertDisabler();

	~SslCertDisabler();
};
