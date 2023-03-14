// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IAndroidDeviceDetection.h: Declares the IAndroidDeviceDetection interface.
=============================================================================*/

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMap;

struct FAndroidDeviceInfo
{
	// Device serial number, used to route ADB commands to a specific device
	FString SerialNumber;

	// Device model name
	FString Model;

	// Device name
	FString DeviceName;

	// User-visible version of android installed (ro.build.version.release)
	FString HumanAndroidVersion;

	// Android SDK version supported by the device (ro.build.version.sdk - note: deprecated in 4 according to docs, but version 4 devices return an empty string when querying the 'replacement' SDK_INT)
	int32 SDKVersion;

	// List of supported OpenGL extensions (retrieved via SurfaceFlinger)
	FString GLESExtensions;

	// Supported GLES version (ro.opengles.version)
	int32 GLESVersion;

	// Is the device authorized for USB communication?  If not, then none of the other properties besides the serial number will be valid
	bool bAuthorizedDevice;

	// TCP port number on our local host forwarded over adb to the device
	uint16 HostMessageBusPort;

	// Holds pixel per inch value.
	int32 DeviceDPI = 0;

	// Holds the display resolution for the device
	int32 ResolutionX = 0;
	int32 ResolutionY = 0;

	// Holds the reported OpenGLES version.
	FString OpenGLVersionString;

	// device's vulkan version, in major.minor.patch format.
	FString VulkanVersion;

	// Holds the GPU family name.
	FString GPUFamilyString;

	// Holds the name of the manufacturer
	FString DeviceBrand;

	// device Hardware field. (or hardware.chipset for qcom)
	FString Hardware;

	// device's total mem (as reported from /proc/meminfo)
	uint32 TotalPhysicalKB = 0;

	// device BuildNumber field
	FString BuildNumber;

	FAndroidDeviceInfo()
		: SDKVersion(INDEX_NONE)
		, GLESVersion(INDEX_NONE)
		, bAuthorizedDevice(true)
		, HostMessageBusPort(0)
	{
	}
};


/**
 * Interface for AndroidDeviceDetection module.
 */
class IAndroidDeviceDetection
{
public:
	virtual void Initialize(const TCHAR* SDKDirectoryEnvVar, const TCHAR* SDKRelativeExePath, const TCHAR* GetPropCommand, bool bGetExtensionsViaSurfaceFlinger) = 0;
	virtual const TMap<FString,FAndroidDeviceInfo>& GetDeviceMap() = 0;
	virtual FCriticalSection* GetDeviceMapLock() = 0;
	virtual void UpdateADBPath() = 0;
	virtual FString GetADBPath() = 0;

	virtual void ExportDeviceProfile(const FString& OutPath, const FString& DeviceName) = 0;
protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IAndroidDeviceDetection() { }
};
