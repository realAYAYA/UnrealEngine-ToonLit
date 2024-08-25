// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "DeferredShadingRenderer.h"
#include "LightSceneProxy.h"
#include "ScenePrivate.h"
#include "Engine/TextureCube.h"
#include "PipelineStateCache.h"
#include "SingleLayerWaterRendering.h"
#include "SceneCore.h"
#include "ScreenPass.h"
#include "TextureResource.h"
#include "PostProcess/PostProcessing.h" // IsPostProcessingWithAlphaChannelSupported
#include "EnvironmentComponentsFlags.h"

DECLARE_GPU_DRAWCALL_STAT(Fog);

static TAutoConsoleVariable<int32> CVarFog(
	TEXT("r.Fog"),
	1,
	TEXT(" 0: disabled\n")
	TEXT(" 1: enabled (default)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<bool> CVarFogUseDepthBounds(
	TEXT("r.FogUseDepthBounds"),
	true,
	TEXT("Allows enable depth bounds optimization on fog full screen pass.\n")
	TEXT(" false: disabled\n")
	TEXT(" true: enabled (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarUpsampleJitterMultiplier(
	TEXT("r.VolumetricFog.UpsampleJitterMultiplier"),
		0.0f,
	TEXT("Multiplier for random offset value used to jitter the sample position of the 3D fog volume to hide fog pixelization due to sampling from a lower resolution texture."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<bool> CVarUnderwaterFogWhenCameraIsAboveWater(
	TEXT("r.Water.SingleLayer.UnderwaterFogWhenCameraIsAboveWater"), 
	false, 
	TEXT("Renders height fog behind the water surface even when the camera is above water. This avoids artifacts when entering and exiting the water with strong height fog in the scene but causes artifacts when looking at the water surface from a distance."),
	ECVF_RenderThreadSafe);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FFogUniformParameters, "FogStruct");

void SetupFogUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FFogUniformParameters& OutParameters)
{
	// Exponential Height Fog
	{
		const FTexture* Cubemap = GWhiteTextureCube;

		if (View.FogInscatteringColorCubemap)
		{
			Cubemap = View.FogInscatteringColorCubemap->GetResource();
		}

		OutParameters.ExponentialFogParameters = View.ExponentialFogParameters;
		OutParameters.ExponentialFogColorParameter = FVector4f(View.ExponentialFogColor, 1.0f - View.FogMaxOpacity);
		OutParameters.ExponentialFogParameters2 = View.ExponentialFogParameters2;
		OutParameters.ExponentialFogParameters3 = View.ExponentialFogParameters3;
		OutParameters.SkyAtmosphereAmbientContributionColorScale = View.SkyAtmosphereAmbientContributionColorScale;
		OutParameters.SinCosInscatteringColorCubemapRotation = View.SinCosInscatteringColorCubemapRotation;
		OutParameters.FogInscatteringTextureParameters = (FVector3f)View.FogInscatteringTextureParameters;
		OutParameters.InscatteringLightDirection = (FVector3f)View.InscatteringLightDirection;
		OutParameters.InscatteringLightDirection.W = View.bUseDirectionalInscattering ? FMath::Max(0.f, View.DirectionalInscatteringStartDistance) : -1.f;
		OutParameters.DirectionalInscatteringColor = FVector4f(FVector3f(View.DirectionalInscatteringColor), FMath::Clamp(View.DirectionalInscatteringExponent, 0.000001f, 1000.0f));
		OutParameters.FogInscatteringColorCubemap = Cubemap->TextureRHI;
		OutParameters.FogInscatteringColorSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	// Volumetric Fog
	{
		if (View.VolumetricFogResources.IntegratedLightScatteringTexture)
		{
			OutParameters.IntegratedLightScattering = View.VolumetricFogResources.IntegratedLightScatteringTexture;
			OutParameters.ApplyVolumetricFog = 1.0f;
		}
		else
		{
			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			OutParameters.IntegratedLightScattering = SystemTextures.VolumetricBlackAlphaOne;
			OutParameters.ApplyVolumetricFog = 0.0f;
		}
		OutParameters.IntegratedLightScatteringSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.VolumetricFogStartDistance = View.VolumetricFogStartDistance;
		OutParameters.VolumetricFogNearFadeInDistanceInv = View.VolumetricFogNearFadeInDistanceInv;
	}
}

TRDGUniformBufferRef<FFogUniformParameters> CreateFogUniformBuffer(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	auto* FogStruct = GraphBuilder.AllocParameters<FFogUniformParameters>();
	SetupFogUniformParameters(GraphBuilder, View, *FogStruct);
	return GraphBuilder.CreateUniformBuffer(FogStruct);
}

/** A vertex shader for rendering height fog. */
class FHeightFogVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHeightFogVS);
	SHADER_USE_PARAMETER_STRUCT(FHeightFogVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHeightFogVS, "/Engine/Private/HeightFogVertexShader.usf", "Main", SF_Vertex);

/** A pixel shader for rendering exponential height fog. */
class FExponentialHeightFogPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FExponentialHeightFogPS);
	SHADER_USE_PARAMETER_STRUCT(FExponentialHeightFogPS, FGlobalShader);

	class FSupportFogInScatteringTexture : SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_INSCATTERING_TEXTURE");
	class FSupportFogDirectionalLightInScattering : SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_DIRECTIONAL_LIGHT_INSCATTERING");
	class FSupportVolumetricFog : SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_VOLUMETRIC_FOG");
	class FSupportLocalFogVolume : SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_LOCAL_FOG_VOLUME");
	class FSampleFogOnClouds : SHADER_PERMUTATION_BOOL("PERMUTATION_SAMPLE_FOG_ON_CLOUDS");
	using FPermutationDomain = TShaderPermutationDomain<FSupportFogInScatteringTexture, FSupportFogDirectionalLightInScattering, FSupportVolumetricFog, FSupportLocalFogVolume, FSampleFogOnClouds>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParameters, FogUniformBuffer)
		SHADER_PARAMETER_STRUCT(FLocalFogVolumeUniformParameters, LFV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OcclusionTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, OcclusionSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, WaterDepthSampler)
		SHADER_PARAMETER(float, bOnlyOnRenderedOpaque)
		SHADER_PARAMETER(uint32, bUseWaterDepthTexture)
		SHADER_PARAMETER(float, UpsampleJitterMultiplier)
		SHADER_PARAMETER(FVector4f, WaterDepthTextureMinMaxUV)
		SHADER_PARAMETER(FVector4f, OcclusionTextureMinMaxUV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SrcCloudDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcCloudDepthSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SrcCloudViewTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcCloudViewSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FExponentialHeightFogPS, "/Engine/Private/HeightFogPixelShader.usf", "ExponentialPixelMain", SF_Pixel);

