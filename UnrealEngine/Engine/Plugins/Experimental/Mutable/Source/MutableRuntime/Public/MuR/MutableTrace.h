// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Trace.h"
#include "HAL/PlatformTime.h"


/** Custom Mutable profiler scope. */
#define MUTABLE_CPUPROFILER_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE(Mutable_##Name)


/** Simple class that saves the time the scope is alive in the given location. */
class FMutableScopeTimer
{
public:
	FMutableScopeTimer(double& InResult) : Result(InResult)
	{
		StartTime = FPlatformTime::Seconds();
	}

	~FMutableScopeTimer()
	{
		Result =  FPlatformTime::Seconds() - StartTime;
	}
	
private:
	double StartTime = 0.0;
	double& Result;
};
