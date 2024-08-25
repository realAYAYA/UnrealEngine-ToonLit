// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "LumenSceneLighting.h"
#include "LumenSceneCardCapture.h"
#include "LumenRadiosity.h"

int32 GLumenSurfaceCacheCompress = 1;
FAutoConsoleVariableRef CVarLumenSurfaceCacheCompress(
	TEXT("r.LumenScene.SurfaceCache.Compress"),
	GLumenSurfaceCacheCompress,
	TEXT("Whether to use run time compression for surface cache.\n")
	TEXT("0 - Disabled\n")
	TEXT("1 - Compress using UAV aliasing if supported\n")
	TEXT("2 - Compress using CopyTexture (may be very slow on some RHIs)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

enum class ELumenSurfaceCacheLayer : uint8
{
	Depth,
	Albedo,
	Opacity,
	Normal,
	Emissive,

	MAX
};

struct FLumenSurfaceLayerConfig
{
	const TCHAR* Name;
	EPixelFormat UncompressedFormat;
	EPixelFormat CompressedFormat;
	EPixelFormat CompressedUAVFormat;
	FVector ClearValue;
};

const FLumenSurfaceLayerConfig& GetSurfaceLayerConfig(ELumenSurfaceCacheLayer Layer)
{
	static FLumenSurfaceLayerConfig Configs[(uint32)ELumenSurfaceCacheLayer::MAX] =
	{
		{ TEXT("Depth"),		PF_G16,				PF_Unknown,	PF_Unknown,				FVector(1.0f, 0.0f, 0.0f) },
		{ TEXT("Albedo"),		PF_R8G8B8A8,		PF_BC7,		PF_R32G32B32A32_UINT,	FVector(0.0f, 0.0f, 0.0f) },
		{ TEXT("Opacity"),		PF_G8,				PF_Unknown,	PF_Unknown,				FVector(1.0f, 0.0f, 0.0f) }, // #lumen_todo: Fix BC4 compression and re-enable
		{ TEXT("Normal"),		PF_R8G8,			PF_BC5,		PF_R32G32B32A32_UINT,	FVector(0.0f, 0.0f, 0.0f) },
		{ TEXT("Emissive"),		PF_FloatR11G11B10,	PF_BC6H,	PF_R32G32B32A32_UINT,	FVector(0.0f, 0.0f, 0.0f) }
	};	

	check((uint32)Layer < UE_ARRAY_COUNT(Configs));

	return Configs[(uint32)Layer];
}

class FLumenCardCopyPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenCardCopyPS);
	SHADER_USE_PARAMETER_STRUCT(FLumenCardCopyPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint4>, RWAtlasBlock4)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint2>, RWAtlasBlock2)
		SHADER_PARAMETER(FVector2f, OneOverSourceAtlasSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceAlbedoAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceNormalAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceEmissiveAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceDepthAtlas)
	END_SHADER_PARAMETER_STRUCT()

	class FSurfaceCacheLayer : SHADER_PERMUTATION_ENUM_CLASS("SURFACE_LAYER", ELumenSurfaceCacheLayer);
	class FCompress : SHADER_PERMUTATION_BOOL("COMPRESS");
	using FPermutationDomain = TShaderPermutationDomain<FSurfaceCacheLayer, FCompress>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!PermutationVector.Get<FCompress>())
		{
			const FLumenSurfaceLayerConfig& LumenSurfaceLayerConfig = GetSurfaceLayerConfig(PermutationVector.Get<FSurfaceCacheLayer>());
			OutEnvironment.SetRenderTargetOutputFormat(0, LumenSurfaceLayerConfig.UncompressedFormat);
		}
	}

};

