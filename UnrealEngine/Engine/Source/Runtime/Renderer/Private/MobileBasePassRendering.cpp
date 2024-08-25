// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "MobileBasePassRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "PrimitiveSceneInfo.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "EditorPrimitivesRendering.h"

#include "FramePro/FrameProProfiler.h"
#include "PostProcess/PostProcessPixelProjectedReflectionMobile.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/SpecularProfile.h"
#include "LocalLightSceneProxy.h"
#include "ReflectionEnvironment.h"
#include "RenderCore.h"

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarMobileDisableVertexFog(
	TEXT("r.Mobile.DisableVertexFog"),
	1,
	TEXT("If true, vertex fog will be omitted from the most of the mobile base pass shaders. Instead, fog will be applied in a separate pass and only when scene has a fog component."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileEnableMovableSpotLightShadows(
	TEXT("r.Mobile.EnableMovableSpotlightsShadow"),
	0,
	TEXT("If 1 then enable movable spotlight shadow support"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileMaxVisibleMovableSpotLightShadows(
	TEXT("r.Mobile.MaxVisibleMovableSpotLightShadows"),
	8,
	TEXT("The max number of visible spotlighs can cast shadow sorted by screen size, should be as less as possible for performance reason"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileSceneDepthAux(
	TEXT("r.Mobile.SceneDepthAux"),
	1,
	TEXT("1: 16F SceneDepthAux Format")
	TEXT("2: 32F SceneDepthAux Format"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobilePropagateAlpha(
	TEXT("r.Mobile.PropagateAlpha"),
	0,
	TEXT("0: Disabled")
	TEXT("1: Propagate Full Alpha Propagate"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);


IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FMobileBasePassUniformParameters, "MobileBasePass", SceneTextures);

static TAutoConsoleVariable<int32> CVarMobileUseHWsRGBEncoding(
	TEXT("r.Mobile.UseHWsRGBEncoding"),
	0,
	TEXT("0: Write sRGB encoding in the shader\n")
	TEXT("1: Use GPU HW to convert linear to sRGB automatically (device must support sRGB write control)\n"),
	ECVF_RenderThreadSafe);

EMobileTranslucentColorTransmittanceMode MobileDefaultTranslucentColorTransmittanceMode(EShaderPlatform Platform)
{
	if (FDataDrivenShaderPlatformInfo::GetSupportsDualSourceBlending(Platform) || IsSimulatedPlatform(Platform))
	{
		return EMobileTranslucentColorTransmittanceMode::DUAL_SRC_BLENDING;
	}
	if (IsMetalMobilePlatform(Platform) || IsAndroidOpenGLESPlatform(Platform))
	{
		return EMobileTranslucentColorTransmittanceMode::PROGRAMMABLE_BLENDING;
	}
	return EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING;
}

static bool SupportsTranslucentColorTransmittanceFallback(EShaderPlatform Platform, EMobileTranslucentColorTransmittanceMode Fallback)
{
	switch (Fallback)
	{
	default:
		return true;
	case EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING:
		return IsSimulatedPlatform(Platform) || IsAndroidPlatform(Platform);
	}
}

EMobileTranslucentColorTransmittanceMode MobileActiveTranslucentColorTransmittanceMode(EShaderPlatform Platform, bool bExplicitDefaultMode)
{
	const EMobileTranslucentColorTransmittanceMode DefaultMode = MobileDefaultTranslucentColorTransmittanceMode(Platform);

	if (DefaultMode == EMobileTranslucentColorTransmittanceMode::DUAL_SRC_BLENDING)
	{
		if (!GSupportsDualSrcBlending)
		{
			if (SupportsTranslucentColorTransmittanceFallback(Platform, EMobileTranslucentColorTransmittanceMode::PROGRAMMABLE_BLENDING) && GSupportsShaderFramebufferFetch)
			{
				return EMobileTranslucentColorTransmittanceMode::PROGRAMMABLE_BLENDING;
			}
			else
			{
				check(SupportsTranslucentColorTransmittanceFallback(Platform, EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING));
				return EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING;
			}
		}
	}
	else if (DefaultMode == EMobileTranslucentColorTransmittanceMode::PROGRAMMABLE_BLENDING)
	{
		if (!GSupportsShaderFramebufferFetch || !GSupportsShaderFramebufferFetchProgrammableBlending)
		{
			check(SupportsTranslucentColorTransmittanceFallback(Platform, EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING));
			return EMobileTranslucentColorTransmittanceMode::SINGLE_SRC_BLENDING;
		}
	}

	return bExplicitDefaultMode ? DefaultMode : EMobileTranslucentColorTransmittanceMode::DEFAULT;
}

bool ShouldCacheShaderByPlatformAndOutputFormat(EShaderPlatform Platform, EOutputFormat OutputFormat)
{
	bool bSupportsMobileHDR = IsMobileHDR();
	bool bShaderUsesLDR = (OutputFormat == LDR_GAMMA_32);

	// only cache this shader if the LDR/HDR output matches what we currently support.  IsMobileHDR can't change, so we don't need
	// the LDR shaders if we are doing HDR, and vice-versa.
	return (bShaderUsesLDR && !bSupportsMobileHDR) || (!bShaderUsesLDR && bSupportsMobileHDR);
}

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TMobileBasePassVS< LightMapPolicyType, LDR_GAMMA_32 > TMobileBasePassVS##LightMapPolicyName##LDRGamma32; \
	typedef TMobileBasePassVS< LightMapPolicyType, HDR_LINEAR_64 > TMobileBasePassVS##LightMapPolicyName##HDRLinear64; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##LDRGamma32, TEXT("/Engine/Private/MobileBasePassVertexShader.usf"), TEXT("Main"), SF_Vertex); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##HDRLinear64, TEXT("/Engine/Private/MobileBasePassVertexShader.usf"), TEXT("Main"), SF_Vertex);

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE2(LightMapPolicyType, LightMapPolicyName, LocalLightSetting, ThinTranslucencyEnum, ThinTranslucencyName) \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, false, LocalLightSetting, EMobileTranslucentColorTransmittanceMode::ThinTranslucencyEnum > TMobileBasePassPS##LightMapPolicyName##LDRGamma32##LocalLightSetting##ThinTranslucencyName; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, false, LocalLightSetting, EMobileTranslucentColorTransmittanceMode::ThinTranslucencyEnum > TMobileBasePassPS##LightMapPolicyName##HDRLinear64##LocalLightSetting##ThinTranslucencyName; \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, true, LocalLightSetting, EMobileTranslucentColorTransmittanceMode::ThinTranslucencyEnum > TMobileBasePassPS##LightMapPolicyName##LDRGamma32##Skylight##LocalLightSetting##ThinTranslucencyName; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, true, LocalLightSetting, EMobileTranslucentColorTransmittanceMode::ThinTranslucencyEnum > TMobileBasePassPS##LightMapPolicyName##HDRLinear64##Skylight##LocalLightSetting##ThinTranslucencyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##LDRGamma32##LocalLightSetting##ThinTranslucencyName, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##HDRLinear64##LocalLightSetting##ThinTranslucencyName, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##LDRGamma32##Skylight##LocalLightSetting##ThinTranslucencyName, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##HDRLinear64##Skylight##LocalLightSetting##ThinTranslucencyName, TEXT("/Engine/Private/MobileBasePassPixelShader.usf"), TEXT("Main"), SF_Pixel);

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, LocalLightSetting) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE2(LightMapPolicyType, LightMapPolicyName, LocalLightSetting, DEFAULT,) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE2(LightMapPolicyType, LightMapPolicyName, LocalLightSetting, SINGLE_SRC_BLENDING, ThinTranslGrey)

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, LOCAL_LIGHTS_DISABLED) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, LOCAL_LIGHTS_ENABLED) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, LOCAL_LIGHTS_BUFFER)

// Implement shader types per lightmap policy 
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP>, FMobileDistanceFieldShadowsAndLQLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM>, FMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_LIGHTMAP>, FMobileDirectionalLightCSMAndLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT>, FMobileDirectionalLightAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT>, FMobileDirectionalLightCSMAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP>, FMobileMovableDirectionalLightWithLightmapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP>, FMobileMovableDirectionalLightCSMWithLightmapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM>, FMobileDirectionalLightAndCSMPolicy);

bool MaterialRequiresColorTransmittanceBlending(const FMaterialShaderParameters& MaterialParameters)
{
	return MaterialParameters.ShadingModels.HasShadingModel(MSM_ThinTranslucent) || MaterialParameters.BlendMode == BLEND_TranslucentColoredTransmittance;
}

bool ShouldCacheShaderForColorTransmittanceFallback(const FMaterialShaderPermutationParameters& Parameters, const EMobileTranslucentColorTransmittanceMode TranslucentColorTransmittanceFallback)
{
	if (TranslucentColorTransmittanceFallback == EMobileTranslucentColorTransmittanceMode::DEFAULT)
	{
		return true;
	}
	
	if (!MaterialRequiresColorTransmittanceBlending(Parameters.MaterialParameters))
	{
		return false;
	}

	return SupportsTranslucentColorTransmittanceFallback(Parameters.Platform, TranslucentColorTransmittanceFallback);
}

// shared defines for mobile base pass VS and PS
void MobileBasePassModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment, EOutputFormat OutputFormat)
{
	static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.UseHWsRGBEncoding"));
	const bool bMobileUseHWsRGBEncoding = (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetValueOnAnyThread() == 1);
	OutEnvironment.SetDefine( TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32 && !bMobileUseHWsRGBEncoding);
	OutEnvironment.SetDefine( TEXT("OUTPUT_MOBILE_HDR"), OutputFormat == HDR_LINEAR_64 ? 1u : 0u);
	
	const bool bTranslucentMaterial = 
		IsTranslucentBlendMode(Parameters.MaterialParameters) ||
		Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_SingleLayerWater);

	// This define simply lets the compilation environment know that we are using a Base Pass PixelShader.
	OutEnvironment.SetDefine(TEXT("IS_BASE_PASS"), 1u);
	OutEnvironment.SetDefine(TEXT("IS_MOBILE_BASE_PASS"), 1u);
		
	const bool bDeferredShadingEnabled = IsMobileDeferredShadingEnabled(Parameters.Platform);
	if (bDeferredShadingEnabled)
	{
		OutEnvironment.SetDefine(TEXT("ENABLE_SHADINGMODEL_SUPPORT_MOBILE_DEFERRED"), MobileUsesGBufferCustomData(Parameters.Platform));
	}

	const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
	OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEPTHREAD_SUBPASS"), bTranslucentMaterial && !bMobileForceDepthRead ? 1u : 0u);
	// translucency is in the same subpass with deferred shading shaders, so it has access to GBuffer
	const bool bDeferredShadingSubpass = (bDeferredShadingEnabled && bTranslucentMaterial && !Parameters.MaterialParameters.bIsMobileSeparateTranslucencyEnabled);
	OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEFERREDSHADING_SUBPASS"), bDeferredShadingSubpass ? 1u : 0u);

	// HLSLcc does not support dual source blending, so force DXC if needed
	if (bTranslucentMaterial && FDataDrivenShaderPlatformInfo::GetSupportsDxc(Parameters.Platform) && IsHlslccShaderPlatform(Parameters.Platform) &&
		MaterialRequiresColorTransmittanceBlending(Parameters.MaterialParameters) && MobileDefaultTranslucentColorTransmittanceMode(Parameters.Platform) == EMobileTranslucentColorTransmittanceMode::DUAL_SRC_BLENDING)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	}
}