/** The fog vertex declaration resource type. */
class FFogVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor
	virtual ~FFogVertexDeclaration() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Vertex declaration for the light function fullscreen 2D quad. */
TGlobalResource<FFogVertexDeclaration> GFogVertexDeclaration;

void FSceneRenderer::InitFogConstants()
{
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		// set fog consts based on height fog components
		if(ShouldRenderFog(*View.Family))
		{
			if (Scene->ExponentialFogs.Num() > 0)
			{
				const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];
				float CollapsedFogParameter[FExponentialHeightFogSceneInfo::NumFogs];
				static constexpr float MaxObserverHeightDifference = 65536.0f;
				float MaxObserverHeight = FLT_MAX;
				for (int i = 0; i < FExponentialHeightFogSceneInfo::NumFogs; i++)
				{
					// Only limit the observer height to fog if it has any density
					if (FogInfo.FogData[i].Density > 0.0f)
					{
						MaxObserverHeight = FMath::Min(MaxObserverHeight, FogInfo.FogData[i].Height + MaxObserverHeightDifference);
					}
				}
				
				// Clamping the observer height to avoid numerical precision issues in the height fog equation. The max observer height is relative to the fog height.
				const float ObserverHeight = FMath::Min<float>(View.ViewMatrices.GetViewOrigin().Z, MaxObserverHeight);

				for (int i = 0; i < FExponentialHeightFogSceneInfo::NumFogs; i++)
				{
					const float CollapsedFogParameterPower = FMath::Clamp(
						-FogInfo.FogData[i].HeightFalloff * (ObserverHeight - FogInfo.FogData[i].Height),
						-126.f + 1.f, // min and max exponent values for IEEE floating points (http://en.wikipedia.org/wiki/IEEE_floating_point)
						+127.f - 1.f
					);

					CollapsedFogParameter[i] = FogInfo.FogData[i].Density * FMath::Pow(2.0f, CollapsedFogParameterPower);
				}

				View.ExponentialFogParameters = FVector4f(CollapsedFogParameter[0], FogInfo.FogData[0].HeightFalloff, MaxObserverHeight, FogInfo.StartDistance);
				View.ExponentialFogParameters2 = FVector4f(CollapsedFogParameter[1], FogInfo.FogData[1].HeightFalloff, FogInfo.FogData[1].Density, FogInfo.FogData[1].Height);
				View.ExponentialFogColor = FVector3f(FogInfo.FogColor.R, FogInfo.FogColor.G, FogInfo.FogColor.B);
				View.FogMaxOpacity = FogInfo.FogMaxOpacity;
				View.ExponentialFogParameters3 = FVector4f(FogInfo.FogData[0].Density, FogInfo.FogData[0].Height, FogInfo.InscatteringColorCubemap ? 1.0f : 0.0f, FogInfo.FogCutoffDistance);
				View.SinCosInscatteringColorCubemapRotation = FVector2f(FMath::Sin(FogInfo.InscatteringColorCubemapAngle), FMath::Cos(FogInfo.InscatteringColorCubemapAngle));
				View.FogInscatteringColorCubemap = FogInfo.InscatteringColorCubemap;
				const float InvRange = 1.0f / FMath::Max(FogInfo.FullyDirectionalInscatteringColorDistance - FogInfo.NonDirectionalInscatteringColorDistance, .00001f);
				float NumMips = 1.0f;

				View.SkyAtmosphereAmbientContributionColorScale = FogInfo.SkyAtmosphereAmbientContributionColorScale;

				if (FogInfo.InscatteringColorCubemap)
				{
					NumMips = FogInfo.InscatteringColorCubemap->GetNumMips();
				}

				View.FogInscatteringTextureParameters = FVector(InvRange, -FogInfo.NonDirectionalInscatteringColorDistance * InvRange, NumMips);

				View.DirectionalInscatteringExponent = FogInfo.DirectionalInscatteringExponent;
				View.DirectionalInscatteringStartDistance = FogInfo.DirectionalInscatteringStartDistance;
				View.InscatteringLightDirection = FVector(0);
				FLightSceneInfo* SunLight = Scene->AtmosphereLights[0] ? Scene->AtmosphereLights[0] : Scene->SimpleDirectionalLight;	// Fog only takes into account a single atmosphere light with index 0, or the default scene directional light.
				if (SunLight)
				{
					View.InscatteringLightDirection = -SunLight->Proxy->GetDirection();
					View.DirectionalInscatteringColor = FogInfo.DirectionalInscatteringColor * SunLight->Proxy->GetColor().GetLuminance();
				}
				View.bUseDirectionalInscattering = SunLight != nullptr;
				View.bEnableVolumetricFog = FogInfo.bEnableVolumetricFog;
				View.VolumetricFogStartDistance = FogInfo.VolumetricFogStartDistance;
				View.VolumetricFogNearFadeInDistanceInv = FogInfo.VolumetricFogNearFadeInDistance > 0.0f ? (1.0f / FogInfo.VolumetricFogNearFadeInDistance) : 100000000.0f;
			}
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FFogPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FHeightFogVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FExponentialHeightFogPS::FParameters, PS)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static FFogPassParameters* CreateDefaultFogPassParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformbuffer,
	TRDGUniformBufferRef<FFogUniformParameters>& FogUniformBuffer,
	FRDGTextureRef LightShaftOcclusionTexture,
	const FScreenPassTextureViewportParameters& LightShaftParameters)
{
	extern int32 GVolumetricFogGridPixelSize;

	FFogPassParameters* PassParameters = GraphBuilder.AllocParameters<FFogPassParameters>();
	PassParameters->SceneTextures = SceneTexturesUniformbuffer;
	PassParameters->VS.ViewUniformBuffer = GetShaderBinding(View.ViewUniformBuffer);
	PassParameters->PS.ViewUniformBuffer = GetShaderBinding(View.ViewUniformBuffer);
	PassParameters->PS.FogUniformBuffer = FogUniformBuffer;
	PassParameters->PS.LFV = View.LocalFogVolumeViewData.UniformParametersStruct;
	PassParameters->PS.OcclusionTexture = LightShaftOcclusionTexture != nullptr ? LightShaftOcclusionTexture : GSystemTextures.GetWhiteDummy(GraphBuilder);
	PassParameters->PS.OcclusionSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PS.WaterDepthTexture = GSystemTextures.GetDepthDummy(GraphBuilder);
	PassParameters->PS.WaterDepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PS.OcclusionTextureMinMaxUV = FVector4f(LightShaftParameters.UVViewportBilinearMin, LightShaftParameters.UVViewportBilinearMax);
	PassParameters->PS.WaterDepthTextureMinMaxUV = FVector4f::Zero();
	PassParameters->PS.UpsampleJitterMultiplier = CVarUpsampleJitterMultiplier.GetValueOnRenderThread() * GVolumetricFogGridPixelSize;
	PassParameters->PS.bOnlyOnRenderedOpaque = View.bFogOnlyOnRenderedOpaque;
	PassParameters->PS.bUseWaterDepthTexture = false;

	PassParameters->PS.SrcCloudDepthTexture = GSystemTextures.GetWhiteDummy(GraphBuilder);
	PassParameters->PS.SrcCloudDepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->PS.SrcCloudViewTexture = GSystemTextures.GetWhiteDummy(GraphBuilder);
	PassParameters->PS.SrcCloudViewSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	return PassParameters;
}

