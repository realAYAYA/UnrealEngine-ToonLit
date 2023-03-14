// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for capturing the scene into reflection capture cubemaps, and prefiltering
=============================================================================*/

#include "ReflectionEnvironmentCapture.h"
#include "Misc/FeedbackContext.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "ShowFlags.h"
#include "UnrealClient.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "LegacyScreenPercentageDriver.h"
#include "Shader.h"
#include "TextureResource.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Components/SkyLightComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/TextureCube.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "SceneViewExtension.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ScreenRendering.h"
#include "ReflectionEnvironment.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "MobileReflectionEnvironmentCapture.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "EngineModule.h"
#include "ClearQuad.h"
#include "VolumetricCloudRendering.h"
#include "VolumetricCloudProxy.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"

/** Near plane to use when capturing the scene. */
float GReflectionCaptureNearPlane = 5;

constexpr int32 MinSupersampleCaptureFactor = 1;
constexpr int32 MaxSupersampleCaptureFactor = 8;

int32 GSupersampleCaptureFactor = 1;
static FAutoConsoleVariableRef CVarGSupersampleCaptureFactor(
	TEXT("r.ReflectionCaptureSupersampleFactor"),
	GSupersampleCaptureFactor,
	TEXT("Super sample factor when rendering reflection captures.\n")
	TEXT("Default = 1, no super sampling\n")
	TEXT("Maximum clamped to 8."),
	ECVF_RenderThreadSafe
	);

/** 
 * Mip map used by a Roughness of 0, counting down from the lowest resolution mip (MipCount - 1).  
 * This has been tweaked along with ReflectionCaptureRoughnessMipScale to make good use of the resolution in each mip, especially the highest resolution mips.
 * This value is duplicated in ReflectionEnvironmentShared.usf!
 */
float ReflectionCaptureRoughestMip = 1;

/** 
 * Scales the log2 of Roughness when computing which mip to use for a given roughness.
 * Larger values make the higher resolution mips sharper.
 * This has been tweaked along with ReflectionCaptureRoughnessMipScale to make good use of the resolution in each mip, especially the highest resolution mips.
 * This value is duplicated in ReflectionEnvironmentShared.usf!
 */
float ReflectionCaptureRoughnessMipScale = 1.2f;

// Chaos addition
static TAutoConsoleVariable<int32> CVarReflectionCaptureStaticSceneOnly(
	TEXT("r.chaos.ReflectionCaptureStaticSceneOnly"),
	1,
	TEXT("")
	TEXT(" 0 is off, 1 is on (default)"),
	ECVF_ReadOnly);

/**
* This CVar might affect the quality and performance for: 
* (1) Captured reflection: Increase volume and light function quality and the cost at lighting build time.
* (2) Sky light scene capture (non real time): Increase quality and the cost at the start of a level when the scene is captured.
* (3) Real time sky light capture: this one does not render volumetric fog or anything that reads a light function. It increases the cost only.
*
* It might also create mismatch when light function is time dependent (e.g., sun light simulating time-varying cloud shadows). Different level building can look inconsistent builds after builds.
*/
static int32 GReflectionCaptureEnableLightFunctions = 0;
static FAutoConsoleVariableRef CVarReflectionCaptureEnableLightFunctions(
	TEXT("r.ReflectionCapture.EnableLightFunctions"),
	GReflectionCaptureEnableLightFunctions,
	TEXT("0. Disable light functions in reflection/sky light capture (default).\n")
	TEXT("Others. Enable light functions."));

BEGIN_SHADER_PARAMETER_STRUCT(FCubeShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
	SHADER_PARAMETER(FVector2f, SvPositionToUVScale)
	SHADER_PARAMETER(int32, CubeFace)
	SHADER_PARAMETER(int32, MipIndex)
	SHADER_PARAMETER(int32, NumMips)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/** Pixel shader used for filtering a mip. */
class FCubeDownsamplePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCubeDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FCubeDownsamplePS, FGlobalShader);
	using FParameters = FCubeShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FCubeDownsamplePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "DownsamplePS", SF_Pixel);

class FCubeFilterPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCubeFilterPS);
	SHADER_USE_PARAMETER_STRUCT(FCubeFilterPS, FGlobalShader);
	using FParameters = FCubeShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FCubeFilterPS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "FilterPS", SF_Pixel);

/** Computes the average brightness of a 1x1 mip of a cubemap. */
class FComputeBrightnessPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComputeBrightnessPS);
	SHADER_USE_PARAMETER_STRUCT(FComputeBrightnessPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, ReflectionEnvironmentColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ReflectionEnvironmentColorSampler)
		SHADER_PARAMETER(int32, NumCaptureArrayMips)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTEBRIGHTNESS_PIXELSHADER"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FComputeBrightnessPS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "ComputeBrightnessMain", SF_Pixel);

/** Vertex shader used when writing to a cubemap. */
class FCopyToCubeFaceVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopyToCubeFaceVS);

	FCopyToCubeFaceVS() = default;
	FCopyToCubeFaceVS(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

IMPLEMENT_GLOBAL_SHADER(FCopyToCubeFaceVS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "CopyToCubeFaceVS", SF_Vertex);

/** Pixel shader used when copying scene color from a scene render into a face of a reflection capture cubemap. */
class FCopySceneColorToCubeFacePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopySceneColorToCubeFacePS);
	SHADER_USE_PARAMETER_STRUCT(FCopySceneColorToCubeFacePS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			// SceneDepth is memoryless on mobile
			OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1u);
		}
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
		SHADER_PARAMETER(FVector4f, SkyLightCaptureParameters)
		SHADER_PARAMETER(FVector4f, LowerHemisphereColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCopySceneColorToCubeFacePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "CopySceneColorToCubeFaceColorPS", SF_Pixel);

/** Pixel shader used when copying a cubemap into a face of a reflection capture cubemap. */
class FCopyCubemapToCubeFacePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCopyCubemapToCubeFacePS);
	SHADER_USE_PARAMETER_STRUCT(FCopyCubemapToCubeFacePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER(FVector4f, SkyLightCaptureParameters)
		SHADER_PARAMETER(FVector4f, LowerHemisphereColor)
		SHADER_PARAMETER(FVector2f, SinCosSourceCubemapRotation)
		SHADER_PARAMETER(FVector2f, SvPositionToUVScale)
		SHADER_PARAMETER(int32, CubeFace)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCopyCubemapToCubeFacePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "CopyCubemapToCubeFaceColorPS", SF_Pixel);

class FReflectionCubemapTexture : public FRenderThreadStructBase
{
public:
	FReflectionCubemapTexture(uint32 InCubemapSize)
		: CubemapSize(InCubemapSize)
	{
		check(GSupportsRenderTargetFormat_PF_FloatRGBA);
	}

	void InitRHI(FRHICommandListImmediate& RHICmdList)
	{
		const int32 NumReflectionCaptureMips = GetNumMips(CubemapSize);

		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::CreateCube(
			CubemapSize, PF_FloatRGBA, FClearValueBinding(FLinearColor(0, 10000, 0, 0)), TexCreate_RenderTargetable | TexCreate_TargetArraySlicesIndependently | TexCreate_DisableDCC, NumReflectionCaptureMips);

		RenderTarget = AllocatePooledTexture(TextureDesc, TEXT("ReflectionCubeTexture"));

		FRDGBuilder GraphBuilder(RHICmdList);

		{
			RDG_EVENT_SCOPE(GraphBuilder, "ClearReflectionCubemap");

			FRDGTextureRef OutputTexture = GetRDG(GraphBuilder);

			for (int32 MipIndex = 0; MipIndex < NumReflectionCaptureMips; MipIndex++)
			{
				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					FRenderTargetParameters* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
					PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::EClear, MipIndex, CubeFace);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("Clear (Mip: %d, Face : %d)", MipIndex, CubeFace),
						PassParameters,
						ERDGPassFlags::Raster,
						[](FRHICommandList&) {});
				}
			}
		}

		GraphBuilder.Execute();
	}

	FRDGTexture* GetRDG(FRDGBuilder& GraphBuilder) const
	{
		return GraphBuilder.RegisterExternalTexture(RenderTarget);
	}

