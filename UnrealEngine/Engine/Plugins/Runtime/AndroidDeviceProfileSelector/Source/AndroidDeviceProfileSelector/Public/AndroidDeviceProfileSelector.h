// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "UObject/NameTypes.h"

namespace FAndroidProfileSelectorSourceProperties
{
	static FName SRC_GPUFamily(TEXT("SRC_GPUFamily"));
	static FName SRC_GLVersion(TEXT("SRC_GLVersion"));
	static FName SRC_VulkanAvailable(TEXT("SRC_VulkanAvailable"));
	static FName SRC_VulkanVersion(TEXT("SRC_VulkanVersion"));
	static FName SRC_AndroidVersion(TEXT("SRC_AndroidVersion"));
	static FName SRC_DeviceMake(TEXT("SRC_DeviceMake"));
	static FName SRC_DeviceModel(TEXT("SRC_DeviceModel"));
	static FName SRC_DeviceBuildNumber(TEXT("SRC_DeviceBuildNumber"));
	static FName SRC_UsingHoudini(TEXT("SRC_UsingHoudini"));
	static FName SRC_Hardware(TEXT("SRC_Hardware"));
	static FName SRC_Chipset(TEXT("SRC_Chipset"));
	static FName SRC_HMDSystemName(TEXT("SRC_HMDSystemName"));
	static FName SRC_TotalPhysicalGB(TEXT("SRC_TotalPhysicalGB"));
	static FName SRC_SM5Available(TEXT("SRC_SM5Available"));
};

class ANDROIDDEVICEPROFILESELECTOR_API FAndroidDeviceProfileSelector
{
	// Container of various device properties used for device profile matching.
	static TMap<FName, FString> SelectorProperties;
	static void VerifySelectorParams();
public:

	static FString FindMatchingProfile(const FString& FallbackProfileName);
	static int32 GetNumProfiles();
	static void SetSelectorProperties(const TMap<FName, FString>& Params) { SelectorProperties = Params; VerifySelectorParams(); }
	static const TMap<FName, FString>& GetSelectorProperties() { return SelectorProperties; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
