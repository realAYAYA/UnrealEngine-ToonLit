// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if defined(WEBSOCKETS_MANAGER_PLATFORM_INCLUDE)
	#include WEBSOCKETS_MANAGER_PLATFORM_INCLUDE
	typedef WEBSOCKETS_MANAGER_PLATFORM_CLASS FPlatformWebSocketsManager;
#else
	#error "Web Sockets not implemented on this platform yet"
#endif