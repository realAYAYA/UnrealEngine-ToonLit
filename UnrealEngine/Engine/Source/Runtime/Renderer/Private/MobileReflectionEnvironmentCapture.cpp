// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileReflectionEnvironmentCapture.h"
#include "ReflectionEnvironmentCapture.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneUtils.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "PostProcess/SceneFilterRendering.h"
#include "OneColorShader.h"
#include "PixelShaderUtils.h"

/** Computes the average brightness of the given reflection capture and stores it in the scene. */
extern void ComputeSingleAverageBrightnessFromCubemap(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, float* OutAverageBrightness);

extern FRDGTexture* FilterCubeMap(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* SourceTexture);

extern void PremultiplyCubeMipAlpha(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, int32 MipIndex);

class FMobileCubeDownsamplePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileCubeDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FMobileCubeDownsamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER(FVector2f, SvPositionToUVScale)
		SHADER_PARAMETER(int32, CubeFace)
		SHADER_PARAMETER(int32, SourceMipIndex)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FMobileCubeDownsamplePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "DownsamplePS_Mobile", SF_Pixel);

namespace MobileReflectionEnvironmentCapture
{
	void CreateCubeMips(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "CreateCubeMips");

		TShaderMapRef<FMobileCubeDownsamplePS> PixelShader(ShaderMap);

		const int32 NumMips = CubemapTexture->Desc.NumMips;

		// Downsample all the mips, each one reads from the mip above it
		for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
		{
			const int32 SourceMipIndex = FMath::Max(MipIndex - 1, 0);
			const int32 MipSize = 1 << (NumMips - MipIndex - 1);
			const FIntRect ViewRect(0, 0, MipSize, MipSize);

			FRDGTextureSRVDesc SourceSRVDesc = FRDGTextureSRVDesc::CreateForMipLevel(CubemapTexture, MipIndex - 1);
			if (GRHISupportsTextureViews == false)
			{
				SourceSRVDesc = FRDGTextureSRVDesc::Create(CubemapTexture);
			}

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FMobileCubeDownsamplePS::FParameters>();
				PassParameters->SourceCubemapTexture = GraphBuilder.CreateSRV(SourceSRVDesc);
				PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				PassParameters->CubeFace = CubeFace;
				PassParameters->SourceMipIndex = SourceMipIndex;
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

	void ComputeAverageBrightness(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* CubemapTexture, float* OutAverageBrightness)
	{
		CreateCubeMips(GraphBuilder, ShaderMap, CubemapTexture);
		ComputeSingleAverageBrightnessFromCubemap(GraphBuilder, ShaderMap, CubemapTexture, OutAverageBrightness);
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
}
