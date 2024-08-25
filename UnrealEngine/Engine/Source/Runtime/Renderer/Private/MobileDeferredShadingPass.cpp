// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileDeferredShadingPass.h"
#include "BasePassRendering.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "PlanarReflectionRendering.h"
#include "LightRendering.h"
#include "LocalLightSceneProxy.h"
#include "Materials/MaterialRenderProxy.h"

int32 GMobileUseClusteredDeferredShading = 0;
static FAutoConsoleVariableRef CVarMobileUseClusteredDeferredShading(
	TEXT("r.Mobile.UseClusteredDeferredShading"),
	GMobileUseClusteredDeferredShading,
	TEXT("Toggle use of clustered deferred shading for lights that support it. 0 is off (default), 1 is on. (requires LightGrid: r.Mobile.Forward.EnableLocalLights=1)"),
	ECVF_RenderThreadSafe
);

static bool UseClusteredDeferredShading(const FStaticShaderPlatform Platform)
{
	// Needs LightGrid to function
	return GMobileUseClusteredDeferredShading != 0 && MobileForwardEnableLocalLights(Platform);
}

int32 GMobileUseLightStencilCulling = 1;
static FAutoConsoleVariableRef CVarMobileUseLightStencilCulling(
	TEXT("r.Mobile.UseLightStencilCulling"),
	GMobileUseLightStencilCulling,
	TEXT("Whether to use stencil to cull local lights. 0 is off, 1 is on (default)"),
	ECVF_RenderThreadSafe
);