private:
	TRefCountPtr<IPooledRenderTarget> RenderTarget;
	uint32 CubemapSize;
};

void CreateCubeMips(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture)
{
	RDG_EVENT_SCOPE(GraphBuilder, "CreateCubeMips");

	TShaderMapRef<FCubeDownsamplePS> PixelShader(ShaderMap);

	const int32 NumMips = CubemapTexture->Desc.NumMips;

	// Downsample all the mips, each one reads from the mip above it
	for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const FIntRect ViewRect(0, 0, MipSize, MipSize);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FCubeDownsamplePS::FParameters>();
			PassParameters->SourceCubemapTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(CubemapTexture, MipIndex - 1));
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->CubeFace = CubeFace;
			PassParameters->MipIndex = MipIndex;
			PassParameters->NumMips = NumMips;
			PassParameters->SvPositionToUVScale = FVector2f(1.0f / MipSize, 1.0f / MipSize);
			PassParameters->RenderTargets[0] = FRenderTargetBinding(CubemapTexture, ERenderTargetLoadAction::ENoAction, MipIndex, CubeFace);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("CreateCubeMips (Mip: %d, Face: %d)", MipIndex, CubeFace),
				PixelShader,
				PassParameters,
				ViewRect);
		}
	}
}

void ComputeSingleAverageBrightnessFromCubemap(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, float* OutAverageBrightness)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ComputeSingleAverageBrightnessFromCubemap");

	FRDGTexture* ReflectionBrightnessTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_FloatRGBA, FClearValueBinding::None, TexCreate_RenderTargetable), TEXT("ReflectionBrightness"));

	auto* PassParameters = GraphBuilder.AllocParameters<FComputeBrightnessPS::FParameters>();

	PassParameters->ReflectionEnvironmentColorTexture = CubemapTexture;
	PassParameters->ReflectionEnvironmentColorSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->NumCaptureArrayMips = CubemapTexture->Desc.NumMips;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(ReflectionBrightnessTexture, ERenderTargetLoadAction::ENoAction);

	TShaderMapRef<FComputeBrightnessPS> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("ReflectionBrightness"),
		PixelShader,
		PassParameters,
		FIntRect(FIntPoint::ZeroValue, FIntPoint(1, 1)));

	AddReadbackTexturePass(GraphBuilder, RDG_EVENT_NAME("ReadbackTexture"), ReflectionBrightnessTexture, [ReflectionBrightnessTexture, OutAverageBrightness](FRHICommandListImmediate& RHICmdList)
	{
		TArray<FFloat16Color> SurfaceData;
		RHICmdList.ReadSurfaceFloatData(ReflectionBrightnessTexture->GetRHI(), FIntRect(0, 0, 1, 1), SurfaceData, CubeFace_PosX, 0, 0);

		// Shader outputs luminance to R
		*OutAverageBrightness = SurfaceData[0].R.GetFloat();
	});
}

void ComputeAverageBrightness(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, float* OutAverageBrightness)
{
	CreateCubeMips(GraphBuilder, ShaderMap, CubemapTexture);
	ComputeSingleAverageBrightnessFromCubemap(GraphBuilder, ShaderMap, CubemapTexture, OutAverageBrightness);
}

FRDGTexture* FilterCubeMap(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* SourceTexture)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FilterCubeMap");

	FRDGTexture* FilteredCubemapTexture = GraphBuilder.CreateTexture(SourceTexture->Desc, TEXT("FilteredCubemapTexture"));

	const int32 NumMips = SourceTexture->Desc.NumMips;

	TShaderMapRef<FCubeFilterPS> PixelShader(ShaderMap);

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const FIntRect ViewRect(0, 0, MipSize, MipSize);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FCubeFilterPS::FParameters>();
			PassParameters->SourceCubemapTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SourceTexture));
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->CubeFace = CubeFace;
			PassParameters->MipIndex = MipIndex;
			PassParameters->NumMips = NumMips;
			PassParameters->SvPositionToUVScale = FVector2f(1.0f / MipSize, 1.0f / MipSize);
			PassParameters->RenderTargets[0] = FRenderTargetBinding(FilteredCubemapTexture, ERenderTargetLoadAction::ENoAction, MipIndex, CubeFace);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("FilterCubeMap (Mip: %d, CubeFace: %d)", MipIndex, CubeFace),
				PixelShader,
				PassParameters,
				ViewRect);
		}
	}

	return FilteredCubemapTexture;
}

// Premultiply alpha in-place using alpha blending

void PremultiplyCubeMipAlpha(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, int32 MipIndex)
{
	const int32 NumMips = CubemapTexture->Desc.NumMips;
	const int32 MipSize = 1 << (NumMips - MipIndex - 1);
	const FIntRect ViewRect(0, 0, MipSize, MipSize);

	for (uint32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		auto* PassParameters = GraphBuilder.AllocParameters<FOneColorPS::FParameters>();
		PassParameters->DrawColorMRT[0] = FLinearColor::Black;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(CubemapTexture, ERenderTargetLoadAction::ELoad, MipIndex, CubeFace);

		TShaderMapRef<FOneColorPS> PixelShader(ShaderMap);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("PremultipliedAlpha (Mip: %d, Face %d", MipIndex, CubeFace),
			PixelShader,
			PassParameters,
			ViewRect,
			TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_DestAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
	}
}

/** Generates mips for glossiness and filters the cubemap for a given reflection. */
FRDGTexture* FilterReflectionEnvironment(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, FSHVectorRGB3* OutIrradianceEnvironmentMap)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FilterReflectionEnvironment");

	PremultiplyCubeMipAlpha(GraphBuilder, ShaderMap, CubemapTexture, 0);
	CreateCubeMips(GraphBuilder, ShaderMap, CubemapTexture);

	if (OutIrradianceEnvironmentMap)
	{
		ComputeDiffuseIrradiance(GraphBuilder, ShaderMap, CubemapTexture, OutIrradianceEnvironmentMap);
	}

	return FilterCubeMap(GraphBuilder, ShaderMap, CubemapTexture);
}

int32 FindOrAllocateCubemapIndex(FScene* Scene, const UReflectionCaptureComponent* Component)
{
	int32 CubemapIndex = -1;

	// Try to find an existing capture index for this component
	const FCaptureComponentSceneState* CaptureSceneStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.AddReference(Component);

	if (CaptureSceneStatePtr)
	{
		CubemapIndex = CaptureSceneStatePtr->CubemapIndex;
	}
	else
	{
		// Reuse a freed index if possible
		CubemapIndex = Scene->ReflectionSceneData.CubemapArraySlotsUsed.FindAndSetFirstZeroBit();
		if (CubemapIndex == INDEX_NONE)
		{
			// If we didn't find a free index, allocate a new one from the CubemapArraySlotsUsed bitfield
			CubemapIndex = Scene->ReflectionSceneData.CubemapArraySlotsUsed.Num();
			Scene->ReflectionSceneData.CubemapArraySlotsUsed.Add(true);
		}

		Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Add(Component, FCaptureComponentSceneState(CubemapIndex));
		Scene->ReflectionSceneData.AllocatedReflectionCaptureStateHasChanged = true;

		check(CubemapIndex < GetMaxNumReflectionCaptures(Scene->GetShaderPlatform()));
	}

	check(CubemapIndex >= 0);
	return CubemapIndex;
}

