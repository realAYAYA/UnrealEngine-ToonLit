// Copyright Epic Games, Inc. All Rights Reserved.

#include "BasePassRendering.h"
#include "DeferredShadingRenderer.h"
#include "DistortionRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "LightMapDensityRendering.h"
#include "MeshPassProcessor.inl"
#include "EditorPrimitivesRendering.h"
#include "TranslucentRendering.h"
#include "SingleLayerWaterRendering.h"
#include "Rendering/SkyAtmosphereCommonData.h"
#include "SceneTextureParameters.h"
#include "CompositionLighting/CompositionLighting.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "SceneViewExtension.h"
#include "VariableRateShadingImageManager.h"
#include "OneColorShader.h"
#include "ClearQuad.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "DebugProbeRendering.h"
#include "AnisotropyRendering.h"
#include "Nanite/NaniteVisualize.h"
#include "Nanite/NaniteMaterials.h"
#include "Nanite/NaniteShading.h"
#include "RenderCore.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "VolumetricFog.h"
#include "PostProcess/SceneRenderTargets.h"

#include "BasePassRendering.inl"

// Instantiate the common policies
template class TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>;
template class TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>;
template class TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>;

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarSelectiveBasePassOutputs(
	TEXT("r.SelectiveBasePassOutputs"),
	0,
	TEXT("Enables shaders to only export to relevant rendertargets.\n") \
	TEXT(" 0: Export in all rendertargets.\n") \
	TEXT(" 1: Export only into relevant rendertarget.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarGlobalClipPlane(
	TEXT("r.AllowGlobalClipPlane"),
	0,
	TEXT("Enables mesh shaders to support a global clip plane, needed for planar reflections, which adds about 15% BasePass GPU cost on PS4."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarVertexFoggingForOpaque(
	TEXT("r.VertexFoggingForOpaque"),
	1,
	TEXT("Causes opaque materials to use per-vertex fogging, which costs less and integrates properly with MSAA.  Only supported with forward shading."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksBasePass(
	TEXT("r.RHICmdFlushRenderThreadTasksBasePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the base pass. A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksBasePass is > 0 we will flush."));

static TAutoConsoleVariable<int32> CVarSupportStationarySkylight(
	TEXT("r.SupportStationarySkylight"),
	1,
	TEXT("Enables Stationary and Dynamic Skylight shader permutations."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSupportLowQualityLightmaps(
	TEXT("r.SupportLowQualityLightmaps"),
	1,
	TEXT("Support low quality lightmap shader permutations"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSupportAllShaderPermutations(
	TEXT("r.SupportAllShaderPermutations"),
	0,
	TEXT("Local user config override to force all shader permutation features on."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarParallelBasePass(
	TEXT("r.ParallelBasePass"),
	1,
	TEXT("Toggles parallel base pass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarClearGBufferDBeforeBasePass(
	TEXT("r.ClearGBufferDBeforeBasePass"),
	1,
	TEXT("Whether to clear GBuffer D before basepass"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPSOPrecacheLightMapPolicyMode(
	TEXT("r.PSOPrecache.LightMapPolicyMode"),
	1,
	TEXT("Defines which light map policies should be checked during PSO precaching of the base pass.\n") \
	TEXT(" 0: All possible LMP will be checked.\n") \
	TEXT(" 1: Only LMP_NO_LIGHTMAP will be precached (default).\n"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarPSOPrecacheTranslucencyAllPass(
	TEXT("r.PSOPrecache.TranslucencyAllPass"),
	0,
	TEXT("Precache PSOs for TranslucencyAll pass.\n") \
	TEXT(" 0: No PSOs are compiled for this pass (default).\n") \
	TEXT(" 1: PSOs are compiled for all primitives which render to a translucency pass.\n"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarPSOPrecacheAlphaColorChannel(
	TEXT("r.PSOPrecache.PrecacheAlphaColorChannel"),
	1,
	TEXT("Also Precache PSOs with scene color alpha channel enabled. Planar reflections and scene captures use this for compositing into a different scene later."),
	ECVF_ReadOnly
);

// Scene color alpha is used during scene captures and planar reflections.  1 indicates background should be shown, 0 indicates foreground is fully present.
static const float kSceneColorClearAlpha = 1.0f;

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSharedBasePassUniformParameters, "BasePass");
IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FOpaqueBasePassUniformParameters, "OpaqueBasePass", SceneTextures);
IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FTranslucentBasePassUniformParameters, "TranslucentBasePass", SceneTextures);

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TBasePassVS< LightMapPolicyType > TBasePassVS##LightMapPolicyName ; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVS##LightMapPolicyName,TEXT("/Engine/Private/BasePassVertexShader.usf"),TEXT("Main"),SF_Vertex);

#define IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,bEnableSkyLight,SkyLightName,GBufferLayout,LayoutName) \
	typedef TBasePassPS<LightMapPolicyType, bEnableSkyLight, GBufferLayout> TBasePassPS##LightMapPolicyName##SkyLightName##LayoutName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassPS##LightMapPolicyName##SkyLightName##LayoutName,TEXT("/Engine/Private/BasePassPixelShader.usf"),TEXT("MainPS"),SF_Pixel);

#define IMPLEMENT_BASEPASS_COMPUTESHADER_TYPE(LightMapPolicyType,LightMapPolicyName,bEnableSkyLight,SkyLightName) \
	typedef TBasePassCS<LightMapPolicyType, bEnableSkyLight> TBasePassCS##LightMapPolicyName##SkyLightName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassCS##LightMapPolicyName##SkyLightName,TEXT("/Engine/Private/BasePassPixelShader.usf"),TEXT("MainCS"),SF_Compute);

// Implement a pixel and compute shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,true,Skylight,GBL_Default,) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,false,,GBL_Default,) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,true,Skylight,GBL_ForceVelocity,ForceVelocity) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,false,,GBL_ForceVelocity,ForceVelocity) \
	IMPLEMENT_BASEPASS_COMPUTESHADER_TYPE(LightMapPolicyType,LightMapPolicyName,true,Skylight) \
	IMPLEMENT_BASEPASS_COMPUTESHADER_TYPE(LightMapPolicyType,LightMapPolicyName,false,)

// Implement shader types per lightmap policy
// If renaming or refactoring these, remember to update FMaterialResource::GetRepresentativeInstructionCounts and FPreviewMaterial::ShouldCache().
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedTranslucencyPolicy, FSelfShadowedTranslucencyPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedCachedPointIndirectLightingPolicy, FSelfShadowedCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedVolumetricLightmapPolicy, FSelfShadowedVolumetricLightmapPolicy );

IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>, FPrecomputedVolumetricLightmapLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>, FCachedVolumeIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_CACHED_POINT_INDIRECT_LIGHTING>, FCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, TLightMapPolicyHQ );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyHQ  );

F128BitRTBasePassPS::F128BitRTBasePassPS() = default;
F128BitRTBasePassPS::F128BitRTBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
	TBasePassPS<TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, false, GBL_Default>(Initializer)
{}

bool F128BitRTBasePassPS::ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	return FDataDrivenShaderPlatformInfo::GetRequiresExplicit128bitRT(Parameters.Platform);
}

void F128BitRTBasePassPS::ModifyCompilationEnvironment(const FMeshMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	TBasePassPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}

IMPLEMENT_MATERIAL_SHADER_TYPE(, F128BitRTBasePassPS, TEXT("/Engine/Private/BasePassPixelShader.usf"), TEXT("MainPS"), SF_Pixel);

DEFINE_GPU_DRAWCALL_STAT(Basepass);

DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ClearGBufferAtMaxZ"), STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ViewExtensionPostRenderBasePass"), STAT_FDeferredShadingSceneRenderer_ViewExtensionPostRenderBasePass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("BasePass"), STAT_CLM_BasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterBasePass"), STAT_CLM_AfterBasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AnisotropyPass"), STAT_CLM_AnisotropyPass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterAnisotropyPass"), STAT_CLM_AfterAnisotropyPass, STATGROUP_CommandListMarkers);

DECLARE_CYCLE_STAT(TEXT("BasePass"), STAT_CLP_BasePass, STATGROUP_ParallelCommandListMarkers);

DEFINE_GPU_STAT(NaniteBasePass);

static bool IsBasePassWaitForTasksEnabled()
{
	return CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0;
}

static bool IsStandardTranslucenyPassSeparated()
{
	static const auto TranslucencyStandardSeparatedCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Translucency.StandardSeparated"));
	return TranslucencyStandardSeparatedCVar && TranslucencyStandardSeparatedCVar->GetValueOnAnyThread() != 0;
}

template<uint32 StencilRef> void SetTranslucentPassDepthStencilState(FMeshPassProcessorRenderState& DrawRenderState, bool bDisableDepthTest)
{
	if (bDisableDepthTest)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
			false, CF_Always,
			true , CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			StencilRef, StencilRef>::GetRHI());
		DrawRenderState.SetStencilRef(StencilRef);
	}
	else
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true , CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			StencilRef, StencilRef>::GetRHI());
		DrawRenderState.SetStencilRef(StencilRef);
	}
}

void SetTranslucentRenderState(FMeshPassProcessorRenderState& DrawRenderState, const FMaterial& Material, const EShaderPlatform Platform, ETranslucencyPass::Type InTranslucencyPassType)
{
	if (Material.IsSubstrateMaterial())
	{
		if (Material.IsDualBlendingEnabled(Platform))
		{
			if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandard || InTranslucencyPassType == ETranslucencyPass::TPT_AllTranslucency)
			{
				// If we are in the transparancy pass (before DoF) we do standard dual blending, and the alpha gets ignored
				// Blend by putting add in target 0 and multiply by background in target 1.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_One, BF_Source1Alpha>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF)
			{
				// In the separate pass (after DoF), we want let alpha pass through, and then multiply our color modulation in the after DoF Modulation pass.
				// Alpha is BF_Zero for source and BF_One for dest, which leaves alpha unchanged
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOFModulate || InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandardModulate)
			{
				// In the separate pass (after DoF) modulate, we want to only darken the target by our multiplication term, and ignore the addition term.
				// For regular dual blending, our function is:
				//     FrameBuffer = MRT0 + MRT1 * FrameBuffer;
				// So we can just remove the MRT0 component and it will modulate as expected.
				// Alpha we will leave unchanged.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterMotionBlur)
			{
				// We don't actually currently support color modulation in the post-motion blur pass at the moment, so just do the same as post-DOF for now
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
		}
		else
		{
			if (Material.GetBlendMode() == BLEND_ColoredTransmittanceOnly)
			{
				// Modulate with the existing scene color, preserve destination alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
			}
			else if (Material.GetBlendMode() == BLEND_AlphaHoldout)
			{
				// Blend by holding out the matte shape of the source alpha
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
			}
			else
			{
				// We always use premultipled alpha for translucent rendering, that works for any surface with additive color.
				// If a material was requesting dual source blending, the shader will use static platofm knowledge to convert colored transmittance to a grey scale transmittance.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			}
		}
	}
	else if (Material.GetShadingModels().HasShadingModel(MSM_ThinTranslucent))
	{
		// Special case for dual blending, which is not exposed as a parameter in the material editor
		if (Material.IsDualBlendingEnabled(Platform))
		{
			if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandard || InTranslucencyPassType == ETranslucencyPass::TPT_AllTranslucency)
			{
				// If we are in the transparancy pass (before DoF) we do standard dual blending, and the alpha gets ignored

				// Blend by putting add in target 0 and multiply by background in target 1.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_One, BF_Source1Alpha>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF)
			{
				// In the separate pass (after DoF), we want let alpha pass through, and then multiply our color modulation in the after DoF Modulation pass.
				// Alpha is BF_Zero for source and BF_One for dest, which leaves alpha unchanged
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOFModulate || InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandardModulate)
			{
				// In the separate pass (after DoF) modulate, we want to only darken the target by our multiplication term, and ignore the addition term.
				// For regular dual blending, our function is:
				//     FrameBuffer = MRT0 + MRT1 * FrameBuffer;
				// So we can just remove the MRT0 component and it will modulate as expected.
				// Alpha we will leave unchanged.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
			else if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterMotionBlur)
			{
				// We don't actually currently support color modulation in the post-motion blur pass at the moment, so just do the same as post-DOF for now
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Source1Color, BO_Add, BF_Zero, BF_One>::GetRHI());
			}
		}
		else
		{
			// If unsupported, we still use premultipled alpha but the shader will use the variation converting color transmittance to a grey scale transmittance.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
		}
	}
	else
	{
		switch (Material.GetBlendMode())
		{
		default:
		case BLEND_Opaque:
			// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
			break;
		case BLEND_Masked:
			// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
			break;
		case BLEND_Translucent:
		case BLEND_TranslucentColoredTransmittance:	// When Substrate is disabled, this falls back to simple Translucency.
			// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
			// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			break;
		case BLEND_Additive:
			// Add to the existing scene color
			// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
			// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			break;
		case BLEND_Modulate:
			// Modulate with the existing scene color, preserve destination alpha.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
			break;
		case BLEND_AlphaComposite:
			// Blend with existing scene color. New color is already pre-multiplied by alpha.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			break;
		case BLEND_AlphaHoldout:
			// Blend by holding out the matte shape of the source alpha
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
			break;
		};
	}

	const bool bDisableDepthTest = Material.ShouldDisableDepthTest();
	const bool bEnableResponsiveAA = Material.ShouldEnableResponsiveAA();
	const bool bIsPostMotionBlur = Material.IsTranslucencyAfterMotionBlurEnabled();

	// When separate standard translucent are used, we must mark the distoprtion bit for the composition to happen correctly for any BeforeDoF translucent.
	const bool bSeparatedStandardTranslucent = (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandard || InTranslucencyPassType == ETranslucencyPass::TPT_AllTranslucency) && IsStandardTranslucenyPassSeparated();

	if (bEnableResponsiveAA && !bIsPostMotionBlur)
	{
		if (bSeparatedStandardTranslucent)
		{
			SetTranslucentPassDepthStencilState<STENCIL_TEMPORAL_RESPONSIVE_AA_MASK | DISTORTION_STENCIL_MASK_BIT>(DrawRenderState, bDisableDepthTest);
		}
		else
		{
			SetTranslucentPassDepthStencilState<STENCIL_TEMPORAL_RESPONSIVE_AA_MASK>(DrawRenderState, bDisableDepthTest);
		}
		}
	else if (bSeparatedStandardTranslucent)
	{
		SetTranslucentPassDepthStencilState<DISTORTION_STENCIL_MASK_BIT>(DrawRenderState, bDisableDepthTest);
	}
	else if (bDisableDepthTest)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
}

FMeshDrawCommandSortKey CalculateTranslucentMeshStaticSortKey(const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, uint16 MeshIdInPrimitive)
{
	uint16 SortKeyPriority = 0;
	float DistanceOffset = 0.0f;

	if (PrimitiveSceneProxy)
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
		SortKeyPriority = (uint16)((int32)PrimitiveSceneInfo->Proxy->GetTranslucencySortPriority() - (int32)SHRT_MIN);
		DistanceOffset = PrimitiveSceneInfo->Proxy->GetTranslucencySortDistanceOffset();
	}

	FMeshDrawCommandSortKey SortKey;
	SortKey.Translucent.MeshIdInPrimitive = MeshIdInPrimitive;
	SortKey.Translucent.Priority = SortKeyPriority;
	SortKey.Translucent.Distance = *(uint32*)(&DistanceOffset); // View specific, so will be filled later inside VisibleMeshCommands.

	return SortKey;
}

FMeshDrawCommandSortKey CalculateBasePassMeshStaticSortKey(EDepthDrawingMode EarlyZPassMode, const bool bIsMasked, const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.BasePass.VertexShaderHash = (VertexShader ? VertexShader->GetSortKey() : 0) & 0xFFFF;
	SortKey.BasePass.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;
	if (EarlyZPassMode != DDM_None)
	{
		SortKey.BasePass.Masked = bIsMasked ? 0 : 1;
	}
	else
	{
		SortKey.BasePass.Masked = bIsMasked ? 1 : 0;
	}

	return SortKey;
}

template<bool bDepthTest, ECompareFunction CompareFunction, uint32 StencilWriteMask>
void SetDepthStencilStateForBasePass_Internal(FMeshPassProcessorRenderState& InDrawRenderState)
{
	InDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
		bDepthTest, CompareFunction,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		0xFF, StencilWriteMask>::GetRHI());
}

template<bool bDepthTest, ECompareFunction CompareFunction>
void SetDepthStencilStateForBasePass_Internal(FMeshPassProcessorRenderState& InDrawRenderState, ERHIFeatureLevel::Type FeatureLevel)
{	
	const static bool bSubstrateDufferPassEnabled = Substrate::IsSubstrateEnabled() && Substrate::IsDBufferPassEnabled(GShaderPlatformForFeatureLevel[FeatureLevel]);
	if (bSubstrateDufferPassEnabled)
	{
		SetDepthStencilStateForBasePass_Internal<bDepthTest, CompareFunction, GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_NORMAL, 1) | GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_DIFFUSE, 1) | GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_ROUGHNESS, 1) | GET_STENCIL_BIT_MASK(DISTANCE_FIELD_REPRESENTATION, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)>(InDrawRenderState);
	}
	else
	{
		SetDepthStencilStateForBasePass_Internal<bDepthTest, CompareFunction, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | GET_STENCIL_BIT_MASK(DISTANCE_FIELD_REPRESENTATION, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)>(InDrawRenderState);
	}
}

void SetDepthStencilStateForBasePass(
	FMeshPassProcessorRenderState& DrawRenderState,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bDitheredLODTransition,
	const FMaterial& MaterialResource,
	bool bEnableReceiveDecalOutput,
	bool bForceEnableStencilDitherState)
{
	const bool bMaskedInEarlyPass = (MaterialResource.IsMasked() || bDitheredLODTransition) && MaskedInEarlyPass(GShaderPlatformForFeatureLevel[FeatureLevel]);
	if (bEnableReceiveDecalOutput)
	{
		if (bMaskedInEarlyPass)
		{
			SetDepthStencilStateForBasePass_Internal<false, CF_Equal>(DrawRenderState, FeatureLevel);
		}
		else if (DrawRenderState.GetDepthStencilAccess() & FExclusiveDepthStencil::DepthWrite)
		{
			SetDepthStencilStateForBasePass_Internal<true, CF_GreaterEqual>(DrawRenderState, FeatureLevel);
		}
		else
		{
			SetDepthStencilStateForBasePass_Internal<false, CF_GreaterEqual>(DrawRenderState, FeatureLevel);
		}
	}
	else if (bMaskedInEarlyPass)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
	}

	if (bForceEnableStencilDitherState)
	{
		SetDepthStencilStateForBasePass_Internal<false, CF_Equal>(DrawRenderState, FeatureLevel);
	}
}

void SetupBasePassState(FExclusiveDepthStencil::Type BasePassDepthStencilAccess, const bool bShaderComplexity, FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetDepthStencilAccess(BasePassDepthStencilAccess);

	if (bShaderComplexity)
	{
		// Additive blending when shader complexity viewmode is enabled.
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
		// Disable depth writes as we have a full depth prepass.
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	}
	else
	{
		// Opaque blending for all G buffer targets, depth tests and writes.
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocityDebug"));
		if (CVar && CVar->GetValueOnRenderThread() == 2)
		{
			DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_NONE>::GetRHI());
		}
		else
		{
			DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
		}

		if (DrawRenderState.GetDepthStencilAccess() & FExclusiveDepthStencil::DepthWrite)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		}
		else
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}
	}
}