int32 GMobileIgnoreDeferredShadingSkyLightChannels = 0;
static FAutoConsoleVariableRef CVarMobileIgnoreDeferredShadingSkyLightChannels(
	TEXT("r.Mobile.IgnoreDeferredShadingSkyLightChannels"),
	GMobileIgnoreDeferredShadingSkyLightChannels,
	TEXT("Whether to ignore primitive lighting channels when applying SkyLighting in a mobile deferred shading.\n" 
		 "This may improve GPU performance at the cost of incorrect lighting for a primitves with non-default lighting channels"),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FMobileDeferredPassParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileSceneTextureUniformParameters, MobileSceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FMobileDirectionalLightFunctionPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FMobileDirectionalLightFunctionPS, Material);
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FMobileDirectionalLightFunctionPS, FMaterialShader)

	class FEnableShadingModelSupport	: SHADER_PERMUTATION_BOOL("ENABLE_SHADINGMODEL_SUPPORT_MOBILE_DEFERRED");
	class FEnableClustredLights			: SHADER_PERMUTATION_BOOL("ENABLE_CLUSTERED_LIGHTS");
	class FEnableClustredReflection		: SHADER_PERMUTATION_BOOL("ENABLE_CLUSTERED_REFLECTION");
	class FEnablePlanarReflection		: SHADER_PERMUTATION_BOOL("ENABLE_PLANAR_REFLECTION");
	class FEnableSkyLight				: SHADER_PERMUTATION_BOOL("ENABLE_SKY_LIGHT");
	class FEnableDynamicSkyLight		: SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FEnableCSM					: SHADER_PERMUTATION_BOOL("ENABLE_MOBILE_CSM");
	class FShadowQuality				: SHADER_PERMUTATION_RANGE_INT("MOBILE_SHADOW_QUALITY", 1, 3); // not using Quality=0
	
	using FPermutationDomain = TShaderPermutationDomain<
		FEnableShadingModelSupport,
		FEnableClustredLights, 
		FEnableClustredReflection, 
		FEnablePlanarReflection,
		FEnableSkyLight,
		FEnableDynamicSkyLight,
		FEnableCSM, 
		FShadowQuality>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FMobileDirectionalLightShaderParameters, MobileDirectionalLight)
		SHADER_PARAMETER_STRUCT_REF(FMobileReflectionCaptureShaderData, MobileReflectionCaptureData)
		SHADER_PARAMETER(FMatrix44f, TranslatedWorldToLight)
		SHADER_PARAMETER(FVector4f, LightFunctionParameters)
		SHADER_PARAMETER(FVector2f, LightFunctionParameters2)
		SHADER_PARAMETER_TEXTURE(Texture2D, ScreenSpaceShadowMaskTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ScreenSpaceShadowMaskSampler)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LIGHT_FUNCTION"), Parameters.MaterialParameters.bIsDefaultMaterial ? 0 : 1);
		OutEnvironment.SetDefine(TEXT("USE_SHADOWMASKTEXTURE"), MobileUsesShadowMaskTexture(Parameters.Platform) ? 1u : 0u);
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEFERREDSHADING_SUBPASS"), 1u);

		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), bMobileForceDepthRead ? 1u : 0u);
	}

	static FPermutationDomain RemapPermutationVector(FPermutationDomain PermutationVector, EShaderPlatform Platform)
	{
		if (MobileUsesShadowMaskTexture(Platform))
		{
			PermutationVector.Set<FEnableCSM>(false);
		}

		if (PermutationVector.Get<FEnableCSM>() == false)
		{
			PermutationVector.Set<FShadowQuality>(1);
		}

		if (!MobileUsesGBufferCustomData(Platform))
		{
			PermutationVector.Set<FEnableShadingModelSupport>(false);
		}

		if (!PermutationVector.Get<FEnableSkyLight>())
		{
			PermutationVector.Set<FEnableDynamicSkyLight>(false);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (Parameters.MaterialParameters.MaterialDomain != MD_LightFunction || 
			!IsMobilePlatform(Parameters.Platform) || 
			!IsMobileDeferredShadingEnabled(Parameters.Platform))
		{
			return false;
		}
		
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// Compile out the shader if this permutation gets remapped.
		if (RemapPermutationVector(PermutationVector, Parameters.Platform) != PermutationVector)
		{
			return false;
		}
		
		return true;
	}

	static FPermutationDomain BuildPermutationVector(const FViewInfo& View, bool bInlineReflectionAndSky, bool bShadingModelSupport, bool bDynamicShadows, bool bSkyLight, bool bDynamicSkyLight, bool bPlanarReflection)
	{
		bool bUseClusteredLights = UseClusteredDeferredShading(View.GetShaderPlatform());
		bool bClustredReflection = bInlineReflectionAndSky && (View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures) > 0;
		bool bEnableSkyLight = bInlineReflectionAndSky && bSkyLight;
		bool bEnableDynamicSkyLight = bInlineReflectionAndSky && bDynamicSkyLight;
		const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(View.GetShaderPlatform());
		int32 ShadowQuality = bDynamicShadows && !bMobileUsesShadowMaskTexture ? (int32)GetShadowQuality() : 0;
				
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableShadingModelSupport>(bShadingModelSupport);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableClustredLights>(bUseClusteredLights);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableClustredReflection>(bClustredReflection);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnablePlanarReflection>(bPlanarReflection);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableSkyLight>(bEnableSkyLight);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableDynamicSkyLight>(bEnableDynamicSkyLight);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FEnableCSM>(ShadowQuality > 0);
		PermutationVector.Set<FMobileDirectionalLightFunctionPS::FShadowQuality>(FMath::Clamp(ShadowQuality, 1, 3));
		return PermutationVector;
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material)
	{
		FMaterialShader::SetParameters(BatchedParameters, Proxy, Material, View);

		// LightFunctions can use primitive data, set identity so we do not crash on a missing binding
		auto& PrimitivePS = GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		SetUniformBufferParameter(BatchedParameters, PrimitivePS, GIdentityPrimitiveUniformBuffer);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMobileDirectionalLightFunctionPS, TEXT("/Engine/Private/MobileDeferredShading.usf"), TEXT("MobileDirectionalLightPS"), SF_Pixel);

/**
 * A pixel shader for projecting a light function onto the scene.
 */
class FMobileRadialLightFunctionPS : public FMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FMobileRadialLightFunctionPS,Material);
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FMobileRadialLightFunctionPS, FMaterialShader)

	class FEnableShadingModelSupport: SHADER_PERMUTATION_BOOL("ENABLE_SHADINGMODEL_SUPPORT_MOBILE_DEFERRED");
	class FSpotLightDim				: SHADER_PERMUTATION_BOOL("IS_SPOT_LIGHT");
	class FIESProfileDim			: SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FSpotLightShadowDim		: SHADER_PERMUTATION_BOOL("SUPPORT_SPOTLIGHTS_SHADOW");
	using FPermutationDomain = TShaderPermutationDomain<FEnableShadingModelSupport, FSpotLightDim, FIESProfileDim, FSpotLightShadowDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, TranslatedWorldToLight)
		SHADER_PARAMETER(FVector4f, LightFunctionParameters)
		SHADER_PARAMETER(FVector2f, LightFunctionParameters2)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLightUniforms)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMobileMovableLocalLightShadowParameters, MobileMovableLocalLightShadow)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (Parameters.MaterialParameters.MaterialDomain != MD_LightFunction || 
			!IsMobilePlatform(Parameters.Platform) || 
			!IsMobileDeferredShadingEnabled(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// Compile out the shader if this permutation gets remapped.
		if (RemapPermutationVector(PermutationVector, Parameters.Platform) != PermutationVector)
		{
			return false;
		}

		return true;
	}

	static FPermutationDomain RemapPermutationVector(FPermutationDomain PermutationVector, EShaderPlatform Platform)
	{
		if (!IsMobileMovableSpotlightShadowsEnabled(Platform))
		{
			PermutationVector.Set<FSpotLightShadowDim>(false);
		}

		if (!MobileUsesGBufferCustomData(Platform))
		{
			PermutationVector.Set<FEnableShadingModelSupport>(false);
		}

		return PermutationVector;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LIGHT_FUNCTION"), Parameters.MaterialParameters.bIsDefaultMaterial ? 0 : 1);
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("USE_SHADOWMASKTEXTURE"), 0);
		OutEnvironment.SetDefine(TEXT("ENABLE_CLUSTERED_LIGHTS"), 0);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEFERREDSHADING_SUBPASS"), 1u);

		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), bMobileForceDepthRead ? 1u : 0u);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* Proxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, Proxy, Material, View);
		
		// LightFunctions can use primitive data, set identity so we do not crash on a missing binding
		auto& PrimitivePS = GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		SetUniformBufferParameter(BatchedParameters, PrimitivePS, GIdentityPrimitiveUniformBuffer);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMobileRadialLightFunctionPS, TEXT("/Engine/Private/MobileDeferredShading.usf"), TEXT("MobileRadialLightPS"), SF_Pixel);