/** Captures the scene for a reflection capture by rendering the scene multiple times and copying into a cubemap texture. */
void CaptureSceneToScratchCubemap(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	const FReflectionCubemapTexture& ReflectionCubemapTexture,
	ECubeFace CubeFace,
	int32 CubemapSize,
	bool bCapturingForSkyLight,
	bool bLowerHemisphereIsBlack,
	const FLinearColor& LowerHemisphereColor,
	bool bCapturingForMobile)
{
	SceneRenderer->RenderThreadBegin(RHICmdList);

	const ERHIFeatureLevel::Type FeatureLevel = SceneRenderer->FeatureLevel;

	FUniformExpressionCacheAsyncUpdateScope AsyncUpdateScope;

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("CubeMapCapture"), FSceneRenderer::GetRDGParalelExecuteFlags(FeatureLevel));

	// We need to execute the pre-render view extensions before we do any view dependent work.
	FSceneRenderer::ViewExtensionPreRender_RenderThread(GraphBuilder, SceneRenderer);

	{
		RDG_EVENT_SCOPE(GraphBuilder, "CubeMapCapture");

		// Render the scene normally for one face of the cubemap
		SceneRenderer->Render(GraphBuilder);

		AddPass(GraphBuilder, RDG_EVENT_NAME("FlushGPU"), [](FRHICommandListImmediate& InRHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_CaptureSceneToScratchCubemap_Flush);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);

			// some platforms may not be able to keep enqueueing commands like crazy, this will
			// allow them to restart their command buffers
			InRHICmdList.SubmitCommandsAndFlushGPU();
		});

		const FViewInfo& View = SceneRenderer->Views[0];

		FRDGTextureRef OutputTexture = ReflectionCubemapTexture.GetRDG(GraphBuilder);

		auto* PassParameters = GraphBuilder.AllocParameters<FCopySceneColorToCubeFacePS::FParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction, 0, CubeFace);
		PassParameters->LowerHemisphereColor = LowerHemisphereColor;

		{
			FVector4f SkyLightParametersValue(ForceInitToZero);
			FScene* Scene = SceneRenderer->Scene;

			if (bCapturingForSkyLight)
			{
				// When capturing reflection captures, support forcing all low hemisphere lighting to be black
				SkyLightParametersValue = FVector4f(0, 0, bLowerHemisphereIsBlack ? 1.0f : 0.0f, 0);
			}
			else if (!bCapturingForMobile && Scene->SkyLight && !Scene->SkyLight->bHasStaticLighting)	
			{
				// Mobile renderer can't blend reflections with a sky at runtime, so we dont use this path when capturing for a mobile renderer
				
				// When capturing reflection captures and there's a stationary sky light, mask out any pixels whose depth classify it as part of the sky
				// This will allow changing the stationary sky light at runtime
				SkyLightParametersValue = FVector4f(1, Scene->SkyLight->SkyDistanceThreshold, 0, 0);
			}
			else
			{
				// When capturing reflection captures and there's no sky light, or only a static sky light, capture all depth ranges
				SkyLightParametersValue = FVector4f(2, 0, 0, 0);
			}

			PassParameters->SkyLightCaptureParameters = SkyLightParametersValue;
		}

		const FMinimalSceneTextures& SceneTextures = View.GetSceneTextures();

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->SceneColorTexture = SceneTextures.Color.Resolve;
        
        if (!IsMobilePlatform(SceneRenderer->ShaderPlatform))
        {
            PassParameters->SceneDepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            PassParameters->SceneDepthTexture = SceneTextures.Depth.Resolve;
        }

		const int32 EffectiveSize = CubemapSize;
		const FIntPoint SceneTextureExtent = SceneTextures.Config.Extent;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("CopySceneToCubeFace"),
			PassParameters,
			ERDGPassFlags::Raster,
			[EffectiveSize, SceneTextureExtent, FeatureLevel, PassParameters](FRHICommandList& InRHICmdList)
		{
			const FIntRect ViewRect(0, 0, EffectiveSize, EffectiveSize);
			InRHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)EffectiveSize, (float)EffectiveSize, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

			TShaderMapRef<FCopyToCubeFaceVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
			TShaderMapRef<FCopySceneColorToCubeFacePS> PixelShader(GetGlobalShaderMap(FeatureLevel));

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(InRHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			const int32 SupersampleCaptureFactor = FMath::Clamp(GSupersampleCaptureFactor, MinSupersampleCaptureFactor, MaxSupersampleCaptureFactor);

			DrawRectangle( 
				InRHICmdList,
				ViewRect.Min.X, ViewRect.Min.Y, 
				ViewRect.Width(), ViewRect.Height(),
				ViewRect.Min.X, ViewRect.Min.Y, 
				ViewRect.Width() * SupersampleCaptureFactor, ViewRect.Height() * SupersampleCaptureFactor,
				FIntPoint(ViewRect.Width(), ViewRect.Height()),
				SceneTextureExtent,
				VertexShader);
		});
	}

	GraphBuilder.Execute();

	SceneRenderer->RenderThreadEnd(RHICmdList);
}

void CopyCubemapToScratchCubemap(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	UTextureCube* SourceCubemap,
	const FReflectionCubemapTexture& ReflectionCubemapTexture,
	int32 CubemapSize,
	bool bIsSkyLight,
	bool bLowerHemisphereIsBlack,
	float SourceCubemapRotation,
	const FLinearColor& LowerHemisphereColorValue)
{
	check(SourceCubemap);

	const FTexture* SourceCubemapResource = SourceCubemap->GetResource();

	if (SourceCubemapResource == nullptr)
	{
		UE_LOG(LogEngine, Warning, TEXT("Unable to copy from cubemap %s, it's RHI resource is null"), *SourceCubemap->GetPathName());
		return;
	}

	FRDGBuilder GraphBuilder(RHICmdList);

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	{
		RDG_EVENT_SCOPE(GraphBuilder, "CopyCubemapToScratchCubemap");

		FRDGTextureRef OutputTexture = ReflectionCubemapTexture.GetRDG(GraphBuilder);

		TShaderMapRef<FCopyCubemapToCubeFacePS> PixelShader(ShaderMap);

		for (uint32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FCopyCubemapToCubeFacePS::FParameters>();
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->SourceCubemapTexture = SourceCubemapResource->TextureRHI;
			PassParameters->LowerHemisphereColor = LowerHemisphereColorValue;
			PassParameters->SkyLightCaptureParameters = FVector3f(bIsSkyLight ? 1.0f : 0.0f, 0.0f, bLowerHemisphereIsBlack ? 1.0f : 0.0f);
			PassParameters->SinCosSourceCubemapRotation = FVector2f(FMath::Sin(SourceCubemapRotation), FMath::Cos(SourceCubemapRotation));
			PassParameters->SvPositionToUVScale = FVector2f(1.0f / CubemapSize, 1.0f / CubemapSize);
			PassParameters->CubeFace = CubeFace;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction, 0, CubeFace);

			const FIntRect ViewRect(0, 0, CubemapSize, CubemapSize);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("CopyCubemapToCubeFace"),
				PixelShader,
				PassParameters,
				ViewRect);
		}
	}

	GraphBuilder.Execute();
}

const int32 MinCapturesForSlowTask = 20;

void BeginReflectionCaptureSlowTask(int32 NumCaptures, const TCHAR* CaptureReason)
{
	if (NumCaptures > MinCapturesForSlowTask)
	{
		FText Status;
		
		if (CaptureReason)
		{
			Status = FText::Format(NSLOCTEXT("Engine", "UpdateReflectionCapturesForX", "Building reflection captures for {0}"), FText::FromString(FString(CaptureReason)));
		}
		else
		{
			Status = FText(NSLOCTEXT("Engine", "UpdateReflectionCaptures", "Building reflection captures..."));
		}

		GWarn->BeginSlowTask(Status, true);
		GWarn->StatusUpdate(0, NumCaptures, Status);
	}
}

void UpdateReflectionCaptureSlowTask(int32 CaptureIndex, int32 NumCaptures)
{
	const int32 UpdateDivisor = FMath::Max(NumCaptures / 5, 1);

	if (NumCaptures > MinCapturesForSlowTask && (CaptureIndex % UpdateDivisor) == 0)
	{
		GWarn->UpdateProgress(CaptureIndex, NumCaptures);
	}
}

void EndReflectionCaptureSlowTask(int32 NumCaptures)
{
	if (NumCaptures > MinCapturesForSlowTask)
	{
		GWarn->EndSlowTask();
	}
}