template <ELightMapPolicyType Policy, EGBufferLayout GBufferLayout>
void AddUniformBasePassPixelShader(bool bEnableSkyLight, bool bUse128bitRT, FMaterialShaderTypes& OutShaderTypes, bool bIsForOITPass = false)
{
	int32 PermutationId = 0;

	if (bIsForOITPass)
	{
		using FMyShader = TBasePassPS<TUniformLightMapPolicy<Policy>, true, GBufferLayout>;
		typename FMyShader::FPermutationDomain PermutationVector;
		PermutationVector.template Set<typename FMyShader::FSupportOITDim>(true);
		PermutationId = PermutationVector.ToDimensionValueId();
	}

	if (bEnableSkyLight)
	{
		OutShaderTypes.AddShaderType<TBasePassPS<TUniformLightMapPolicy<Policy>, true, GBufferLayout>>(PermutationId);
	}
	else
	{
		if (bUse128bitRT && (Policy == LMP_NO_LIGHTMAP) && (GBufferLayout == GBL_Default))
		{
			OutShaderTypes.AddShaderType<F128BitRTBasePassPS>(PermutationId);
		}
		else
		{
			OutShaderTypes.AddShaderType<TBasePassPS<TUniformLightMapPolicy<Policy>, false, GBufferLayout>>(PermutationId);
		}
	}
}

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <ELightMapPolicyType Policy>
bool GetUniformBasePassShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableSkyLight,
	bool bUse128bitRT,
	EGBufferLayout GBufferLayout,
	TShaderRef<TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>>* VertexShader,
	TShaderRef<TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>>* PixelShader,
	bool bIsForOITPass = false
)
{
	FMaterialShaderTypes ShaderTypes;
	if (VertexShader)
	{
		ShaderTypes.AddShaderType<TBasePassVS<TUniformLightMapPolicy<Policy>>>();
	}

	if (PixelShader)
	{
		switch (GBufferLayout)
		{
		case GBL_Default:
			AddUniformBasePassPixelShader<Policy, GBL_Default>(bEnableSkyLight, bUse128bitRT, ShaderTypes, bIsForOITPass);
			break;
		case GBL_ForceVelocity:
			AddUniformBasePassPixelShader<Policy, GBL_ForceVelocity>(bEnableSkyLight, bUse128bitRT, ShaderTypes, bIsForOITPass);
			break;
		default:
			check(false);
			break;
		}
	}

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

template <>
bool GetBasePassShaders<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	const FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableSkyLight,
    bool bUse128bitRT,
	EGBufferLayout GBufferLayout,
	TShaderRef<TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicy>>* VertexShader,
	TShaderRef<TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>>* PixelShader,
	bool bIsForOITPass
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
		return GetUniformBasePassShaders<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
		return GetUniformBasePassShaders<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_CACHED_POINT_INDIRECT_LIGHTING:
		return GetUniformBasePassShaders<LMP_CACHED_POINT_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_LQ_LIGHTMAP:
		return GetUniformBasePassShaders<LMP_LQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_HQ_LIGHTMAP:
		return GetUniformBasePassShaders<LMP_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		return GetUniformBasePassShaders<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	case LMP_NO_LIGHTMAP:
		return GetUniformBasePassShaders<LMP_NO_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, bUse128bitRT, GBufferLayout, VertexShader, PixelShader, bIsForOITPass);
	default:
		check(false);
		return false;
	}
}

template <ELightMapPolicyType Policy>
void AddUniformBasePassComputeShader(bool bEnableSkyLight, FMaterialShaderTypes& OutShaderTypes)
{
	if (bEnableSkyLight)
	{
		OutShaderTypes.AddShaderType<TBasePassCS<TUniformLightMapPolicy<Policy>, true>>();
	}
	else
	{
		OutShaderTypes.AddShaderType<TBasePassCS<TUniformLightMapPolicy<Policy>, false>>();
	}
}

template <ELightMapPolicyType Policy>
bool GetUniformBasePassShader(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableSkyLight,
	TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>>* ComputeShader
)
{
	FMaterialShaderTypes ShaderTypes;

	if (ComputeShader)
	{
		AddUniformBasePassComputeShader<Policy>(bEnableSkyLight, ShaderTypes);
	}

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetComputeShader(ComputeShader);
	return true;
}

