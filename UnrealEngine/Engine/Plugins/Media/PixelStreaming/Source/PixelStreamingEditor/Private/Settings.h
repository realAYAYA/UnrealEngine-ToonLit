// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"

namespace UE::EditorPixelStreaming::Settings
{
    extern void InitialiseSettings();

	// Begin Editor CVars
	extern TAutoConsoleVariable<bool> CVarEditorPixelStreamingStartOnLaunch;
    // End Editor CVars
}