static void RenderViewFog(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View, 
	FIntRect ViewRect, 
	FFogPassParameters* PassParameters, 
	bool bShouldRenderVolumetricFog,
	bool bFogComposeLocalFogVolumes,
	bool bSampleFogOnClouds = false,
	bool bEnableBlending = true)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	if (bEnableBlending)
	{
		const bool bSupportsAlpha = IsPostProcessingWithAlphaChannelSupported();
		if (bSupportsAlpha)
		{
			// Coverage is the alpha output of the shader in this case.
			if (IsExponentialFogHoldout(View.CachedViewUniformShaderParameters->EnvironmentComponentsFlags) && View.CachedViewUniformShaderParameters->RenderingReflectionCaptureMask == 0.0f)
			{
				// Alpha holdout: apply only when requested and when not rendering reflections. We want to punch a hole according to the Coverage. (black as throughput=0 should become brighter for see throught)
				// SceneAlpha = Coverage*1 + (1.0-Coverage)*(SceneThroughput)
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			}
			else
			{
				// Same color blending. Alpha blending is kept so that the throughput of height fog can still be applied on the background (for instance when the sky or a mesh is holdout).
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
			}
		}
		else
		{
			// Disable alpha writes in order to preserve scene depth values on PC
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha>::GetRHI();
		}
	}
	else
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	}

	TShaderMapRef<FHeightFogVS> VertexShader(View.ShaderMap);
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFogVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

	const bool bUseFogInscatteringColorCubemap = View.FogInscatteringColorCubemap != nullptr;
	FExponentialHeightFogPS::FPermutationDomain PsPermutationVector;
	PsPermutationVector.Set<FExponentialHeightFogPS::FSupportFogInScatteringTexture>(bUseFogInscatteringColorCubemap);
	PsPermutationVector.Set<FExponentialHeightFogPS::FSupportFogDirectionalLightInScattering>(!bUseFogInscatteringColorCubemap && View.bUseDirectionalInscattering);
	PsPermutationVector.Set<FExponentialHeightFogPS::FSupportVolumetricFog>(bShouldRenderVolumetricFog);
	PsPermutationVector.Set<FExponentialHeightFogPS::FSupportLocalFogVolume>(bFogComposeLocalFogVolumes);
	PsPermutationVector.Set<FExponentialHeightFogPS::FSampleFogOnClouds>(bSampleFogOnClouds);
	TShaderMapRef<FExponentialHeightFogPS> PixelShader(View.ShaderMap, PsPermutationVector);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

	// Setup the depth bound optimization if possible on that platform.
	GraphicsPSOInit.bDepthBounds = GSupportsDepthBoundsTest && CVarFogUseDepthBounds.GetValueOnAnyThread() && !bSampleFogOnClouds;
	if (GraphicsPSOInit.bDepthBounds)
	{
		float FogStartDistance = GetViewFogCommonStartDistance(View, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes);

		// Here we compute the nearest z value the fog can start
		// to skip shader execution on pixels that are closer.
		// This means with a bigger distance specified more pixels are
		// are culled and don't need to be rendered. This is faster if
		// there is opaque content nearer than the computed z.
		// This optimization is achieved using depth bound tests.
		// Mobile platforms typically does not support that feature 
		// but typically renders the world using forward shading 
		// with height fog evaluated as part of the material vertex or pixel shader.
		FMatrix InvProjectionMatrix = View.ViewMatrices.GetInvProjectionMatrix();
		FVector ViewSpaceCorner = InvProjectionMatrix.TransformFVector4(FVector4(1, 1, 1, 1));
		float Ratio = ViewSpaceCorner.Z / ViewSpaceCorner.Size();
		FVector ViewSpaceStartFogPoint(0.0f, 0.0f, FogStartDistance * Ratio);
		FVector4f ClipSpaceMaxDistance = (FVector4f)View.ViewMatrices.GetProjectionMatrix().TransformPosition(ViewSpaceStartFogPoint); // LWC_TODO: precision loss
		float FogClipSpaceZ = ClipSpaceMaxDistance.Z / ClipSpaceMaxDistance.W;
		FogClipSpaceZ = FMath::Clamp(FogClipSpaceZ, 0.f, 1.f);

		if (bool(ERHIZBuffer::IsInverted))
		{
			RHICmdList.SetDepthBounds(0.0f, FogClipSpaceZ);
		}
		else
		{
			RHICmdList.SetDepthBounds(FogClipSpaceZ, 1.0f);
		}
	}

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
	ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

	// Draw a quad covering the view.
	RHICmdList.SetStreamSource(0, GScreenSpaceVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(GTwoTrianglesIndexBuffer.IndexBufferRHI, 0, 0, 4, 0, 2, 1);
}

