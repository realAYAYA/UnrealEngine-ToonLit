// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

namespace UE::LowLevelTests
{

class FCheckScopeOutputDeviceError;

///@brief Scope that captures failed `check` calls
struct FCheckScope
{
	FCheckScope();

	///@brief Only catches failed `check` calls that contain Msg
	///@param Msg string to look for in the `check` message
	FCheckScope(const ANSICHAR* Msg);
	~FCheckScope();

	int GetCount();
private:
	FCheckScopeOutputDeviceError* DeviceError;
	bool bIgnoreDebugger;
	bool bCriticalError;
}; 

}