/**
 * A pixel shader for reflection env and sky lighting. 
 */
class FMobileReflectionEnvironmentSkyLightingPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileReflectionEnvironmentSkyLightingPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileReflectionEnvironmentSkyLightingPS, FGlobalShader);
	
	class FEnableShadingModelSupport	: SHADER_PERMUTATION_BOOL("ENABLE_SHADINGMODEL_SUPPORT_MOBILE_DEFERRED");
	class FEnableClustredReflection		: SHADER_PERMUTATION_BOOL("ENABLE_CLUSTERED_REFLECTION");
	class FEnablePlanarReflection		: SHADER_PERMUTATION_BOOL("ENABLE_PLANAR_REFLECTION");
	class FEnableSkyLight				: SHADER_PERMUTATION_BOOL("ENABLE_SKY_LIGHT");
	class FEnableDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	
	using FPermutationDomain = TShaderPermutationDomain<
		FEnableShadingModelSupport, 
		FEnableClustredReflection, 
		FEnablePlanarReflection,
		FEnableSkyLight,
		FEnableDynamicSkyLight
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_REF(FMobileReflectionCaptureShaderData, MobileReflectionCaptureData)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform) ||
			!IsMobileDeferredShadingEnabled(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (!MobileUsesGBufferCustomData(Parameters.Platform) && PermutationVector.Get<FEnableShadingModelSupport>())
		{
			return false;
		}

		if (!PermutationVector.Get<FEnableSkyLight>() && PermutationVector.Get<FEnableDynamicSkyLight>())
		{
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEFERREDSHADING_SUBPASS"), 1u);

		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), bMobileForceDepthRead ? 1u : 0u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileReflectionEnvironmentSkyLightingPS, "/Engine/Private/MobileDeferredShading.usf", "MobileReflectionEnvironmentSkyLightingPS", SF_Pixel);

extern uint8 GetMobileShadingModelStencilValue(FMaterialShadingModelField ShadingModel);

constexpr uint32 GetLightingChannel(uint32 LightingChannelMask)
{
	return (LightingChannelMask & 0x1) ? 0u : ((LightingChannelMask & 0x2) ? 1u : 2u);
}

constexpr uint8 GetLightingChannelStencilValue(uint32 LightingChannel)
{
	// LightingChannel_0 has an inverted bit in the stencil. 0 - means LightingChannel_0 is enabled. See FPrimitiveSceneProxy::GetLightingChannelStencilValue()
	return (LightingChannel == 0u ? 0u : (1u << LightingChannel));
}

constexpr bool IsOnlyDefaultLitShadingModel(uint32 ShadingModelMask)
{
	constexpr uint32 LitOpaqueMask = ~(1u << MSM_Unlit | 1u << MSM_SingleLayerWater | 1u << MSM_ThinTranslucent);
	constexpr uint32 DefaultLitMask = (1u << MSM_DefaultLit);
	return (ShadingModelMask & LitOpaqueMask) == DefaultLitMask;
}

struct FCachedLightMaterial
{
	const FMaterial* Material;
	const FMaterialRenderProxy* MaterialProxy;
};

template<class ShaderType>
static void GetLightMaterial(const FCachedLightMaterial& DefaultLightMaterial, const FMaterialRenderProxy* MaterialProxy, int32 PermutationId, FCachedLightMaterial& OutLightMaterial, TShaderRef<ShaderType>& OutShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<ShaderType>(PermutationId);
	FMaterialShaders Shaders;

	if (MaterialProxy)
	{
		const FMaterial* Material = MaterialProxy->GetMaterialNoFallback(ERHIFeatureLevel::ES3_1);
		if (Material && Material->IsLightFunction())
		{
			OutLightMaterial.Material = Material;
			OutLightMaterial.MaterialProxy = MaterialProxy;
			if (Material->TryGetShaders(ShaderTypes, nullptr, Shaders))
			{
				Shaders.TryGetPixelShader(OutShader);
				return;
			}
		}
	}

	// use default material
	OutLightMaterial.Material = DefaultLightMaterial.Material;
	OutLightMaterial.MaterialProxy = DefaultLightMaterial.MaterialProxy;
	const FMaterialShaderMap* MaterialShaderMap = OutLightMaterial.Material->GetRenderingThreadShaderMap();
	OutShader = MaterialShaderMap->GetShader<ShaderType>(PermutationId);
}