template<typename LightMapPolicyType>
bool TMobileBasePassPSPolicyParamType<LightMapPolicyType>::ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment)
{
	// Get quality settings for shader platform
	const UShaderPlatformQualitySettings* MaterialShadingQuality = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(Platform);
	const FMaterialQualityOverrides& QualityOverrides = MaterialShadingQuality->GetQualityOverrides(QualityLevel);

	// the point of this check is to keep the logic between enabling overrides here and in UMaterial::GetQualityLevelUsage() in sync
	checkf(QualityOverrides.CanOverride(Platform), TEXT("ShaderPlatform %d was not marked as being able to use quality overrides! Include it in CanOverride() and recook."), static_cast<int32>(Platform));
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_FULLY_ROUGH"), QualityOverrides.bEnableOverride && QualityOverrides.bForceFullyRough != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_NONMETAL"), QualityOverrides.bEnableOverride && QualityOverrides.bForceNonMetal != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("QL_FORCEDISABLE_LM_DIRECTIONALITY"), QualityOverrides.bEnableOverride && QualityOverrides.bForceDisableLMDirectionality != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_DISABLE_PREINTEGRATEDGF"), QualityOverrides.bEnableOverride && QualityOverrides.bForceDisablePreintegratedGF != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_SHADOW_QUALITY"), (uint32)QualityOverrides.MobileShadowQuality);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_DISABLE_MATERIAL_NORMAL"), QualityOverrides.bEnableOverride && QualityOverrides.bDisableMaterialNormalCalculation);
	return true;
}

