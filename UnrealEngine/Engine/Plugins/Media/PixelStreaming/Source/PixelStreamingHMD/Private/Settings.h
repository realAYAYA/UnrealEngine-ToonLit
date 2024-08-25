// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"

namespace UE::PixelStreamingHMD::Settings
{
	extern void InitialiseSettings();
	extern void CommandLineParseOption();

	// Begin CVars
	extern TAutoConsoleVariable<bool> CVarPixelStreamingEnableHMD;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingHMDMatchAspectRatio;
	extern TAutoConsoleVariable<float> CVarPixelStreamingHMDHFOV;
	extern TAutoConsoleVariable<float> CVarPixelStreamingHMDVFOV;
	// End CVars

} // namespace UE::PixelStreamingHMD::Settings