IMPLEMENT_GLOBAL_SHADER(FLumenCardCopyPS, "/Engine/Private/Lumen/SurfaceCache/LumenSurfaceCache.usf", "LumenCardCopyPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenCardCopyParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardCopyPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FClearLumenRectsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FClearLumenCardsPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FCopyCardCaptureLightingToAtlasParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FCopyCardCaptureLightingToAtlasPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FCopyTextureParameters, )
	RDG_TEXTURE_ACCESS(InputTexture, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(OutputTexture, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

FRDGTextureRef CreateCardAtlas(FRDGBuilder& GraphBuilder, const FIntPoint PageAtlasSize, ESurfaceCacheCompression PhysicalAtlasCompression, ELumenSurfaceCacheLayer LayerId, const TCHAR* Name)
{
	const FLumenSurfaceLayerConfig& Config = GetSurfaceLayerConfig(LayerId);
	ETextureCreateFlags TexFlags = TexCreate_ShaderResource | TexCreate_NoFastClear;
	const bool bCompressed = PhysicalAtlasCompression != ESurfaceCacheCompression::Disabled && Config.CompressedFormat != PF_Unknown;

	// Without compression we can write directly into this surface
	if (!bCompressed)
	{
		TexFlags |= TexCreate_RenderTargetable;
	}

	FRDGTextureDesc CreateInfo = FRDGTextureDesc::Create2D(
		PageAtlasSize,
		bCompressed ? Config.CompressedFormat : Config.UncompressedFormat,
		FClearValueBinding::None,
		TexFlags);

	// With UAV aliasing we can directly write into a BC target by overriding UAV format
	if (PhysicalAtlasCompression == ESurfaceCacheCompression::UAVAliasing && Config.CompressedFormat != PF_Unknown)
	{
		CreateInfo.Flags |= TexCreate_UAV;
		CreateInfo.UAVFormat = Config.CompressedUAVFormat;
	}

	return GraphBuilder.CreateTexture(CreateInfo, Name);
}

void FLumenSceneData::AllocateCardAtlases(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	const FIntPoint PageAtlasSize = GetPhysicalAtlasSize();

	FrameTemporaries.AlbedoAtlas = CreateCardAtlas(GraphBuilder, PageAtlasSize, PhysicalAtlasCompression, ELumenSurfaceCacheLayer::Albedo, TEXT("Lumen.SceneAlbedo"));
	FrameTemporaries.OpacityAtlas = CreateCardAtlas(GraphBuilder, PageAtlasSize, PhysicalAtlasCompression, ELumenSurfaceCacheLayer::Opacity, TEXT("Lumen.SceneOpacity"));
	FrameTemporaries.DepthAtlas = CreateCardAtlas(GraphBuilder, PageAtlasSize, PhysicalAtlasCompression, ELumenSurfaceCacheLayer::Depth, TEXT("Lumen.SceneDepth"));
	FrameTemporaries.NormalAtlas = CreateCardAtlas(GraphBuilder, PageAtlasSize, PhysicalAtlasCompression, ELumenSurfaceCacheLayer::Normal, TEXT("Lumen.SceneNormal"));
	FrameTemporaries.EmissiveAtlas = CreateCardAtlas(GraphBuilder, PageAtlasSize, PhysicalAtlasCompression, ELumenSurfaceCacheLayer::Emissive, TEXT("Lumen.SceneEmissive"));

	FrameTemporaries.DirectLightingAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			PageAtlasSize,
			Lumen::GetDirectLightingAtlasFormat(),
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV
		), TEXT("Lumen.SceneDirectLighting"));

	FrameTemporaries.IndirectLightingAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			GetRadiosityAtlasSize(),
			Lumen::GetIndirectLightingAtlasFormat(),
				FClearValueBinding::Black,
				TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV
			), TEXT("Lumen.SceneIndirectLighting"));

	FrameTemporaries.RadiosityNumFramesAccumulatedAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			GetRadiosityAtlasSize(),
			Lumen::GetNumFramesAccumulatedAtlasFormat(),
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV
		), TEXT("Lumen.SceneNumFramesAccumulatedAtlas"));

	FrameTemporaries.FinalLightingAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			PageAtlasSize,
			PF_FloatR11G11B10,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV
		), TEXT("Lumen.SceneFinalLighting"));
}

