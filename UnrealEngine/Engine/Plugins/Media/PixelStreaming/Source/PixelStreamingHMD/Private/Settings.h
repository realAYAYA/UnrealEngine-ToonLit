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
	// End CVars

} // namespace UE::PixelStreamingHMD::Settings
