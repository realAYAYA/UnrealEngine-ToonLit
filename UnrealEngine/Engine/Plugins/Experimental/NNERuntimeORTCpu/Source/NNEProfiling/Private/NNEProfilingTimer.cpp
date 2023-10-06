// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEProfilingTimer.h"
#include "HAL/PlatformTime.h"

namespace UE::NNEProfiling::Internal
{
	void FTimer::Tic()
	{
		TimeStart = { FPlatformTime::Seconds() };
	}

	double FTimer::Toc() const
	{
		return (FPlatformTime::Seconds() - TimeStart) * 1e3;
	}
} // UE::NNEProfiling::Internal