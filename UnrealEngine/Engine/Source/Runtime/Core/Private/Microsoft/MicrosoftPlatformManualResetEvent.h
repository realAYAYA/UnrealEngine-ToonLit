// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#define UE_API CORE_API

namespace UE { struct FMonotonicTimePoint; }

namespace UE::HAL::Private
{

/** @see FGenericPlatformManualResetEvent */
class FMicrosoftPlatformManualResetEvent
{
public:
	FMicrosoftPlatformManualResetEvent() = default;
	FMicrosoftPlatformManualResetEvent(const FMicrosoftPlatformManualResetEvent&) = delete;
	FMicrosoftPlatformManualResetEvent& operator=(const FMicrosoftPlatformManualResetEvent&) = delete;

	void Reset() { bWait = true; }

	UE_API void Wait();
	UE_API bool WaitUntil(FMonotonicTimePoint WaitTime);
	UE_API void Notify();

private:
	bool bWait = true;
};

} // UE::HAL::Private

#undef UE_API