void RenderFogOnClouds(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	FRDGTextureRef SrcCloudDepth,
	FRDGTextureRef SrcCloudView,
	FRDGTextureRef DstCloudView,
	const bool bShouldRenderVolumetricFog,
	const bool bUseVolumetricRenderTarget)
{
	if (Scene->ExponentialFogs.Num() > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ExponentialHeightFog on Clouds");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Fog);

		if (View.IsPerspectiveProjection())
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

			TRDGUniformBufferRef<FFogUniformParameters> FogUniformBuffer = CreateFogUniformBuffer(GraphBuilder, View);

			// Light shaft is not accounted for in this case
			const FScreenPassTextureViewportParameters LightShaftParameters;
			// Local fog volume are not accounted for in this case
			const bool bFogComposeLocalFogVolumes = false;

			FFogPassParameters* PassParameters = CreateDefaultFogPassParameters(
				GraphBuilder, View, 
				CreateSceneTextureUniformBuffer(GraphBuilder,View, ESceneTextureSetupMode::None),
				FogUniformBuffer, nullptr /*LightShaftOcclusionTexture*/, LightShaftParameters);

			// Patch the pass parameter for it to work on clouds
			PassParameters->VS.ViewUniformBuffer = GetShaderBinding(bUseVolumetricRenderTarget ? View.VolumetricRenderTargetViewUniformBuffer : View.ViewUniformBuffer);
			PassParameters->PS.ViewUniformBuffer = GetShaderBinding(bUseVolumetricRenderTarget ? View.VolumetricRenderTargetViewUniformBuffer : View.ViewUniformBuffer);
			PassParameters->PS.bOnlyOnRenderedOpaque = false;

			PassParameters->PS.SrcCloudDepthTexture = SrcCloudDepth;
			PassParameters->PS.SrcCloudViewTexture = SrcCloudView;

			PassParameters->RenderTargets[0] = FRenderTargetBinding(DstCloudView, ERenderTargetLoadAction::ENoAction);
			// No depth target

			// We enable the blending when volumetric render target is not enabled. Because in this case, the fog pass is compositing directly over the scene.
			const bool bEnableBlending = !bUseVolumetricRenderTarget;

			FIntRect ViewRect(0, 0, SrcCloudView->Desc.Extent.X, SrcCloudView->Desc.Extent.Y);
			GraphBuilder.AddPass(RDG_EVENT_NAME("Fog"), PassParameters, ERDGPassFlags::Raster,
				[&View, ViewRect, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes, bEnableBlending](FRHICommandList& RHICmdList)
				{
					const bool bSampleFogOnClouds = true;
					RenderViewFog(RHICmdList, View, ViewRect, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes, bSampleFogOnClouds, bEnableBlending);
				});
		}
	}
}