static int32 GetMaxReflectionCapturesAtSize(int32 CaptureSize)
{
	FRHITextureDesc RHITexDesc(
		ETextureDimension::TextureCube,
		ETextureCreateFlags::None,
		PF_FloatRGBA,
		FClearValueBinding::Black,
		FIntPoint(CaptureSize, CaptureSize),
		1,										// Depth
		1,										// ArraySize
		FMath::CeilLogTwo(CaptureSize) + 1,		// InNumMips
		1,										// Samples
		0);

	SIZE_T TexMemRequiredPerCapture = RHICalcTexturePlatformSize(RHITexDesc).Size;

	// Attempt to limit the resource size to within percentage (3/4) of total video memory to give consistent stable results.
	// Also limit max size to 4 GB (technically 4 GB minus one), as D3D12 allocation fails for individual resources 4 GB or over,
	// and we'll assume the same is true for other platforms.
	FTextureMemoryStats TextureMemStats;
	RHIGetTextureMemoryStats(TextureMemStats);
		
	SIZE_T DedicatedVideoMemoryLimit = ((SIZE_T)TextureMemStats.DedicatedVideoMemory * (SIZE_T)3) / (SIZE_T)4;
	if (!DedicatedVideoMemoryLimit)
	{
		DedicatedVideoMemoryLimit = (SIZE_T)UINT_MAX;
	}

	const SIZE_T MaxResourceVideoMemoryFootprint = FMath::Min(DedicatedVideoMemoryLimit, (SIZE_T)UINT_MAX);

	return MaxResourceVideoMemoryFootprint / TexMemRequiredPerCapture;
}

int32 NumUniqueReflectionCaptures(const TSparseArray<UReflectionCaptureComponent*>& CaptureComponents)
{
	TSet<FGuid> Guids;
	for (TSparseArray<UReflectionCaptureComponent*>::TConstIterator It(CaptureComponents); It; ++It)
	{
		Guids.Add((*It)->MapBuildDataId);
	}

	return Guids.Num();
}

/** 
 * Allocates reflection captures in the scene's reflection cubemap array and updates them by recapturing the scene.
 * Existing captures will only be uploaded.  Must be called from the game thread.
 */
void FScene::AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures, const TCHAR* CaptureReason, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick)
{
	if (NewCaptures.Num() > 0)
	{
		if (SupportsTextureCubeArray(GetFeatureLevel()))
		{
			int32_t PlatformMaxNumReflectionCaptures = FMath::Min(FMath::FloorToInt(GMaxTextureArrayLayers / 6.0f), GetMaxNumReflectionCaptures(GetShaderPlatform()));

			for (int32 CaptureIndex = 0; CaptureIndex < NewCaptures.Num(); CaptureIndex++)
			{
				bool bAlreadyExists = false;

				// Try to find an existing allocation
				for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
				{
					UReflectionCaptureComponent* OtherComponent = *It;

					if (OtherComponent == NewCaptures[CaptureIndex])
					{
						bAlreadyExists = true;
					}
				}
				
				// Add the capture to the allocated list
				if (!bAlreadyExists && ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num() < PlatformMaxNumReflectionCaptures)
				{
					ReflectionSceneData.AllocatedReflectionCapturesGameThread.Add(NewCaptures[CaptureIndex]);
				}
			}

			// Request the exact amount and size needed by default
			int32 DesiredMaxCubemaps = NumUniqueReflectionCaptures(ReflectionSceneData.AllocatedReflectionCapturesGameThread);
			int32 DesiredCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize();
			int32 ReflectionCaptureSize = DesiredCaptureSize;

			DesiredMaxCubemaps = FMath::Min(DesiredMaxCubemaps, PlatformMaxNumReflectionCaptures);

#if WITH_EDITOR
			// Reduce the capture size (resolution) until we can fit all the captures we need in a single texture cube array resource
			int32 MaxCapturesAtSize;
			for (MaxCapturesAtSize = GetMaxReflectionCapturesAtSize(ReflectionCaptureSize);
				 MaxCapturesAtSize < DesiredMaxCubemaps;
				 MaxCapturesAtSize = GetMaxReflectionCapturesAtSize(ReflectionCaptureSize))
			{
				ReflectionCaptureSize >>= 1;
			}

			if (ReflectionCaptureSize != DesiredCaptureSize)
			{
				UE_LOG(LogEngine, Error, TEXT("Requested reflection capture cube size of %d with %d elements results in too large a resource for host machine, limiting reflection capture cube size to %d"), DesiredCaptureSize, DesiredMaxCubemaps, ReflectionCaptureSize);
			}
#else
			// When not in editor, let the code proceed with the desired size and number, but warn the user an OOM failure is likely and why
			int32 MaxCapturesAtSize = GetMaxReflectionCapturesAtSize(ReflectionCaptureSize);
			if (MaxCapturesAtSize < DesiredMaxCubemaps)
			{
				UE_LOG(LogEngine, Error, TEXT("Reflection capture of size %d with %d elements exceeds estimated GPU memory limit of %d elements, OOM likely"), DesiredCaptureSize, DesiredMaxCubemaps, MaxCapturesAtSize);
				
				// We expect an OOM, but set the limit to the exact number desired to give the best chance of it succeeding anyway
				MaxCapturesAtSize = DesiredMaxCubemaps;
			}
#endif

			if (DesiredMaxCubemaps > 0)
			{
				// In the editor, some captures might have been marked dirty and requested new guids
				//   So remove any probes that can no longer be referenced from the cache

				TSet<FGuid> ToKeep;
				ToKeep.Reserve(DesiredMaxCubemaps);
				for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
				{
					if (IsValid(*It) && (*It)->MapBuildDataId.IsValid())
					{
						ToKeep.Add((*It)->MapBuildDataId);
					}
				}

				FScene* Scene = this;
				ENQUEUE_RENDER_COMMAND(PruneReflectionCaptures)(
					[Scene, ToKeep = MoveTemp(ToKeep)](FRHICommandListImmediate& RHICmdList)
				{
					TArray<int32> ReleasedIndices;
					Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Prune(ToKeep, ReleasedIndices);

					for (int32 Index : ReleasedIndices)
					{
						Scene->ReflectionSceneData.CubemapArraySlotsUsed[Index] = false;
					}
				});
			}

			// If this is not the first time the scene has allocated the cubemap array, include slack to reduce reallocations
			const float MaxCubemapsRoundUpBase = 1.5f;
			if (ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread > 0)
			{
				float Exponent = FMath::LogX(MaxCubemapsRoundUpBase, DesiredMaxCubemaps);

				// Round up to the next integer exponent to provide stability and reduce reallocations
				DesiredMaxCubemaps = FMath::Pow(MaxCubemapsRoundUpBase, FMath::TruncToInt(Exponent) + 1);
			}

			// After slack calculation, need to clamp again at our hard limits.
			DesiredMaxCubemaps = FMath::Min3(DesiredMaxCubemaps, PlatformMaxNumReflectionCaptures, MaxCapturesAtSize);

			bool bNeedsUpdateAllCaptures = ReflectionSceneData.DoesAllocatedDataNeedUpdate(DesiredMaxCubemaps, ReflectionCaptureSize);

			if (bNeedsUpdateAllCaptures)
			{
				// If we're not in the editor, we discard the CPU-side reflection capture data after loading to save memory, so we can't resize if the resolution changes. If this happens, we assert
				check(GIsEditor || ReflectionCaptureSize == ReflectionSceneData.CubemapArray.GetCubemapSize() || ReflectionSceneData.CubemapArray.GetCubemapSize() == 0);

				if (ReflectionCaptureSize == ReflectionSceneData.CubemapArray.GetCubemapSize())
				{
					// We can do a fast GPU copy to realloc the array, so we don't need to update all captures
					ReflectionSceneData.SetGameThreadTrackingData(DesiredMaxCubemaps, ReflectionCaptureSize, DesiredCaptureSize);

					FScene* Scene = this;
					uint32 MaxSize = ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread;
					ENQUEUE_RENDER_COMMAND(GPUResizeArrayCommand)(
						[Scene, MaxSize, ReflectionCaptureSize](FRHICommandListImmediate& RHICmdList)
						{
							// Update the scene's cubemap array, preserving the original contents with a GPU-GPU copy
							Scene->ReflectionSceneData.ResizeCubemapArrayGPU(MaxSize, ReflectionCaptureSize);
						});

					bNeedsUpdateAllCaptures = false;
				}
			}

			if (bNeedsUpdateAllCaptures)
			{
				ReflectionSceneData.SetGameThreadTrackingData(DesiredMaxCubemaps, ReflectionCaptureSize, DesiredCaptureSize);

				FScene* Scene = this;
				uint32 MaxSize = ReflectionSceneData.MaxAllocatedReflectionCubemapsGameThread;
				ENQUEUE_RENDER_COMMAND(ResizeArrayCommand)(
					[Scene, MaxSize, ReflectionCaptureSize](FRHICommandListImmediate& RHICmdList)
					{
						// Update the scene's cubemap array, which will reallocate it, so we no longer have the contents of existing entries
						Scene->ReflectionSceneData.CubemapArray.UpdateMaxCubemaps(MaxSize, ReflectionCaptureSize);
					});

				// Recapture all reflection captures now that we have reallocated the cubemap array
				UpdateAllReflectionCaptures(CaptureReason, ReflectionCaptureSize, bVerifyOnlyCapturing, bCapturingForMobile, bInsideTick);
			}
			else
			{
				const int32 NumCapturesForStatus = bVerifyOnlyCapturing ? NewCaptures.Num() : 0;
				BeginReflectionCaptureSlowTask(NumCapturesForStatus, CaptureReason);

				// No teardown of the cubemap array was needed, just update the captures that were requested
				for (int32 CaptureIndex = 0; CaptureIndex < NewCaptures.Num(); CaptureIndex++)
				{
					UReflectionCaptureComponent* CurrentComponent = NewCaptures[CaptureIndex];
					UpdateReflectionCaptureSlowTask(CaptureIndex, NumCapturesForStatus);

					bool bAllocated = false;

					for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
					{
						if (*It == CurrentComponent)
						{
							bAllocated = true;
						}
					}

					if (bAllocated)
					{
						CaptureOrUploadReflectionCapture(CurrentComponent, ReflectionCaptureSize, bVerifyOnlyCapturing, bCapturingForMobile, bInsideTick);
					}
				}

				EndReflectionCaptureSlowTask(NumCapturesForStatus);
			}
		}

		for (int32 CaptureIndex = 0; CaptureIndex < NewCaptures.Num(); CaptureIndex++)
		{
			UReflectionCaptureComponent* Component = NewCaptures[CaptureIndex];

			Component->SetCaptureCompleted();

			if (Component->SceneProxy)
			{
				// Update the transform of the reflection capture
				// This is not done earlier by the reflection capture when it detects that it is dirty,
				// To ensure that the RT sees both the new transform and the new contents on the same frame.
				Component->SendRenderTransform_Concurrent();
			}
		}
	}
}

