// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Functionality for computing SH diffuse irradiance from a cubemap
=============================================================================*/

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "RHIStaticStates.h"
#include "ReflectionEnvironmentCapture.h"
#include "GlobalShader.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "VisualizeTexture.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

int32 GDiffuseIrradianceCubemapSize = 32;

int32 GetDiffuseConvolutionSourceMip(FRDGTexture* Texture)
{
	const int32 NumMips = Texture->Desc.NumMips;
	const int32 NumDiffuseMips = GetNumMips(GDiffuseIrradianceCubemapSize);
	return FMath::Max(0, NumMips - NumDiffuseMips);
}

/** Pixel shader used for copying to diffuse irradiance texture. */
class FCopyDiffuseIrradiancePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyDiffuseIrradiancePS);
	SHADER_USE_PARAMETER_STRUCT(FCopyDiffuseIrradiancePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER(FVector4f, CoefficientMask0)
		SHADER_PARAMETER(FVector4f, CoefficientMask1)
		SHADER_PARAMETER(FVector2f, SvPositionToUVScale)
		SHADER_PARAMETER(float, CoefficientMask2)
		SHADER_PARAMETER(int32, CubeFace)
		SHADER_PARAMETER(int32, SourceMipIndex)
		SHADER_PARAMETER(int32, NumSamples)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCopyDiffuseIrradiancePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "DiffuseIrradianceCopyPS", SF_Pixel)

class FAccumulateDiffuseIrradiancePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAccumulateDiffuseIrradiancePS);
	SHADER_USE_PARAMETER_STRUCT(FAccumulateDiffuseIrradiancePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER(FVector4f, Sample01)
		SHADER_PARAMETER(FVector4f, Sample23)
		SHADER_PARAMETER(FVector2f, SvPositionToUVScale)
		SHADER_PARAMETER(int32, CubeFace)
		SHADER_PARAMETER(int32, SourceMipIndex)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FAccumulateDiffuseIrradiancePS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "DiffuseIrradianceAccumulatePS", SF_Pixel)

class FAccumulateCubeFacesPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAccumulateCubeFacesPS);
	SHADER_USE_PARAMETER_STRUCT(FAccumulateCubeFacesPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, SourceCubemapTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceCubemapSampler)
		SHADER_PARAMETER(int32, SourceMipIndex)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FAccumulateCubeFacesPS, "/Engine/Private/ReflectionEnvironmentShaders.usf", "AccumulateCubeFacesPS", SF_Pixel)

