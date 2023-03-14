// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScreenPass.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "RenderGraphUtils.h"

IMPLEMENT_GLOBAL_SHADER(FScreenPassVS, "/Engine/Private/ScreenPass.usf", "ScreenPassVS", SF_Vertex);


RENDERER_API const FScreenTransform FScreenTransform::Identity(FVector2f(1.0f, 1.0f), FVector2f(0.0f, 0.0f));
RENDERER_API const FScreenTransform FScreenTransform::ScreenPosToViewportUV(FVector2f(0.5f, -0.5f), FVector2f(0.5f, 0.5f));
RENDERER_API const FScreenTransform FScreenTransform::ViewportUVToScreenPos(FVector2f(2.0f, -2.0f), FVector2f(-1.0f, 1.0f));

FRHITexture* GetMiniFontTexture()
{
	if (GSystemTextures.AsciiTexture)
	{
		return GSystemTextures.AsciiTexture->GetRHI();
	}
	else
	{
		return GSystemTextures.WhiteDummy->GetRHI();
	}
}

FRDGTextureRef TryCreateViewFamilyTexture(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily)
{
	FRHITexture* TextureRHI = ViewFamily.RenderTarget->GetRenderTargetTexture();
	FRDGTextureRef Texture = nullptr;
	if (TextureRHI)
	{
		Texture = RegisterExternalTexture(GraphBuilder, TextureRHI, TEXT("ViewFamilyTexture"));
		GraphBuilder.SetTextureAccessFinal(Texture, ERHIAccess::RTV);
	}
	return Texture;
}

FScreenPassTextureViewportParameters GetScreenPassTextureViewportParameters(const FScreenPassTextureViewport& InViewport)
{
	const FVector2f Extent(InViewport.Extent);
	const FVector2f ViewportMin(InViewport.Rect.Min.X, InViewport.Rect.Min.Y);
	const FVector2f ViewportMax(InViewport.Rect.Max.X, InViewport.Rect.Max.Y);
	const FVector2f ViewportSize = ViewportMax - ViewportMin;

	FScreenPassTextureViewportParameters Parameters;

	if (!InViewport.IsEmpty())
	{
		Parameters.Extent = Extent;
		Parameters.ExtentInverse = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);

		Parameters.ScreenPosToViewportScale = FVector2f(0.5f, -0.5f) * ViewportSize;
		Parameters.ScreenPosToViewportBias = (0.5f * ViewportSize) + ViewportMin;

		Parameters.ViewportMin = InViewport.Rect.Min;
		Parameters.ViewportMax = InViewport.Rect.Max;

		Parameters.ViewportSize = ViewportSize;
		Parameters.ViewportSizeInverse = FVector2f(1.0f / Parameters.ViewportSize.X, 1.0f / Parameters.ViewportSize.Y);

		Parameters.UVViewportMin = ViewportMin * Parameters.ExtentInverse;
		Parameters.UVViewportMax = ViewportMax * Parameters.ExtentInverse;

		Parameters.UVViewportSize = Parameters.UVViewportMax - Parameters.UVViewportMin;
		Parameters.UVViewportSizeInverse = FVector2f(1.0f / Parameters.UVViewportSize.X, 1.0f / Parameters.UVViewportSize.Y);

		Parameters.UVViewportBilinearMin = Parameters.UVViewportMin + 0.5f * Parameters.ExtentInverse;
		Parameters.UVViewportBilinearMax = Parameters.UVViewportMax - 0.5f * Parameters.ExtentInverse;
	}

	return Parameters;
}

// static
FScreenTransform FScreenTransform::ChangeTextureUVCoordinateFromTo(
	const FScreenPassTextureViewport& SrcViewport,
	const FScreenPassTextureViewport& DestViewport)
{
	return (
		ChangeTextureBasisFromTo(SrcViewport, ETextureBasis::TextureUV, ETextureBasis::ViewportUV) *
		ChangeTextureBasisFromTo(DestViewport, ETextureBasis::ViewportUV, ETextureBasis::TextureUV));
}

// static
FScreenTransform FScreenTransform::SvPositionToViewportUV(const FIntRect& SrcViewport)
{
	return (FScreenTransform::Identity - SrcViewport.Min) / SrcViewport.Size();
}

// static
FScreenTransform FScreenTransform::DispatchThreadIdToViewportUV(const FIntRect& SrcViewport)
{
	return (FScreenTransform::Identity + 0.5f) / SrcViewport.Size();
}

