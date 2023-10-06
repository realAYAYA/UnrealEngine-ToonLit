// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

#include "HAL/Platform.h"

#if PLATFORM_64BITS
	#pragma pack(push,16)
#else
	#pragma pack(push,8)
#endif

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#define INITGUID
THIRD_PARTY_INCLUDES_START
	#include <d3d12.h>
	#include <d3dx12.h>
	#include <dxgi1_6.h>
	#include <dxgidebug.h>
THIRD_PARTY_INCLUDES_END
#include "Microsoft/HideMicrosoftPlatformTypes.h"

#undef DrawText

#pragma pack(pop)
#pragma warning(pop)