extern void SetupDummyForwardLightUniformParameters(FRDGBuilder& GraphBuilder, FForwardLightData& ForwardLightData, EShaderPlatform ShaderPlatform);

void SetupMobileBasePassUniformParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	EMobileBasePass BasePass,
	EMobileSceneTextureSetupMode SetupMode,
	const FMobileBasePassTextures& MobileBasePassTextures,
	FMobileBasePassUniformParameters& BasePassParameters)
{
	SetupFogUniformParameters(GraphBuilder, View, BasePassParameters.Fog);

	if (View.ForwardLightingResources.ForwardLightData)
	{
		BasePassParameters.Forward = *View.ForwardLightingResources.ForwardLightData;
	}
	else
	{
		SetupDummyForwardLightUniformParameters(GraphBuilder, BasePassParameters.Forward, View.GetShaderPlatform());
	}

	// Setup forward light data for mobile multi-view secondary view if enabled and available
	const FViewInfo* InstancedView = View.GetInstancedView();
	const FForwardLightData* InstancedForwardLightData = InstancedView ? InstancedView->ForwardLightingResources.ForwardLightData : nullptr;
	if (View.bIsMobileMultiViewEnabled && InstancedForwardLightData)
	{
		BasePassParameters.ForwardMMV = *InstancedForwardLightData;
	}
	else
	{
		BasePassParameters.ForwardMMV = BasePassParameters.Forward; // Duplicate primary data if not
	}

	const FScene* Scene = View.Family->Scene ? View.Family->Scene->GetRenderScene() : nullptr;
	const FPlanarReflectionSceneProxy* ReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;
	SetupPlanarReflectionUniformParameters(View, ReflectionSceneProxy, BasePassParameters.PlanarReflection);
	if (View.PrevViewInfo.MobilePixelProjectedReflection.IsValid())
	{
		BasePassParameters.PlanarReflection.PlanarReflectionTexture = View.PrevViewInfo.MobilePixelProjectedReflection->GetRHI();
		BasePassParameters.PlanarReflection.PlanarReflectionSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
	else if (BasePassParameters.PlanarReflection.PlanarReflectionTexture == nullptr)
	{
		BasePassParameters.PlanarReflection.PlanarReflectionTexture = GBlackTexture->TextureRHI;
		BasePassParameters.PlanarReflection.PlanarReflectionSampler = GBlackTexture->SamplerStateRHI;
	}

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	SetupMobileSceneTextureUniformParameters(GraphBuilder, View.GetSceneTexturesChecked(), SetupMode, BasePassParameters.SceneTextures);

	BasePassParameters.PreIntegratedGFTexture = GSystemTextures.PreintegratedGF->GetRHI();
	BasePassParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	BasePassParameters.EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));

	FRDGTextureRef AmbientOcclusionTexture = SystemTextures.White;
	if (BasePass == EMobileBasePass::Opaque && MobileBasePassTextures.ScreenSpaceAO != nullptr)
	{
		AmbientOcclusionTexture = MobileBasePassTextures.ScreenSpaceAO;
	}
	
	BasePassParameters.DBuffer = GetDBufferParameters(GraphBuilder, MobileBasePassTextures.DBufferTextures, View.GetShaderPlatform());

	BasePassParameters.AmbientOcclusionTexture = AmbientOcclusionTexture;
	BasePassParameters.AmbientOcclusionSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	BasePassParameters.AmbientOcclusionStaticFraction = FMath::Clamp(View.FinalPostProcessSettings.AmbientOcclusionStaticFraction, 0.0f, 1.0f);

	const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(View.GetShaderPlatform());
	
	if (bMobileUsesShadowMaskTexture && GScreenSpaceShadowMaskTextureMobileOutputs.ScreenSpaceShadowMaskTextureMobile.IsValid())
	{
		FRDGTextureRef ScreenShadowMaskTexture = GraphBuilder.RegisterExternalTexture(GScreenSpaceShadowMaskTextureMobileOutputs.ScreenSpaceShadowMaskTextureMobile, TEXT("ScreenSpaceShadowMaskTextureMobile"));
		BasePassParameters.ScreenSpaceShadowMaskTexture = ScreenShadowMaskTexture;
		BasePassParameters.ScreenSpaceShadowMaskSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
	else
	{
		BasePassParameters.ScreenSpaceShadowMaskTexture = GSystemTextures.GetWhiteDummy(GraphBuilder);
		BasePassParameters.ScreenSpaceShadowMaskSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	FRDGBuffer* OcclusionBuffer = View.ViewState ? View.ViewState->OcclusionFeedback.GetGPUFeedbackBuffer() : nullptr;
	if (!OcclusionBuffer)
	{
		OcclusionBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("OcclusionBufferFallback"));
		
	}
	BasePassParameters.RWOcclusionBufferUAV = GraphBuilder.CreateUAV(OcclusionBuffer);

	// Substrate
	Substrate::BindSubstrateMobileForwardPasslUniformParameters(GraphBuilder, View, BasePassParameters.Substrate);

	BasePassParameters.LFV = View.LocalFogVolumeViewData.UniformParametersStruct;

	SetupReflectionUniformParameters(GraphBuilder, View, BasePassParameters.ReflectionsParameters);
}