/** Updates the contents of all reflection captures in the scene.  Must be called from the game thread. */
void FScene::UpdateAllReflectionCaptures(const TCHAR* CaptureReason, int32 ReflectionCaptureSize, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick)
{
	if (IsReflectionEnvironmentAvailable(GetFeatureLevel()))
	{
		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(CaptureCommand)(
			[Scene](FRHICommandListImmediate& RHICmdList)
			{
				Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Empty();
				Scene->ReflectionSceneData.CubemapArraySlotsUsed.Reset();
			});

		// Only display status during building reflection captures, otherwise we may interrupt a editor widget manipulation of many captures
		const int32 NumCapturesForStatus = bVerifyOnlyCapturing ? ReflectionSceneData.AllocatedReflectionCapturesGameThread.Num() : 0;
		BeginReflectionCaptureSlowTask(NumCapturesForStatus, CaptureReason);

		int32 CaptureIndex = 0;

		for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
		{
			UpdateReflectionCaptureSlowTask(CaptureIndex, NumCapturesForStatus);

			CaptureIndex++;
			UReflectionCaptureComponent* CurrentComponent = *It;
			CaptureOrUploadReflectionCapture(CurrentComponent, ReflectionCaptureSize, bVerifyOnlyCapturing, bCapturingForMobile, bInsideTick);
		}

		EndReflectionCaptureSlowTask(NumCapturesForStatus);
	}
}

void FScene::ResetReflectionCaptures(bool bOnlyIfOOM)
{
	if (bOnlyIfOOM == false || ReflectionSceneData.ReflectionCaptureSizeGameThread != ReflectionSceneData.DesiredReflectionCaptureSizeGameThread)
	{
		ReflectionSceneData.Reset(this);
	}
}

void GetReflectionCaptureData_RenderingThread(FRHICommandListImmediate& RHICmdList, FScene* Scene, const UReflectionCaptureComponent* Component, FReflectionCaptureData* OutCaptureData)
{
	const FCaptureComponentSceneState* ComponentStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(Component);

	if (ComponentStatePtr)
	{
		FRHITexture* EffectiveDest = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget()->GetRHI();

		const int32 CubemapIndex = ComponentStatePtr->CubemapIndex;
		const int32 NumMips = EffectiveDest->GetNumMips();
		const int32 EffectiveTopMipSize = FMath::Pow(2.f, NumMips - 1);

		int32 CaptureDataSize = 0;

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				CaptureDataSize += MipSize * MipSize * sizeof(FFloat16Color);
			}
		}

		OutCaptureData->FullHDRCapturedData.Empty(CaptureDataSize);
		OutCaptureData->FullHDRCapturedData.AddZeroed(CaptureDataSize);
		int32 MipBaseIndex = 0;

		for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			check(EffectiveDest->GetFormat() == PF_FloatRGBA);
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);
			const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				TArray<FFloat16Color> SurfaceData;
				// Read each mip face
				//@todo - do this without blocking the GPU so many times
				//@todo - pool the temporary textures in RHIReadSurfaceFloatData instead of always creating new ones
				RHICmdList.ReadSurfaceFloatData(EffectiveDest, FIntRect(0, 0, MipSize, MipSize), SurfaceData, (ECubeFace)CubeFace, CubemapIndex, MipIndex);
				const int32 DestIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
				uint8* FaceData = &OutCaptureData->FullHDRCapturedData[DestIndex];
				check(SurfaceData.Num() * SurfaceData.GetTypeSize() == CubeFaceBytes);
				FMemory::Memcpy(FaceData, SurfaceData.GetData(), CubeFaceBytes);
			}

			MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
		}

		OutCaptureData->CubemapSize = EffectiveTopMipSize;

		OutCaptureData->AverageBrightness = ComponentStatePtr->AverageBrightness;
	}
}

void FScene::GetReflectionCaptureData(UReflectionCaptureComponent* Component, FReflectionCaptureData& OutCaptureData) 
{
	check(GetFeatureLevel() >= ERHIFeatureLevel::SM5);

	FScene* Scene = this;
	FReflectionCaptureData* OutCaptureDataPtr = &OutCaptureData;
	ENQUEUE_RENDER_COMMAND(GetReflectionDataCommand)(
		[Scene, Component, OutCaptureDataPtr](FRHICommandListImmediate& RHICmdList)
		{
			GetReflectionCaptureData_RenderingThread(RHICmdList, Scene, Component, OutCaptureDataPtr);
		});

	// Necessary since the RT is writing to OutDerivedData directly
	FlushRenderingCommands();
}

