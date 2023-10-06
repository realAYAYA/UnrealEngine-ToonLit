// Copyright Epic Games, Inc. All Rights Reserved.

#include "Microsoft/MicrosoftPlatformManualResetEvent.h"

#include "HAL/PlatformMath.h"
#include "Misc/MonotonicTime.h"

#include "Microsoft/WindowsHWrapper.h"

namespace UE::HAL::Private
{

void FMicrosoftPlatformManualResetEvent::Wait()
{
	WaitUntil(FMonotonicTimePoint::Infinity());
}

bool FMicrosoftPlatformManualResetEvent::WaitUntil(FMonotonicTimePoint WaitTime)
{
	bool bLocalWait = true;
	if (WaitTime.IsInfinity())
	{
		for (;;)
		{
			if (WaitOnAddress(&bWait, &bLocalWait, sizeof(bool), INFINITE) && !bWait)
			{
				return true;
			}
		}
	}
	else
	{
		for (;;)
		{
			FMonotonicTimeSpan WaitSpan = WaitTime - FMonotonicTimePoint::Now();
			if (WaitSpan <= FMonotonicTimeSpan::Zero())
			{
				return false;
			}
			const DWORD WaitMs = DWORD(FPlatformMath::CeilToInt64(WaitSpan.ToMilliseconds()));
			if (WaitOnAddress(&bWait, &bLocalWait, sizeof(bool), WaitMs) && !bWait)
			{
				return true;
			}
		}
	}
}

void FMicrosoftPlatformManualResetEvent::Notify()
{
	bWait = false;
	WakeByAddressSingle((void*)&bWait);
}

} // UE::HAL::Private