TRDGUniformBufferRef<FMobileBasePassUniformParameters> CreateMobileBasePassUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	EMobileBasePass BasePass,
	EMobileSceneTextureSetupMode SetupMode,
	const FMobileBasePassTextures& MobileBasePassTextures)
{
	FMobileBasePassUniformParameters* BasePassParameters = GraphBuilder.AllocParameters<FMobileBasePassUniformParameters>();
	SetupMobileBasePassUniformParameters(GraphBuilder, View, BasePass, SetupMode, MobileBasePassTextures, *BasePassParameters);
#if WITH_DEBUG_VIEW_MODES
	if (View.Family->UseDebugViewPS())
	{
		SetupDebugViewModePassUniformBufferConstants(View, BasePassParameters->DebugViewMode);
	}
#endif

	// QuadOverdraw is a UAV so it needs to be initialized even if not used
	const FSceneTextures* SceneTextures = View.GetSceneTexturesChecked();
	FRDGTextureRef QuadOverdrawTexture = SceneTextures ? SceneTextures->QuadOverdraw : nullptr;
	if (!QuadOverdrawTexture)
	{
		QuadOverdrawTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_R32_UINT, FClearValueBinding::None, TexCreate_UAV), TEXT("DummyOverdrawUAV"));
	}
	BasePassParameters->QuadOverdraw = GraphBuilder.CreateUAV(QuadOverdrawTexture);
	
	return GraphBuilder.CreateUniformBuffer(BasePassParameters);
}