void UploadReflectionCapture_RenderingThread(FScene* Scene, const FReflectionCaptureData* CaptureData, const UReflectionCaptureComponent* CaptureComponent)
{
	// Due to memory limitations, it's possible for the in-memory size to be smaller than the originally captured size.
	const int32 EffectiveTopMipSize = Scene->ReflectionSceneData.CubemapArray.GetCubemapSize();
	const int32 NumMipsSource = FMath::CeilLogTwo(CaptureData->CubemapSize) + 1;
	const int32 NumMipsDest = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	const int32 CaptureIndex = FindOrAllocateCubemapIndex(Scene, CaptureComponent);
	check(CaptureData->CubemapSize >= Scene->ReflectionSceneData.CubemapArray.GetCubemapSize());
	check(CaptureIndex < Scene->ReflectionSceneData.CubemapArray.GetMaxCubemaps());
	FRHITexture* CubeMapArray = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget()->GetRHI();
	check(CubeMapArray->GetFormat() == PF_FloatRGBA);

	int32 MipBaseIndex = 0;

	// Skip over mips in originally captured data, based on what we can fit in memory.
	for (int32 MipIndex = 0; MipIndex < NumMipsSource - NumMipsDest; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMipsSource - MipIndex - 1);
		const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

		MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
	}

	for (int32 MipIndex = 0; MipIndex < NumMipsDest; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMipsDest - MipIndex - 1);
		const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			uint32 DestStride = 0;
			uint8* DestBuffer = (uint8*)RHILockTextureCubeFace(CubeMapArray, CubeFace, CaptureIndex, MipIndex, RLM_WriteOnly, DestStride, false);

			// Handle DestStride by copying each row
			for (int32 Y = 0; Y < MipSize; Y++)
			{
				FFloat16Color* DestPtr = (FFloat16Color*)((uint8*)DestBuffer + Y * DestStride);
				const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes + Y * MipSize * sizeof(FFloat16Color);
				const uint8* SourcePtr = &CaptureData->FullHDRCapturedData[SourceIndex];
				FMemory::Memcpy(DestPtr, SourcePtr, MipSize * sizeof(FFloat16Color));
			}

			RHIUnlockTextureCubeFace(CubeMapArray, CubeFace, CaptureIndex, MipIndex, false);
		}

		MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
	}

	FCaptureComponentSceneState& FoundState = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.FindChecked(CaptureComponent);
	FoundState.AverageBrightness = CaptureData->AverageBrightness;
}

/** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
FMatrix CalcCubeFaceViewRotationMatrix(ECubeFace Face)
{
	FMatrix Result(FMatrix::Identity);

	static const FVector XAxis(1.f,0.f,0.f);
	static const FVector YAxis(0.f,1.f,0.f);
	static const FVector ZAxis(0.f,0.f,1.f);

	// vectors we will need for our basis
	FVector vUp(YAxis);
	FVector vDir;

	switch( Face )
	{
	case CubeFace_PosX:
		vDir = XAxis;
		break;
	case CubeFace_NegX:
		vDir = -XAxis;
		break;
	case CubeFace_PosY:
		vUp = -ZAxis;
		vDir = YAxis;
		break;
	case CubeFace_NegY:
		vUp = ZAxis;
		vDir = -YAxis;
		break;
	case CubeFace_PosZ:
		vDir = ZAxis;
		break;
	case CubeFace_NegZ:
		vDir = -ZAxis;
		break;
	}

	// derive right vector
	FVector vRight( vUp ^ vDir );
	// create matrix from the 3 axes
	Result = FBasisVectorMatrix( vRight, vUp, vDir, FVector::ZeroVector );	

	return Result;
}

FMatrix GetCubeProjectionMatrix(float HalfFovDeg, float CubeMapSize, float NearPlane)
{
	if ((bool)ERHIZBuffer::IsInverted)
	{
		return FReversedZPerspectiveMatrix(HalfFovDeg * float(PI) / 180.0f, CubeMapSize, CubeMapSize, NearPlane);
	}
	return FPerspectiveMatrix(HalfFovDeg, CubeMapSize, CubeMapSize, NearPlane);
}

void CaptureSceneIntoScratchCubemap(
	FScene* Scene, 
	const FReflectionCubemapTexture& ReflectionCubemapTexture,
	FVector CapturePosition,
	int32 CubemapSize,
	bool bCapturingForSkyLight,
	bool bStaticSceneOnly, 
	float SkyLightNearPlane,
	bool bLowerHemisphereIsBlack, 
	bool bCaptureEmissiveOnly,
	const FLinearColor& LowerHemisphereColor,
	bool bCapturingForMobile,
	bool bInsideTick
	)
{
	int32 SupersampleCaptureFactor = FMath::Clamp(GSupersampleCaptureFactor, MinSupersampleCaptureFactor, MaxSupersampleCaptureFactor);

	class FDummyRenderTarget final : public FRenderTarget, public FRenderThreadStructBase
	{
	public:
		FDummyRenderTarget() = default;

		const FTexture2DRHIRef& GetRenderTargetTexture() const override
		{
			static FTexture2DRHIRef DummyTexture;
			return DummyTexture;
		}

		void SetSize(int32 TargetSize) { Size = TargetSize; }
		FIntPoint GetSizeXY() const override { return FIntPoint(Size, Size); }
		float GetDisplayGamma() const override { return 1.0f; }

	private:
		int32 Size = 0;
	};

	TRenderThreadStruct<FDummyRenderTarget> DummyRenderTarget;

	for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
	{
		if( !bCapturingForSkyLight && !bInsideTick )
		{
			// Alert the RHI that we're rendering a new frame
			// Not really a new frame, but it will allow pooling mechanisms to update, like the uniform buffer pool
			ENQUEUE_RENDER_COMMAND(BeginFrame)(
				[](FRHICommandList& RHICmdList)
			{
				GFrameNumberRenderThread++;
				RHICmdList.BeginFrame();
			});
		}

		DummyRenderTarget->SetSize(CubemapSize);

		auto ViewFamilyInit = FSceneViewFamily::ConstructionValues(
			DummyRenderTarget.Get(),
			Scene,
			FEngineShowFlags(ESFIM_Game)
			)
			.SetResolveScene(false);

		if( bStaticSceneOnly )
		{
			ViewFamilyInit.SetTime(FGameTime());
		}

		FSceneViewFamilyContext ViewFamily( ViewFamilyInit );

		// Disable features that are not desired when capturing the scene
		ViewFamily.EngineShowFlags.PostProcessing = 0;
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.SetOnScreenDebug(false);
		ViewFamily.EngineShowFlags.HMDDistortion = 0;
		// Conditionally exclude particles and light functions as they are usually dynamic, and can't be captured well
		ViewFamily.EngineShowFlags.Particles = 0;
		ViewFamily.EngineShowFlags.LightFunctions = abs(GReflectionCaptureEnableLightFunctions) ? 1 : 0;
		ViewFamily.EngineShowFlags.SetCompositeEditorPrimitives(false);
		// These are highly dynamic and can't be captured effectively
		ViewFamily.EngineShowFlags.LightShafts = 0;
		// Don't apply sky lighting diffuse when capturing the sky light source, or we would have feedback
		ViewFamily.EngineShowFlags.SkyLighting = !bCapturingForSkyLight;
		// Skip lighting for emissive only
		ViewFamily.EngineShowFlags.Lighting = !bCaptureEmissiveOnly;
		// Never do screen percentage in reflection environment capture.
		ViewFamily.EngineShowFlags.ScreenPercentage = false;

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::Black;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, CubemapSize * SupersampleCaptureFactor, CubemapSize * SupersampleCaptureFactor));

		const float NearPlane = bCapturingForSkyLight ? SkyLightNearPlane : GReflectionCaptureNearPlane;

		// Projection matrix based on the fov, near / far clip settings
		// Each face always uses a 90 degree field of view
		ViewInitOptions.ProjectionMatrix = GetCubeProjectionMatrix(45.0f, (float)CubemapSize * SupersampleCaptureFactor, NearPlane);

		ViewInitOptions.ViewOrigin = CapturePosition;
		ViewInitOptions.ViewRotationMatrix = CalcCubeFaceViewRotationMatrix((ECubeFace)CubeFace);

		FSceneView* View = new FSceneView(ViewInitOptions);

		// Force all surfaces diffuse
		View->RoughnessOverrideParameter = FVector2D( 1.0f, 0.0f );

		if (bCaptureEmissiveOnly)
		{
			View->DiffuseOverrideParameter = FVector4f(0, 0, 0, 0);
			View->SpecularOverrideParameter = FVector4f(0, 0, 0, 0);
		}

		View->bIsReflectionCapture = true;
		View->bStaticSceneOnly = bStaticSceneOnly;
		View->StartFinalPostprocessSettings(CapturePosition);
		View->EndFinalPostprocessSettings(ViewInitOptions);

		ViewFamily.Views.Add(View);

		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamily, /* GlobalResolutionFraction = */ 1.0f));

		FSceneViewExtensionContext ViewExtensionContext(Scene);
		ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
		for (const FSceneViewExtensionRef& Extension : ViewFamily.ViewExtensions)
		{
			Extension->SetupViewFamily(ViewFamily);
			Extension->SetupView(ViewFamily, *View);
		}

		FSceneRenderer* SceneRenderer = FSceneRenderer::CreateSceneRenderer(&ViewFamily, nullptr);

		ENQUEUE_RENDER_COMMAND(CaptureCommand)(
			[SceneRenderer, &ReflectionCubemapTexture, CubeFace, CubemapSize, bCapturingForSkyLight, bLowerHemisphereIsBlack, LowerHemisphereColor, bCapturingForMobile, bInsideTick](FRHICommandListImmediate& RHICmdList)
		{
			CaptureSceneToScratchCubemap(RHICmdList, SceneRenderer, ReflectionCubemapTexture, (ECubeFace)CubeFace, CubemapSize, bCapturingForSkyLight, bLowerHemisphereIsBlack, LowerHemisphereColor, bCapturingForMobile);

			if (!bCapturingForSkyLight && !bInsideTick)
			{
				RHICmdList.EndFrame();
			}
		});
	}
}

