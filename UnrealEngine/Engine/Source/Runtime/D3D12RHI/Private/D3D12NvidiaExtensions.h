// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_NVAPI
	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	THIRD_PARTY_INCLUDES_START
		#include "nvapi.h"
		#include "nvShaderExtnEnums.h"
	THIRD_PARTY_INCLUDES_END
	#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif

#if NV_AFTERMATH
	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	THIRD_PARTY_INCLUDES_START
		#define GFSDK_Aftermath_WITH_DX12 1
		#include "GFSDK_Aftermath.h"
		#include "GFSDK_Aftermath_GpuCrashdump.h"
		#undef GFSDK_Aftermath_WITH_DX12
	THIRD_PARTY_INCLUDES_END
	#include "Microsoft/HideMicrosoftPlatformTypes.h"

	extern bool GDX12NVAfterMathModuleLoaded;
	extern int32 GDX12NVAfterMathEnabled;
	extern int32 GDX12NVAfterMathTrackResources;
	extern float GDX12NVAfterMathDumpWaitTime;
	extern int32 GDX12NVAfterMathMarkers;
#endif // NV_AFTERMATH