void RenderReflectionEnvironmentSkyLighting(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View)
{
	// Skylights with static lighting already had their diffuse contribution baked into lightmaps
	const bool bSkyLight = Scene.SkyLight && !Scene.SkyLight->bHasStaticLighting && View.Family->EngineShowFlags.SkyLighting;
	const bool bDynamicSkyLight = bSkyLight && !Scene.SkyLight->bWantsStaticShadowing;
	const bool bClustredReflection = (View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures) > 0;
	const bool bPlanarReflection = Scene.GetForwardPassGlobalPlanarReflection() != nullptr;
	if (!(bSkyLight || bClustredReflection || bPlanarReflection))
	{
		return;
	}
		
	SCOPED_DRAW_EVENT(RHICmdList, ReflectionEnvironmentSkyLighting);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	// Add to emissive in SceneColor
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		GET_STENCIL_MOBILE_SM_MASK(0xff), 0x00>::GetRHI();

	FMobileReflectionEnvironmentSkyLightingPS::FParameters PassParameters;
	PassParameters.View = GetShaderBinding(View.ViewUniformBuffer);
	PassParameters.MobileReflectionCaptureData = GetShaderBinding(View.MobileReflectionCaptureUniformBuffer);

	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

	// Do two passes, first masking DefautLit, second masking all other shading models
	const bool bOnlyDefaultLitInView = IsOnlyDefaultLitShadingModel(View.ShadingModelMaskInView);
	int32 NumPasses = !bOnlyDefaultLitInView && MobileUsesGBufferCustomData(Scene.GetShaderPlatform()) ? 2 : 1;
	uint8 PassShadingModelStencilValue[2] =
	{
		GetMobileShadingModelStencilValue(MSM_DefaultLit),
		GetMobileShadingModelStencilValue(FMaterialShadingModelField())
	};
			
	for (int32 PassIndex = 0; PassIndex < NumPasses; PassIndex++)
	{
		const bool bEnableShadingModelSupport = (PassIndex > 0);
		
		FMobileReflectionEnvironmentSkyLightingPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMobileReflectionEnvironmentSkyLightingPS::FEnableShadingModelSupport>(bEnableShadingModelSupport);
		PermutationVector.Set<FMobileReflectionEnvironmentSkyLightingPS::FEnableClustredReflection>(bClustredReflection);
		PermutationVector.Set<FMobileReflectionEnvironmentSkyLightingPS::FEnablePlanarReflection>(bPlanarReflection);
		PermutationVector.Set<FMobileReflectionEnvironmentSkyLightingPS::FEnableSkyLight>(bSkyLight);
		PermutationVector.Set<FMobileReflectionEnvironmentSkyLightingPS::FEnableDynamicSkyLight>(bDynamicSkyLight);
		TShaderMapRef<FMobileReflectionEnvironmentSkyLightingPS> PixelShader(View.ShaderMap, PermutationVector);
		
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		uint8 StencilRef = GET_STENCIL_MOBILE_SM_MASK(PassShadingModelStencilValue[PassIndex]);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters);

		const FIntPoint TargetSize = View.GetSceneTexturesConfig().Extent;

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			TargetSize,
			VertexShader);
	}
}

template<uint32 LightingChannelIdx>
static void SetDirectionalLightDepthStencilState(FGraphicsPipelineStateInitializer& GraphicsPSOInit)
{
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		GET_STENCIL_MOBILE_SM_MASK(0xff) | STENCIL_LIGHTING_CHANNELS_MASK(1u << LightingChannelIdx), 0x00>::GetRHI();
}

