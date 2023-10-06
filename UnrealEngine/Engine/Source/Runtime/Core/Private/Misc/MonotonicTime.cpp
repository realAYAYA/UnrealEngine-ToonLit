// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/MonotonicTime.h"

#include "HAL/PlatformTime.h"

namespace UE
{

FMonotonicTimePoint FMonotonicTimePoint::Now()
{
	return FromSeconds(FPlatformTime::Seconds());
}

} // namespace UE
