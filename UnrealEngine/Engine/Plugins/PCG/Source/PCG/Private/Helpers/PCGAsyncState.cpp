// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGAsyncState.h"

#include "HAL/PlatformTime.h"

bool FPCGAsyncState::ShouldStop() const
{
	return FPlatformTime::Seconds() > EndTime;
}