void CopyToSceneArray(FRDGBuilder& GraphBuilder, FScene* Scene, FRDGTexture* FilteredCubeTexture, FReflectionCaptureProxy* ReflectionProxy, int32 CaptureIndex)
{
	RDG_EVENT_SCOPE(GraphBuilder, "CopyToSceneArray");

	const int32 NumMips = GetNumMips(Scene->ReflectionSceneData.CubemapArray.GetCubemapSize());

	FRDGTexture* DestCubeTexture = GraphBuilder.RegisterExternalTexture(Scene->ReflectionSceneData.CubemapArray.GetRenderTarget());

	// GPU copy back to the scene's texture array, which is not a render target
	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			FRHICopyTextureInfo CopyInfo;
			CopyInfo.SourceMipIndex   = MipIndex;
			CopyInfo.DestMipIndex     = MipIndex;
			CopyInfo.SourceSliceIndex = CubeFace;
			CopyInfo.DestSliceIndex   = CaptureIndex * CubeFace_MAX + CubeFace;

			AddCopyTexturePass(GraphBuilder, FilteredCubeTexture, DestCubeTexture, CopyInfo);
		}
	}
}

/** 
 * Updates the contents of the given reflection capture by rendering the scene. 
 * This must be called on the game thread.
 */
void FScene::CaptureOrUploadReflectionCapture(UReflectionCaptureComponent* CaptureComponent, int32 ReflectionCaptureSize, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick)
{
	if (IsReflectionEnvironmentAvailable(GetFeatureLevel()))
	{
		FReflectionCaptureData* CaptureData = CaptureComponent->GetMapBuildData();

		// Upload existing derived data if it exists, instead of capturing
		if (CaptureData)
		{
			// Safety check during the reflection capture build, there should not be any map build data
			ensure(!bVerifyOnlyCapturing);

			check(SupportsTextureCubeArray(GetFeatureLevel()));

			FScene* Scene = this;

			ENQUEUE_RENDER_COMMAND(UploadCaptureCommand)
				([Scene, CaptureData, CaptureComponent](FRHICommandListImmediate& RHICmdList)
			{
				// After the final upload we cannot upload again because we tossed the source MapBuildData,
				// After uploading it into the scene's texture array, to guaratee there's only one copy in memory.
				// This means switching between LightingScenarios only works if the scenario level is reloaded (not simply made hidden / visible again)
				if (!CaptureData->HasBeenUploadedFinal())
				{
					UploadReflectionCapture_RenderingThread(Scene, CaptureData, CaptureComponent);

					CaptureData->OnDataUploadedToGPUFinal();
				}
				else
				{
					const FCaptureComponentSceneState* CaptureSceneStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(CaptureComponent);
					
					if (!CaptureSceneStatePtr)
					{
						ensureMsgf(CaptureSceneStatePtr, TEXT("Reflection capture %s uploaded twice without reloading its lighting scenario level.  The Lighting scenario level must be loaded once for each time the reflection capture is uploaded."), *CaptureComponent->GetPathName());
					}
				}
			});
		}
		// Capturing only supported in the editor.  Game can only use built reflection captures.
		else if (bIsEditorScene)
		{
			if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap && !CaptureComponent->Cubemap)
			{
				return;
			}

			if (FPlatformProperties::RequiresCookedData())
			{
				UE_LOG(LogEngine, Warning, TEXT("No built data for %s, skipping generation in cooked build."), *CaptureComponent->GetPathName());
				return;
			}

			// Prefetch all virtual textures so that we have content available
			if (UseVirtualTexturing(GetFeatureLevel()))
			{
				const ERHIFeatureLevel::Type InFeatureLevel = FeatureLevel;
				const FVector2D ScreenSpaceSize(ReflectionCaptureSize, ReflectionCaptureSize);

				ENQUEUE_RENDER_COMMAND(LoadTiles)(
					[InFeatureLevel, ScreenSpaceSize](FRHICommandListImmediate& RHICmdList)
				{
					GetRendererModule().RequestVirtualTextureTiles(ScreenSpaceSize, -1);
					GetRendererModule().LoadPendingVirtualTextureTiles(RHICmdList, InFeatureLevel);
				});

				FlushRenderingCommands();
			}

			TRenderThreadStruct<FReflectionCubemapTexture> ReflectionCubemapTexture(ReflectionCaptureSize);

			if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::CapturedScene)
			{
				static const auto* AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
				const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);

				// Reflection Captures are a form of static lighting, so only capture scene elements that are static
				// However if the project has static lighting disabled, Reflection Captures can still be made to work by capturing Movable lights
				bool const bCaptureStaticSceneOnly = CVarReflectionCaptureStaticSceneOnly.GetValueOnGameThread() != 0 && bAllowStaticLighting;
				CaptureSceneIntoScratchCubemap(this, *ReflectionCubemapTexture, CaptureComponent->GetComponentLocation() + CaptureComponent->CaptureOffset, ReflectionCaptureSize, false, bCaptureStaticSceneOnly, 0, false, false, FLinearColor(), bCapturingForMobile, bInsideTick);
			}
			else if (CaptureComponent->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap)
			{
				UTextureCube* SourceCubemap = CaptureComponent->Cubemap;
				float SourceCubemapRotation = CaptureComponent->SourceCubemapAngle * (PI / 180.f);
				ENQUEUE_RENDER_COMMAND(CopyCubemapCommand)(
					[FeatureLevel = FeatureLevel, SourceCubemap, ReflectionCubemapTexture = ReflectionCubemapTexture.Get(), ReflectionCaptureSize, SourceCubemapRotation](FRHICommandListImmediate& RHICmdList)
				{
					CopyCubemapToScratchCubemap(RHICmdList, FeatureLevel, SourceCubemap, *ReflectionCubemapTexture, ReflectionCaptureSize, false, false, SourceCubemapRotation, FLinearColor());
				});
			}
			else
			{
				check(!TEXT("Unknown reflection source type"));
			}

			// Create a proxy to represent the reflection capture to the rendering thread
			// The rendering thread will be responsible for deleting this when done with the filtering operation
			// We can't use the component's SceneProxy here because the component may not be registered with the scene
			FReflectionCaptureProxy* ReflectionProxy = new FReflectionCaptureProxy(CaptureComponent);

			ENQUEUE_RENDER_COMMAND(FilterCommand)(
				[Scene = this, FeatureLevel = FeatureLevel, ReflectionCubemapTexture = ReflectionCubemapTexture.Get(), ReflectionCaptureSize, CaptureComponent, ReflectionProxy](FRHICommandListImmediate& RHICmdList)
			{
				const int32 CubemapIndex = FindOrAllocateCubemapIndex(Scene, CaptureComponent);
				FCaptureComponentSceneState& FoundState = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.FindChecked(CaptureComponent);

				FRDGBuilder GraphBuilder(RHICmdList);

				auto* ShaderMap = GetGlobalShaderMap(FeatureLevel);

				FRDGTexture* SceneCubemapTexture = ReflectionCubemapTexture->GetRDG(GraphBuilder);

				ComputeAverageBrightness(GraphBuilder, ShaderMap, SceneCubemapTexture, &FoundState.AverageBrightness);

				FRDGTexture* FilteredSceneCubemapTexture = FilterReflectionEnvironment(GraphBuilder, ShaderMap, SceneCubemapTexture, nullptr);

				if (FeatureLevel >= ERHIFeatureLevel::SM5)
				{
					CopyToSceneArray(GraphBuilder, Scene, FilteredSceneCubemapTexture, ReflectionProxy, CubemapIndex);
				}

				GraphBuilder.Execute();

				// Clean up the proxy now that the rendering thread is done with it
				delete ReflectionProxy;
			});
		}
	}
}