static void RenderDirectionalLight(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View, const FCachedLightMaterial& DefaultLightMaterial, const FLightSceneInfo& DirectionalLight, uint32 LightingChannel, bool bInlineReflectionAndSky)
{
	FString LightNameWithLevel;
	FSceneRenderer::GetLightNameForDrawEvent(DirectionalLight.Proxy, LightNameWithLevel);
	SCOPED_DRAW_EVENTF(RHICmdList, DirectionalLight, TEXT("%s"), *LightNameWithLevel);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	// Add to emissive in SceneColor
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();

	uint8 LightingChannelStencilValue = GetLightingChannelStencilValue(LightingChannel);
	if (LightingChannel == 1u)
	{
		SetDirectionalLightDepthStencilState<1u>(GraphicsPSOInit);
	}
	else if (LightingChannel == 2u)
	{
		SetDirectionalLightDepthStencilState<2u>(GraphicsPSOInit);
	}
	else
	{
		SetDirectionalLightDepthStencilState<0u>(GraphicsPSOInit);
	}
	
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	
	const FMaterialRenderProxy* LightFunctionMaterialProxy = nullptr;
	if (View.Family->EngineShowFlags.LightFunctions)
	{
		LightFunctionMaterialProxy = DirectionalLight.Proxy->GetLightFunctionMaterial();
	}

	FMobileDirectionalLightFunctionPS::FParameters PassParameters;
	PassParameters.MobileDirectionalLight = Scene.UniformBuffers.MobileDirectionalLightUniformBuffers[LightingChannel + 1];
	PassParameters.MobileReflectionCaptureData = GetShaderBinding(View.MobileReflectionCaptureUniformBuffer);
	PassParameters.LightFunctionParameters = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);

	const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(View.GetShaderPlatform());

	if (bMobileUsesShadowMaskTexture && GScreenSpaceShadowMaskTextureMobileOutputs.ScreenSpaceShadowMaskTextureMobile.IsValid())
	{
		PassParameters.ScreenSpaceShadowMaskTexture = GScreenSpaceShadowMaskTextureMobileOutputs.ScreenSpaceShadowMaskTextureMobile->GetRHI();
		PassParameters.ScreenSpaceShadowMaskSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}
	else
	{
		PassParameters.ScreenSpaceShadowMaskTexture = GSystemTextures.WhiteDummy->GetRHI();
		PassParameters.ScreenSpaceShadowMaskSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	{
		PassParameters.LightFunctionParameters2 = FVector2f(DirectionalLight.Proxy->GetLightFunctionFadeDistance(), DirectionalLight.Proxy->GetLightFunctionDisabledBrightness());
		const FVector Scale = DirectionalLight.Proxy->GetLightFunctionScale();
		// Switch x and z so that z of the user specified scale affects the distance along the light direction
		const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
		const FMatrix WorldToLight = DirectionalLight.Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));
		PassParameters.TranslatedWorldToLight = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToLight);
	}

	// Skylights with static lighting already had their diffuse contribution baked into lightmaps
	const bool bSkyLight = Scene.SkyLight && !Scene.SkyLight->bHasStaticLighting && View.Family->EngineShowFlags.SkyLighting;
	const bool bDynamicSkyLight = bSkyLight && !Scene.SkyLight->bWantsStaticShadowing;
	const bool bDynamicShadows = DirectionalLight.Proxy->CastsDynamicShadow() && View.Family->EngineShowFlags.DynamicShadows;
	const bool bPlanarReflection = Scene.GetForwardPassGlobalPlanarReflection() != nullptr;

	// Do two passes, first masking DefautLit, second masking all other shading models
	const bool bOnlyDefaultLitInView = IsOnlyDefaultLitShadingModel(View.ShadingModelMaskInView);
	int32 NumPasses = !bOnlyDefaultLitInView && MobileUsesGBufferCustomData(Scene.GetShaderPlatform()) ? 2 : 1;
	uint8 PassShadingModelStencilValue[2] =
	{
		GetMobileShadingModelStencilValue(MSM_DefaultLit),
		GetMobileShadingModelStencilValue(FMaterialShadingModelField())
	};
	
	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		const bool bEnableShadingModelSupport = (PassIndex > 0);
		
		FMobileDirectionalLightFunctionPS::FPermutationDomain PermutationVector = FMobileDirectionalLightFunctionPS::BuildPermutationVector(
			View,
			bInlineReflectionAndSky,
			bEnableShadingModelSupport,
			bDynamicShadows,
			bSkyLight,
			bDynamicSkyLight,
			bPlanarReflection
		);
		FCachedLightMaterial LightMaterial;
		TShaderRef<FMobileDirectionalLightFunctionPS> PixelShader;
		GetLightMaterial(DefaultLightMaterial, LightFunctionMaterialProxy, PermutationVector.ToDimensionValueId(), LightMaterial, PixelShader);

		uint8 StencilRef = 
			GET_STENCIL_MOBILE_SM_MASK(PassShadingModelStencilValue[PassIndex]) | 
			STENCIL_LIGHTING_CHANNELS_MASK(LightingChannelStencilValue);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

		SetShaderParametersMixedPS(RHICmdList, PixelShader, PassParameters, View, LightMaterial.MaterialProxy, *LightMaterial.Material);

		const FIntPoint TargetSize = View.GetSceneTexturesConfig().Extent;

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			TargetSize,
			VertexShader);
	}
}

static void RenderDirectionalLights(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View, const FCachedLightMaterial& DefaultLightMaterial)
{
	uint32 NumLights = 0;
	for (uint32 ChannelIdx = 0; ChannelIdx < UE_ARRAY_COUNT(Scene.MobileDirectionalLights); ChannelIdx++)
	{
		NumLights += (Scene.MobileDirectionalLights[ChannelIdx] ? 1 : 0);
	}
	// We can merge reflection and skylight pass with a sole directional light pass and if all primitives and the directional light use the default lighting channel
	bool bPrimitivesUseLightingChannels = (View.bUsesLightingChannels && GMobileIgnoreDeferredShadingSkyLightChannels == 0);
	const bool bInlineReflectionAndSky = (NumLights == 1) && !bPrimitivesUseLightingChannels && (Scene.MobileDirectionalLights[0] != nullptr);

	for (uint32 ChannelIdx = 0; ChannelIdx < UE_ARRAY_COUNT(Scene.MobileDirectionalLights); ChannelIdx++)
	{
		FLightSceneInfo* DirectionalLight = Scene.MobileDirectionalLights[ChannelIdx];
		if (DirectionalLight)
		{
			RenderDirectionalLight(RHICmdList, Scene, View, DefaultLightMaterial, *DirectionalLight, ChannelIdx, bInlineReflectionAndSky);
		}
	}

	if (!bInlineReflectionAndSky)
	{
		RenderReflectionEnvironmentSkyLighting(RHICmdList, Scene, View);
	}
}

template<uint32 LightingChannel, bool bWithStencilCulling>
static void SetLocalLightRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds)
{
	if (bWithStencilCulling)
	{
		// Render backfaces with depth and stencil tests
		// and clear stencil to zero for next light mask
		GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
			false, CF_LessEqual,
			false, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
			true, CF_Equal, SO_Zero, SO_Keep, SO_Zero,
			GET_STENCIL_MOBILE_SM_MASK(0xff) | STENCIL_LIGHTING_CHANNELS_MASK(1u << LightingChannel) | STENCIL_SANDBOX_MASK,
			STENCIL_SANDBOX_MASK
		>::GetRHI();
	}
	else
	{

		const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f)
			// Always draw backfaces in ortho
			//@todo - accurate ortho camera / light intersection
			|| !View.IsPerspectiveProjection();

		if (bCameraInsideLightGeometry)
		{
			// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
			GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				false, CF_Always,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				GET_STENCIL_MOBILE_SM_MASK(0xff) | STENCIL_LIGHTING_CHANNELS_MASK(1u << LightingChannel), 0x00>::GetRHI();
		}
		else
		{
			// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
			GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				GET_STENCIL_MOBILE_SM_MASK(0xff) | STENCIL_LIGHTING_CHANNELS_MASK(1u << LightingChannel), 0x00>::GetRHI();
		}
	}
}

