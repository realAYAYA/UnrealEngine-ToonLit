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

struct CORE_API FUnixOutputDevices : public FGenericPlatformOutputDevices
{
	static void SetupOutputDevices();
	static FString GetAbsoluteLogFilename();
	static FOutputDevice* GetEventLog();
	static FOutputDeviceError* GetError();
};

typedef FUnixOutputDevices FPlatformOutputDevices;