void ReadbackRadianceMap(FRDGBuilder& GraphBuilder, FRDGTexture* InputTexture, TArray<FFloat16Color>* OutRadianceMap)
{
	check(InputTexture->Desc.Format == PF_FloatRGBA);

	AddReadbackTexturePass(GraphBuilder, RDG_EVENT_NAME("ReadbackRadianceMap"), InputTexture, [InputTexture, &OutRadianceMap = *OutRadianceMap](FRHICommandListImmediate& RHICmdList)
	{
		const FIntPoint Extent = InputTexture->Desc.Extent;

		const int32 MipIndex = 0;
		const int32 CubeFaceBytes = Extent.X * Extent.Y * OutRadianceMap.GetTypeSize();

		OutRadianceMap.Empty(Extent.X* Extent.Y * 6);
		OutRadianceMap.AddZeroed(Extent.X* Extent.Y * 6);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			TArray<FFloat16Color> SurfaceData;

			// Read each mip face
			RHICmdList.ReadSurfaceFloatData(InputTexture->GetRHI(), FIntRect(FIntPoint::ZeroValue, Extent), SurfaceData, (ECubeFace)CubeFace, 0, MipIndex);
			const int32 DestIndex = CubeFace * Extent.X * Extent.Y;
			FFloat16Color* FaceData = &OutRadianceMap[DestIndex];
			check(SurfaceData.Num() * SurfaceData.GetTypeSize() == CubeFaceBytes);
			FMemory::Memcpy(FaceData, SurfaceData.GetData(), CubeFaceBytes);
		}
	});
}

void CopyToSkyTexture(FRDGBuilder& GraphBuilder, FScene* Scene, FRDGTexture* InputTexture, FTexture* ProcessedTexture)
{
	if (ProcessedTexture->TextureRHI)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "CopyToSkyTexture");

		FRDGTexture* OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ProcessedTexture->TextureRHI, TEXT("SkyTexture")));

		// GPU copy back to the skylight's texture, which is not a render target
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size.X = InputTexture->Desc.Extent.X;
		CopyInfo.Size.Y = InputTexture->Desc.Extent.Y;
		CopyInfo.NumSlices = 6;
		CopyInfo.NumMips = GetNumMips(ProcessedTexture->GetSizeX());

		AddCopyTexturePass(GraphBuilder, InputTexture, OutputTexture, CopyInfo);
	}
}

// Warning: returns before writes to OutIrradianceEnvironmentMap have completed, as they are queued on the rendering thread
void FScene::UpdateSkyCaptureContents(
	const USkyLightComponent* CaptureComponent, 
	bool bCaptureEmissiveOnly, 
	UTextureCube* SourceCubemap, 
	FTexture* OutProcessedTexture, 
	float& OutAverageBrightness, 
	FSHVectorRGB3& OutIrradianceEnvironmentMap,
	TArray<FFloat16Color>* OutRadianceMap)
{
	if (GSupportsRenderTargetFormat_PF_FloatRGBA || FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateSkyCaptureContents);

		World = GetWorld();

		if (World)
		{
			//guarantee that all render proxies are up to date before kicking off this render
			World->SendAllEndOfFrameUpdates();
		}

		const int32 CubemapResolution = CaptureComponent->CubemapResolution;
		const bool bLowerHemisphereIsBlack = CaptureComponent->bLowerHemisphereIsBlack;
		const FLinearColor LowerHemisphereColor = CaptureComponent->LowerHemisphereColor;

		TRenderThreadStruct<FReflectionCubemapTexture> ReflectionCubemapTexture(CubemapResolution);

		if (CaptureComponent->SourceType == SLS_CapturedScene)
		{
			const bool bStaticSceneOnly = CaptureComponent->Mobility == EComponentMobility::Static;
			const bool bCapturingForMobile = false;
			CaptureSceneIntoScratchCubemap(this, *ReflectionCubemapTexture, CaptureComponent->GetComponentLocation(), CubemapResolution, true, bStaticSceneOnly, CaptureComponent->SkyDistanceThreshold, bLowerHemisphereIsBlack, bCaptureEmissiveOnly, LowerHemisphereColor, bCapturingForMobile, false);
		}
		else if (CaptureComponent->SourceType == SLS_SpecifiedCubemap)
		{
			const float SourceCubemapRotation = CaptureComponent->SourceCubemapAngle * (PI / 180.f);
			ENQUEUE_RENDER_COMMAND(CopyCubemapCommand)(
				[FeatureLevel = FeatureLevel, SourceCubemap, ReflectionCubemapTexture = ReflectionCubemapTexture.Get(), CubemapResolution, bLowerHemisphereIsBlack, SourceCubemapRotation, LowerHemisphereColor](FRHICommandListImmediate& RHICmdList)
			{
				CopyCubemapToScratchCubemap(RHICmdList, FeatureLevel, SourceCubemap, *ReflectionCubemapTexture, CubemapResolution, true, bLowerHemisphereIsBlack, SourceCubemapRotation, LowerHemisphereColor);
			});
		}
		else if (CaptureComponent->IsRealTimeCaptureEnabled())
		{
			ensureMsgf(false, TEXT("A sky light with RealTimeCapture enabled cannot be scheduled for a cubemap update. This will be done dynamically each frame by the renderer."));
			return;
		}
		else
		{
			checkNoEntry();
		}

		ENQUEUE_RENDER_COMMAND(UpdateCaptureContents)(
			[Scene = this, FeatureLevel = FeatureLevel, ReflectionCubemapTexture = ReflectionCubemapTexture.Get(), OutAverageBrightness = &OutAverageBrightness, OutIrradianceEnvironmentMap = &OutIrradianceEnvironmentMap, OutProcessedTexture, OutRadianceMap](FRHICommandListImmediate& RHICmdList)
		{
			auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

			FRDGBuilder GraphBuilder(RHICmdList);

			FRDGTexture* SceneCubemapTexture = ReflectionCubemapTexture->GetRDG(GraphBuilder);

			if (OutRadianceMap)
			{
				ReadbackRadianceMap(GraphBuilder, SceneCubemapTexture, OutRadianceMap);
			}

			FRDGTexture* FilteredSceneCubemapTexture;

			if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
			{
				MobileReflectionEnvironmentCapture::ComputeAverageBrightness(GraphBuilder, ShaderMap, SceneCubemapTexture, OutAverageBrightness);
				FilteredSceneCubemapTexture = MobileReflectionEnvironmentCapture::FilterReflectionEnvironment(GraphBuilder, ShaderMap, SceneCubemapTexture, OutIrradianceEnvironmentMap);
			}
			else
			{
				ComputeAverageBrightness(GraphBuilder, ShaderMap, SceneCubemapTexture, OutAverageBrightness);
				FilteredSceneCubemapTexture = FilterReflectionEnvironment(GraphBuilder, ShaderMap, SceneCubemapTexture, OutIrradianceEnvironmentMap);
			}

			if (OutProcessedTexture)
			{
				CopyToSkyTexture(GraphBuilder, Scene, FilteredSceneCubemapTexture, OutProcessedTexture);
			}

			GraphBuilder.Execute();

			Scene->PathTracingSkylightTexture = nullptr;
			Scene->PathTracingSkylightPdf = nullptr;
		});
	}
}
