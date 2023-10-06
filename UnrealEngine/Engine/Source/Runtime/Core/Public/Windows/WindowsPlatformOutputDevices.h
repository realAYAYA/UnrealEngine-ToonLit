// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"

class FOutputDevice;
class FOutputDeviceConsole;
class FOutputDeviceError;
class FFeedbackContext;

struct FWindowsPlatformOutputDevices
	: public FGenericPlatformOutputDevices
{
	static CORE_API FOutputDevice*			GetEventLog();
	static CORE_API FOutputDeviceError*      GetError();
};


typedef FWindowsPlatformOutputDevices FPlatformOutputDevices;
