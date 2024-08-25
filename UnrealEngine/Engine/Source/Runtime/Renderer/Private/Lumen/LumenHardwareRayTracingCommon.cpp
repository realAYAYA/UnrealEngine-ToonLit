// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenHardwareRayTracingCommon.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "ComponentRecreateRenderStateContext.h"
#include "LumenReflections.h"
#include "LumenVisualize.h"

static TAutoConsoleVariable<int32> CVarLumenUseHardwareRayTracing(
	TEXT("r.Lumen.HardwareRayTracing"),
	0,
	TEXT("Uses Hardware Ray Tracing for Lumen features, when available.\n")
	TEXT("Lumen will fall back to Software Ray Tracing otherwise.\n")
	TEXT("Note: Hardware ray tracing has significant scene update costs for\n")
	TEXT("scenes with more than 100k instances."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// Recreate proxies so that FPrimitiveSceneProxy::UpdateVisibleInLumenScene() can pick up any changed state
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Note: Driven by URendererSettings and must match the enum exposed there
static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingLightingMode(
	TEXT("r.Lumen.HardwareRayTracing.LightingMode"),
	0,
	TEXT("Determines the lighting mode (Default = 0)\n")
	TEXT("0: interpolate final lighting from the surface cache\n")
	TEXT("1: evaluate material, and interpolate irradiance and indirect irradiance from the surface cache\n")
	TEXT("2: evaluate material and direct lighting, and interpolate indirect irradiance from the surface cache\n")
	TEXT("3: evaluate material, direct lighting, and unshadowed skylighting at the hit point"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingHitLightingReflectionCaptures(
	TEXT("r.Lumen.HardwareRayTracing.HitLighting.ReflectionCaptures"),
	0,
	TEXT("Whether to apply Reflection Captures to ray hits when using Hit Lighting."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarLumenUseHardwareRayTracingInline(
	TEXT("r.Lumen.HardwareRayTracing.Inline"),
	1,
	TEXT("Uses Hardware Inline Ray Tracing for selected Lumen passes, when available.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingPullbackBias(
	TEXT("r.Lumen.HardwareRayTracing.PullbackBias"),
	8.0,
	TEXT("Determines the pull-back bias when resuming a screen-trace ray (default = 8.0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingFarFieldBias(
	TEXT("r.Lumen.HardwareRayTracing.FarFieldBias"),
	200.0f,
	TEXT("Determines bias for the far field traces. Default = 200"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingMaxIterations(
	TEXT("r.Lumen.HardwareRayTracing.MaxIterations"),
	8192,
	TEXT("Limit number of ray tracing traversal iterations on supported platfoms.\n"
		"Incomplete misses will be treated as hitting a black surface (can cause overocculsion).\n"
		"Incomplete hits will be treated as a hit (can cause leaking)."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenHardwareRayTracingMinTraceDistanceToSampleSurfaceCache(
	TEXT("r.Lumen.HardwareRayTracing.MinTraceDistanceToSampleSurfaceCache"),
	10.0f,
	TEXT("Ray hit distance from which we can start sampling surface cache in order to fix feedback loop where surface cache texel hits itself and propagates lighting."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool LumenHardwareRayTracing::IsInlineSupported()
{
	return GRHISupportsInlineRayTracing;
}

bool LumenHardwareRayTracing::IsRayGenSupported()
{
	// Indirect RayGen dispatch is required for Lumen RayGen shaders
	return GRHISupportsRayTracingShaders && GRHISupportsRayTracingDispatchIndirect;
}

bool Lumen::UseHardwareRayTracing(const FSceneViewFamily& ViewFamily)
{
#if RHI_RAYTRACING
	return IsRayTracingEnabled(ViewFamily.GetShaderPlatform())
		&& (LumenHardwareRayTracing::IsInlineSupported() || LumenHardwareRayTracing::IsRayGenSupported())
		&& CVarLumenUseHardwareRayTracing.GetValueOnAnyThread() != 0
		// Lumen HWRT does not support split screen yet, but stereo views can be allowed
		&& (ViewFamily.Views.Num() == 1 || (ViewFamily.Views.Num() == 2 && IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0])));
#else
	return false;
#endif
}

bool Lumen::IsUsingRayTracingLightingGrid(const FSceneViewFamily& ViewFamily, const FViewInfo& View, bool bLumenGIEnabled)
{
	if (UseHardwareRayTracing(ViewFamily) 
		&& (LumenReflections::UseHitLighting(View, bLumenGIEnabled) || LumenVisualize::UseHitLighting(View, bLumenGIEnabled)))
	{
		return true;
	}

	return false;
}

float LumenHardwareRayTracing::GetMinTraceDistanceToSampleSurfaceCache()
{
	return CVarLumenHardwareRayTracingMinTraceDistanceToSampleSurfaceCache.GetValueOnRenderThread();
}

Lumen::EHardwareRayTracingLightingMode Lumen::GetHardwareRayTracingLightingMode(const FViewInfo& View, bool bLumenGIEnabled)
{
#if RHI_RAYTRACING
	
	if (!bLumenGIEnabled)
	{
		// ShouldRenderLumenReflections should have prevented this
		check(GRHISupportsRayTracingShaders);
		// Force hit lighting and no surface cache when using standalone Lumen Reflections
		return Lumen::EHardwareRayTracingLightingMode::EvaluateMaterialAndDirectLightingAndSkyLighting;
	}

	int32 LightingModeInt = CVarLumenHardwareRayTracingLightingMode.GetValueOnAnyThread();

	// Without ray tracing shaders (RayGen) support we can only use Surface Cache mode.
	if (View.FinalPostProcessSettings.LumenRayLightingMode == ELumenRayLightingModeOverride::SurfaceCache || !LumenHardwareRayTracing::IsRayGenSupported())
	{
		LightingModeInt = static_cast<int32>(Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache);
	}
	else if (View.FinalPostProcessSettings.LumenRayLightingMode == ELumenRayLightingModeOverride::HitLighting)
	{
		LightingModeInt = static_cast<int32>(Lumen::EHardwareRayTracingLightingMode::EvaluateMaterialAndDirectLighting);
	}

	LightingModeInt = FMath::Clamp<int32>(LightingModeInt, 0, (int32)Lumen::EHardwareRayTracingLightingMode::MAX - 1);
	return static_cast<Lumen::EHardwareRayTracingLightingMode>(LightingModeInt);
#else
	return Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache;
#endif
}

bool Lumen::UseReflectionCapturesForHitLighting()
{
	int32 UseReflectionCaptures = CVarLumenHardwareRayTracingHitLightingReflectionCaptures.GetValueOnRenderThread();
	return UseReflectionCaptures != 0;
}

bool Lumen::UseHardwareInlineRayTracing(const FSceneViewFamily& ViewFamily)
{
#if RHI_RAYTRACING
	if (Lumen::UseHardwareRayTracing(ViewFamily)
		&& LumenHardwareRayTracing::IsInlineSupported()
		// Can't disable inline tracing if RayGen isn't supported
		&& (CVarLumenUseHardwareRayTracingInline.GetValueOnRenderThread() != 0 || !LumenHardwareRayTracing::IsRayGenSupported()))
	{
		return true;
	}
#endif

	return false;
}

float LumenHardwareRayTracing::GetFarFieldBias()
{
	return FMath::Max(CVarLumenHardwareRayTracingFarFieldBias.GetValueOnRenderThread(), 0.0f);
}

uint32 LumenHardwareRayTracing::GetMaxTraversalIterations()
{
	return FMath::Max(CVarLumenHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);
}

#if RHI_RAYTRACING

FLumenHardwareRayTracingShaderBase::FLumenHardwareRayTracingShaderBase() = default;
FLumenHardwareRayTracingShaderBase::FLumenHardwareRayTracingShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
}

void FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ESurfaceCacheSampling SurfaceCacheSampling, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_FEEDBACK"), SurfaceCacheSampling == Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback ? 0 : 1);
	OutEnvironment.SetDefine(TEXT("SURFACE_CACHE_HIGH_RES_PAGES"), SurfaceCacheSampling == Lumen::ESurfaceCacheSampling::HighResPages ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("LUMEN_HARDWARE_RAYTRACING"), 1);

	// GPU Scene definitions
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);

	// Inline
	const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
	if (bInlineRayTracing)
	{
		OutEnvironment.SetDefine(TEXT("LUMEN_HARDWARE_INLINE_RAYTRACING"), 1);
		OutEnvironment.CompilerFlags.Add(CFLAG_InlineRayTracing);
	}
}

void FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironmentInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ERayTracingShaderDispatchSize Size, bool UseThreadGroupSize64, FShaderCompilerEnvironment& OutEnvironment)
{
	if (DispatchSize == Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)
	{
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DISPATCH_1D"), 1);
	}

	const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
	if (bInlineRayTracing && !UseThreadGroupSize64)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
}

FIntPoint FLumenHardwareRayTracingShaderBase::GetThreadGroupSizeInternal(Lumen::ERayTracingShaderDispatchType ShaderDispatchType, Lumen::ERayTracingShaderDispatchSize ShaderDispatchSize, bool UseThreadGroupSize64)
{
	// Current inline ray tracing implementation requires 1:1 mapping between thread groups and waves.
	const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
	if (bInlineRayTracing)
	{
		switch (ShaderDispatchSize)
		{
		case Lumen::ERayTracingShaderDispatchSize::DispatchSize2D: return UseThreadGroupSize64 ? FIntPoint(8, 8) : FIntPoint(8, 4);
		case Lumen::ERayTracingShaderDispatchSize::DispatchSize1D: return UseThreadGroupSize64 ? FIntPoint(64, 1) : FIntPoint(32, 1);
		default:
			checkNoEntry();
		}
	}

	return FIntPoint(1, 1);
}

bool FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
{
	const bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;
	if (bInlineRayTracing)
	{
		return IsRayTracingEnabledForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform) && RHISupportsRayTracing(Parameters.Platform) && RHISupportsInlineRayTracing(Parameters.Platform);
	}
	else
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}
}

bool FLumenHardwareRayTracingShaderBase::UseThreadGroupSize64(EShaderPlatform ShaderPlatform)
{
	return !Lumen::UseThreadGroupSize32() && RHISupportsWaveSize64(ShaderPlatform);
}

namespace Lumen
{
	const TCHAR* GetRayTracedNormalModeName(int NormalMode)
	{
		if (NormalMode == 0)
		{
			return TEXT("SDF");
		}

		return TEXT("Geometry");
	}

	float GetHardwareRayTracingPullbackBias()
	{
		return CVarLumenHardwareRayTracingPullbackBias.GetValueOnRenderThread();
	}
}

void SetLumenHardwareRayTracingSharedParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	FLumenHardwareRayTracingShaderBase::FSharedParameters* SharedParameters)
{
	SharedParameters->SceneTextures = SceneTextures;
	SharedParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;
	SharedParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

	//SharedParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
	SharedParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);

	// Lighting data
	SharedParameters->LightGridParameters = View.RayTracingLightGridUniformBuffer;
	SharedParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
	SharedParameters->Forward = View.ForwardLightingResources.ForwardLightUniformBuffer;

	// Inline
	SharedParameters->HitGroupData = View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer ? GraphBuilder.CreateSRV(View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer) : nullptr;
	SharedParameters->LumenHardwareRayTracingUniformBuffer = View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer ? View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer : nullptr;
	checkf(View.RayTracingSceneInitTask == nullptr, TEXT("RayTracingSceneInitTask must be completed before creating SRV for RayTracingSceneMetadata."));
	SharedParameters->RayTracingSceneMetadata = View.GetRayTracingSceneChecked()->GetOrCreateMetadataBufferSRV(GraphBuilder.RHICmdList);

	// Use surface cache, instead
	SharedParameters->TracingParameters = TracingParameters;
}

#endif // RHI_RAYTRACING