void SetupMobileDirectionalLightUniformParameters(
	const FScene& Scene,
	const FViewInfo& SceneView,
	const TArray<FVisibleLightInfo,SceneRenderingAllocator>& VisibleLightInfos,
	int32 ChannelIdx,
	bool bDynamicShadows,
	FMobileDirectionalLightShaderParameters& Params)
{
	ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();
	FLightSceneInfo* Light = Scene.MobileDirectionalLights[ChannelIdx];
	if (Light)
	{
		Params.DirectionalLightColor = Light->Proxy->GetSunIlluminanceAccountingForSkyAtmospherePerPixelTransmittance();
		Params.DirectionalLightDirectionAndShadowTransition = FVector4f((FVector3f)-Light->Proxy->GetDirection(), 0.f);

		const FVector2D FadeParams = Light->Proxy->GetDirectionalLightDistanceFadeParameters(FeatureLevel, Light->IsPrecomputedLightingValid(), SceneView.MaxShadowCascades);
		Params.DirectionalLightDistanceFadeMADAndSpecularScale.X = FadeParams.Y;
		Params.DirectionalLightDistanceFadeMADAndSpecularScale.Y = -FadeParams.X * FadeParams.Y;
		Params.DirectionalLightDistanceFadeMADAndSpecularScale.Z = Light->Proxy->GetSpecularScale();

		const int32 ShadowMapChannel = IsStaticLightingAllowed() ? Light->Proxy->GetShadowMapChannel() : INDEX_NONE;
		int32 DynamicShadowMapChannel = Light->GetDynamicShadowMapChannel();

		// Static shadowing uses ShadowMapChannel, dynamic shadows are packed into light attenuation using DynamicShadowMapChannel
		Params.DirectionalLightShadowMapChannelMask =
			(ShadowMapChannel == 0 ? 1 : 0) |
			(ShadowMapChannel == 1 ? 2 : 0) |
			(ShadowMapChannel == 2 ? 4 : 0) |
			(ShadowMapChannel == 3 ? 8 : 0) |
			(DynamicShadowMapChannel == 0 ? 16 : 0) |
			(DynamicShadowMapChannel == 1 ? 32 : 0) |
			(DynamicShadowMapChannel == 2 ? 64 : 0) |
			(DynamicShadowMapChannel == 3 ? 128 : 0);

		if (bDynamicShadows && VisibleLightInfos.IsValidIndex(Light->Id) && VisibleLightInfos[Light->Id].AllProjectedShadows.Num() > 0)
		{
			const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& DirectionalLightShadowInfos = VisibleLightInfos[Light->Id].AllProjectedShadows;
			static_assert(MAX_MOBILE_SHADOWCASCADES <= 4, "more than 4 cascades not supported by the shader and uniform buffer");

			const int32 NumShadowsToCopy = DirectionalLightShadowInfos.Num();
			int32_t OutShadowIndex = 0;
			for (int32 i = 0; i < NumShadowsToCopy && OutShadowIndex < SceneView.MaxShadowCascades; ++i)
			{
				const FProjectedShadowInfo* ShadowInfo = DirectionalLightShadowInfos[i];

				if (ShadowInfo->ShadowDepthView && !ShadowInfo->bRayTracedDistanceField && ShadowInfo->CacheMode != SDCM_StaticPrimitivesOnly && ShadowInfo->DependentView == &SceneView)
				{
					if (OutShadowIndex == 0)
					{
						const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution();
						const FVector4f ShadowBufferSizeValue((float)ShadowBufferResolution.X, (float)ShadowBufferResolution.Y, 1.0f / (float)ShadowBufferResolution.X, 1.0f / (float)ShadowBufferResolution.Y);

						Params.DirectionalLightShadowTexture = ShadowInfo->RenderTargets.DepthTarget->GetRHI();
						Params.DirectionalLightDirectionAndShadowTransition.W = 1.0f / ShadowInfo->ComputeTransitionSize();
						Params.DirectionalLightShadowSize = ShadowBufferSizeValue;
						Params.DirectionalLightDistanceFadeMADAndSpecularScale.W = ShadowInfo->GetShaderReceiverDepthBias();
					}
					Params.DirectionalLightScreenToShadow[OutShadowIndex] = FMatrix44f(ShadowInfo->GetScreenToShadowMatrix(SceneView));		// LWC_TODO: Precision loss?
					Params.DirectionalLightShadowDistances[OutShadowIndex] = ShadowInfo->CascadeSettings.SplitFar;
					Params.DirectionalLightNumCascades++;
					OutShadowIndex++;
				}
			}
		}
	}
}

