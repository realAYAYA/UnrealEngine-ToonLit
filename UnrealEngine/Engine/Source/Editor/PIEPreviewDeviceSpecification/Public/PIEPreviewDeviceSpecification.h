// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "PIEPreviewDeviceSpecification.generated.h"

UENUM()
enum class EPIEPreviewDeviceType : uint8
{
	Unset,
	Android,
	IOS,
	TVOS,
	Switch,
	MAX,
};

UCLASS()
class PIEPREVIEWDEVICESPECIFICATION_API UPIEPreviewDeviceSpecification : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	EPIEPreviewDeviceType PreviewDeviceType;

	UPROPERTY()
	FString GPUFamily;
	UPROPERTY()
	FString GLVersion;
	UPROPERTY()
	FString VulkanVersion;
	UPROPERTY()
	FString AndroidVersion;
	UPROPERTY()
	FString DeviceMake;
	UPROPERTY()
	FString DeviceModel;
	UPROPERTY()
	FString DeviceBuildNumber;
	UPROPERTY()
	bool UsingHoudini;
	UPROPERTY()
	FString Hardware;
	UPROPERTY()
	FString Chipset;
};

USTRUCT()
struct FPIERHIOverrideState
{
public:
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeX = 0;
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeY = 0;
	UPROPERTY()
	int32 MaxTextureDimensions = 0;
	UPROPERTY()
	int32 MaxCubeTextureDimensions = 0;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_G8 = false;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_FloatRGBA = false;
	UPROPERTY()
	bool SupportsMultipleRenderTargets = false;
};

USTRUCT()
struct FPIEAndroidDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString GPUFamily;
	UPROPERTY()
	FString GLVersion;
	UPROPERTY()
	FString VulkanVersion;
	UPROPERTY()
	FString AndroidVersion;
	UPROPERTY()
	FString DeviceMake;
	UPROPERTY()
	FString DeviceModel;
	UPROPERTY()
	FString DeviceBuildNumber;
	UPROPERTY()
	bool VulkanAvailable = false;
	UPROPERTY()
	bool UsingHoudini = false;
	UPROPERTY()
	FString Hardware;
	UPROPERTY()
	FString Chipset;
	UPROPERTY()
	FString TotalPhysicalGB;
	UPROPERTY()
	FString HMDSystemName;

	UPROPERTY()
	FPIERHIOverrideState GLES31RHIState;
	UPROPERTY()
	bool SM5Available = false;
// 	UPROPERTY()
// 	FPIERHIOverrideState VulkanRHIState;
};

USTRUCT()
struct FPIEIOSDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceModel;

	UPROPERTY()
	float NativeScaleFactor = 0.0f;

	UPROPERTY()
	FPIERHIOverrideState MetalRHIState;
};

USTRUCT()
struct FPIESwitchDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool Docked = false;
};


USTRUCT()
struct FPIEPreviewDeviceBezelViewportRect
{
public:
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	int32 X = 0;
	UPROPERTY()
	int32 Y = 0;
	UPROPERTY()
	int32 Width = 0;
	UPROPERTY()
	int32 Height = 0;
}; 

USTRUCT()
struct FPIEBezelProperties
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceBezelFile;
	UPROPERTY()
	FPIEPreviewDeviceBezelViewportRect BezelViewportRect;
};

USTRUCT()
struct FPIEPreviewDeviceSpecifications
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	EPIEPreviewDeviceType DevicePlatform = EPIEPreviewDeviceType::Unset;
	UPROPERTY()
	int32 ResolutionX = 0;
	UPROPERTY()
	int32 ResolutionY = 0;
	UPROPERTY()
	int32 ResolutionYImmersiveMode = 0;

	UPROPERTY()
	int32 PPI = 0;

	UPROPERTY()
	TArray<float> ScaleFactors;
	
	UPROPERTY()
	FPIEBezelProperties BezelProperties;

	UPROPERTY()
	FPIEAndroidDeviceProperties AndroidProperties;

	UPROPERTY()
	FPIEIOSDeviceProperties IOSProperties;

	UPROPERTY()
	FPIESwitchDeviceProperties SwitchProperties;
};