template <>
bool GetBasePassShader<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	const FVertexFactoryType* VertexFactoryType,
	FUniformLightMapPolicy LightMapPolicy,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bEnableSkyLight,
	TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>>* ComputeShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
		return GetUniformBasePassShader<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
		return GetUniformBasePassShader<LMP_CACHED_VOLUME_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_CACHED_POINT_INDIRECT_LIGHTING:
		return GetUniformBasePassShader<LMP_CACHED_POINT_INDIRECT_LIGHTING>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_LQ_LIGHTMAP:
		return GetUniformBasePassShader<LMP_LQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_HQ_LIGHTMAP:
		return GetUniformBasePassShader<LMP_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
		return GetUniformBasePassShader<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	case LMP_NO_LIGHTMAP:
		return GetUniformBasePassShader<LMP_NO_LIGHTMAP>(Material, VertexFactoryType, FeatureLevel, bEnableSkyLight, ComputeShader);
	default:
		check(false);
		return false;
	}
}

extern void SetupDummyForwardLightUniformParameters(FRDGBuilder& GraphBuilder, FForwardLightData& ForwardLightData, EShaderPlatform ShaderPlatform);

void SetupSharedBasePassParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const int32 ViewIndex,
	bool bLumenGIEnabled,
	FSharedBasePassUniformParameters& SharedParameters)
{
	if (View.ForwardLightingResources.ForwardLightData)
	{
		SharedParameters.Forward = *View.ForwardLightingResources.ForwardLightData;
	}
	else
	{
		SetupDummyForwardLightUniformParameters(GraphBuilder, SharedParameters.Forward, View.GetShaderPlatform());
	}

	SetupFogUniformParameters(GraphBuilder, View, SharedParameters.Fog);

	if (View.IsInstancedStereoPass())
	{
		const FViewInfo& InstancedView = *View.GetInstancedView();
		SharedParameters.ForwardISR = *InstancedView.ForwardLightingResources.ForwardLightData;
		SetupFogUniformParameters(GraphBuilder, (FViewInfo&)InstancedView, SharedParameters.FogISR);
	}
	else
	{
		if (View.ForwardLightingResources.ForwardLightData)
		{
			SharedParameters.ForwardISR = *View.ForwardLightingResources.ForwardLightData;
		}
		else
		{
			SharedParameters.ForwardISR = SharedParameters.Forward;
		}
		SharedParameters.FogISR = SharedParameters.Fog;
	}

	SharedParameters.LFV = View.LocalFogVolumeViewData.UniformParametersStruct;

	SharedParameters.LightFunctionAtlas = *LightFunctionAtlas::GetGlobalParametersStruct(GraphBuilder, View);

	const FScene* Scene = View.Family->Scene ? View.Family->Scene->GetRenderScene() : nullptr;
	const FPlanarReflectionSceneProxy* ReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;

	SetupReflectionUniformParameters(GraphBuilder, View, SharedParameters.Reflection);
	SetupPlanarReflectionUniformParameters(View, ReflectionSceneProxy, SharedParameters.PlanarReflection);

	// Skip base pass skylight if Lumen GI is enabled, as Lumen handles the skylight.
	// Ideally we would choose a different shader permutation to skip skylight, but Lumen GI is only known per-view
	SharedParameters.UseBasePassSkylight = bLumenGIEnabled ? 0 : 1;
}

TRDGUniformBufferRef<FOpaqueBasePassUniformParameters> CreateOpaqueBasePassUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const int32 ViewIndex,
	const FForwardBasePassTextures& ForwardBasePassTextures,
	const FDBufferTextures& DBufferTextures,
	bool bLumenGIEnabled)
{
	FOpaqueBasePassUniformParameters& BasePassParameters = *GraphBuilder.AllocParameters<FOpaqueBasePassUniformParameters>();
	SetupSharedBasePassParameters(GraphBuilder, View, ViewIndex, bLumenGIEnabled, BasePassParameters.Shared);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	// Forward shading
	{
		BasePassParameters.UseForwardScreenSpaceShadowMask = 0;
		BasePassParameters.ForwardScreenSpaceShadowMaskTexture = SystemTextures.White;
		BasePassParameters.IndirectOcclusionTexture = SystemTextures.White;
		BasePassParameters.ResolvedSceneDepthTexture = SystemTextures.White;

		if (ForwardBasePassTextures.ScreenSpaceShadowMask)
		{
			BasePassParameters.UseForwardScreenSpaceShadowMask = 1;
			BasePassParameters.ForwardScreenSpaceShadowMaskTexture = ForwardBasePassTextures.ScreenSpaceShadowMask;
		}

		if (HasBeenProduced(ForwardBasePassTextures.ScreenSpaceAO))
		{
			BasePassParameters.IndirectOcclusionTexture = ForwardBasePassTextures.ScreenSpaceAO;
		}

		if (ForwardBasePassTextures.SceneDepthIfResolved)
		{
			BasePassParameters.ResolvedSceneDepthTexture = ForwardBasePassTextures.SceneDepthIfResolved;
		}
		BasePassParameters.Is24BitUnormDepthStencil = ForwardBasePassTextures.bIs24BitUnormDepthStencil ? 1 : 0;
	}

	// DBuffer Decals
	BasePassParameters.DBuffer = GetDBufferParameters(GraphBuilder, DBufferTextures, View.GetShaderPlatform());

	// Substrate
	Substrate::BindSubstrateBasePassUniformParameters(GraphBuilder, View, BasePassParameters.Substrate);

	// Misc
	BasePassParameters.PreIntegratedGFTexture = GSystemTextures.PreintegratedGF->GetRHI();
	BasePassParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	BasePassParameters.EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));

	return GraphBuilder.CreateUniformBuffer(&BasePassParameters);
}

static void ClearGBufferAtMaxZ(
	FRDGBuilder& GraphBuilder,
	TArrayView<const FViewInfo> Views,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	FLinearColor ClearColor0)
{
	check(Views.Num() > 0);

	SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ);
	RDG_EVENT_SCOPE(GraphBuilder, "ClearGBufferAtMaxZ");

	const uint32 ActiveTargetCount = BasePassRenderTargets.GetActiveCount();
	FGlobalShaderMap* ShaderMap = Views[0].ShaderMap;

	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	TOneColorPixelShaderMRT::FPermutationDomain PermutationVector;
	PermutationVector.Set<TOneColorPixelShaderMRT::TOneColorPixelShaderNumOutputs>(ActiveTargetCount);
	TShaderMapRef<TOneColorPixelShaderMRT>PixelShader(ShaderMap, PermutationVector);

	auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
	PassParameters->RenderTargets = BasePassRenderTargets;

	// Clear each viewport by drawing background color at MaxZ depth
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		GraphBuilder.AddPass(
			{},
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, VertexShader, PixelShader, ActiveTargetCount, ClearColor0](FRHICommandList& RHICmdList)
		{
			const FLinearColor ClearColors[MaxSimultaneousRenderTargets] =
			{
				ClearColor0,
				FLinearColor(0.5f,0.5f,0.5f,0),
				FLinearColor(0,0,0,1),
				FLinearColor(0,0,0,0),
				FLinearColor(0,1,1,1),
				FLinearColor(1,1,1,1),
				FLinearColor::Transparent,
				FLinearColor::Transparent
			};

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			// Opaque rendering, depth test but no depth writes
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParametersLegacyVS(RHICmdList, VertexShader, float(ERHIZBuffer::FarPlane));

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

			TOneColorPixelShaderMRT::FParameters PixelParameters;
			PixelShader->FillParameters(PixelParameters, ClearColors, ActiveTargetCount);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);

			RHICmdList.SetStreamSource(0, GClearVertexBuffer.VertexBufferRHI, 0);
			RHICmdList.DrawPrimitive(0, 2, 1);
		});
	}
}

bool IsGBufferLayoutSupportedForMaterial(EGBufferLayout Layout, const FMeshMaterialShaderPermutationParameters& Parameters)
{
	switch (Layout)
	{
	case GBL_Default:
		// All Nanite and non-Nanite base pass shaders support the default layout
		return true;

	case GBL_ForceVelocity:
		// Only Nanite materials with WPO support this GBuffer layout
		// NOTE: FMaterialShaderParameters::bHasVertexPositionOffsetConnected means that the material *could* have WPO.
		// It's still possible for the material to disable WPO after translation.
		return !IsUsingBasePassVelocity(Parameters.Platform) && 
			Parameters.VertexFactoryType->SupportsNaniteRendering() &&
			Parameters.MaterialParameters.bIsUsedWithNanite &&
			Parameters.MaterialParameters.bHasVertexPositionOffsetConnected;

	default:
		checkf(false, TEXT("Unhandled GBuffer Layout!"));
		return false;
	}
}

void ModifyBasePassCSPSCompilationEnvironment(const FMeshMaterialShaderPermutationParameters& Parameters, EGBufferLayout GBufferLayout, bool bEnableSkyLight, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), Parameters.MaterialParameters.MaterialDomain != MD_Surface);
	OutEnvironment.SetDefine(TEXT("ENABLE_DBUFFER_TEXTURES"), Parameters.MaterialParameters.MaterialDomain == MD_Surface);
	OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkyLight);
	OutEnvironment.SetDefine(TEXT("PLATFORM_FORCE_SIMPLE_SKY_DIFFUSE"), ForceSimpleSkyDiffuse(Parameters.Platform));
	OutEnvironment.SetDefine(TEXT("GBUFFER_LAYOUT"), GBufferLayout);

	// This define simply lets the compilation environment know that we are using BasePassPixelShader.usf, so that we can check for more
	// complicated defines later in the compilation pipe.
	OutEnvironment.SetDefine(TEXT("IS_BASE_PASS"), 1);
	OutEnvironment.SetDefine(TEXT("IS_MOBILE_BASE_PASS"), 0);

	const bool bTranslucent = IsTranslucentBlendMode(Parameters.MaterialParameters);
	const bool bIsSingleLayerWater = Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_SingleLayerWater);
	const bool bSupportVirtualShadowMap = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	if (bSupportVirtualShadowMap && (bTranslucent || bIsSingleLayerWater))
	{
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VIRTUAL_SHADOW_MAP"), 1);
	}

	OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	Substrate::SetBasePassRenderTargetOutputFormat(Parameters.Platform, Parameters.MaterialParameters, OutEnvironment, GBufferLayout);

	const bool bOutputVelocity = (GBufferLayout == GBL_ForceVelocity) ||
		FVelocityRendering::BasePassCanOutputVelocity(Parameters.Platform);
	if (bOutputVelocity)
	{
		// As defined in BasePassPixelShader.usf. Also account for Substrate setting velocity in slot 1 as described in FetchLegacyGBufferInfo.
		const int32 VelocityIndex = Substrate::IsSubstrateEnabled() ? 1 : (IsForwardShadingEnabled(Parameters.Platform) ? 1 : 4);
		OutEnvironment.SetRenderTargetOutputFormat(VelocityIndex, PF_G16R16);
	}

	const bool bNeedsSeparateMainDirLightTexture = IsWaterDistanceFieldShadowEnabled(Parameters.Platform) || IsWaterVirtualShadowMapFilteringEnabled(Parameters.Platform);
	if (bIsSingleLayerWater && bNeedsSeparateMainDirLightTexture)
	{
		// See FShaderCompileUtilities::FetchGBufferParamsRuntime for the details
		const bool bHasTangent = false;
		bool bHasPrecShadowFactor = IsStaticLightingAllowed();

		uint32 TargetSeparatedMainDirLight = 5;
		if (bOutputVelocity == false && bHasTangent == false)
		{
			TargetSeparatedMainDirLight = 5;
			if (bHasPrecShadowFactor)
			{
				TargetSeparatedMainDirLight = 6;
			}
		}
		else if (bOutputVelocity)
		{
			TargetSeparatedMainDirLight = 6;
			if (bHasPrecShadowFactor)
			{
				TargetSeparatedMainDirLight = 7;
			}
		}
		else if (bHasTangent)
		{
			TargetSeparatedMainDirLight = 6;
			if (bHasPrecShadowFactor)
			{
				TargetSeparatedMainDirLight = 7;
			}
		}
		OutEnvironment.SetRenderTargetOutputFormat(TargetSeparatedMainDirLight, PF_FloatR11G11B10);
	}
}