template<uint32 LightingChannel>
static void SetLocalLightRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds)
{
	if (GMobileUseLightStencilCulling != 0)
	{
		SetLocalLightRasterizerAndDepthState<LightingChannel, true>(GraphicsPSOInit, View, LightBounds);
	}
	else
	{
		SetLocalLightRasterizerAndDepthState<LightingChannel, false>(GraphicsPSOInit, View, LightBounds);
	}
}

static void RenderLocalLight_StencilMask(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View, const FLightSceneInfo& LightSceneInfo)
{
	const uint8 LightType = LightSceneInfo.Proxy->GetLightType();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
	GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	// set stencil to 1 where depth test fails
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_DepthNearOrEqual,
		true, CF_Always, SO_Keep, SO_Replace, SO_Keep,		
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		0x00, STENCIL_SANDBOX_MASK>::GetRHI();

	FDeferredLightVS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDeferredLightVS::FRadialLight>(true);
	TShaderMapRef<FDeferredLightVS> VertexShader(View.ShaderMap, PermutationVector);
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = nullptr;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 1);

	FDeferredLightVS::FParameters ParametersVS = FDeferredLightVS::GetParameters(View, &LightSceneInfo);
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);

	if (LightType == LightType_Point)
	{
		StencilingGeometry::DrawSphere(RHICmdList);
	}
	else // LightType_Spot
	{
		StencilingGeometry::DrawCone(RHICmdList);
	}
}

static void RenderLocalLight(
	FRHICommandList& RHICmdList, 
	const FScene& Scene, 
	const FViewInfo& View, 
	const FLightSceneInfo& LightSceneInfo, 
	const FCachedLightMaterial& DefaultLightMaterial,
	const TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos)
{
	uint8 LightingChannelMask = LightSceneInfo.Proxy->GetLightingChannelMask();
	if (!LightSceneInfo.ShouldRenderLight(View) || LightingChannelMask == 0)
	{
		return;
	}
	
	const uint8 LightType = LightSceneInfo.Proxy->GetLightType();
	const bool bIsSpotLight = LightType == LightType_Spot;
	const bool bIsPointLight = LightType == LightType_Point;
	if (!bIsSpotLight && !bIsPointLight)
	{
		return;
	}

	FString LightNameWithLevel;
	FSceneRenderer::GetLightNameForDrawEvent(LightSceneInfo.Proxy, LightNameWithLevel);
	SCOPED_DRAW_EVENTF(RHICmdList, LocalLight, TEXT("%s"), *LightNameWithLevel);
	check(LightSceneInfo.Proxy->IsLocalLight());
	
	if (GMobileUseLightStencilCulling != 0)
	{
		RenderLocalLight_StencilMask(RHICmdList, Scene, View, LightSceneInfo);
	}

	const bool bUseIESTexture = View.Family->EngineShowFlags.TexturedLightProfiles && LightSceneInfo.Proxy->GetIESTextureResource();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	const FSphere LightBounds = LightSceneInfo.Proxy->GetBoundingSphere();

	uint32 LightingChannel = GetLightingChannel(LightingChannelMask);
	uint8 LightingChannelStencilValue = GetLightingChannelStencilValue(LightingChannel);

	// TODO: support multi-channel ligths?
	if (LightingChannel == 1u)
	{
		SetLocalLightRasterizerAndDepthState<1u>(GraphicsPSOInit, View, LightBounds);
	}
	else if (LightingChannel == 2u)
	{
		SetLocalLightRasterizerAndDepthState<2u>(GraphicsPSOInit, View, LightBounds);
	}
	else
	{
		SetLocalLightRasterizerAndDepthState<0u>(GraphicsPSOInit, View, LightBounds);
	}

	FDeferredLightVS::FPermutationDomain PermutationVectorVS;
	PermutationVectorVS.Set<FDeferredLightVS::FRadialLight>(true);
	TShaderMapRef<FDeferredLightVS> VertexShader(View.ShaderMap, PermutationVectorVS);
	FDeferredLightVS::FParameters ParametersVS = FDeferredLightVS::GetParameters(View, &LightSceneInfo);
		
	const FMaterialRenderProxy* LightFunctionMaterialProxy = nullptr;
	if (View.Family->EngineShowFlags.LightFunctions)
	{
		LightFunctionMaterialProxy = LightSceneInfo.Proxy->GetLightFunctionMaterial();
	}

	FMobileRadialLightFunctionPS::FParameters PassParameters;
	const bool bShouldCastShadow = LightSceneInfo.SetupMobileMovableLocalLightShadowParameters(View, VisibleLightInfos, PassParameters.MobileMovableLocalLightShadow);

	PassParameters.DeferredLightUniforms = TUniformBufferRef<FDeferredLightUniformStruct>::CreateUniformBufferImmediate(GetDeferredLightParameters(View, LightSceneInfo), EUniformBufferUsage::UniformBuffer_SingleFrame);
	const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo.Proxy->GetOuterConeAngle()) : 1.0f;
	PassParameters.LightFunctionParameters = FVector4f(TanOuterAngle, 1.0f /*ShadowFadeFraction*/, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f);
	PassParameters.LightFunctionParameters2 = FVector2f(LightSceneInfo.Proxy->GetLightFunctionFadeDistance(), LightSceneInfo.Proxy->GetLightFunctionDisabledBrightness());
	const FVector Scale = LightSceneInfo.Proxy->GetLightFunctionScale();
	// Switch x and z so that z of the user specified scale affects the distance along the light direction
	const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
	const FMatrix WorldToLight = LightSceneInfo.Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));
	PassParameters.TranslatedWorldToLight = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToLight);

	// Do two passes, first masking DefautLit, second masking all other shading models
	const bool bOnlyDefaultLitInView = IsOnlyDefaultLitShadingModel(View.ShadingModelMaskInView);
	int32 NumPasses = !bOnlyDefaultLitInView && MobileUsesGBufferCustomData(Scene.GetShaderPlatform()) ? 2 : 1;
	uint8 PassShadingModelStencilValue[2] =
	{
		GetMobileShadingModelStencilValue(MSM_DefaultLit),
		GetMobileShadingModelStencilValue(FMaterialShadingModelField())
	};

	for (int32 PassIndex = 0; PassIndex < NumPasses; PassIndex++)
	{
		const bool bEnableShadingModelSupport = (PassIndex > 0);

		FMobileRadialLightFunctionPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMobileRadialLightFunctionPS::FEnableShadingModelSupport>(bEnableShadingModelSupport);
		PermutationVector.Set<FMobileRadialLightFunctionPS::FSpotLightDim>(bIsSpotLight);
		PermutationVector.Set<FMobileRadialLightFunctionPS::FIESProfileDim>(bUseIESTexture);
		PermutationVector.Set<FMobileRadialLightFunctionPS::FSpotLightShadowDim>(bShouldCastShadow);
		FCachedLightMaterial LightMaterial;
		TShaderRef<FMobileRadialLightFunctionPS> PixelShader;
		GetLightMaterial(DefaultLightMaterial, LightFunctionMaterialProxy, PermutationVector.ToDimensionValueId(), LightMaterial, PixelShader);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		uint8 StencilRef = 
			GET_STENCIL_MOBILE_SM_MASK(PassShadingModelStencilValue[PassIndex]) | 
			STENCIL_LIGHTING_CHANNELS_MASK(LightingChannelStencilValue);

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);

		SetShaderParametersMixedPS(RHICmdList, PixelShader, PassParameters, View, LightMaterial.MaterialProxy, *LightMaterial.Material);

		if (LightType == LightType_Point)
		{
			StencilingGeometry::DrawSphere(RHICmdList);
		}
		else // LightType_Spot
		{
			StencilingGeometry::DrawCone(RHICmdList);
		}
	}
}

