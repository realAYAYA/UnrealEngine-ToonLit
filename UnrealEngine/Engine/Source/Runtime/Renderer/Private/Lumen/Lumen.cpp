// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lumen.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "LumenSceneData.h"
#include "RenderUtils.h"

int32 GLumenSupported = 1;
FAutoConsoleVariableRef CVarLumenSupported(
	TEXT("r.Lumen.Supported"),
	GLumenSupported,
	TEXT("Whether Lumen is supported at all for the project, regardless of platform.  This can be used to avoid compiling shaders and other load time overhead."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenAsyncCompute(
	TEXT("r.Lumen.AsyncCompute"),
	1,
	TEXT("Whether Lumen should use async compute if supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenThreadGroupSize32(
	TEXT("r.Lumen.ThreadGroupSize32"),
	1,
	TEXT("Whether to prefer dispatches in groups of 32 threads on HW which supports it (instead of standard 64)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool DoesRuntimePlatformSupportLumen()
{
	return UE::PixelFormat::HasCapabilities(PF_R16_UINT, EPixelFormatCapabilities::TypedUAVLoad);
}

bool Lumen::UseAsyncCompute(const FViewFamilyInfo& ViewFamily)
{
	bool bUseAsync = GSupportsEfficientAsyncCompute
		&& CVarLumenAsyncCompute.GetValueOnRenderThread() != 0;
		
	if (Lumen::UseHardwareRayTracing(ViewFamily))
	{
		// Async for Lumen HWRT path can only be used by inline ray tracing
		bUseAsync &= CVarLumenAsyncCompute.GetValueOnRenderThread() != 0 && Lumen::UseHardwareInlineRayTracing(ViewFamily);
	}

	return bUseAsync;
}

bool Lumen::UseThreadGroupSize32()
{
	return GRHISupportsWaveOperations && GRHIMinimumWaveSize <= 32 && CVarLumenThreadGroupSize32.GetValueOnRenderThread() != 0;
}

namespace Lumen
{
	bool AnyLumenHardwareRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
#if RHI_RAYTRACING

		const bool bLumenGI = ShouldRenderLumenDiffuseGI(Scene, View);
		const bool bLumenReflections = ShouldRenderLumenReflections(View);

		if (bLumenGI
			&& (UseHardwareRayTracedScreenProbeGather(*View.Family) 
				|| UseHardwareRayTracedRadianceCache(*View.Family) 
				|| UseHardwareRayTracedDirectLighting(*View.Family)
				|| UseHardwareRayTracedTranslucencyVolume(*View.Family)))
		{
			return true;
		}

		if (bLumenReflections
			&& UseHardwareRayTracedReflections(*View.Family))
		{
			return true;
		}

		if ((bLumenGI || bLumenReflections) && Lumen::ShouldVisualizeHardwareRayTracing(*View.Family))
		{
			return true;
		}

		if ((bLumenGI || bLumenReflections) && Lumen::ShouldRenderRadiosityHardwareRayTracing(*View.Family))
		{
			return true;
		}
#endif
		return false;
	}

	bool AnyLumenHardwareInlineRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
		if (!AnyLumenHardwareRayTracingPassEnabled(Scene, View))
		{
			return false;
		}

		return Lumen::UseHardwareInlineRayTracing(*View.Family);
	}
}

bool Lumen::ShouldHandleSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& (Scene->SkyLight->ProcessedTexture || Scene->SkyLight->bRealTimeCaptureEnabled)
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5
		&& !IsForwardShadingEnabled(Scene->GetShaderPlatform())
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool ShouldRenderLumenForViewFamily(const FScene* Scene, const FSceneViewFamily& ViewFamily, bool bSkipProjectCheck)
{
	return Scene
		&& Scene->GetLumenSceneData(*ViewFamily.Views[0])
		&& ViewFamily.Views.Num() <= LUMEN_MAX_VIEWS
		&& DoesPlatformSupportLumenGI(Scene->GetShaderPlatform(), bSkipProjectCheck);
}

bool Lumen::IsSoftwareRayTracingSupported()
{
	return DoesProjectSupportDistanceFields();
}

bool Lumen::IsLumenFeatureAllowedForView(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck, bool bSkipProjectCheck)
{
	return View.Family
		&& DoesRuntimePlatformSupportLumen()
		&& ShouldRenderLumenForViewFamily(Scene, *View.Family, bSkipProjectCheck)
		// Don't update scene lighting for secondary views
		&& !View.bIsPlanarReflection
		&& !View.bIsSceneCaptureCube
		&& !View.bIsReflectionCapture
		&& View.State
		&& (bSkipTracingDataCheck || Lumen::UseHardwareRayTracing(*View.Family) || IsSoftwareRayTracingSupported());
}

bool Lumen::UseGlobalSDFObjectGrid(const FSceneViewFamily& ViewFamily)
{
	if (!Lumen::IsSoftwareRayTracingSupported())
	{
		return false;
	}

	// All features use Hardware RayTracing, no need to update voxel lighting
	if (Lumen::UseHardwareRayTracedSceneLighting(ViewFamily)
		&& Lumen::UseHardwareRayTracedScreenProbeGather(ViewFamily)
		&& Lumen::UseHardwareRayTracedReflections(ViewFamily)
		&& Lumen::UseHardwareRayTracedRadianceCache(ViewFamily)
		&& Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily)
		&& Lumen::UseHardwareRayTracedVisualize(ViewFamily))
	{
		return false;
	}

	return true;
}

uint32 Lumen::GetMeshCardDistanceBin(float Distance)
{
	uint32 OffsetDistance = FMath::Max(1, (int32)(Distance - 1000));
	uint32 Bin = FMath::Min(FMath::FloorLog2(OffsetDistance), Lumen::NumDistanceBuckets - 1);
	return Bin;
}
