// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/BootProfiling.h"
#include "CoreGlobals.h"

double GEnginePreInitPreStartupScreenEndTime;
double GEnginePreInitPostStartupScreenEndTime;
double GEngineInitEndTime;

double FBootProfiling::GetBootDuration()
{
	return GEngineInitEndTime - GStartTime;
}

double FBootProfiling::GetPreInitPreStartupScreenDuration()
{
	return GEnginePreInitPreStartupScreenEndTime - GStartTime;
}

double FBootProfiling::GetPreInitPostStartupScreenDuration()
{
	return GEnginePreInitPostStartupScreenEndTime - GEnginePreInitPreStartupScreenEndTime;
}

double FBootProfiling::GetEngineInitDuration()
{
	return GEngineInitEndTime - GEnginePreInitPostStartupScreenEndTime;
}