void SetScreenPassPipelineState(FRHICommandList& RHICmdList, const FScreenPassPipelineState& ScreenPassDraw)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = ScreenPassDraw.BlendState;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = ScreenPassDraw.DepthStencilState;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = ScreenPassDraw.VertexDeclaration;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = ScreenPassDraw.VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ScreenPassDraw.PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, ScreenPassDraw.StencilRef);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	FIntPoint InputPosition,
	FIntPoint OutputPosition,
	FIntPoint Size)
{
	const FRDGTextureDesc& InputDesc = InputTexture->Desc;
	const FRDGTextureDesc& OutputDesc = OutputTexture->Desc;

	// Use a hardware copy if formats match.
	if (InputDesc.Format == OutputDesc.Format)
	{
		return AddCopyTexturePass(GraphBuilder, InputTexture, OutputTexture, InputPosition, OutputPosition, Size);
	}

	if (Size == FIntPoint::ZeroValue)
	{
		// Copy entire input texture to output texture.
		Size = InputTexture->Desc.Extent;
	}

	// Don't prime color data if the whole texture is being overwritten.
	const ERenderTargetLoadAction LoadAction = (OutputPosition == FIntPoint::ZeroValue && Size == OutputDesc.Extent)
		? ERenderTargetLoadAction::ENoAction
		: ERenderTargetLoadAction::ELoad;

	const FScreenPassTextureViewport InputViewport(InputDesc.Extent, FIntRect(InputPosition, InputPosition + Size));
	const FScreenPassTextureViewport OutputViewport(OutputDesc.Extent, FIntRect(OutputPosition, OutputPosition + Size));

	TShaderMapRef<FCopyRectPS> PixelShader(View.ShaderMap);

	FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
	Parameters->InputTexture = InputTexture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, LoadAction);

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DrawTexture"), View, OutputViewport, InputViewport, PixelShader, Parameters);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture Input,
	FScreenPassRenderTarget Output)
{
	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	TShaderMapRef<FCopyRectPS> PixelShader(View.ShaderMap);

	FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DrawTexture"), View, OutputViewport, InputViewport, PixelShader, Parameters);
}

class FDownsampleDepthPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDownsampleDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FDownsampleDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER(FVector2f, DestinationTexelSize)
		SHADER_PARAMETER(FVector2f, SourceMaxUV)
		SHADER_PARAMETER(FVector2f, DestinationResolution)
		SHADER_PARAMETER(uint32, DownsampleDepthFilter)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDownsampleDepthPS, "/Engine/Private/DownsampleDepthPixelShader.usf", "Main", SF_Pixel);

void AddDownsampleDepthPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture Input,
	FScreenPassRenderTarget Output,
	EDownsampleDepthFilter DownsampleDepthFilter)
{
	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FDownsampleDepthPS> PixelShader(View.ShaderMap);

	FDownsampleDepthPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleDepthPS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->DepthTexture = Input.Texture;
	PassParameters->DestinationTexelSize = FVector2f(1.0f / OutputViewport.Extent.X, 1.0f / OutputViewport.Extent.X);
	PassParameters->SourceMaxUV = FVector2f((View.ViewRect.Max.X - 0.5f) / InputViewport.Extent.X, (View.ViewRect.Max.Y - 0.5f) / InputViewport.Extent.Y);
	PassParameters->DownsampleDepthFilter = (uint32)DownsampleDepthFilter;

	const int32 DownsampledSizeX = OutputViewport.Rect.Width();
	const int32 DownsampledSizeY = OutputViewport.Rect.Height();
	PassParameters->DestinationResolution = FVector2f(DownsampledSizeX, DownsampledSizeY);

	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(Output.Texture, Output.LoadAction, Output.LoadAction, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	static const TCHAR* kFilterNames[] = {
		TEXT("Point"),
		TEXT("Max"),
		TEXT("CheckerMinMax"),
	};

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("DownsampleDepth(%s) %dx%dx -> %dx%d",
			kFilterNames[int32(DownsampleDepthFilter)],
			InputViewport.Rect.Width(),
			InputViewport.Rect.Height(),
			OutputViewport.Rect.Width(),
			OutputViewport.Rect.Height()),
		View,
		OutputViewport, InputViewport,
		VertexShader, PixelShader,
		DepthStencilState,
		PassParameters);
}