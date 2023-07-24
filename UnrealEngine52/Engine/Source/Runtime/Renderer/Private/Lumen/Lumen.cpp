// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lumen.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "SceneRendering.h"

static TAutoConsoleVariable<int32> CVarLumenAsyncCompute(
	TEXT("r.Lumen.AsyncCompute"),
	1,
	TEXT("Whether Lumen should use async compute if supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingAsyncCompute(
	TEXT("r.Lumen.HardwareRayTracing.AsyncCompute"),
	0,
	TEXT("Whether Lumen when using Hardware Ray Tracing should use async compute if supported (default = 0)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenThreadGroupSize32(
	TEXT("r.Lumen.ThreadGroupSize32"),
	1,
	TEXT("Whether to prefer dispatches in groups of 32 threads on HW which supports it (instead of standard 64)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool Lumen::UseAsyncCompute(const FViewFamilyInfo& ViewFamily)
{
	bool bUseAsync = GSupportsEfficientAsyncCompute
		&& CVarLumenAsyncCompute.GetValueOnRenderThread() != 0;
		
	if (Lumen::UseHardwareRayTracing(ViewFamily))
	{
		// Async for Lumen HWRT path can only be used by inline ray tracing
		bUseAsync &= CVarLumenHardwareRayTracingAsyncCompute.GetValueOnRenderThread() != 0 && Lumen::UseHardwareInlineRayTracing(ViewFamily);
	}

	return bUseAsync;
}

bool Lumen::UseThreadGroupSize32()
{
	return GRHISupportsWaveOperations && GRHIMinimumWaveSize <= 32 && CVarLumenThreadGroupSize32.GetValueOnRenderThread() != 0;
}