static void RenderSimpleLights(
	FRHICommandList& RHICmdList, 
	const FScene& Scene, 
	int32 ViewIndex,
	int32 NumViews,
	const FViewInfo& View,
	const FSortedLightSetSceneInfo &SortedLightSet, 
	const FCachedLightMaterial& DefaultMaterial)
{
	const FSimpleLightArray& SimpleLights = SortedLightSet.SimpleLights;
	if (SimpleLights.InstanceData.Num() == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, SimpleLights);

	FDeferredLightVS::FPermutationDomain PermutationVectorVS;
	PermutationVectorVS.Set<FDeferredLightVS::FRadialLight>(true);
	TShaderMapRef<FDeferredLightVS> VertexShader(View.ShaderMap, PermutationVectorVS);

	// Setup stencil mask PSO
	FGraphicsPipelineStateInitializer GraphicsPSOLightMask;
	{
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOLightMask);
		GraphicsPSOLightMask.PrimitiveType = PT_TriangleList;
		GraphicsPSOLightMask.BlendState = TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
		GraphicsPSOLightMask.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
		// set stencil to 1 where depth test fails
		GraphicsPSOLightMask.DepthStencilState = TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Always, SO_Keep, SO_Replace, SO_Keep,		
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0x00, STENCIL_SANDBOX_MASK>::GetRHI();
		GraphicsPSOLightMask.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOLightMask.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOLightMask.BoundShaderState.PixelShaderRHI = nullptr;
	}

	const FMaterialShaderMap* MaterialShaderMap = DefaultMaterial.Material->GetRenderingThreadShaderMap();

	// Setup PSOs we going to use for light rendering 
	// Do two passes, first masking DefautLit, second masking all other shading models
	const bool bOnlyDefaultLitInView = IsOnlyDefaultLitShadingModel(View.ShadingModelMaskInView);
	int32 NumPasses = !bOnlyDefaultLitInView && MobileUsesGBufferCustomData(Scene.GetShaderPlatform()) ? 2 : 1;
	uint8 PassShadingModelStencilValue[2] =
	{
		GetMobileShadingModelStencilValue(MSM_DefaultLit),
		GetMobileShadingModelStencilValue(FMaterialShadingModelField())
	};
	TShaderRef<FMobileRadialLightFunctionPS> PassPixelShaders[2];
	FGraphicsPipelineStateInitializer GraphicsPSOLight[2];

	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		const bool bEnableShadingModelSupport = (PassIndex > 0);
		
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOLight[PassIndex]);
		// Use additive blending for color
		GraphicsPSOLight[PassIndex].BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
		GraphicsPSOLight[PassIndex].PrimitiveType = PT_TriangleList;
		GraphicsPSOLight[PassIndex].BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOLight[PassIndex].BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		SetLocalLightRasterizerAndDepthState<0u, true>(GraphicsPSOLight[PassIndex], View, FSphere());

		TShaderRef<FMobileRadialLightFunctionPS> PixelShader;
		FMobileRadialLightFunctionPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMobileRadialLightFunctionPS::FEnableShadingModelSupport>(bEnableShadingModelSupport);
		PermutationVector.Set<FMobileRadialLightFunctionPS::FSpotLightDim>(false);
		PermutationVector.Set<FMobileRadialLightFunctionPS::FIESProfileDim>(false);
		PassPixelShaders[PassIndex] = MaterialShaderMap->GetShader<FMobileRadialLightFunctionPS>(PermutationVector);
		GraphicsPSOLight[PassIndex].BoundShaderState.PixelShaderRHI = PassPixelShaders[PassIndex].GetPixelShader();
	}
		
	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];
		const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, NumViews);
		const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);

		if (NumViews > 1)
		{
			// set viewports only we we have more than one 
			// otherwise it is set at the start of the pass
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		}

		// Render light mask
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOLightMask, 1);
		FDeferredLightVS::FParameters ParametersVS = FDeferredLightVS::GetParameters(View, LightBounds);
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
		StencilingGeometry::DrawSphere(RHICmdList);

		// Render light
		FMobileRadialLightFunctionPS::FParameters PassParameters;
		FDeferredLightUniformStruct DeferredLightUniformsValue = GetSimpleDeferredLightParameters(View, SimpleLight, SimpleLightPerViewData);
		PassParameters.DeferredLightUniforms = TUniformBufferRef<FDeferredLightUniformStruct>::CreateUniformBufferImmediate(DeferredLightUniformsValue, EUniformBufferUsage::UniformBuffer_SingleFrame);

		for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
		{
			uint8 StencilRef = GET_STENCIL_MOBILE_SM_MASK(PassShadingModelStencilValue[PassIndex]);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOLight[PassIndex], StencilRef);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);

			SetShaderParametersMixedPS(RHICmdList, PassPixelShaders[PassIndex], PassParameters, View, DefaultMaterial.MaterialProxy, *DefaultMaterial.Material);

			// Apply the point or spot light with some approximately bounding geometry,
			// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
			StencilingGeometry::DrawSphere(RHICmdList);
		}
	}
}