void SetupMobileSkyReflectionUniformParameters(FSkyLightSceneProxy* SkyLight, FMobileReflectionCaptureShaderParameters& Parameters)
{
	float Brightness = 0.f;
	float SkyMaxMipIndex = 0.f;
	FTexture* CaptureTexture = GBlackTextureCube;
	FTexture* CaptureTextureBlend = GBlackTextureCube;
	float BlendFraction = 0.f;

	bool bSkyLightIsDynamic = false;
	if (SkyLight && SkyLight->ProcessedTexture)
	{
		check(SkyLight->ProcessedTexture->IsInitialized());
		CaptureTexture = SkyLight->ProcessedTexture;
		SkyMaxMipIndex = FMath::Log2(static_cast<float>(CaptureTexture->GetSizeX()));
		Brightness = SkyLight->AverageBrightness;
		bSkyLightIsDynamic = !SkyLight->bHasStaticLighting && !SkyLight->bWantsStaticShadowing;

		BlendFraction = SkyLight->BlendFraction;
		if (BlendFraction > 0.0 && SkyLight->BlendDestinationProcessedTexture)
		{
			CaptureTextureBlend = SkyLight->BlendDestinationProcessedTexture;
		}
	}
	
	//To keep ImageBasedReflectionLighting coherence with PC, use AverageBrightness instead of InvAverageBrightness to calculate the IBL contribution
	Parameters.Params = FVector4f(Brightness, SkyMaxMipIndex, bSkyLightIsDynamic ? 1.0f : 0.0f, BlendFraction);
	Parameters.Texture = CaptureTexture->TextureRHI;
	Parameters.TextureSampler = CaptureTexture->SamplerStateRHI;
	Parameters.TextureBlend = CaptureTextureBlend->TextureRHI;
	Parameters.TextureBlendSampler = CaptureTextureBlend->SamplerStateRHI;
}