void FDeferredShadingSceneRenderer::RenderBasePass(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> InViews,
	FSceneTextures& SceneTextures,
	const FDBufferTextures& DBufferTextures,
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess,
	FRDGTextureRef ForwardShadowMaskTexture,
	FInstanceCullingManager& InstanceCullingManager,
	bool bNaniteEnabled,
	FNaniteShadingCommands& NaniteBasePassShadingCommands,
	const TArrayView<Nanite::FRasterResults>& NaniteRasterResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::RenderBasePass);

	const bool bEnableParallelBasePasses = GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePass.GetValueOnRenderThread();

	static const auto ClearMethodCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearSceneMethod"));
	bool bRequiresRHIClear = true;
	bool bRequiresFarZQuadClear = false;

	if (ClearMethodCVar)
	{
		int32 ClearMethod = ClearMethodCVar->GetValueOnRenderThread();

		if (ClearMethod == 0 && !ViewFamily.EngineShowFlags.Game)
		{
			// Do not clear the scene only if the view family is in game mode.
			ClearMethod = 1;
		}

		switch (ClearMethod)
		{
		case 0: // No clear
			bRequiresRHIClear = false;
			bRequiresFarZQuadClear = false;
			break;

		case 1: // RHICmdList.Clear
			bRequiresRHIClear = true;
			bRequiresFarZQuadClear = false;
			break;

		case 2: // Clear using far-z quad
			bRequiresFarZQuadClear = true;
			bRequiresRHIClear = false;
			break;
		}
	}

	// Always perform a full buffer clear for wireframe, shader complexity view mode, and stationary light overlap viewmode.
	if (ViewFamily.EngineShowFlags.Wireframe || ViewFamily.EngineShowFlags.ShaderComplexity || ViewFamily.EngineShowFlags.StationaryLightOverlap)
	{
		bRequiresRHIClear = true;
		bRequiresFarZQuadClear = false;
	}

	const bool bIsWireframeRenderpass = ViewFamily.EngineShowFlags.Wireframe && FSceneRenderer::ShouldCompositeEditorPrimitives(InViews[0]);
	const bool bDebugViewMode = ViewFamily.UseDebugViewPS();
	const bool bRenderLightmapDensity = ViewFamily.EngineShowFlags.LightMapDensity && AllowDebugViewmodes();
	const bool bRenderSkyAtmosphereEditorNotifications = ShouldRenderSkyAtmosphereEditorNotifications();
	const bool bDoParallelBasePass = bEnableParallelBasePasses && !bDebugViewMode && !bRenderLightmapDensity; // DebugView and LightmapDensity are non-parallel substitutions inside BasePass
	const bool bNeedsBeginRender = AllowDebugViewmodes() &&
		(ViewFamily.EngineShowFlags.RequiredTextureResolution ||
			ViewFamily.EngineShowFlags.VirtualTexturePendingMips ||
			ViewFamily.EngineShowFlags.MaterialTextureScaleAccuracy ||
			ViewFamily.EngineShowFlags.MeshUVDensityAccuracy ||
			ViewFamily.EngineShowFlags.PrimitiveDistanceAccuracy ||
			ViewFamily.EngineShowFlags.ShaderComplexity ||
			ViewFamily.EngineShowFlags.LODColoration ||
			ViewFamily.EngineShowFlags.HLODColoration);

	const bool bForwardShadingEnabled = IsForwardShadingEnabled(SceneTextures.Config.ShaderPlatform);

	const FExclusiveDepthStencil ExclusiveDepthStencil(BasePassDepthStencilAccess);

	TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> BasePassTextures;
	uint32 BasePassTextureCount = SceneTextures.GetGBufferRenderTargets(BasePassTextures);
	Substrate::AppendSubstrateMRTs(*this, BasePassTextureCount, BasePassTextures);
	TArrayView<FTextureRenderTargetBinding> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);
	FRDGTextureRef BasePassDepthTexture = SceneTextures.Depth.Target;
	FLinearColor SceneColorClearValue;

	if (bRequiresRHIClear)
	{
		if (ViewFamily.EngineShowFlags.ShaderComplexity && SceneTextures.QuadOverdraw)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(SceneTextures.QuadOverdraw), FUintVector4(0, 0, 0, 0));
		}

		if (ViewFamily.EngineShowFlags.ShaderComplexity || ViewFamily.EngineShowFlags.StationaryLightOverlap)
		{
			SceneColorClearValue = FLinearColor(0, 0, 0, kSceneColorClearAlpha);
		}
		else
		{
			SceneColorClearValue = FLinearColor(InViews[0].BackgroundColor.R, InViews[0].BackgroundColor.G, InViews[0].BackgroundColor.B, kSceneColorClearAlpha);
		}

		ERenderTargetLoadAction ColorLoadAction = ERenderTargetLoadAction::ELoad;

		if (SceneTextures.Color.Target->Desc.ClearValue.GetClearColor() == SceneColorClearValue)
		{
			ColorLoadAction = ERenderTargetLoadAction::EClear;
		}
		else
		{
			ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		}

		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets = GetRenderTargetBindings(ColorLoadAction, BasePassTexturesView);

		const FGBufferBindings& GBufferBindings = SceneTextures.Config.GBufferBindings[GBL_Default];
		if (!CVarClearGBufferDBeforeBasePass.GetValueOnRenderThread() && GBufferBindings.GBufferD.Index > 0 && GBufferBindings.GBufferD.Index < (int32)BasePassTextureCount)
		{
			PassParameters->RenderTargets[GBufferBindings.GBufferD.Index].SetLoadAction(ERenderTargetLoadAction::ENoAction);
		}

		GraphBuilder.AddPass(RDG_EVENT_NAME("GBufferClear"), PassParameters, ERDGPassFlags::Raster,
			[PassParameters, ColorLoadAction, SceneColorClearValue](FRHICommandList& RHICmdList)
		{
			// If no fast-clear action was used, we need to do an MRT shader clear.
			if (ColorLoadAction == ERenderTargetLoadAction::ENoAction)
			{
				const FRenderTargetBindingSlots& RenderTargets = PassParameters->RenderTargets;
				FLinearColor ClearColors[MaxSimultaneousRenderTargets];
				FRHITexture* Textures[MaxSimultaneousRenderTargets];
				int32 TextureIndex = 0;

				RenderTargets.Enumerate([&](const FRenderTargetBinding& RenderTarget)
				{
					FRHITexture* TextureRHI = RenderTarget.GetTexture()->GetRHI();
					ClearColors[TextureIndex] = TextureIndex == 0 ? SceneColorClearValue : TextureRHI->GetClearColor();
					Textures[TextureIndex] = TextureRHI;
					++TextureIndex;
				});

				// Clear color only; depth-stencil is fast cleared.
				DrawClearQuadMRT(RHICmdList, true, TextureIndex, ClearColors, false, 0, false, 0);
			}
		});

		if (bRenderSkyAtmosphereEditorNotifications)
		{
			// We only render this warning text when bRequiresRHIClear==true to make sure the scene color buffer is allocated at this stage.
			// When false, the option specifies that all pixels must be written to by a sky dome anyway.
			RenderSkyAtmosphereEditorNotifications(GraphBuilder, SceneTextures.Color.Target);
		}
	}

#if WITH_EDITOR
	if (ViewFamily.EngineShowFlags.Wireframe)
	{
		checkf(ExclusiveDepthStencil.IsDepthWrite(), TEXT("Wireframe base pass requires depth-write, but it is set to read-only."));

		BasePassTextureCount = 1;
		BasePassTextures[0] = SceneTextures.EditorPrimitiveColor;
		BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

		BasePassDepthTexture = SceneTextures.EditorPrimitiveDepth;

		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::EClear, BasePassTexturesView);
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(BasePassDepthTexture, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, ExclusiveDepthStencil);

		GraphBuilder.AddPass(RDG_EVENT_NAME("WireframeClear"), PassParameters, ERDGPassFlags::Raster, [](FRHICommandList&) {});
	}
#endif

	// Render targets bindings should remain constant at this point.
	FRenderTargetBindingSlots BasePassRenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::ELoad, BasePassTexturesView);
	BasePassRenderTargets.DepthStencil = FDepthStencilBinding(BasePassDepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, ExclusiveDepthStencil);
	
	FForwardBasePassTextures ForwardBasePassTextures{};

	if (bForwardShadingEnabled)
	{
		ForwardBasePassTextures.SceneDepthIfResolved = SceneTextures.Depth.IsSeparate() ? SceneTextures.Depth.Resolve : nullptr;
		ForwardBasePassTextures.ScreenSpaceAO = SceneTextures.ScreenSpaceAO;
		ForwardBasePassTextures.ScreenSpaceShadowMask = ForwardShadowMaskTexture;
	}
	else if (!ExclusiveDepthStencil.IsDepthWrite())
	{
		// If depth write is not enabled, we can bound the depth texture as read only
		ForwardBasePassTextures.SceneDepthIfResolved = SceneTextures.Depth.Resolve;
	}
	ForwardBasePassTextures.bIs24BitUnormDepthStencil = ForwardBasePassTextures.SceneDepthIfResolved ? GPixelFormats[ForwardBasePassTextures.SceneDepthIfResolved->Desc.Format].bIs24BitUnormDepthStencil : 1;

	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_BasePass));
	RenderBasePassInternal(GraphBuilder, InViews, SceneTextures, BasePassRenderTargets, BasePassDepthStencilAccess, ForwardBasePassTextures, DBufferTextures, bDoParallelBasePass, bRenderLightmapDensity, InstanceCullingManager, bNaniteEnabled, NaniteBasePassShadingCommands, NaniteRasterResults);
	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterBasePass));

	for (const TSharedRef<ISceneViewExtension>& ViewExtension : ViewFamily.ViewExtensions)
	{
		for (FViewInfo& View : InViews)
		{
			ViewExtension->PostRenderBasePassDeferred_RenderThread(GraphBuilder, View, BasePassRenderTargets, SceneTextures.UniformBuffer);
		}
	}

	if (bRequiresFarZQuadClear)
	{
		ClearGBufferAtMaxZ(GraphBuilder, InViews, BasePassRenderTargets, SceneColorClearValue);
	}

	if (ShouldRenderAnisotropyPass(InViews))
	{
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AnisotropyPass));
		RenderAnisotropyPass(GraphBuilder, SceneTextures, bEnableParallelBasePasses);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterAnisotropyPass));
	}

#if !(UE_BUILD_SHIPPING)
	if (!bForwardShadingEnabled)
	{
		StampDeferredDebugProbeMaterialPS(GraphBuilder, InViews, BasePassRenderTargets, SceneTextures);
	}
#endif
}

BEGIN_SHADER_PARAMETER_STRUCT(FOpaqueBasePassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static void RenderEditorPrimitivesForDPG(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FOpaqueBasePassParameters* PassParameters,
	const FMeshPassProcessorRenderState& DrawRenderState,
	ESceneDepthPriorityGroup DepthPriorityGroup,
	FInstanceCullingManager& InstanceCullingManager)
{
	const FScene* Scene = View.Family->Scene->GetRenderScene();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("%s", *UEnum::GetValueAsString(DepthPriorityGroup)),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, DrawRenderState, DepthPriorityGroup](FRHICommandList& RHICmdList)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, DepthPriorityGroup);

		if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
		{
			DrawDynamicMeshPass(View, RHICmdList,
				[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;

				for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

			const FBatchedElements& BatchedViewElements = DepthPriorityGroup == SDPG_World ? View.BatchedViewElements : View.TopBatchedViewElements;

			DrawDynamicMeshPass(View, RHICmdList,
				[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;

				for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

			// Draw the view's batched simple elements(lines, sprites, etc).
			View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, View.GetFeatureLevel(), View, false);
		}
	});
}

static bool HasEditorPrimitivesForDPG(const FViewInfo& View, ESceneDepthPriorityGroup DepthPriorityGroup)
{
	bool bHasPrimitives = View.SimpleElementCollector.HasPrimitives(DepthPriorityGroup);

	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const TIndirectArray<FMeshBatch, SceneRenderingAllocator>& ViewMeshElementList = (DepthPriorityGroup == SDPG_Foreground ? View.TopViewMeshElements : View.ViewMeshElements);
		bHasPrimitives |= ViewMeshElementList.Num() > 0;

		const FBatchedElements& BatchedViewElements = DepthPriorityGroup == SDPG_World ? View.BatchedViewElements : View.TopBatchedViewElements;
		bHasPrimitives |= BatchedViewElements.HasPrimsToDraw();
	}

	return bHasPrimitives;
}

