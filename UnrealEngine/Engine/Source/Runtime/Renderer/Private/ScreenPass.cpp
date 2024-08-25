// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScreenPass.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "EngineGlobals.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RendererModule.h"
#include "RenderGraphUtils.h"
#include "ScenePrivate.h"
#include "SystemTextures.h"
#include "UnrealClient.h"

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

// static
FScreenPassTexture FScreenPassTexture::CopyFromSlice(FRDGBuilder& GraphBuilder, const FScreenPassTextureSlice& ScreenTextureSlice)
{
	if (!ScreenTextureSlice.TextureSRV)
	{
		return FScreenPassTexture(nullptr, ScreenTextureSlice.ViewRect);
	}
	else if (!ScreenTextureSlice.TextureSRV->Desc.Texture->Desc.IsTextureArray())
	{
		return FScreenPassTexture(ScreenTextureSlice.TextureSRV->Desc.Texture, ScreenTextureSlice.ViewRect);
	}

	FRDGTextureDesc Desc = ScreenTextureSlice.TextureSRV->Desc.Texture->Desc;
	Desc.Dimension = ETextureDimension::Texture2D;
	Desc.ArraySize = 1;

	FRDGTextureRef NewTexture = GraphBuilder.CreateTexture(Desc, TEXT("CopyToScreenPassTexture2D"));

	FRHICopyTextureInfo CopyInfo;
	CopyInfo.SourceSliceIndex = ScreenTextureSlice.TextureSRV->Desc.FirstArraySlice;
	CopyInfo.NumMips = ScreenTextureSlice.TextureSRV->Desc.Texture->Desc.NumMips;

	AddCopyTexturePass(
		GraphBuilder,
		ScreenTextureSlice.TextureSRV->Desc.Texture,
		NewTexture,
		CopyInfo);

	return FScreenPassTexture(NewTexture, ScreenTextureSlice.ViewRect);
}

// static
FScreenPassTextureSlice FScreenPassTextureSlice::CreateFromScreenPassTexture(FRDGBuilder& GraphBuilder, const FScreenPassTexture& ScreenTexture)
{
	if (!ScreenTexture.Texture)
	{
		return FScreenPassTextureSlice(nullptr, ScreenTexture.ViewRect);
	}

	return FScreenPassTextureSlice(GraphBuilder.CreateSRV(FRDGTextureSRVDesc(ScreenTexture.Texture)), ScreenTexture.ViewRect);
}

FScreenPassRenderTarget FScreenPassRenderTarget::CreateFromInput(
	FRDGBuilder& GraphBuilder,
	FScreenPassTexture Input,
	ERenderTargetLoadAction OutputLoadAction,
	const TCHAR* OutputName)
{
	check(Input.IsValid());

	FRDGTextureDesc OutputDesc = Input.Texture->Desc;
	OutputDesc.Reset();

	return FScreenPassRenderTarget(GraphBuilder.CreateTexture(OutputDesc, OutputName), Input.ViewRect, OutputLoadAction);
}

FScreenPassRenderTarget FScreenPassRenderTarget::CreateViewFamilyOutput(FRDGTextureRef ViewFamilyTexture, const FViewInfo& View)
{
	const FIntRect ViewRect = View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::RawOutput ? View.ViewRect : View.UnscaledViewRect;

	ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ENoAction;

	if (!View.IsFirstInFamily() || View.Family->bAdditionalViewFamily)
	{
		LoadAction = ERenderTargetLoadAction::ELoad;
	}
	else if (ViewRect.Min != FIntPoint::ZeroValue || ViewRect.Size() != ViewFamilyTexture->Desc.Extent)
	{
		LoadAction = ERenderTargetLoadAction::EClear;
	}

	return FScreenPassRenderTarget(
		ViewFamilyTexture,
		// Raw output mode uses the original view rect. Otherwise the final unscaled rect is used.
		ViewRect,
		// First view clears the view family texture; all remaining views load.
		LoadAction);
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

void DrawScreenPass_PostSetup(
	FRHICommandList& RHICmdList,
	const FScreenPassViewInfo& ViewInfo,
	const FScreenPassTextureViewport& OutputViewport,
	const FScreenPassTextureViewport& InputViewport,
	const FScreenPassPipelineState& PipelineState,
	EScreenPassDrawFlags Flags)
{
	const FIntRect InputRect = InputViewport.Rect;
	const FIntPoint InputSize = InputViewport.Extent;
	const FIntRect OutputRect = OutputViewport.Rect;
	const FIntPoint OutputSize = OutputViewport.Rect.Size();

	FIntPoint LocalOutputPos(FIntPoint::ZeroValue);
	FIntPoint LocalOutputSize(OutputSize);
	EDrawRectangleFlags DrawRectangleFlags = EDRF_UseTriangleOptimization;

	const bool bUseHMDHiddenAreaMask = EnumHasAllFlags(Flags, EScreenPassDrawFlags::AllowHMDHiddenAreaMask) && ViewInfo.bHMDHiddenAreaMaskActive;

	DrawPostProcessPass(
		RHICmdList,
		LocalOutputPos.X, LocalOutputPos.Y, LocalOutputSize.X, LocalOutputSize.Y,
		InputRect.Min.X, InputRect.Min.Y, InputRect.Width(), InputRect.Height(),
		OutputSize,
		InputSize,
		PipelineState.VertexShader,
		ViewInfo.StereoViewIndex,
		bUseHMDHiddenAreaMask,
		DrawRectangleFlags);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	FIntPoint InputPosition,
	FIntPoint InputSize,
	FIntPoint OutputPosition,
	FIntPoint OutputSize)
{
	const FRDGTextureDesc& InputDesc = InputTexture->Desc;
	const FRDGTextureDesc& OutputDesc = OutputTexture->Desc;

	// Use a hardware copy if formats and sizes match.
	if (InputDesc.Format == OutputDesc.Format && InputSize == OutputSize)
	{
		return AddCopyTexturePass(GraphBuilder, InputTexture, OutputTexture, InputPosition, OutputPosition, InputSize);
	}

	if (InputSize == FIntPoint::ZeroValue)
	{
		// Copy entire input texture to output texture.
		InputSize = InputTexture->Desc.Extent;
	}

	// Don't prime color data if the whole texture is being overwritten.
	const ERenderTargetLoadAction LoadAction = (OutputPosition == FIntPoint::ZeroValue && InputSize == OutputDesc.Extent)
		? ERenderTargetLoadAction::ENoAction
		: ERenderTargetLoadAction::ELoad;

	const FScreenPassTextureViewport InputViewport(InputDesc.Extent, FIntRect(InputPosition, InputPosition + InputSize));
	const FScreenPassTextureViewport OutputViewport(OutputDesc.Extent, FIntRect(OutputPosition, OutputPosition + OutputSize));

	TShaderMapRef<FCopyRectPS> PixelShader(static_cast<const FViewInfo&>(View).ShaderMap);

	FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
	Parameters->InputTexture = InputTexture;
	Parameters->InputSampler = TStaticSamplerState<>::GetRHI();
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, LoadAction);

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DrawTexture"), View, OutputViewport, InputViewport, PixelShader, Parameters);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	FIntPoint InputPosition,
	FIntPoint OutputPosition,
	FIntPoint Size)
{
	AddDrawTexturePass(
		GraphBuilder,
		View,
		InputTexture,
		OutputTexture,
		InputPosition,
		Size,
		OutputPosition,
		Size);
}

void AddDrawTexturePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FScreenPassTexture Input,
	FScreenPassRenderTarget Output)
{
	const FScreenPassTextureViewport InputViewport(Input);
	const FScreenPassTextureViewport OutputViewport(Output);

	TShaderMapRef<FCopyRectPS> PixelShader(static_cast<const FViewInfo&>(View).ShaderMap);

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

	class FOutputMinAndMaxDepth : SHADER_PERMUTATION_BOOL("OUTPUT_MIN_AND_MAX_DEPTH");

	using FPermutationDomain = TShaderPermutationDomain<FOutputMinAndMaxDepth>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER(FVector2f, DstToSrcPixelScale)
		SHADER_PARAMETER(FVector2f, SourceMaxUV)
		SHADER_PARAMETER(FVector2f, DestinationResolution)
		SHADER_PARAMETER(uint32, DownsampleDepthFilter)
		SHADER_PARAMETER(FIntVector4, DstPixelCoordMinAndMax)
		SHADER_PARAMETER(FIntVector4, SrcPixelCoordMinAndMax)
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

	const bool bIsMinAndMaxDepthFilter = DownsampleDepthFilter == EDownsampleDepthFilter::MinAndMaxDepth;
	FDownsampleDepthPS::FPermutationDomain Permutation;
	Permutation.Set<FDownsampleDepthPS::FOutputMinAndMaxDepth>(bIsMinAndMaxDepthFilter ? 1 : 0);
	TShaderMapRef<FDownsampleDepthPS> PixelShader(View.ShaderMap, Permutation);

	// The lower right corner pixel whose coordinate is max considered excluded https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d11-rect
	// That is why we subtract -1 from the maximum value of the source viewport.

	FDownsampleDepthPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleDepthPS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->DepthTexture = Input.Texture;
	PassParameters->DstToSrcPixelScale = FVector2f(float(InputViewport.Extent.X) / float(OutputViewport.Extent.X), float(InputViewport.Extent.Y) / float(OutputViewport.Extent.Y));
	PassParameters->SourceMaxUV = FVector2f((float(View.ViewRect.Max.X) -1.0f - 0.51f) / InputViewport.Extent.X, (float(View.ViewRect.Max.Y) - 1.0f - 0.51f) / InputViewport.Extent.Y);
	PassParameters->DownsampleDepthFilter = (uint32)DownsampleDepthFilter;

	const int32 DownsampledSizeX = OutputViewport.Rect.Width();
	const int32 DownsampledSizeY = OutputViewport.Rect.Height();
	PassParameters->DestinationResolution = FVector2f(DownsampledSizeX, DownsampledSizeY);

	PassParameters->DstPixelCoordMinAndMax = FIntVector4(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, OutputViewport.Rect.Max.X-1, OutputViewport.Rect.Max.Y-1);
	PassParameters->SrcPixelCoordMinAndMax = FIntVector4( InputViewport.Rect.Min.X,  InputViewport.Rect.Min.Y,  InputViewport.Rect.Max.X-1,  InputViewport.Rect.Max.Y-1);

	FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	if (bIsMinAndMaxDepthFilter)
	{
		DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, Output.LoadAction);
	}
	else
	{
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(Output.Texture, Output.LoadAction, Output.LoadAction, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}

	static const TCHAR* kFilterNames[] = {
		TEXT("Point"),
		TEXT("Max"),
		TEXT("CheckerMinMax"),
		TEXT("MinAndMaxDepth"),
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