void FDeferredShadingSceneRenderer::RenderFog(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef LightShaftOcclusionTexture,
	bool bFogComposeLocalFogVolumes)
{
	if (Scene->ExponentialFogs.Num() > 0 
		// Fog must be done in the base pass for MSAA to work
		&& !IsForwardShadingEnabled(ShaderPlatform))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ExponentialHeightFog");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Fog);

		const bool bShouldRenderVolumetricFog = ShouldRenderVolumetricFog();

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (View.IsPerspectiveProjection())
			{
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

				TRDGUniformBufferRef<FFogUniformParameters> FogUniformBuffer = CreateFogUniformBuffer(GraphBuilder, View);

				const FScreenPassTextureViewport SceneViewport(SceneTextures.Config.Extent, View.ViewRect);
				const FScreenPassTextureViewport OutputViewport(GetDownscaledViewport(SceneViewport, GetLightShaftDownsampleFactor()));
				const FScreenPassTextureViewportParameters LightShaftParameters = GetScreenPassTextureViewportParameters(OutputViewport);

				FFogPassParameters* PassParameters = CreateDefaultFogPassParameters(GraphBuilder, View, SceneTextures.UniformBuffer, FogUniformBuffer, LightShaftOcclusionTexture, LightShaftParameters);
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
				PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite);

				GraphBuilder.AddPass(RDG_EVENT_NAME("Fog"), PassParameters, ERDGPassFlags::Raster, 
					[this, &View, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes](FRHICommandList& RHICmdList)
				{
					RenderViewFog(RHICmdList, View, View.ViewRect, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes);
				});
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderUnderWaterFog(
	FRDGBuilder& GraphBuilder,
	const FSceneWithoutWaterTextures& SceneWithoutWaterTextures,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesWithDepth)
{
	if (Scene->ExponentialFogs.Num() > 0
		// Fog must be done in the base pass for MSAA to work
		&& !IsForwardShadingEnabled(ShaderPlatform))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "SLW::ExponentialHeightFog");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Fog);

		FRDGTextureRef WaterDepthTexture = SceneWithoutWaterTextures.DepthTexture;
		check(WaterDepthTexture);

		const bool bShouldRenderVolumetricFog = ShouldRenderVolumetricFog();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (View.IsPerspectiveProjection() && (View.IsUnderwater() || CVarUnderwaterFogWhenCameraIsAboveWater.GetValueOnRenderThread()))
			{
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

				const auto& SceneWithoutWaterView = SceneWithoutWaterTextures.Views[ViewIndex];

				TRDGUniformBufferRef<FFogUniformParameters> FogUniformBuffer = CreateFogUniformBuffer(GraphBuilder, View);

				// TODO add support for occlusion texture on water
				FRDGTextureRef LightShaftOcclusionTexture = nullptr;	
				FScreenPassTextureViewportParameters LightShaftParameters = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(FIntRect(0, 0, 1, 1)));

				FFogPassParameters* PassParameters = CreateDefaultFogPassParameters(GraphBuilder, View, SceneTexturesWithDepth, FogUniformBuffer, LightShaftOcclusionTexture, LightShaftParameters);
				PassParameters->PS.WaterDepthTexture = WaterDepthTexture;
				PassParameters->PS.bUseWaterDepthTexture = true;
				PassParameters->PS.WaterDepthTextureMinMaxUV = SceneWithoutWaterView.MinMaxUV;
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneWithoutWaterTextures.ColorTexture, ERenderTargetLoadAction::ELoad);
				// No depth/stencil bound so depth bound clip will not work. If we enable this at some point, we will have to check LocalFogVolume to disable depth bound. Or have a start depth for it.

				const bool bFogComposeLocalFogVolumes = ShouldRenderLocalFogVolume(Scene, ViewFamily); // Always render LFV as part of underwater fog, if present, to see them through the water.
				GraphBuilder.AddPass(RDG_EVENT_NAME("FogBehindWater"), PassParameters, ERDGPassFlags::Raster, [this, &View, SceneWithoutWaterView, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes](FRHICommandList& RHICmdList)
				{
					RenderViewFog(RHICmdList, View, SceneWithoutWaterView.ViewRect, PassParameters, bShouldRenderVolumetricFog, bFogComposeLocalFogVolumes);
				});
			}
		}
	}
}