static void RenderEditorPrimitives(
	FRDGBuilder& GraphBuilder,
	FOpaqueBasePassParameters* PassParameters,
	const FViewInfo& View,
	const FMeshPassProcessorRenderState& DrawRenderState,
	FInstanceCullingManager& InstanceCullingManager)
{
	RDG_EVENT_SCOPE(GraphBuilder, "EditorPrimitives");

	RenderEditorPrimitivesForDPG(GraphBuilder, View, PassParameters, DrawRenderState, SDPG_World, InstanceCullingManager);

	if (HasEditorPrimitivesForDPG(View, SDPG_Foreground))
	{
		// Write foreground primitives into depth buffer without testing 
		{
			auto* DepthWritePassParameters = GraphBuilder.AllocParameters<FOpaqueBasePassParameters>();
			*DepthWritePassParameters = *PassParameters;

			// Change to depth writable
			DepthWritePassParameters->RenderTargets.DepthStencil.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);

			FMeshPassProcessorRenderState NoDepthTestDrawRenderState(DrawRenderState);
			NoDepthTestDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
			NoDepthTestDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
			RenderEditorPrimitivesForDPG(GraphBuilder, View, DepthWritePassParameters, NoDepthTestDrawRenderState, SDPG_Foreground, InstanceCullingManager);
		}

		// Render foreground primitives with depth testing
		RenderEditorPrimitivesForDPG(GraphBuilder, View, PassParameters, DrawRenderState, SDPG_Foreground, InstanceCullingManager);
	}
}

void FDeferredShadingSceneRenderer::RenderBasePassInternal(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> InViews,
	const FSceneTextures& SceneTextures,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess,
	const FForwardBasePassTextures& ForwardBasePassTextures,
	const FDBufferTextures& DBufferTextures,
	bool bParallelBasePass,
	bool bRenderLightmapDensity,
	FInstanceCullingManager& InstanceCullingManager,
	bool bNaniteEnabled,
	FNaniteShadingCommands& NaniteBasePassShadingCommands,
	const TArrayView<Nanite::FRasterResults>& NaniteRasterResults)
{
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderBasePass);
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderBasePass, FColor::Emerald);

#if WITH_DEBUG_VIEW_MODES
	Nanite::EDebugViewMode NaniteDebugViewMode = Nanite::EDebugViewMode::None;
	if (ViewFamily.EngineShowFlags.Wireframe)
	{
		NaniteDebugViewMode = Nanite::EDebugViewMode::Wireframe;
	}
	else if (bRenderLightmapDensity)
	{
		NaniteDebugViewMode = Nanite::EDebugViewMode::LightmapDensity;
	}
	else if (ViewFamily.EngineShowFlags.ActorColoration)
	{
		NaniteDebugViewMode = Nanite::EDebugViewMode::PrimitiveColor;
	}
	else if (ViewFamily.UseDebugViewPS())
	{
	    switch (ViewFamily.GetDebugViewShaderMode())
	    {
	    case DVSM_ShaderComplexity:							// Default shader complexity viewmode
	    case DVSM_ShaderComplexityContainedQuadOverhead:	// Show shader complexity with quad overdraw scaling the PS instruction count.
	    case DVSM_ShaderComplexityBleedingQuadOverhead:		// Show shader complexity with quad overdraw bleeding the PS instruction count over the quad.
	    case DVSM_QuadComplexity:							// Show quad overdraw only.
		    NaniteDebugViewMode = Nanite::EDebugViewMode::ShaderComplexity;
		    break;
    
	    default:
		    break;
	    }
	}
#endif

	FRDGTextureRef NaniteColorTarget = SceneTextures.Color.Target;
	FRDGTextureRef NaniteDepthTarget = SceneTextures.Depth.Target;
#if WITH_EDITOR && WITH_DEBUG_VIEW_MODES
	if (NaniteDebugViewMode == Nanite::EDebugViewMode::Wireframe)
	{
		NaniteColorTarget = SceneTextures.EditorPrimitiveColor;
		NaniteDepthTarget = SceneTextures.EditorPrimitiveDepth;
	}
#endif

	auto RenderNaniteBasePass = [&](FViewInfo& View, int32 ViewIndex)
	{
		Nanite::FRasterResults& RasterResults = NaniteRasterResults[ViewIndex];
	#if WITH_DEBUG_VIEW_MODES
		if (NaniteDebugViewMode != Nanite::EDebugViewMode::None)
		{
			Nanite::RenderDebugViewMode(
				GraphBuilder,
				NaniteDebugViewMode,
				*Scene,
				View,
				ViewFamily,
				RasterResults,
				NaniteColorTarget,
				NaniteDepthTarget,
				SceneTextures.QuadOverdraw
			);
		}
		else
	#endif
		{
			RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteBasePass);

			if (UseNaniteComputeMaterials())
			{
				Nanite::DispatchBasePass(
					GraphBuilder,
					NaniteBasePassShadingCommands,
					*this,
					SceneTextures,
					BasePassRenderTargets,
					DBufferTextures,
					*Scene,
					View,
					uint32(ViewIndex),
					RasterResults
				);
			}
			else
			{
				checkf(NaniteLegacyMaterialsSupported(), TEXT("Must have either compute or legacy materials enabled in Nanite!"));
				Nanite::DrawBasePass(
					GraphBuilder,
					View.NaniteMaterialPassCommands,
					*this,
					SceneTextures,
					BasePassRenderTargets,
					DBufferTextures,
					*Scene,
					View,
					RasterResults
				);
			}
		}
	};

	if (bRenderLightmapDensity || ViewFamily.UseDebugViewPS())
	{
		// Debug view support for Nanite
		if (bNaniteEnabled)
		{
			// Should always have a full Z prepass with Nanite
			check(ShouldRenderPrePass());

			for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
			{
				FViewInfo& View = InViews[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, InViews.Num() > 1, "View%d", ViewIndex);

				RenderNaniteBasePass(View, ViewIndex);
			}
		}

		if (bRenderLightmapDensity)
		{
			// Override the base pass with the lightmap density pass if the viewmode is enabled.
			RenderLightMapDensities(GraphBuilder, InViews, BasePassRenderTargets);
		}
		else if (ViewFamily.UseDebugViewPS())
		{
			// Override the base pass with one of the debug view shader mode (see EDebugViewShaderMode) if required.
			RenderDebugViewMode(GraphBuilder, InViews, SceneTextures.QuadOverdraw, BasePassRenderTargets);
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);
		RDG_EVENT_SCOPE(GraphBuilder, "BasePass");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Basepass);

		const bool bDrawSceneViewsInOneNanitePass = InViews.Num() > 1 && Nanite::ShouldDrawSceneViewsInOneNanitePass(InViews[0]);
		if (bParallelBasePass)
		{
			RDG_WAIT_FOR_TASKS_CONDITIONAL(GraphBuilder, IsBasePassWaitForTasksEnabled());

			for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
			{
				FViewInfo& View = InViews[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, InViews.Num() > 1, "View%d", ViewIndex);
				View.BeginRenderView();

				const bool bLumenGIEnabled = GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;

				FMeshPassProcessorRenderState DrawRenderState;
				SetupBasePassState(BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, DrawRenderState);

				FOpaqueBasePassParameters* PassParameters = GraphBuilder.AllocParameters<FOpaqueBasePassParameters>();
				PassParameters->View = View.GetShaderParameters();
				PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
				PassParameters->BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View, ViewIndex, ForwardBasePassTextures, DBufferTextures, bLumenGIEnabled);
				PassParameters->RenderTargets = BasePassRenderTargets;
				PassParameters->RenderTargets.ShadingRateTexture = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, View, FVariableRateShadingImageManager::EVRSPassType::BasePass);

				const bool bShouldRenderView = View.ShouldRenderView();
				if (bShouldRenderView)
				{
					View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("BasePassParallel"),
						PassParameters,
						ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
						[this, &View, PassParameters](const FRDGPass* InPass, FRHICommandListImmediate& RHICmdList)
					{
						FRDGParallelCommandListSet ParallelCommandListSet(InPass, RHICmdList, GET_STATID(STAT_CLP_BasePass), View, FParallelCommandListBindings(PassParameters));
						View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(&ParallelCommandListSet, RHICmdList, &PassParameters->InstanceCullingDrawParams);
					});
				}

				const bool bShouldRenderViewForNanite = bNaniteEnabled
					&& !View.bHasNoVisiblePrimitive
					&& (!bDrawSceneViewsInOneNanitePass || ViewIndex == 0); // when bDrawSceneViewsInOneNanitePass, the first view should cover all the other atlased ones
				if (bShouldRenderViewForNanite)
				{
					// Should always have a full Z prepass with Nanite
					check(ShouldRenderPrePass());

					RenderNaniteBasePass(View, ViewIndex);
				}

				RenderEditorPrimitives(GraphBuilder, PassParameters, View, DrawRenderState, InstanceCullingManager);

				if (bShouldRenderView && View.Family->EngineShowFlags.Atmosphere)
				{
					FOpaqueBasePassParameters* SkyPassPassParameters = GraphBuilder.AllocParameters<FOpaqueBasePassParameters>();
					SkyPassPassParameters->BasePass = PassParameters->BasePass;
					SkyPassPassParameters->RenderTargets = BasePassRenderTargets;
					SkyPassPassParameters->View = View.GetShaderParameters();
					SkyPassPassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;

					View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, SkyPassPassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("SkyPassParallel"),
						SkyPassPassParameters,
						ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
						[this, &View, SkyPassPassParameters](const FRDGPass* InPass, FRHICommandListImmediate& RHICmdList)
					{
						FRDGParallelCommandListSet ParallelCommandListSet(InPass, RHICmdList, GET_STATID(STAT_CLP_BasePass), View, FParallelCommandListBindings(SkyPassPassParameters));
						View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].DispatchDraw(&ParallelCommandListSet, RHICmdList, &SkyPassPassParameters->InstanceCullingDrawParams);
					});
				}
			}
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
			{
				FViewInfo& View = InViews[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, InViews.Num() > 1, "View%d", ViewIndex);
				View.BeginRenderView();

				const bool bLumenGIEnabled = GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;

				FMeshPassProcessorRenderState DrawRenderState;
				SetupBasePassState(BasePassDepthStencilAccess, ViewFamily.EngineShowFlags.ShaderComplexity, DrawRenderState);

				FOpaqueBasePassParameters* PassParameters = GraphBuilder.AllocParameters<FOpaqueBasePassParameters>();
				PassParameters->View = View.GetShaderParameters();
				PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
				PassParameters->BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View, ViewIndex, ForwardBasePassTextures, DBufferTextures, bLumenGIEnabled);
				PassParameters->RenderTargets = BasePassRenderTargets;
				PassParameters->RenderTargets.ShadingRateTexture = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, View, FVariableRateShadingImageManager::EVRSPassType::BasePass);

				const bool bShouldRenderView = View.ShouldRenderView();
				if (bShouldRenderView)
				{
					View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("BasePass"),
						PassParameters,
						ERDGPassFlags::Raster,
						[this, &View, PassParameters](FRHICommandList& RHICmdList)
						{
							SetStereoViewport(RHICmdList, View, 1.0f);
							View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
						}
					);
				}

				const bool bShouldRenderViewForNanite = bNaniteEnabled && (!bDrawSceneViewsInOneNanitePass || ViewIndex == 0); // when bDrawSceneViewsInOneNanitePass, the first view should cover all the other atlased ones
				if (bShouldRenderViewForNanite)
				{
					// Should always have a full Z prepass with Nanite
					check(ShouldRenderPrePass());

					RenderNaniteBasePass(View, ViewIndex);
				}

				RenderEditorPrimitives(GraphBuilder, PassParameters, View, DrawRenderState, InstanceCullingManager);

				if (bShouldRenderView && View.Family->EngineShowFlags.Atmosphere)
				{
					FOpaqueBasePassParameters* SkyPassParameters = GraphBuilder.AllocParameters<FOpaqueBasePassParameters>();
					SkyPassParameters->BasePass = PassParameters->BasePass;
					SkyPassParameters->RenderTargets = BasePassRenderTargets;
					SkyPassParameters->View = View.GetShaderParameters();
					SkyPassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;

					View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, SkyPassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("SkyPass"),
						SkyPassParameters,
						ERDGPassFlags::Raster,
						[this, &View, SkyPassParameters](FRHICommandList& RHICmdList)
						{
							SetStereoViewport(RHICmdList, View, 1.0f);
							View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].DispatchDraw(nullptr, RHICmdList, &SkyPassParameters->InstanceCullingDrawParams);
						}
					);
				}
			}
		}
	}
}

