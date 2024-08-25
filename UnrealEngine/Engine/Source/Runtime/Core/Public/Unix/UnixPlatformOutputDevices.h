// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UnixPlatformOutputDevices.h: Unix platform OutputDevices functions
==============================================================================================*/

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"

class FOutputDevice;
class FOutputDeviceError;

struct FUnixOutputDevices : public FGenericPlatformOutputDevices
{
	static CORE_API FString GetAbsoluteLogFilename();
	static CORE_API FOutputDevice* GetEventLog();
	static CORE_API FOutputDeviceError* GetError();
};

typedef FUnixOutputDevices FPlatformOutputDevices;
