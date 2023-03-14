// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** Name of the current OpenVR SDK version in use (matches directory name) */
#define OPENVR_SDK_VER TEXT("OpenVRv1_5_17")

// for STEAMVR_SUPPORTED_PLATFORMS, keep at top
#include "ISteamVRPlugin.h"
#include "Engine/Engine.h"

#include "IHeadMountedDisplay.h"
#include "Runtime/Engine/Public/ScreenRendering.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if STEAMVR_SUPPORTED_PLATFORMS
#include "openvr.h"
#endif // STEAMVR_SUPPORTED_PLATFORMS