template<typename LightMapPolicyType>
void FBasePassMeshProcessor::CollectPSOInitializersForLMPolicy(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	const FMaterial& RESTRICT MaterialResource,
	FMaterialShadingModelField ShadingModels,
	const bool bRenderSkylight,
	const bool bDitheredLODTransition,
	const LightMapPolicyType& RESTRICT LightMapPolicy,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	EPrimitiveType PrimitiveType, 
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Get the shaders if possible for given vertex factory
	TMeshProcessorShaders<
		TBasePassVertexShaderPolicyParamType<LightMapPolicyType>,
		TBasePassPixelShaderPolicyParamType<LightMapPolicyType>> BasePassShaders;
	if (!GetBasePassShaders<LightMapPolicyType>(
		MaterialResource,
		VertexFactoryData.VertexFactoryType,
		LightMapPolicy,
		FeatureLevel,
		bRenderSkylight,
		Get128BitRequirement(),
		GBL_Default, // Currently only Nanite supports non-default layout
		&BasePassShaders.VertexShader,
		&BasePassShaders.PixelShader,
		bOITBasePass
		))
	{
		return;
	}

	// Generate multiple PSO's for LOD transition support? Have to check runtime hit for this?
	// Could do it only when hint is given from the PSOPrecacheParams
	bool bForceEnableStencilDitherState = false;

	// Setup the draw state
	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);
	SetDepthStencilStateForBasePass(
		DrawRenderState,
		FeatureLevel,
		bDitheredLODTransition,
		MaterialResource,
		bEnableReceiveDecalOutput,
		bForceEnableStencilDitherState);
	if (bTranslucentBasePass)
	{
		SetTranslucentRenderState(DrawRenderState, MaterialResource, GShaderPlatformForFeatureLevel[FeatureLevel], TranslucencyPassType);
	}

	// Setup the render target info for basepass
	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;

	// If translucent use different render target setup (see FDeferredShadingSceneRenderer::RenderTranslucencyInner)
	if (bTranslucentBasePass)
	{
		// Extent & scale is not important for PSOs
		FSeparateTranslucencyDimensions SeparateTranslucencyDimensions;
		SeparateTranslucencyDimensions.NumSamples = RenderTargetsInfo.NumSamples;

		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		const float DownsampleScale = 1.0;
		const bool bIsModulate = TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOFModulate;
		const bool bDepthTest = TranslucencyPassType != ETranslucencyPass::TPT_TranslucencyAfterMotionBlur;
		const bool bRenderInSeparateTranslucency = IsSeparateTranslucencyEnabled(TranslucencyPassType, SeparateTranslucencyDimensions.Scale);

		// Always create PSO without separate translucency (could be used when under water for all translucent passes)
		EPixelFormat SceneColorFormat = SceneTexturesConfig.ColorFormat;
		ETextureCreateFlags SceneColorCreateFlags = SceneTexturesConfig.ColorCreateFlags;
		AddRenderTargetInfo(SceneColorFormat, SceneColorCreateFlags, RenderTargetsInfo);

		if (bDepthTest)
		{
			ETextureCreateFlags DepthStencilCreateFlags = SceneTexturesConfig.DepthCreateFlags;
			SetupDepthStencilInfo(PF_DepthStencil, DepthStencilCreateFlags, ERenderTargetLoadAction::ELoad,
				ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite, RenderTargetsInfo);
		}

		AddGraphicsPipelineStateInitializer(
			VertexFactoryData,
			MaterialResource,
			DrawRenderState,
			RenderTargetsInfo,
			BasePassShaders,
			MeshFillMode,
			MeshCullMode,
			PrimitiveType,
			EMeshPassFeatures::Default,
			true /*bRequired*/,
			PSOInitializers);

		// Add another PSO when render in separate translucency because render target format could have changed
		if (bRenderInSeparateTranslucency)
		{
			const FRDGTextureDesc TextureDesc = GetPostDOFTranslucentTextureDesc(TranslucencyPassType, SeparateTranslucencyDimensions, bIsModulate, ShaderPlatform);
			RenderTargetsInfo.RenderTargetFormats[0] = TextureDesc.Format;
			RenderTargetsInfo.RenderTargetFlags[0] = TextureDesc.Flags;

			AddGraphicsPipelineStateInitializer(
				VertexFactoryData,
				MaterialResource,
				DrawRenderState,
				RenderTargetsInfo,
				BasePassShaders,
				MeshFillMode,
				MeshCullMode,
				PrimitiveType,
				EMeshPassFeatures::Default,
				true /*bRequired*/,
				PSOInitializers);
		}
	}
	else
	{
		if (PreCacheParams.bCanvasMaterial)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			SetTranslucentRenderState(DrawRenderState, MaterialResource, GShaderPlatformForFeatureLevel[FeatureLevel], ETranslucencyPass::TPT_AllTranslucency);
			AddRenderTargetInfo(PreCacheParams.GetBassPixelFormat() != PF_Unknown ? PreCacheParams.GetBassPixelFormat() : PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
		}
		else
		{
			// Regular base pass with gbuffer bindings
			SetupGBufferRenderTargetInfo(SceneTexturesConfig, RenderTargetsInfo, true /*bSetupDepthStencil*/);
		}

		AddBasePassGraphicsPipelineStateInitializer(
			FeatureLevel,
			VertexFactoryData,
			MaterialResource,
			DrawRenderState,
			RenderTargetsInfo,
			BasePassShaders,
			MeshFillMode,
			MeshCullMode,
			PrimitiveType,
			true /*bPrecacheAlphaColorChannel*/,
			PSOCollectorIndex,
			PSOInitializers);
	}	
}

template<typename LightMapPolicyType>
bool FBasePassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const bool bIsMasked,
	const bool bIsTranslucent,
	FMaterialShadingModelField ShadingModels,
	const LightMapPolicyType& RESTRICT LightMapPolicy,
	const typename LightMapPolicyType::ElementDataType& RESTRICT LightMapElementData,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	const bool bRenderSkylight = Scene && Scene->ShouldRenderSkylightInBasePass(bIsTranslucent) && ShadingModels.IsLit();

	TMeshProcessorShaders<
		TBasePassVertexShaderPolicyParamType<LightMapPolicyType>,
		TBasePassPixelShaderPolicyParamType<LightMapPolicyType>> BasePassShaders;

	if (!GetBasePassShaders<LightMapPolicyType>(
		MaterialResource,
		VertexFactory->GetType(),
		LightMapPolicy,
		FeatureLevel,
		bRenderSkylight,
		Get128BitRequirement(),
		GBL_Default, // Currently only Nanite uses non-default layout
		&BasePassShaders.VertexShader,
		&BasePassShaders.PixelShader,
		bOITBasePass
		))
	{
		return false;
	}


	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	bool bForceEnableStencilDitherState = false;
	if (ViewIfDynamicMeshCommand && StaticMeshId >= 0 && MeshBatch.bDitheredLODTransition)
	{
		checkSlow(ViewIfDynamicMeshCommand->bIsViewInfo);
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(ViewIfDynamicMeshCommand);
		if (ViewInfo->bAllowStencilDither)
		{
			if (ViewInfo->StaticMeshFadeOutDitheredLODMap[StaticMeshId] || ViewInfo->StaticMeshFadeInDitheredLODMap[StaticMeshId])
			{
				bForceEnableStencilDitherState = true;
			}
		}
	}

	SetDepthStencilStateForBasePass(
		DrawRenderState,
		FeatureLevel,
		MeshBatch.bDitheredLODTransition,
		MaterialResource,
		bEnableReceiveDecalOutput,
		bForceEnableStencilDitherState);
	
	if (bEnableReceiveDecalOutput)
	{
		static const bool bSubstrateDufferPassEnabled = Substrate::IsSubstrateEnabled() && Substrate::IsDBufferPassEnabled(GShaderPlatformForFeatureLevel[FeatureLevel]);

		// Set stencil value for this draw call
		// This is effectively extending the GBuffer using the stencil bits
		uint8 StencilValue = 0;
		if (bSubstrateDufferPassEnabled)
		{
			// Set material's decal responsness through stencil bit. This is only used when Substrate DBuffer pass is enabled.
			// This 'stencil marking' allows Substrate's DBuffer pass to blend decal appropriately. It is only used for Simple 
			// and Single materials, as other materials (Complex, ...) get decal applied during the base pass.
			const uint8 SubstrateMaterialType	= MaterialResource.MaterialGetSubstrateMaterialType_RenderThread();
			const uint32 DecalResponse		= MaterialResource.GetMaterialDecalResponse();
			const bool bSupportDBufferPass	= SubstrateMaterialType == SUBSTRATE_MATERIAL_TYPE_SIMPLE || SubstrateMaterialType == SUBSTRATE_MATERIAL_TYPE_SINGLE;
			const bool bRoughnessResponse	= bSupportDBufferPass && (DecalResponse == MDR_ColorNormalRoughness || DecalResponse == MDR_ColorRoughness	|| DecalResponse == MDR_NormalRoughness || DecalResponse == MDR_Roughness);
			const bool bColorResponse		= bSupportDBufferPass && (DecalResponse == MDR_ColorNormalRoughness || DecalResponse == MDR_ColorNormal		|| DecalResponse == MDR_ColorRoughness	|| DecalResponse == MDR_Color);
			const bool bNormalResponse		= bSupportDBufferPass && (DecalResponse == MDR_ColorNormalRoughness || DecalResponse == MDR_ColorNormal		|| DecalResponse == MDR_NormalRoughness || DecalResponse == MDR_Normal);

			StencilValue =
			  GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_NORMAL, bNormalResponse ? 0x1 : 0x0)
			| GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_DIFFUSE, bColorResponse ? 0x1 : 0x0)
			| GET_STENCIL_BIT_MASK(SUBSTRATE_RECEIVE_DBUFFER_ROUGHNESS, bRoughnessResponse ? 0x1 : 0x0)
			| GET_STENCIL_BIT_MASK(DISTANCE_FIELD_REPRESENTATION, PrimitiveSceneProxy ? PrimitiveSceneProxy->HasDistanceFieldRepresentation() : 0x00)
			| STENCIL_LIGHTING_CHANNELS_MASK(PrimitiveSceneProxy ? PrimitiveSceneProxy->GetLightingChannelStencilValue() : 0x00);
		}
		else
		{
			StencilValue = 
			  GET_STENCIL_BIT_MASK(RECEIVE_DECAL, PrimitiveSceneProxy ? !!PrimitiveSceneProxy->ReceivesDecals() : 0x00)
			| GET_STENCIL_BIT_MASK(DISTANCE_FIELD_REPRESENTATION, PrimitiveSceneProxy ? PrimitiveSceneProxy->HasDistanceFieldRepresentation() : 0x00)
			| STENCIL_LIGHTING_CHANNELS_MASK(PrimitiveSceneProxy ? PrimitiveSceneProxy->GetLightingChannelStencilValue() : 0x00);
		}
		DrawRenderState.SetStencilRef(StencilValue);
	}

	if (bTranslucentBasePass)
	{
		SetTranslucentRenderState(DrawRenderState, MaterialResource, GShaderPlatformForFeatureLevel[FeatureLevel], TranslucencyPassType);
	}

	TBasePassShaderElementData<LightMapPolicyType> ShaderElementData(LightMapElementData);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	FMeshDrawCommandSortKey SortKey = FMeshDrawCommandSortKey::Default;

	if (bTranslucentBasePass)
	{
		SortKey = CalculateTranslucentMeshStaticSortKey(PrimitiveSceneProxy, MeshBatch.MeshIdInPrimitive);
	}
	else
	{
		SortKey = CalculateBasePassMeshStaticSortKey(EarlyZPassMode, bIsMasked, BasePassShaders.VertexShader.GetShader(), BasePassShaders.PixelShader.GetShader());
	}

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

void FBasePassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material && Material->GetRenderingThreadShaderMap())
			{
				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

bool FBasePassMeshProcessor::ShouldDraw(const FMaterial& Material)
{
	// Determine the mesh's material and blend mode.
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	
	bool bShouldDraw = false;
	if (bTranslucentBasePass)
	{
		if (bIsTranslucent && !Material.IsDeferredDecal())
		{
			switch (TranslucencyPassType)
			{
			case ETranslucencyPass::TPT_TranslucencyStandard:
				bShouldDraw = !Material.IsTranslucencyAfterDOFEnabled() && !Material.IsTranslucencyAfterMotionBlurEnabled();
				if (AutoBeforeDOFTranslucencyBoundary > 0.0f)
				{
					bShouldDraw = bShouldDraw || (Material.IsTranslucencyAfterDOFEnabled() && BlendMode != BLEND_ColoredTransmittanceOnly);
					bShouldDraw = bShouldDraw || (Material.IsTranslucencyAfterDOFEnabled() && (Material.IsDualBlendingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || IsModulateBlendMode(BlendMode)));
				}
				break;

			case ETranslucencyPass::TPT_TranslucencyStandardModulate:
				bShouldDraw = !Material.IsTranslucencyAfterDOFEnabled() && !Material.IsTranslucencyAfterMotionBlurEnabled() 
					&& (Material.IsDualBlendingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || IsModulateBlendMode(BlendMode));
				break;

			case ETranslucencyPass::TPT_TranslucencyAfterDOF:
				bShouldDraw = Material.IsTranslucencyAfterDOFEnabled() && BlendMode != BLEND_ColoredTransmittanceOnly;
				break;

				// only dual blended or modulate surfaces need background modulation
			case ETranslucencyPass::TPT_TranslucencyAfterDOFModulate:
				bShouldDraw = Material.IsTranslucencyAfterDOFEnabled() && (Material.IsDualBlendingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || IsModulateBlendMode(BlendMode));
				break;

			case ETranslucencyPass::TPT_TranslucencyAfterMotionBlur:
				bShouldDraw = Material.IsTranslucencyAfterMotionBlurEnabled();
				break;

			case ETranslucencyPass::TPT_AllTranslucency:
				bShouldDraw = true;
				break;
			}
		}
	}
	else
	{
		bShouldDraw = !bIsTranslucent;
	}

	return bShouldDraw;
}

bool FBasePassMeshProcessor::TryAddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material)
{
	// Determine the mesh's material and blend mode.
	const EBlendMode BlendMode = Material.GetBlendMode();
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	const bool bIsMasked = IsMaskedBlendMode(BlendMode);
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);


	bool bShouldDraw = false;
	if (AutoBeforeDOFTranslucencyBoundary > 0.0f && PrimitiveSceneProxy && bIsTranslucent && !Material.IsDeferredDecal())
	{
		check(ViewIfDynamicMeshCommand);
		check(TranslucencyPassType != ETranslucencyPass::TPT_MAX);

		const FVector& BoundsOrigin = PrimitiveSceneProxy->GetBounds().Origin;

		const FVector& ViewOrigin = ViewIfDynamicMeshCommand->ViewMatrices.GetViewOrigin();
		const FVector ViewForward = ViewIfDynamicMeshCommand->ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(2);

		const FVector CameraToObject = BoundsOrigin - ViewOrigin;
		float Distance = FVector::DotProduct(CameraToObject, ViewForward);
		bool bIsInDOFBackground = Distance > AutoBeforeDOFTranslucencyBoundary;

		bool bIsStandardTranslucency = !Material.IsTranslucencyAfterDOFEnabled() && !Material.IsTranslucencyAfterMotionBlurEnabled();
		bool bIsAfterDOF = Material.IsTranslucencyAfterDOFEnabled() && !Material.IsTranslucencyAfterMotionBlurEnabled() && BlendMode != BLEND_ColoredTransmittanceOnly;
		bool bIsAfterDOFModulate = Material.IsTranslucencyAfterDOFEnabled() && (Material.IsDualBlendingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || IsModulateBlendMode(BlendMode));

		// When AutoBeforeDOFTranslucencyBoundary is valid, we automatically move After DOF translucent meshes (never blurred by DOF)
		// before DOF if those elements are behind the focus distance.
		if (TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandard)
		{
			if (bIsStandardTranslucency)
			{
				bShouldDraw = true;
			}
			else if (bIsInDOFBackground)
			{
				bShouldDraw = bIsAfterDOF || bIsAfterDOFModulate;
			}
		}
		else if (TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF)
		{
			bShouldDraw = bIsAfterDOF && !bIsInDOFBackground;
		}
		else if (TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOFModulate)
		{
			bShouldDraw = bIsAfterDOFModulate && !bIsInDOFBackground;
		}
		else
		{
			unimplemented();
		}
	}
	else
	{
		bShouldDraw = ShouldDraw(Material);
	}

	// Only draw opaque materials.
	bool bResult = true;
	if (bShouldDraw
		&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
		&& ShouldIncludeMaterialInDefaultOpaquePass(Material))
	{
		// Check for a cached light-map.
		const bool bIsLitMaterial = ShadingModels.IsLit();
		const bool bAllowStaticLighting = IsStaticLightingAllowed();

		const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
			? MeshBatch.LCI->GetLightMapInteraction(FeatureLevel)
			: FLightMapInteraction();

		const bool bAllowIndirectLightingCache = Scene && Scene->PrecomputedLightVolumes.Num() > 0;
		const bool bUseVolumetricLightmap = Scene && Scene->VolumetricLightmapSceneData.HasData();

		FMeshMaterialShaderElementData MeshMaterialShaderElementData;
		MeshMaterialShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

		// Render volumetric translucent self-shadowing only for >= SM4 and fallback to non-shadowed for lesser shader models
		if (bIsLitMaterial
			&& bIsTranslucent
			&& PrimitiveSceneProxy
			&& PrimitiveSceneProxy->CastsVolumetricTranslucentShadow())
		{
			checkSlow(ViewIfDynamicMeshCommand && ViewIfDynamicMeshCommand->bIsViewInfo);
			const FViewInfo* ViewInfo = (FViewInfo*)ViewIfDynamicMeshCommand;

			const int32 PrimitiveIndex = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex();

			const FUniformBufferRHIRef* UniformBufferPtr = ViewInfo->TranslucentSelfShadowUniformBufferMap.Find(PrimitiveIndex);

			FSelfShadowLightCacheElementData ElementData;
			ElementData.LCI = MeshBatch.LCI;
			ElementData.SelfShadowTranslucencyUniformBuffer = UniformBufferPtr ? (*UniformBufferPtr).GetReference() : GEmptyTranslucentSelfShadowUniformBuffer.GetUniformBufferRHI();

			if (bIsLitMaterial
				&& bAllowStaticLighting
				&& bUseVolumetricLightmap
				&& PrimitiveSceneProxy)
			{
				bResult = Process< FSelfShadowedVolumetricLightmapPolicy >(
					MeshBatch,
					BatchElementMask,
					StaticMeshId,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					Material,
					bIsMasked,
					bIsTranslucent,
					ShadingModels,
					FSelfShadowedVolumetricLightmapPolicy(),
					ElementData,
					MeshFillMode,
					MeshCullMode);
			}
			else if (IsIndirectLightingCacheAllowed(FeatureLevel)
				&& bAllowIndirectLightingCache
				&& PrimitiveSceneProxy)
			{
				// Apply cached point indirect lighting as well as self shadowing if needed
				bResult = Process< FSelfShadowedCachedPointIndirectLightingPolicy >(
					MeshBatch,
					BatchElementMask,
					StaticMeshId,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					Material,
					bIsMasked,
					bIsTranslucent,
					ShadingModels,
					FSelfShadowedCachedPointIndirectLightingPolicy(),
					ElementData,
					MeshFillMode,
					MeshCullMode);
			}
			else
			{
				bResult = Process< FSelfShadowedTranslucencyPolicy >(
					MeshBatch,
					BatchElementMask,
					StaticMeshId,
					PrimitiveSceneProxy,
					MaterialRenderProxy,
					Material,
					bIsMasked,
					bIsTranslucent,
					ShadingModels,
					FSelfShadowedTranslucencyPolicy(),
					ElementData.SelfShadowTranslucencyUniformBuffer,
					MeshFillMode,
					MeshCullMode);
			}
		}
		else
		{
			ELightMapPolicyType UniformLightMapPolicyType = GetUniformLightMapPolicyType(FeatureLevel, Scene, MeshBatch.LCI, PrimitiveSceneProxy, Material);
			bResult = Process< FUniformLightMapPolicy >(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				Material,
				bIsMasked,
				bIsTranslucent,
				ShadingModels,
				FUniformLightMapPolicy(UniformLightMapPolicyType),
				MeshBatch.LCI,
				MeshFillMode,
				MeshCullMode);
		}
	}

	return bResult;
}