// Copy captured cards into surface cache. Possibly with compression. Has three paths:
// - Compress from capture atlas to surface cache (for platforms supporting GRHISupportsUAVFormatAliasing or when compression is disabled)
// - Compress from capture atlas into a temporary atlas and copy results into surface cache
// - Straight copy into uncompressed atlas
void FDeferredShadingSceneRenderer::UpdateLumenSurfaceCacheAtlas(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender,
	FRDGBufferSRVRef CardCaptureRectBufferSRV,
	const FCardCaptureAtlas& CardCaptureAtlas,
	const FResampledCardCaptureAtlas& ResampledCardCaptureAtlas)
{
	LLM_SCOPE_BYTAG(Lumen);
	RDG_EVENT_SCOPE(GraphBuilder, "CopyCardsToSurfaceCache");

	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

	// Create rect buffer
	FRDGBufferRef SurfaceCacheRectBuffer;
	{
		FRDGUploadData<FUintVector4> SurfaceCacheRectArray(GraphBuilder, CardPagesToRender.Num());
		for (int32 Index = 0; Index < CardPagesToRender.Num(); Index++)
		{
			const FCardPageRenderData& CardPageRenderData = CardPagesToRender[Index];
			FUintVector4& Rect = SurfaceCacheRectArray[Index];
			Rect.X = FMath::Max(CardPageRenderData.SurfaceCacheAtlasRect.Min.X, 0);
			Rect.Y = FMath::Max(CardPageRenderData.SurfaceCacheAtlasRect.Min.Y, 0);
			Rect.Z = FMath::Max(CardPageRenderData.SurfaceCacheAtlasRect.Max.X, 0);
			Rect.W = FMath::Max(CardPageRenderData.SurfaceCacheAtlasRect.Max.Y, 0);
		}

		SurfaceCacheRectBuffer =
			CreateUploadBuffer(GraphBuilder, TEXT("Lumen.SurfaceCacheRects"),
				sizeof(FUintVector4), FMath::RoundUpToPowerOfTwo(CardPagesToRender.Num()),
				SurfaceCacheRectArray);
	}
	FRDGBufferSRVRef SurfaceCacheRectBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(SurfaceCacheRectBuffer, PF_R32G32B32A32_UINT));

	const FIntPoint PhysicalAtlasSize = LumenSceneData.GetPhysicalAtlasSize();
	const ESurfaceCacheCompression PhysicalAtlasCompression = LumenSceneData.GetPhysicalAtlasCompression();
	const FIntPoint CardCaptureAtlasSize = LumenSceneData.GetCardCaptureAtlasSize();

	struct FPassConfig
	{
		FRDGTextureRef SurfaceCacheAtlas = nullptr;
		ELumenSurfaceCacheLayer Layer = ELumenSurfaceCacheLayer::MAX;
	};

	FPassConfig PassConfigs[(uint32)ELumenSurfaceCacheLayer::MAX] =
	{
		{ FrameTemporaries.DepthAtlas,		ELumenSurfaceCacheLayer::Depth },
		{ FrameTemporaries.AlbedoAtlas,		ELumenSurfaceCacheLayer::Albedo },
		{ FrameTemporaries.OpacityAtlas,	ELumenSurfaceCacheLayer::Opacity },
		{ FrameTemporaries.NormalAtlas,		ELumenSurfaceCacheLayer::Normal },
		{ FrameTemporaries.EmissiveAtlas,	ELumenSurfaceCacheLayer::Emissive },
	};

	for (FPassConfig& Pass : PassConfigs)
	{
		const FLumenSurfaceLayerConfig& LayerConfig = GetSurfaceLayerConfig(Pass.Layer);

		if (PhysicalAtlasCompression == ESurfaceCacheCompression::UAVAliasing && LayerConfig.CompressedFormat != PF_Unknown)
		{
			// Compress to surface cache directly
			{
				const FIntPoint CompressedCardCaptureAtlasSize = FIntPoint::DivideAndRoundUp(CardCaptureAtlasSize, 4);
				const FIntPoint CompressedPhysicalAtlasSize = FIntPoint::DivideAndRoundUp(PhysicalAtlasSize, 4);

				FLumenCardCopyParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardCopyParameters>();
				PassParameters->PS.View = View.ViewUniformBuffer;
				PassParameters->PS.RWAtlasBlock4 = LayerConfig.CompressedUAVFormat == PF_R32G32B32A32_UINT ? GraphBuilder.CreateUAV(Pass.SurfaceCacheAtlas) : nullptr;
				PassParameters->PS.RWAtlasBlock2 = LayerConfig.CompressedUAVFormat == PF_R32G32_UINT ? GraphBuilder.CreateUAV(Pass.SurfaceCacheAtlas) : nullptr;
				PassParameters->PS.SourceAlbedoAtlas = CardCaptureAtlas.Albedo;
				PassParameters->PS.SourceNormalAtlas = CardCaptureAtlas.Normal;
				PassParameters->PS.SourceEmissiveAtlas = CardCaptureAtlas.Emissive;
				PassParameters->PS.SourceDepthAtlas = CardCaptureAtlas.DepthStencil;
				PassParameters->PS.OneOverSourceAtlasSize = FVector2f(1.0f, 1.0f) / FVector2f(CardCaptureAtlasSize);

				FLumenCardCopyPS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenCardCopyPS::FSurfaceCacheLayer>(Pass.Layer);
				PermutationVector.Set<FLumenCardCopyPS::FCompress>(true);
				auto PixelShader = View.ShaderMap->GetShader<FLumenCardCopyPS>(PermutationVector);

				FPixelShaderUtils::AddRasterizeToRectsPass<FLumenCardCopyPS>(GraphBuilder,
					View.ShaderMap,
					RDG_EVENT_NAME("CompressToSurfaceCache %s", LayerConfig.Name),
					PixelShader,
					PassParameters,
					CompressedPhysicalAtlasSize,
					SurfaceCacheRectBufferSRV,
					CardPagesToRender.Num(),
					/*BlendState*/ nullptr,
					/*RasterizerState*/ nullptr,
					/*DepthStencilState*/ nullptr,
					/*StencilRef*/ 0,
					/*TextureSize*/ CompressedCardCaptureAtlasSize,
					/*RectUVBufferSRV*/ CardCaptureRectBufferSRV,
					/*DownsampleFactor*/ 4,
					/*SkipRenderPass*/ (PassParameters->RenderTargets.GetActiveCount()==0));
			}
		}
		else if (PhysicalAtlasCompression == ESurfaceCacheCompression::CopyTextureRegion && LayerConfig.CompressedFormat != PF_Unknown)
		{
			// Compress through a temp surface
			const FIntPoint TempAtlasSize = FIntPoint::DivideAndRoundUp(CardCaptureAtlasSize, 4);

			// TempAtlas is required on platforms without UAV aliasing (GRHISupportsUAVFormatAliasing), where we can't directly compress into the final surface cache
			FRDGTextureRef TempAtlas = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					TempAtlasSize,
					LayerConfig.CompressedUAVFormat,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource | TexCreate_NoFastClear),
				TEXT("Lumen.TempCaptureAtlas"));

			// Compress into temporary atlas
			{
				FLumenCardCopyParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardCopyParameters>();

				PassParameters->PS.View = View.ViewUniformBuffer;
				PassParameters->PS.RWAtlasBlock4 = LayerConfig.CompressedUAVFormat == PF_R32G32B32A32_UINT ? GraphBuilder.CreateUAV(TempAtlas) : nullptr;
				PassParameters->PS.RWAtlasBlock2 = LayerConfig.CompressedUAVFormat == PF_R32G32_UINT ? GraphBuilder.CreateUAV(TempAtlas) : nullptr;
				PassParameters->PS.SourceAlbedoAtlas = CardCaptureAtlas.Albedo;
				PassParameters->PS.SourceNormalAtlas = CardCaptureAtlas.Normal;
				PassParameters->PS.SourceEmissiveAtlas = CardCaptureAtlas.Emissive;
				PassParameters->PS.SourceDepthAtlas = CardCaptureAtlas.DepthStencil;
				PassParameters->PS.OneOverSourceAtlasSize = FVector2f(1.0f, 1.0f) / FVector2f(CardCaptureAtlasSize);

				FLumenCardCopyPS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenCardCopyPS::FSurfaceCacheLayer>(Pass.Layer);
				PermutationVector.Set<FLumenCardCopyPS::FCompress>(true);
				auto PixelShader = View.ShaderMap->GetShader<FLumenCardCopyPS>(PermutationVector);

				FPixelShaderUtils::AddRasterizeToRectsPass<FLumenCardCopyPS>(GraphBuilder,
					View.ShaderMap,
					RDG_EVENT_NAME("CompressToTemp %s", LayerConfig.Name),
					PixelShader,
					PassParameters,
					TempAtlasSize,
					CardCaptureRectBufferSRV,
					CardPagesToRender.Num(),
					/*BlendState*/ nullptr,
					/*RasterizerState*/ nullptr,
					/*DepthStencilState*/ nullptr,
					/*StencilRef*/ 0,
					/*TextureSize*/ TempAtlasSize,
					/*RectUVBufferSRV*/ nullptr,
					/*DownsampleFactor*/ 4,
					/*SkipRenderPass*/ (PassParameters->RenderTargets.GetActiveCount() == 0));
			}

			// Copy from temporary atlas to surface cache
			{
				FCopyTextureParameters* Parameters = GraphBuilder.AllocParameters<FCopyTextureParameters>();
				Parameters->InputTexture = TempAtlas;
				Parameters->OutputTexture = Pass.SurfaceCacheAtlas;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("CopyTempToSurfaceCache %s", LayerConfig.Name),
					Parameters,
					ERDGPassFlags::Copy,
					[&CardPagesToRender, InputTexture = TempAtlas, OutputTexture = Pass.SurfaceCacheAtlas](FRHICommandList& RHICmdList)
				{
					for (int32 PageIndex = 0; PageIndex < CardPagesToRender.Num(); ++PageIndex)
					{
						const FCardPageRenderData& Page = CardPagesToRender[PageIndex];

						FRHICopyTextureInfo CopyInfo;
						CopyInfo.Size.X = Page.CardCaptureAtlasRect.Width() / 4;
						CopyInfo.Size.Y = Page.CardCaptureAtlasRect.Height() / 4;
						CopyInfo.Size.Z = 1;
						CopyInfo.SourcePosition.X = Page.CardCaptureAtlasRect.Min.X / 4;
						CopyInfo.SourcePosition.Y = Page.CardCaptureAtlasRect.Min.Y / 4;
						CopyInfo.SourcePosition.Z = 0;
						CopyInfo.DestPosition.X = Page.SurfaceCacheAtlasRect.Min.X;
						CopyInfo.DestPosition.Y = Page.SurfaceCacheAtlasRect.Min.Y;
						CopyInfo.DestPosition.Z = 0;

						RHICmdList.CopyTexture(InputTexture->GetRHI(), OutputTexture->GetRHI(), CopyInfo);
					}
				});
			}
		}
		else
		{
			// Copy uncompressed to surface cache
			{
				FLumenCardCopyParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardCopyParameters>();

				PassParameters->RenderTargets[0] = FRenderTargetBinding(Pass.SurfaceCacheAtlas, ERenderTargetLoadAction::ELoad, 0);
				PassParameters->PS.View = View.ViewUniformBuffer;
				PassParameters->PS.SourceAlbedoAtlas = CardCaptureAtlas.Albedo;
				PassParameters->PS.SourceNormalAtlas = CardCaptureAtlas.Normal;
				PassParameters->PS.SourceEmissiveAtlas = CardCaptureAtlas.Emissive;
				PassParameters->PS.SourceDepthAtlas = CardCaptureAtlas.DepthStencil;
				PassParameters->PS.OneOverSourceAtlasSize = FVector2f(1.0f, 1.0f) / FVector2f(CardCaptureAtlasSize);

				FLumenCardCopyPS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenCardCopyPS::FSurfaceCacheLayer>(Pass.Layer);
				PermutationVector.Set<FLumenCardCopyPS::FCompress>(false);
				auto PixelShader = View.ShaderMap->GetShader<FLumenCardCopyPS>(PermutationVector);

				FPixelShaderUtils::AddRasterizeToRectsPass<FLumenCardCopyPS>(GraphBuilder,
					View.ShaderMap,
					RDG_EVENT_NAME("CopyToSurfaceCache %s", LayerConfig.Name),
					PixelShader,
					PassParameters,
					PhysicalAtlasSize,
					SurfaceCacheRectBufferSRV,
					CardPagesToRender.Num(),
					/*BlendState*/ nullptr,
					/*RasterizerState*/ nullptr,
					/*DepthStencilState*/ nullptr,
					/*StencilRef*/ 0,
					/*TextureSize*/ CardCaptureAtlasSize,
					/*RectUVBufferSRV*/ CardCaptureRectBufferSRV);
			}
		}
	}

	// Fill lighting for newly captured cards
	{
		// Downsampled radiosity atlas copy not implemented yet
		check(LumenRadiosity::GetAtlasDownsampleFactor() == 1);

		extern int32 GLumenSceneSurfaceCacheResampleLighting;
		const bool bResample = GLumenSceneSurfaceCacheResampleLighting != 0 && ResampledCardCaptureAtlas.DirectLighting != nullptr;
		const bool bRadiosityEnabled = LumenRadiosity::IsEnabled(ViewFamily);

		FCopyCardCaptureLightingToAtlasParameters* PassParameters = GraphBuilder.AllocParameters<FCopyCardCaptureLightingToAtlasParameters>();

		PassParameters->RenderTargets[0] = FRenderTargetBinding(FrameTemporaries.DirectLightingAtlas, ERenderTargetLoadAction::ELoad, 0);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(FrameTemporaries.FinalLightingAtlas, ERenderTargetLoadAction::ELoad, 0);
		PassParameters->RenderTargets[2] = FRenderTargetBinding(FrameTemporaries.IndirectLightingAtlas, ERenderTargetLoadAction::ELoad, 0);
		PassParameters->RenderTargets[3] = FRenderTargetBinding(FrameTemporaries.RadiosityNumFramesAccumulatedAtlas, ERenderTargetLoadAction::ELoad, 0);

		PassParameters->PS.View = View.ViewUniformBuffer;
		PassParameters->PS.DiffuseColorBoost = 1.0f / FMath::Max(View.FinalPostProcessSettings.LumenDiffuseColorBoost, 1.0f);
		PassParameters->PS.AlbedoCardCaptureAtlas = CardCaptureAtlas.Albedo;
		PassParameters->PS.EmissiveCardCaptureAtlas = CardCaptureAtlas.Emissive;
		PassParameters->PS.DirectLightingCardCaptureAtlas = ResampledCardCaptureAtlas.DirectLighting;
		PassParameters->PS.RadiosityCardCaptureAtlas = ResampledCardCaptureAtlas.IndirectLighting;
		PassParameters->PS.RadiosityNumFramesAccumulatedCardCaptureAtlas = ResampledCardCaptureAtlas.NumFramesAccumulated;

		FCopyCardCaptureLightingToAtlasPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCopyCardCaptureLightingToAtlasPS::FIndirectLighting>(bRadiosityEnabled);
		PermutationVector.Set<FCopyCardCaptureLightingToAtlasPS::FResample>(bResample);
		auto PixelShader = View.ShaderMap->GetShader<FCopyCardCaptureLightingToAtlasPS>(PermutationVector);

		FPixelShaderUtils::AddRasterizeToRectsPass<FCopyCardCaptureLightingToAtlasPS>(GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("CopyCardCaptureLightingToAtlas"),
			PixelShader,
			PassParameters,
			LumenSceneData.GetPhysicalAtlasSize(),
			SurfaceCacheRectBufferSRV,
			CardPagesToRender.Num(),
			/*BlendState*/ nullptr,
			/*RasterizerState*/ nullptr,
			/*DepthStencilState*/ nullptr,
			/*StencilRef*/ 0,
			/*TextureSize*/ CardCaptureAtlasSize,
			/*RectUVBufferSRV*/ CardCaptureRectBufferSRV,
			/*DownsampleFactor*/ 1);
	}
}

class FClearCompressedAtlasCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearCompressedAtlasCS)
	SHADER_USE_PARAMETER_STRUCT(FClearCompressedAtlasCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint4>, RWAtlasBlock4)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint2>, RWAtlasBlock2)
		SHADER_PARAMETER(FVector3f, ClearValue)
		SHADER_PARAMETER(FIntPoint, OutputAtlasSize)
	END_SHADER_PARAMETER_STRUCT()

	class FSurfaceCacheLayer : SHADER_PERMUTATION_ENUM_CLASS("SURFACE_LAYER", ELumenSurfaceCacheLayer);

	using FPermutationDomain = TShaderPermutationDomain<FSurfaceCacheLayer>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 8;
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearCompressedAtlasCS, "/Engine/Private/Lumen/SurfaceCache/LumenSurfaceCache.usf", "ClearCompressedAtlasCS", SF_Compute);

// Clear entire Lumen surface cache to debug default values
// Surface cache can be compressed
void FDeferredShadingSceneRenderer::ClearLumenSurfaceCacheAtlas(
	FRDGBuilder& GraphBuilder,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FGlobalShaderMap* GlobalShaderMap)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ClearLumenSurfaceCache");

	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

	struct FPassConfig
	{
		FRDGTextureRef SurfaceCacheAtlas = nullptr;
		ELumenSurfaceCacheLayer Layer = ELumenSurfaceCacheLayer::MAX;
	};

	FPassConfig PassConfigs[(uint32)ELumenSurfaceCacheLayer::MAX] =
	{
		{ FrameTemporaries.DepthAtlas,		ELumenSurfaceCacheLayer::Depth },
		{ FrameTemporaries.AlbedoAtlas,		ELumenSurfaceCacheLayer::Albedo },
		{ FrameTemporaries.OpacityAtlas,	ELumenSurfaceCacheLayer::Opacity },
		{ FrameTemporaries.NormalAtlas,		ELumenSurfaceCacheLayer::Normal },
		{ FrameTemporaries.EmissiveAtlas,	ELumenSurfaceCacheLayer::Emissive },
	};

	const FIntPoint PhysicalAtlasSize = LumenSceneData.GetPhysicalAtlasSize();
	const ESurfaceCacheCompression PhysicalAtlasCompression = LumenSceneData.GetPhysicalAtlasCompression();

	for (FPassConfig& Pass : PassConfigs)
	{
		const FLumenSurfaceLayerConfig& LayerConfig = GetSurfaceLayerConfig(Pass.Layer);

		if (PhysicalAtlasCompression == ESurfaceCacheCompression::UAVAliasing && LayerConfig.CompressedFormat != PF_Unknown)
		{
			// Clear compressed surface cache directly
			{
				const FRDGTextureUAVDesc CompressedSurfaceUAVDesc(Pass.SurfaceCacheAtlas, 0, LayerConfig.CompressedUAVFormat);

				FClearCompressedAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearCompressedAtlasCS::FParameters>();
				PassParameters->RWAtlasBlock4 = LayerConfig.CompressedUAVFormat == PF_R32G32B32A32_UINT ? GraphBuilder.CreateUAV(CompressedSurfaceUAVDesc) : nullptr;
				PassParameters->RWAtlasBlock2 = LayerConfig.CompressedUAVFormat == PF_R32G32_UINT ? GraphBuilder.CreateUAV(CompressedSurfaceUAVDesc) : nullptr;
				PassParameters->ClearValue = (FVector3f)LayerConfig.ClearValue;
				PassParameters->OutputAtlasSize = PhysicalAtlasSize;

				FClearCompressedAtlasCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FClearCompressedAtlasCS::FSurfaceCacheLayer>(Pass.Layer);
				auto ComputeShader = GlobalShaderMap->GetShader<FClearCompressedAtlasCS>(PermutationVector);

				FIntPoint GroupSize(FIntPoint::DivideAndRoundUp(PhysicalAtlasSize, FClearCompressedAtlasCS::GetGroupSize()));

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearCompressedAtlas %s", LayerConfig.Name),
					ComputeShader,
					PassParameters,
					FIntVector(GroupSize.X, GroupSize.Y, 1));
			}
		}
		else if (PhysicalAtlasCompression == ESurfaceCacheCompression::CopyTextureRegion && LayerConfig.CompressedFormat != PF_Unknown)
		{
			// Temporary atlas is required on platforms without UAV aliasing (GRHISupportsUAVFormatAliasing), where we can't directly compress into the final surface cache
			const FIntPoint TempAtlasSize = FIntPoint::DivideAndRoundUp(LumenSceneData.GetCardCaptureAtlasSize(), 4);

			const EPixelFormat TempFormat = LayerConfig.CompressedUAVFormat;

			FRDGTextureRef TempAtlas = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					TempAtlasSize,
					LayerConfig.CompressedUAVFormat,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource | TexCreate_NoFastClear),
				TEXT("Lumen.TempCaptureAtlas"));

			// Clear temporary atlas
			{
				FClearCompressedAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearCompressedAtlasCS::FParameters>();
				PassParameters->RWAtlasBlock4 = LayerConfig.CompressedUAVFormat == PF_R32G32B32A32_UINT ? GraphBuilder.CreateUAV(TempAtlas) : nullptr;
				PassParameters->RWAtlasBlock2 = LayerConfig.CompressedUAVFormat == PF_R32G32_UINT ? GraphBuilder.CreateUAV(TempAtlas) : nullptr;
				PassParameters->ClearValue = (FVector3f)LayerConfig.ClearValue;
				PassParameters->OutputAtlasSize = TempAtlasSize;

				FClearCompressedAtlasCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FClearCompressedAtlasCS::FSurfaceCacheLayer>(Pass.Layer);
				auto ComputeShader = GlobalShaderMap->GetShader<FClearCompressedAtlasCS>(PermutationVector);

				FIntPoint GroupSize(FIntPoint::DivideAndRoundUp(TempAtlasSize, FClearCompressedAtlasCS::GetGroupSize()));

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearCompressedAtlas %s", LayerConfig.Name),
					ComputeShader,
					PassParameters,
					FIntVector(GroupSize.X, GroupSize.Y, 1));
			}

			// Copy from temporary atlas into surface cache
			{
				FCopyTextureParameters* Parameters = GraphBuilder.AllocParameters<FCopyTextureParameters>();
				Parameters->InputTexture = TempAtlas;
				Parameters->OutputTexture = Pass.SurfaceCacheAtlas;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("CopyToSurfaceCache %s", LayerConfig.Name),
					Parameters,
					ERDGPassFlags::Copy,
					[InputTexture = TempAtlas, PhysicalAtlasSize, TempAtlasSize, OutputTexture = Pass.SurfaceCacheAtlas](FRHICommandList& RHICmdList)
				{
					const int32 NumTilesX = FMath::DivideAndRoundDown(PhysicalAtlasSize.X / 4, TempAtlasSize.X);
					const int32 NumTilesY = FMath::DivideAndRoundDown(PhysicalAtlasSize.Y / 4, TempAtlasSize.Y);

					for (int32 TileY = 0; TileY < NumTilesY; ++TileY)
					{
						for (int32 TileX = 0; TileX < NumTilesX; ++TileX)
						{
							FRHICopyTextureInfo CopyInfo;
							CopyInfo.Size.X = TempAtlasSize.X;
							CopyInfo.Size.Y = TempAtlasSize.Y;
							CopyInfo.Size.Z = 1;
							CopyInfo.SourcePosition.X = 0;
							CopyInfo.SourcePosition.Y = 0;
							CopyInfo.SourcePosition.Z = 0;
							CopyInfo.DestPosition.X = TileX * TempAtlasSize.X * 4;
							CopyInfo.DestPosition.Y = TileY * TempAtlasSize.Y * 4;
							CopyInfo.DestPosition.Z = 0;

							RHICmdList.CopyTexture(InputTexture->GetRHI(), OutputTexture->GetRHI(), CopyInfo);
						}
					}
				});
			}
		}
		else
		{
			// Simple clear of an uncompressed surface cache
			AddClearRenderTargetPass(GraphBuilder, Pass.SurfaceCacheAtlas, FLinearColor(LayerConfig.ClearValue));
		}
	}

	AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.DirectLightingAtlas);
	AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.IndirectLightingAtlas);
	AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.RadiosityNumFramesAccumulatedAtlas);
	AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.FinalLightingAtlas);
}