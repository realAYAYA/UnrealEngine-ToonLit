// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
 * Microsoft-shared implementation of the stack walking.
 **/
struct CORE_API FMicrosoftPlatformStackWalk
	: public FGenericPlatformStackWalk
{

protected:
	// Extract debug info for a module from the module header in memory. 
	// Can directly read the information even when the current target can't load the symbols itself or use certain DbgHelp APIs.
	static bool ExtractInfoFromModule(void* ProcessHandle, void* ModuleHandle, FStackWalkModuleInfo& OutInfo);
};