// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHI.h: Public D3D RHI definitions.
=============================================================================*/

#pragma once

#include "HAL/Platform.h"

// This is a value that should be tweaked to fit the app, lower numbers will have better performance
// Titles using many terrain layers may want to set MAX_SRVS to 64 to avoid shader compilation errors. This will have a small performance hit of around 0.1%
#define MAX_SRVS		64
#define MAX_SAMPLERS	16
#define MAX_UAVS		16
#define MAX_CBS			16

#if !defined(WITH_PIX_EVENT_RUNTIME)
	#define WITH_PIX_EVENT_RUNTIME 0
#endif

#if PLATFORM_WINDOWS
	#define ENABLE_RESIDENCY_MANAGEMENT				1
	#define PIPELINE_STATE_FILE_LOCATION			FPaths::ProjectSavedDir()
	#define USE_PIX									WITH_PIX_EVENT_RUNTIME
	#define D3D12RHI_SUPPORTS_WIN_PIX				USE_PIX

	#define FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT		D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
#else
	#include "D3D12RHIPlatformPublic.h"
#endif