void ComputeDiffuseIrradiance(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTexture* LightingSource, FSHVectorRGB3* OutIrradianceEnvironmentMap)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ComputeDiffuseIrradiance");

	const int32 LightingSourceMipIndex = GetDiffuseConvolutionSourceMip(LightingSource);
	const int32 NumMips = GetNumMips(GDiffuseIrradianceCubemapSize);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	FRDGTexture* SkySHIrradianceTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(FSHVector3::MaxSHBasis, 1), PF_FloatRGBA, FClearValueBinding(FLinearColor(0, 10000, 0, 0)), TexCreate_RenderTargetable), TEXT("SkySHIrradiance"));

	TShaderMapRef<FCopyDiffuseIrradiancePS> CopyDiffuseIrradiancePS(ShaderMap);
	TShaderMapRef<FAccumulateDiffuseIrradiancePS> AccumulateDiffuseIrradiancePS(ShaderMap);
	TShaderMapRef<FAccumulateCubeFacesPS> AccumulateCubeFacesPS(ShaderMap);

	for (int32 CoefficientIndex = 0; CoefficientIndex < FSHVector3::MaxSHBasis; CoefficientIndex++)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Coefficient %d", CoefficientIndex);

		const FRDGTextureDesc IrradianceCubemapDesc(
			FRDGTextureDesc::CreateCube(GDiffuseIrradianceCubemapSize, PF_FloatRGBA, FClearValueBinding(FLinearColor(0, 10000, 0, 0)), TexCreate_TargetArraySlicesIndependently | TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_DisableDCC, NumMips));

		FRDGTexture* IrradianceCubemapTexture = GraphBuilder.CreateTexture(IrradianceCubemapDesc, TEXT("DiffuseIrradianceCubemap"));

		// Copy the starting mip from the lighting texture, apply texel area weighting and appropriate SH coefficient
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CopyDiffuseIrradiance");

			const FVector4f Mask0(CoefficientIndex == 0, CoefficientIndex == 1, CoefficientIndex == 2, CoefficientIndex == 3);
			const FVector4f Mask1(CoefficientIndex == 4, CoefficientIndex == 5, CoefficientIndex == 6, CoefficientIndex == 7);
			const float Mask2 = CoefficientIndex == 8;

			const int32 MipIndex = 0;
			const int32 MipSize = GDiffuseIrradianceCubemapSize;
			const FIntRect ViewRect(0, 0, MipSize, MipSize);

			for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FCopyDiffuseIrradiancePS::FParameters>();
				PassParameters->SourceCubemapTexture = LightingSource;
				PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				PassParameters->CoefficientMask0 = Mask0;
				PassParameters->CoefficientMask1 = Mask1;
				PassParameters->CoefficientMask2 = Mask2;
				PassParameters->SvPositionToUVScale = FVector2f(1.0f / MipSize, 1.0f / MipSize);
				PassParameters->CubeFace = CubeFace;
				PassParameters->SourceMipIndex = LightingSourceMipIndex;
				PassParameters->NumSamples = MipSize * MipSize * 6;
				PassParameters->RenderTargets[0] = FRenderTargetBinding(IrradianceCubemapTexture, ERenderTargetLoadAction::ENoAction, 0, CubeFace);

				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					ShaderMap,
					RDG_EVENT_NAME("Face: %d", CubeFace),
					CopyDiffuseIrradiancePS,
					PassParameters,
					ViewRect);
			}
		}

		{
			RDG_EVENT_SCOPE(GraphBuilder, "AccumulateDiffuseIrradiance");

			// Accumulate all the texel values through downsampling to 1x1 mip
			for (int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
			{
				const int32 MipSize = 1 << (NumMips - MipIndex - 1);
				const FIntRect ViewRect(0, 0, MipSize, MipSize);

				const int32 SourceMipIndex = FMath::Max(MipIndex - 1, 0);
				const int32 SourceMipSize = 1 << (NumMips - SourceMipIndex - 1);

				const float HalfSourceTexelSize = 0.5f / SourceMipSize;
				const FVector4f Sample01(-HalfSourceTexelSize, -HalfSourceTexelSize, HalfSourceTexelSize, -HalfSourceTexelSize);
				const FVector4f Sample23(-HalfSourceTexelSize, HalfSourceTexelSize, HalfSourceTexelSize, HalfSourceTexelSize);

				FRDGTextureSRVDesc SourceSRVDesc = FRDGTextureSRVDesc::CreateForMipLevel(IrradianceCubemapTexture, MipIndex - 1); 
				if (GRHISupportsTextureViews == false)
				{
					SourceSRVDesc = FRDGTextureSRVDesc::Create(IrradianceCubemapTexture);
				}

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					auto* PassParameters = GraphBuilder.AllocParameters<FAccumulateDiffuseIrradiancePS::FParameters>();
					PassParameters->SourceCubemapTexture = GraphBuilder.CreateSRV(SourceSRVDesc);
					PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
					PassParameters->Sample01 = Sample01;
					PassParameters->Sample23 = Sample23;
					PassParameters->SvPositionToUVScale = FVector2f(1.0f / MipSize, 1.0f / MipSize);
					PassParameters->CubeFace = CubeFace;
					PassParameters->SourceMipIndex = SourceMipIndex;
					PassParameters->RenderTargets[0] = FRenderTargetBinding(IrradianceCubemapTexture, ERenderTargetLoadAction::ELoad, MipIndex, CubeFace);

					FPixelShaderUtils::AddFullscreenPass(
						GraphBuilder,
						ShaderMap,
						RDG_EVENT_NAME("Mip: %d, Face: %d", MipIndex, CubeFace),
						AccumulateDiffuseIrradiancePS,
						PassParameters,
						ViewRect);
				}
			}
		}

		{
			const int32 SourceMipIndex = NumMips - 1;
			const int32 MipSize = 1;
			const FIntRect ViewRect(CoefficientIndex, 0, CoefficientIndex + 1, 1);

			auto* PassParameters = GraphBuilder.AllocParameters<FAccumulateCubeFacesPS::FParameters>();
			PassParameters->SourceCubemapTexture = IrradianceCubemapTexture;
			PassParameters->SourceCubemapSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->SourceMipIndex = SourceMipIndex;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SkySHIrradianceTexture, ERenderTargetLoadAction::ELoad);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("GatherCoefficients"),
				AccumulateCubeFacesPS,
				PassParameters,
				ViewRect);
		}
	}

	AddReadbackTexturePass(GraphBuilder, RDG_EVENT_NAME("ReadbackCoefficients"), SkySHIrradianceTexture, [SkySHIrradianceTexture, OutIrradianceEnvironmentMap](FRHICommandListImmediate& RHICmdList)
	{
		TArray<FFloat16Color> SurfaceData;
		RHICmdList.ReadSurfaceFloatData(SkySHIrradianceTexture->GetRHI(), FIntRect(0, 0, FSHVector3::MaxSHBasis, 1), SurfaceData, CubeFace_PosX, 0, 0);
		check(SurfaceData.Num() == FSHVector3::MaxSHBasis);

		for (int32 CoefficientIndex = 0; CoefficientIndex < FSHVector3::MaxSHBasis; CoefficientIndex++)
		{
			const FLinearColor CoefficientValue(SurfaceData[CoefficientIndex]);
			OutIrradianceEnvironmentMap->R.V[CoefficientIndex] = CoefficientValue.R;
			OutIrradianceEnvironmentMap->G.V[CoefficientIndex] = CoefficientValue.G;
			OutIrradianceEnvironmentMap->B.V[CoefficientIndex] = CoefficientValue.B;
		}
	});
}