void FMobileSceneRenderer::RenderMobileBasePass(FRHICommandList& RHICmdList, const FViewInfo& View, const FInstanceCullingDrawParams* InstanceCullingDrawParams)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderBasePass);
	SCOPED_DRAW_EVENT(RHICmdList, MobileBasePass);
	SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);
	SCOPED_GPU_STAT(RHICmdList, Basepass);

	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
	View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(nullptr, RHICmdList, InstanceCullingDrawParams);
		
	if (View.Family->EngineShowFlags.Atmosphere)
	{
		View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].DispatchDraw(nullptr, RHICmdList, &SkyPassInstanceCullingDrawParams);
	}

	// editor primitives
	FMeshPassProcessorRenderState DrawRenderState;
	DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilAccess(Scene->DefaultBasePassDepthStencilAccess);
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RenderMobileEditorPrimitives(RHICmdList, View, DrawRenderState, InstanceCullingDrawParams);
}

void FMobileSceneRenderer::RenderMobileEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, const FInstanceCullingDrawParams* InstanceCullingDrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EditorDynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, DynamicEd);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);
	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);

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

		// Draw the view's batched simple elements(lines, sprites, etc).
		View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false);

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
		
		// DrawDynamicMeshPass may change BatchedPrimitive binding, so we need to restore it
		FUniformBufferStaticSlot BatchedPrimitiveSlot = FInstanceCullingContext::GetUniformBufferViewStaticSlot(View.GetShaderPlatform());
		if (IsUniformBufferStaticSlotValid(BatchedPrimitiveSlot) && InstanceCullingDrawParams->BatchedPrimitive)
		{
			FRHIUniformBuffer* BatchedPrimitiveBufferRHI = InstanceCullingDrawParams->BatchedPrimitive.GetUniformBuffer()->GetRHI();
			check(BatchedPrimitiveBufferRHI);
			RHICmdList.SetStaticUniformBuffer(BatchedPrimitiveSlot, BatchedPrimitiveBufferRHI);
		}

		// Draw the view's batched simple elements(lines, sprites, etc).
		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false);
	}
}

#if UE_ENABLE_DEBUG_DRAWING
void FMobileSceneRenderer::RenderMobileDebugPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugDynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, DynamicDebug);

	FMeshPassProcessorRenderState DrawRenderState;
	DrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
	DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
		
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	View.DebugSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);

	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
	View.DebugSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);
}
#endif