bool ShouldRenderFog(const FSceneViewFamily& Family)
{
	const FEngineShowFlags EngineShowFlags = Family.EngineShowFlags;

	return EngineShowFlags.Fog
		&& EngineShowFlags.Materials 
		&& !Family.UseDebugViewPS()
		&& CVarFog.GetValueOnRenderThread() == 1
		&& !EngineShowFlags.StationaryLightOverlap 
		&& !EngineShowFlags.LightMapDensity;
}
float GetFogDefaultStartDistance()
{
	return 30.0f;
}

float GetViewFogCommonStartDistance(const FViewInfo& View, bool bShouldRenderVolumetricFog, bool bShouldRenderLocalFogVolumes)
{
	float ExpFogStartDistance = View.ExponentialFogParameters.W;
	float VolFogStartDistance = bShouldRenderVolumetricFog ? View.VolumetricFogStartDistance : ExpFogStartDistance;

	// The fog can be set to start at a certain euclidean distance.
	// clamp the value to be behind the near plane z, according to the smallest distance between volumetric fog and height fog (if they are enabled). 
	float FogCommonStartDistance = FMath::Min(ExpFogStartDistance, VolFogStartDistance);

	if (bShouldRenderLocalFogVolumes)
	{
		FogCommonStartDistance = FMath::Min(GetLocalFogVolumeGlobalStartDistance(), FogCommonStartDistance);
	}

	FogCommonStartDistance = FMath::Max(GetFogDefaultStartDistance(), FogCommonStartDistance);

	return FogCommonStartDistance;
}
