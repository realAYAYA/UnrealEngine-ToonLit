// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS
	#include "Win/WinPlatform.h"
#elif PLATFORM_LINUX
	#include "Linux/LinuxPlatform.h"
#else
	#error "The platform is not specified or not defined"
#endif
