// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynchronizedClock.h"

namespace Electra
{

int64 MEDIAutcTime::CurrentMSec()
{
	FTimespan localTime(FDateTime::UtcNow().GetTicks());
	return (int64)localTime.GetTotalMilliseconds();
};

}