void MobileDeferredShadingPass(
	FRHICommandList& RHICmdList,
	int32 ViewIndex,
	int32 NumViews,
	const FViewInfo& View,
	const FScene& Scene, 
	const FSortedLightSetSceneInfo& SortedLightSet,
	const TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos)
{
	SCOPED_DRAW_EVENT(RHICmdList, DeferredShading);
	
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	// Default material for light rendering
	FCachedLightMaterial DefaultMaterial;
	DefaultMaterial.MaterialProxy = UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy();
	DefaultMaterial.Material = DefaultMaterial.MaterialProxy->GetMaterialNoFallback(ERHIFeatureLevel::ES3_1);
	check(DefaultMaterial.Material);

	RenderDirectionalLights(RHICmdList, Scene, View, DefaultMaterial);
	
	const bool bMobileUseClusteredDeferredShading = UseClusteredDeferredShading(View.GetShaderPlatform());
	if (!bMobileUseClusteredDeferredShading)
	{
		// Render non-clustered simple lights
		RenderSimpleLights(RHICmdList, Scene, ViewIndex, NumViews, View, SortedLightSet, DefaultMaterial);
	}

	// Render non-clustered local lights
	int32 NumLights = SortedLightSet.SortedLights.Num();
	const int32 UnbatchedLightStart = SortedLightSet.UnbatchedLightStart;
	int32 StandardDeferredStart = SortedLightSet.SimpleLightsEnd;
	if (bMobileUseClusteredDeferredShading)
	{
		StandardDeferredStart = SortedLightSet.ClusteredSupportedEnd;
	}

	// Draw non-shadowed non-light function lights
	for (int32 LightIdx = StandardDeferredStart; LightIdx < UnbatchedLightStart; ++LightIdx)
	{
		const FSortedLightSceneInfo& SortedLight = SortedLightSet.SortedLights[LightIdx];
		const FLightSceneInfo& LightSceneInfo = *SortedLight.LightSceneInfo;
		RenderLocalLight(RHICmdList, Scene, View, LightSceneInfo, DefaultMaterial, VisibleLightInfos);
	}

	// Draw shadowed and light function lights
	for (int32 LightIdx = UnbatchedLightStart; LightIdx < NumLights; ++LightIdx)
	{
		const FSortedLightSceneInfo& SortedLight = SortedLightSet.SortedLights[LightIdx];
		const FLightSceneInfo& LightSceneInfo = *SortedLight.LightSceneInfo;
		RenderLocalLight(RHICmdList, Scene, View, LightSceneInfo, DefaultMaterial, VisibleLightInfos);
	}
}