ELightMapPolicyType FBasePassMeshProcessor::GetUniformLightMapPolicyType(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FLightCacheInterface* LCI, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, const FMaterial& Material)
{
	// Check for a cached light-map.
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	const bool bIsLitMaterial = ShadingModels.IsLit();
	const bool bAllowStaticLighting = IsStaticLightingAllowed();

	const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && LCI && bIsLitMaterial)
		? LCI->GetLightMapInteraction(FeatureLevel)
		: FLightMapInteraction();

	// force LQ lightmaps based on system settings
	const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);
	const bool bAllowHighQualityLightMaps = bPlatformAllowsHighQualityLightMaps && LightMapInteraction.AllowsHighQualityLightmaps();

	const bool bAllowIndirectLightingCache = Scene && Scene->PrecomputedLightVolumes.Num() > 0;
	const bool bUseVolumetricLightmap = Scene && Scene->VolumetricLightmapSceneData.HasData();

	static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
	const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

	ELightMapPolicyType LightMapPolicyType = LMP_DUMMY;
	switch (LightMapInteraction.GetType())
	{
	case LMIT_Texture:
		if (bAllowHighQualityLightMaps)
		{
			const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && LCI && bIsLitMaterial)
				? LCI->GetShadowMapInteraction(FeatureLevel)
				: FShadowMapInteraction();

			if (ShadowMapInteraction.GetType() == SMIT_Texture)
			{
				LightMapPolicyType = LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP;
			}
			else
			{
				LightMapPolicyType = LMP_HQ_LIGHTMAP;
			}
		}
		else if (bAllowLowQualityLightMaps)
		{
			LightMapPolicyType = LMP_LQ_LIGHTMAP;
		}
		else
		{
			LightMapPolicyType = LMP_NO_LIGHTMAP;
		}
		break;
	default:
		if (bIsLitMaterial
			&& bAllowStaticLighting
			&& bUseVolumetricLightmap
			&& PrimitiveSceneProxy
			&& (PrimitiveSceneProxy->IsMovable()
				|| PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting()
				|| PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
		{
			LightMapPolicyType = LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING;
		}
		else if (bIsLitMaterial
			&& IsIndirectLightingCacheAllowed(FeatureLevel)
			&& Scene
			&& Scene->PrecomputedLightVolumes.Num() > 0
			&& PrimitiveSceneProxy)
		{
			const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
			const bool bPrimitiveIsMovable = PrimitiveSceneProxy->IsMovable();
			const bool bPrimitiveUsesILC = PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

			// Use the indirect lighting cache shaders if the object has a cache allocation
			// This happens for objects with unbuilt lighting
			if (bPrimitiveUsesILC &&
				((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
					// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
					// And movable objects are sometimes rendered in the static draw lists
					|| bPrimitiveIsMovable))
			{
				if (CanIndirectLightingCacheUseVolumeTexture(FeatureLevel)
					// Translucency forces point sample for pixel performance
					&& !bIsTranslucent
					&& ((IndirectLightingCacheAllocation && !IndirectLightingCacheAllocation->bPointSample)
						|| (bPrimitiveIsMovable && PrimitiveSceneProxy->GetIndirectLightingCacheQuality() == ILCQ_Volume)))
				{
					// Use a lightmap policy that supports reading indirect lighting from a volume texture for dynamic objects
					LightMapPolicyType = LMP_CACHED_VOLUME_INDIRECT_LIGHTING;
				}
				else
				{
					// Use a lightmap policy that supports reading indirect lighting from a single SH sample
					LightMapPolicyType = LMP_CACHED_POINT_INDIRECT_LIGHTING;
				}
			}
			else
			{
				LightMapPolicyType = LMP_NO_LIGHTMAP;
			}
		}
		else
		{
			LightMapPolicyType = LMP_NO_LIGHTMAP;
		}
		break;
	};

	check(LightMapPolicyType != LMP_DUMMY);

	return LightMapPolicyType;
}

TArray<ELightMapPolicyType, TInlineAllocator<2>> FBasePassMeshProcessor::GetUniformLightMapPolicyTypeForPSOCollection(ERHIFeatureLevel::Type FeatureLevel, const FMaterial& Material)
{
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();

	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
	const bool bIsLitMaterial = ShadingModels.IsLit();
	const bool bAllowStaticLighting = IsStaticLightingAllowed();

	const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);
	static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
	const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

	// Retrieve those values or have as global precache params (if not known then we have to assume they can be used for now)
	const bool bAllowIndirectLightingCache = true;// Scene&& Scene->PrecomputedLightVolumes.Num() > 0;
	const bool bUseVolumetricLightmap = true;// Scene&& Scene->VolumetricLightmapSceneData.HasData();

	static const auto CVarSupportLightMapPolicyMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PSOPrecache.LightMapPolicyMode"));
	const bool bNoLightMapOnlyMode = (CVarSupportLightMapPolicyMode && CVarSupportLightMapPolicyMode->GetValueOnAnyThread() == 1);

	TArray<ELightMapPolicyType, TInlineAllocator<2>> UniformLightMapPolicyTypes;

	// always add the fallback no lightmap mode
	UniformLightMapPolicyTypes.Add(LMP_NO_LIGHTMAP);

	if (!bNoLightMapOnlyMode)
	{
		// Simplified version of GetUniformLightMapPolicyType with worst case to collect all types which could be enabled at runtime	

		// LightMapInterationType::LMIT_Texture
		{
			if (bPlatformAllowsHighQualityLightMaps)
			{
				// EShadowMapInteractionType::SMIT_Texture
				if (bAllowStaticLighting && bIsLitMaterial)
				{
					UniformLightMapPolicyTypes.Add(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP);
				}
				// else
				{
					UniformLightMapPolicyTypes.Add(LMP_HQ_LIGHTMAP);
				}
			}
			if (bAllowLowQualityLightMaps)
			{
				UniformLightMapPolicyTypes.Add(LMP_LQ_LIGHTMAP);
			}
		}
		// else LightMapInterationType
		{
			if (bIsLitMaterial && bAllowStaticLighting && bUseVolumetricLightmap)
			{
				UniformLightMapPolicyTypes.Add(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING);
			}

			if (IsIndirectLightingCacheAllowed(FeatureLevel) && bAllowIndirectLightingCache)
			{
				if (CanIndirectLightingCacheUseVolumeTexture(FeatureLevel)
					// Translucency forces point sample for pixel performance
					&& !bIsTranslucent)
				{
					UniformLightMapPolicyTypes.Add(LMP_CACHED_VOLUME_INDIRECT_LIGHTING);
				}
				// else
				{
					UniformLightMapPolicyTypes.Add(LMP_CACHED_POINT_INDIRECT_LIGHTING);
				}
			}
		}
	}

	return UniformLightMapPolicyTypes;
}

void FBasePassMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Check if material should be rendered
	bool bShouldDraw = ShouldDraw(Material)
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
		&& ShouldIncludeMaterialInDefaultOpaquePass(Material);
	bShouldDraw = bShouldDraw || (PreCacheParams.bCanvasMaterial && MeshPassType == EMeshPass::BasePass);
	if (!bShouldDraw || !PreCacheParams.bRenderInMainPass)
	{
		return;
	}

	// PSO precaching enabled for TranslucencyAll
	if (MeshPassType == EMeshPass::TranslucencyAll && CVarPSOPrecacheTranslucencyAllPass.GetValueOnAnyThread() == 0)
	{
		return;
	}

	// Determine the mesh's material and blend mode.
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	bool bMovable = PreCacheParams.Mobility == EComponentMobility::Movable || PreCacheParams.Mobility == EComponentMobility::Stationary;
	bool bDitheredLODTransition = !bMovable && Material.IsDitheredLODTransition() && !PreCacheParams.bForceLODModel;

	{
		bool bRenderSkyLight = true; // generate for both skylight enabled/disabled? Or can this be known already at this point?
		CollectPSOInitializersForSkyLight(SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, bRenderSkyLight, bDitheredLODTransition, MeshFillMode, MeshCullMode, (EPrimitiveType)PreCacheParams.PrimitiveType, PSOInitializers);
		bRenderSkyLight = false;
		CollectPSOInitializersForSkyLight(SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, bRenderSkyLight, bDitheredLODTransition, MeshFillMode, MeshCullMode, (EPrimitiveType)PreCacheParams.PrimitiveType, PSOInitializers);
	}
}

void FBasePassMeshProcessor::CollectPSOInitializersForSkyLight(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	const FMaterial& RESTRICT Material,
	const bool bRenderSkyLight,
	const bool bDitheredLODTransition,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	EPrimitiveType PrimitiveType, 
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();

	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
	const bool bIsLitMaterial = ShadingModels.IsLit();
	
	static const auto CVarSupportLightMapPolicyMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PSOPrecache.LightMapPolicyMode"));
	const bool bNoLightMapOnlyMode = (CVarSupportLightMapPolicyMode && CVarSupportLightMapPolicyMode->GetValueOnAnyThread() == 1);

	if (!bNoLightMapOnlyMode && bIsLitMaterial && bIsTranslucent)
	{
		const bool bAllowStaticLighting = IsStaticLightingAllowed();

		// Retrieve those values or have as global precache params (if not known then we have to assume they can be used for now)
		const bool bAllowIndirectLightingCache = true;// Scene&& Scene->PrecomputedLightVolumes.Num() > 0;
		const bool bUseVolumetricLightmap = true;// Scene&& Scene->VolumetricLightmapSceneData.HasData();

		if (bAllowStaticLighting && bUseVolumetricLightmap)
		{
			CollectPSOInitializersForLMPolicy< FSelfShadowedVolumetricLightmapPolicy >(
				SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, ShadingModels, bRenderSkyLight, bDitheredLODTransition,
				FSelfShadowedVolumetricLightmapPolicy(), MeshFillMode, MeshCullMode, PrimitiveType, PSOInitializers);
		}

		if (IsIndirectLightingCacheAllowed(FeatureLevel) && bAllowIndirectLightingCache)
		{
			CollectPSOInitializersForLMPolicy< FSelfShadowedCachedPointIndirectLightingPolicy >(
				SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, ShadingModels, bRenderSkyLight, bDitheredLODTransition,
				FSelfShadowedCachedPointIndirectLightingPolicy(), MeshFillMode, MeshCullMode, PrimitiveType, PSOInitializers);
		}

		CollectPSOInitializersForLMPolicy< FSelfShadowedTranslucencyPolicy >(
			SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, ShadingModels, bRenderSkyLight, bDitheredLODTransition,
			FSelfShadowedTranslucencyPolicy(), MeshFillMode, MeshCullMode, PrimitiveType, PSOInitializers);
	}

	TArray<ELightMapPolicyType, TInlineAllocator<2>> UniformLightMapPolicyTypes = GetUniformLightMapPolicyTypeForPSOCollection(FeatureLevel, Material);
	for (ELightMapPolicyType LightMapPolicyType : UniformLightMapPolicyTypes)
	{
		CollectPSOInitializersForLMPolicy< FUniformLightMapPolicy >(
			SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, ShadingModels, bRenderSkyLight, bDitheredLODTransition,
			FUniformLightMapPolicy(LightMapPolicyType), MeshFillMode, MeshCullMode, PrimitiveType, PSOInitializers);
	}
}

FBasePassMeshProcessor::FBasePassMeshProcessor(
	EMeshPass::Type InMeshPassType,
	const FScene* Scene,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext,
	EFlags Flags,
	ETranslucencyPass::Type InTranslucencyPassType)
	: FMeshPassProcessor(InMeshPassType, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
	, TranslucencyPassType(InTranslucencyPassType)
	, bTranslucentBasePass(InTranslucencyPassType != ETranslucencyPass::TPT_MAX)
	, bOITBasePass(InTranslucencyPassType != ETranslucencyPass::TPT_MAX && OIT::IsSortedPixelsEnabled(GetFeatureLevelShaderPlatform(InFeatureLevel)))
	, bEnableReceiveDecalOutput((Flags & EFlags::CanUseDepthStencil) == EFlags::CanUseDepthStencil)
	, EarlyZPassMode(Scene ? Scene->EarlyZPassMode : DDM_None)
	, bRequiresExplicit128bitRT((Flags & EFlags::bRequires128bitRT) == EFlags::bRequires128bitRT)
{
	if (InTranslucencyPassType != ETranslucencyPass::TPT_MAX && InViewIfDynamicMeshCommand && ViewIfDynamicMeshCommand->bIsViewInfo)
	{
		const FViewInfo* ViewInfo = (FViewInfo*)ViewIfDynamicMeshCommand;
		if (InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyStandard ||
			InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF ||
			InTranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOFModulate)
		{
			AutoBeforeDOFTranslucencyBoundary = ViewInfo->AutoBeforeDOFTranslucencyBoundary;
		}
	}
}

FMeshPassProcessor* CreateBasePassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	const FExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess = FScene::GetDefaultBasePassDepthStencilAccess(FeatureLevel);
	
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupBasePassState(DefaultBasePassDepthStencilAccess, false, PassDrawRenderState);

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::BasePass, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags);
}

FMeshPassProcessor* CreateTranslucencyStandardPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyStandard, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyStandard);
}

FMeshPassProcessor* CreateTranslucencyStandardModulatePassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyStandardModulate, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyStandardModulate);
}

FMeshPassProcessor* CreateTranslucencyAfterDOFProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyAfterDOF, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyAfterDOF);
}

FMeshPassProcessor* CreateTranslucencyAfterDOFModulateProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyAfterDOFModulate, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyAfterDOFModulate);
}

FMeshPassProcessor* CreateTranslucencyAfterMotionBlurProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;	
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop);

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::None;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyAfterMotionBlur, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_TranslucencyAfterMotionBlur);
}

FMeshPassProcessor* CreateTranslucencyAllPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	const FBasePassMeshProcessor::EFlags Flags = FBasePassMeshProcessor::EFlags::CanUseDepthStencil;

	return new FBasePassMeshProcessor(EMeshPass::TranslucencyAll, Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, Flags, ETranslucencyPass::TPT_AllTranslucency);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(BasePass, CreateBasePassProcessor, EShadingPath::Deferred, EMeshPass::BasePass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyStandardPass, CreateTranslucencyStandardPassProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyStandard, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyStandardModulatePass, CreateTranslucencyStandardModulatePassProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyStandardModulate, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyAfterDOFPass, CreateTranslucencyAfterDOFProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAfterDOF, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyAfterDOFModulatePass, CreateTranslucencyAfterDOFModulateProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAfterDOFModulate, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyAfterMotionBlurPass, CreateTranslucencyAfterMotionBlurProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAfterMotionBlur, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(TranslucencyAllPass, CreateTranslucencyAllPassProcessor, EShadingPath::Deferred, EMeshPass::TranslucencyAll, EMeshPassFlags::MainView);
