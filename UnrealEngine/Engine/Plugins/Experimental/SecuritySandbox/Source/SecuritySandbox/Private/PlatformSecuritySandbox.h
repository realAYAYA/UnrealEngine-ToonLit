// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformSecuritySandbox.h"
typedef FWindowsPlatformSecuritySandbox FPlatformSecuritySandbox;
#else
#include "GenericPlatform/GenericPlatformSecuritySandbox.h"
typedef FGenericPlatformSecuritySandbox FPlatformSecuritySandbox;
#endif