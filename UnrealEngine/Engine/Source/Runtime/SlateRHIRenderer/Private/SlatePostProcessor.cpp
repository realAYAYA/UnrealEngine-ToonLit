// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlatePostProcessor.h"
#include "SlatePostProcessResource.h"
#include "SlateShaders.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"
#include "RendererInterface.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"
#include "HDRHelper.h"
#include "RendererUtils.h"

DECLARE_CYCLE_STAT(TEXT("Slate PostProcessing RT"), STAT_SlatePostProcessingRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate ColorDeficiency RT"), STAT_SlateColorDeficiencyRTTime, STATGROUP_Slate);

int GSlateEnableDeleteUnusedPostProcess = 0;
static FAutoConsoleVariableRef CVarSlateEnableDeleteUnusedPostProcess(
	TEXT("Slate.EnableDeleteUnusedPostProcess"),
	GSlateEnableDeleteUnusedPostProcess,
	TEXT("Greater than zero implies that post process render targets will be deleted when they are not used after n frames.")
);

static const int32 NumIntermediateTargets = 2;

FSlatePostProcessResource* FindSlatePostProcessResource(TArray<FSlatePostProcessResource*>& IntermediateTargetsArray, EPixelFormat PixelFormat)
{
	for (int32 ui = 0; ui < IntermediateTargetsArray.Num(); ++ui)
	{
		if (IntermediateTargetsArray[ui]->GetPixelFormat() == PixelFormat || IntermediateTargetsArray[ui]->GetPixelFormat() == PF_Unknown)
		{
			return IntermediateTargetsArray[ui];
		}
	}

	FSlatePostProcessResource* NewSlatePostProcessResource = new FSlatePostProcessResource(NumIntermediateTargets);
	IntermediateTargetsArray.Add(NewSlatePostProcessResource);
	BeginInitResource(NewSlatePostProcessResource);
	return NewSlatePostProcessResource;
}

FSlatePostProcessor::FSlatePostProcessor()
{
	uint32 MaximumDifferentPixelFormat = 8;
 	for (uint32 ui=0; ui < MaximumDifferentPixelFormat; ++ui)
 	{
 		IntermediateTargetsArray.Add(new FSlatePostProcessResource(NumIntermediateTargets));
 		BeginInitResource(IntermediateTargetsArray[ui]);
 	}
}

FSlatePostProcessor::~FSlatePostProcessor()
{
	// Note this is deleted automatically because it implements FDeferredCleanupInterface.
	for (int32 ui = 0; ui < IntermediateTargetsArray.Num(); ++ui)
	{
		IntermediateTargetsArray[ui]->CleanUp();
	}
 	IntermediateTargetsArray.Empty();
}


// Pixel shader to composite UI over HDR buffer
class FBlitUIToHDRPSBase : public FGlobalShader
{
	DECLARE_TYPE_LAYOUT(FBlitUIToHDRPSBase, NonVirtual);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}

	FBlitUIToHDRPSBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		HDRTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		UITexture.Bind(Initializer.ParameterMap, TEXT("UITexture"));
		UIWriteMaskTexture.Bind(Initializer.ParameterMap, TEXT("UIWriteMaskTexture"));
		UISampler.Bind(Initializer.ParameterMap, TEXT("UISampler"));
		UILevel.Bind(Initializer.ParameterMap, TEXT("UILevel"));
		UILuminance.Bind(Initializer.ParameterMap, TEXT("UILuminance"));
		UITextureSize.Bind(Initializer.ParameterMap, TEXT("UITextureSize"));
	}
	FBlitUIToHDRPSBase() = default;

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* UITextureRHI, FRHITexture* HDRTextureRHI, FRHITexture* UITextureWriteMaskRHI, const FVector2f& InUITextureSize)
	{
		SetTextureParameter(BatchedParameters, HDRTexture, UISampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), HDRTextureRHI);
		SetTextureParameter(BatchedParameters, UITexture, UISampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), UITextureRHI);
		static auto CVarHDRUILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level"));
		static auto CVarHDRUILuminance = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Luminance"));
		SetShaderValue(BatchedParameters, UILevel, CVarHDRUILevel ? CVarHDRUILevel->GetFloat() : 1.0f);
		SetShaderValue(BatchedParameters, UILuminance, CVarHDRUILuminance ? CVarHDRUILuminance->GetFloat() : 300.0f);
		SetShaderValue(BatchedParameters, UITextureSize, InUITextureSize);
		if (UITextureWriteMaskRHI != nullptr)
		{
			SetTextureParameter(BatchedParameters, UIWriteMaskTexture, UITextureWriteMaskRHI);
		}
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("BLIT_UI_TO_HDR"), 1);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/CompositeUIPixelShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("BlitUIToHDRPS");
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, HDRTexture);
	LAYOUT_FIELD(FShaderResourceParameter, UITexture);
	LAYOUT_FIELD(FShaderResourceParameter, UISampler);
	LAYOUT_FIELD(FShaderResourceParameter, UIWriteMaskTexture);
	LAYOUT_FIELD(FShaderParameter, UILevel);
	LAYOUT_FIELD(FShaderParameter, UILuminance);
	LAYOUT_FIELD(FShaderParameter, UITextureSize);
};

IMPLEMENT_TYPE_LAYOUT(FBlitUIToHDRPSBase);

template<uint32 EncodingType>
class FBlitUIToHDRPS : public FBlitUIToHDRPSBase
{
	DECLARE_SHADER_TYPE(FBlitUIToHDRPS, Global);
public:
	FBlitUIToHDRPS() = default;
	FBlitUIToHDRPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FBlitUIToHDRPSBase(Initializer) {}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FBlitUIToHDRPSBase::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCRGB_ENCODING"), EncodingType);
	}
};

IMPLEMENT_SHADER_TYPE(template<>, FBlitUIToHDRPS<0>, FBlitUIToHDRPS::GetSourceFilename(), FBlitUIToHDRPS::GetFunctionName(), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FBlitUIToHDRPS<1>, FBlitUIToHDRPS::GetSourceFilename(), FBlitUIToHDRPS::GetFunctionName(), SF_Pixel);

static void BlitUIToHDRScene(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& RectParams, FTexture2DRHIRef DestTexture, const FIntPoint& AllocatedSize)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlatePostProcessBlitUIToHDR);

	FRHITexture* UITexture = RectParams.PostProcessDest == EPostProcessDestination::DestTexture 
		? RectParams.DestTexture->GetTexture2D() 
		: RectParams.UITarget->GetRHI();

	RHICmdList.Transition(FRHITransitionInfo(UITexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	TRefCountPtr<IPooledRenderTarget> UITargetRTMask;
	if (RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform) && RectParams.PostProcessDest != EPostProcessDestination::DestTexture)
	{
		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		IPooledRenderTarget* RenderTargets[] = { RectParams.UITarget.GetReference() };
		FRenderTargetWriteMask::Decode(RHICmdList, ShaderMap, RenderTargets, UITargetRTMask, TexCreate_None, TEXT("UIRTWriteMask"));
	}
	FRHITexture* UITargetRTMaskTexture = RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform) ? UITargetRTMask->GetRHI() : nullptr;

	// Source is the viewport.  This is the width and height of the viewport backbuffer
	const int32 SrcTextureWidth = RectParams.SourceTextureSize.X;
	const int32 SrcTextureHeight = RectParams.SourceTextureSize.Y;

	// Rect of the viewport
	const FSlateRect& SourceRect = RectParams.SourceRect;

	// Rect of the final destination post process effect (not downsample rect).  This is the area we sample from
	const FSlateRect& DestRect = RectParams.DestRect;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	bool bIsSCRGB = (RectParams.SourceTexture->GetDesc().Format == PF_FloatRGBA);

	TShaderRef<FBlitUIToHDRPSBase> PixelShader;  
	if (bIsSCRGB)
	{
		PixelShader = TShaderMapRef<FBlitUIToHDRPS<1> >(ShaderMap);
	}
	else
	{
		PixelShader = TShaderMapRef<FBlitUIToHDRPS<0> >(ShaderMap);
	}

	TArray<FRHITransitionInfo> RHITransitionInfos =
	{
		FRHITransitionInfo(UITexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics),
		FRHITransitionInfo(RectParams.SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics),
		FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV)
	};


	if (UITargetRTMaskTexture)
	{
		RHITransitionInfos.Add(FRHITransitionInfo(UITargetRTMaskTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
	}

	RHICmdList.Transition(RHITransitionInfos);

	const FVector2f InvSrcTextureSize(1.f / SrcTextureWidth, 1.f / SrcTextureHeight);

	// If using a custom output target, sample the source rect for UV's, else do not as the dest rect may be a subsection
	const FSlateRect& UVRect = RectParams.PostProcessDest == EPostProcessDestination::DestTexture
		? SourceRect
		: DestRect;

	// no guard band is actually needed because GaussianBlurMain already takes ensure that sampling happens inside the rectangle
	const FVector2f UVStart = FVector2f(UVRect.Left - 0.0f, UVRect.Top - 0.0f) * InvSrcTextureSize;
	const FVector2f UVEnd = FVector2f(UVRect.Right + 0.0f, UVRect.Bottom + 0.0f) * InvSrcTextureSize;
	const FVector2f SizeUV = UVEnd - UVStart;

	RHICmdList.SetViewport(0, 0, 0, SrcTextureWidth, SrcTextureHeight, 0.0f);
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("BlitUIToHDR"));
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		SetShaderParametersLegacyPS(RHICmdList, PixelShader, UITexture, RectParams.SourceTexture, UITargetRTMaskTexture, FVector2f(SrcTextureWidth, SrcTextureHeight));

		RendererModule.DrawRectangle(
			RHICmdList,
			0, 0,
			(float)AllocatedSize.X, (float)AllocatedSize.Y,
			UVStart.X, UVStart.Y,
			SizeUV.X, SizeUV.Y,
			DestTexture->GetDesc().Extent,
			FIntPoint(1, 1),
			VertexShader,
			EDRF_Default);
	}
	RHICmdList.EndRenderPass();
}

void FSlatePostProcessor::BlurRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FBlurRectParams& Params, const FPostProcessRectParams& RectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_SlatePostProcessingRTTime);
	check(RHICmdList.IsOutsideRenderPass());

	TArray<FVector4f> WeightsAndOffsets;
	const int32 SampleCount = ComputeBlurWeights(Params.KernelSize, Params.Strength, WeightsAndOffsets);


	const bool bDownsample = Params.DownsampleAmount > 0;

	FIntPoint DestRectSize = RectParams.DestRect.GetSize().IntPoint();
	FIntPoint RequiredSize = bDownsample
										? FIntPoint(FMath::DivideAndRoundUp(DestRectSize.X, Params.DownsampleAmount), FMath::DivideAndRoundUp(DestRectSize.Y, Params.DownsampleAmount))
										: DestRectSize;

	// The max size can get ridiculous with large scale values.  Clamp to size of the backbuffer
	RequiredSize.X = FMath::Min(RequiredSize.X, RectParams.SourceTextureSize.X);
	RequiredSize.Y = FMath::Min(RequiredSize.Y, RectParams.SourceTextureSize.Y);
	
	SCOPED_DRAW_EVENTF(RHICmdList, SlatePostProcess, TEXT("Slate Post Process Blur Background Kernel: %dx%d Size: %dx%d"), SampleCount, SampleCount, RequiredSize.X, RequiredSize.Y);


	const FIntPoint DownsampleSize = RequiredSize;

	EPixelFormat IntermediatePixelFormat = RectParams.SourceTexture->GetDesc().Format;

	bool bIsHDRSource = RectParams.UITarget.IsValid() && RectParams.UITarget->GetRHI() != RectParams.SourceTexture.GetReference();
	if (bIsHDRSource)
	{
		IntermediatePixelFormat = PF_FloatR11G11B10;
	}

	FSlatePostProcessResource* IntermediateTargets = FindSlatePostProcessResource(IntermediateTargetsArray, IntermediatePixelFormat);
	IntermediateTargets->Update(RequiredSize, IntermediatePixelFormat);

	if (bIsHDRSource)
	{
		// in HDR mode, we are going to blur SourceTexture, but still need to take into account the UI already rendered. Blit UI into HDR target
		BlitUIToHDRScene(RHICmdList, RendererModule, RectParams, IntermediateTargets->GetRenderTarget(0), RequiredSize);
	}

	else if(bDownsample)
	{
		DownsampleRect(RHICmdList, RendererModule, RectParams, DownsampleSize, IntermediateTargets);
	}

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

#if 1
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	check(ShaderMap);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FSlatePostProcessBlurPS> PixelShader(ShaderMap);

	const int32 SrcTextureWidth = RectParams.SourceTextureSize.X;
	const int32 SrcTextureHeight = RectParams.SourceTextureSize.Y;

	const int32 DestTextureWidth = IntermediateTargets->GetWidth();
	const int32 DestTextureHeight = IntermediateTargets->GetHeight();

	const FSlateRect& SourceRect = RectParams.SourceRect;
	const FSlateRect& DestRect = RectParams.DestRect;

	FVertexDeclarationRHIRef VertexDecl = GFilterVertexDeclaration.VertexDeclarationRHI;
	check(IsValidRef(VertexDecl));
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)DestTextureWidth, (float)DestTextureHeight, 0.0f);
	
	const FVector2f InvBufferSize = FVector2f(1.0f / (float)DestTextureWidth, 1.0f / (float)DestTextureHeight);
	const FVector2f HalfTexelOffset = FVector2f(0.5f/ (float)DestTextureWidth, 0.5f/ (float)DestTextureHeight);

	for (int32 PassIndex = 0; PassIndex < 2; ++PassIndex)
	{
		// First pass render to the render target with the post process fx
		if (PassIndex == 0)
		{
			FTexture2DRHIRef SourceTexture = (bDownsample || bIsHDRSource) ? IntermediateTargets->GetRenderTarget(0) : RectParams.SourceTexture;
			FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(1);

			RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
			RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

			FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateBlurRectPass0"));
			{
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDecl;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

				PixelShader->SetWeightsAndOffsets(BatchedParameters, WeightsAndOffsets, SampleCount);
				PixelShader->SetTexture(BatchedParameters, SourceTexture, BilinearClamp);

				if (bDownsample || bIsHDRSource)
				{
					PixelShader->SetUVBounds(BatchedParameters, FVector4f(FVector2f::ZeroVector, FVector2f((float)DownsampleSize.X / DestTextureWidth, (float)DownsampleSize.Y / DestTextureHeight) - HalfTexelOffset));
					PixelShader->SetBufferSizeAndDirection(BatchedParameters, InvBufferSize, FVector2f(1.f, 0.f));

					RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);

					RendererModule.DrawRectangle(
						RHICmdList,
						0.f, 0.f,
						(float)DownsampleSize.X, (float)DownsampleSize.Y,
						0, 0,
						(float)DownsampleSize.X, (float)DownsampleSize.Y,
						FIntPoint(DestTextureWidth, DestTextureHeight),
						FIntPoint(DestTextureWidth, DestTextureHeight),
						VertexShader,
						EDRF_Default);
				}
				else
				{
					const FVector2f InvSrcTextureSize(1.f / SrcTextureWidth, 1.f / SrcTextureHeight);

					// If using a custom output target, sample the source rect for UV's, else do not as the dest rect may be a subsection
					const FSlateRect& UVRect = RectParams.PostProcessDest == EPostProcessDestination::DestTexture
						? SourceRect
						: DestRect;

					const FVector2f UVStart = FVector2f(UVRect.Left, UVRect.Top) * InvSrcTextureSize;
					const FVector2f UVEnd = FVector2f(UVRect.Right, UVRect.Bottom) * InvSrcTextureSize;
					const FVector2f SizeUV = UVEnd - UVStart;

					PixelShader->SetUVBounds(BatchedParameters, FVector4f(UVStart, UVEnd));
					PixelShader->SetBufferSizeAndDirection(BatchedParameters, InvSrcTextureSize, FVector2f(1.f, 0.f));

					RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);

					RendererModule.DrawRectangle(
						RHICmdList,
						0.f, 0.f,
						(float)RequiredSize.X, (float)RequiredSize.Y,
						UVStart.X, UVStart.Y,
						SizeUV.X, SizeUV.Y,
						FIntPoint(DestTextureWidth, DestTextureHeight),
						FIntPoint(1, 1),
						VertexShader,
						EDRF_Default);
				}
			}
			RHICmdList.EndRenderPass();
		}
		else
		{
			FTexture2DRHIRef SourceTexture = IntermediateTargets->GetRenderTarget(1);
			FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(0);

			RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
			RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

			FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateBlurRect"));
			{
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDecl;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

				PixelShader->SetWeightsAndOffsets(BatchedParameters, WeightsAndOffsets, SampleCount);
				PixelShader->SetUVBounds(BatchedParameters, FVector4f(FVector2f::ZeroVector, FVector2f((float)DownsampleSize.X / DestTextureWidth, (float)DownsampleSize.Y / DestTextureHeight) - HalfTexelOffset));
				PixelShader->SetTexture(BatchedParameters, SourceTexture, BilinearClamp);
				PixelShader->SetBufferSizeAndDirection(BatchedParameters, InvBufferSize, FVector2f(0.f, 1.f));

				RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);

				RendererModule.DrawRectangle(
					RHICmdList,
					0.f, 0.f,
					(float)DownsampleSize.X, (float)DownsampleSize.Y,
					0.f, 0.f,
					(float)DownsampleSize.X, (float)DownsampleSize.Y,
					FIntPoint(DestTextureWidth, DestTextureHeight),
					FIntPoint(DestTextureWidth, DestTextureHeight),
					VertexShader,
					EDRF_Default);
			}
			RHICmdList.EndRenderPass();
		}	
	}

#endif

	UpsampleRect(RHICmdList, RendererModule, RectParams, DownsampleSize, BilinearClamp, IntermediateTargets);
}

void FSlatePostProcessor::ColorDeficiency(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& RectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateColorDeficiencyRTTime);

	FIntPoint DestRectSize = RectParams.DestRect.GetSize().IntPoint();
	FIntPoint RequiredSize = DestRectSize;

	FSlatePostProcessResource* IntermediateTargets = FindSlatePostProcessResource(IntermediateTargetsArray, RectParams.SourceTexture->GetDesc().Format);

	IntermediateTargets->Update(RequiredSize, RectParams.SourceTexture->GetDesc().Format);

#if 1
	FSamplerStateRHIRef PointClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	check(ShaderMap);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FSlatePostProcessColorDeficiencyPS> PixelShader(ShaderMap);

	const int32 SrcTextureWidth = RectParams.SourceTextureSize.X;
	const int32 SrcTextureHeight = RectParams.SourceTextureSize.Y;

	const int32 DestTextureWidth = IntermediateTargets->GetWidth();
	const int32 DestTextureHeight = IntermediateTargets->GetHeight();

	const FSlateRect& SourceRect = RectParams.SourceRect;
	const FSlateRect& DestRect = RectParams.DestRect;

	FVertexDeclarationRHIRef VertexDecl = GFilterVertexDeclaration.VertexDeclarationRHI;
	check(IsValidRef(VertexDecl));

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)DestTextureWidth, (float)DestTextureHeight, 0.0f);

	// 
	{
		FTexture2DRHIRef SourceTexture = RectParams.SourceTexture;
		FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(0);

		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

		FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("ColorDeficiency"));
		{
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDecl;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

			PixelShader->SetColorRules(BatchedParameters, GSlateColorDeficiencyCorrection, GSlateColorDeficiencyType, GSlateColorDeficiencySeverity);
			PixelShader->SetShowCorrectionWithDeficiency(BatchedParameters, GSlateShowColorDeficiencyCorrectionWithDeficiency);
			PixelShader->SetTexture(BatchedParameters, SourceTexture, PointClamp);

			RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);

			RendererModule.DrawRectangle(
				RHICmdList,
				0.f, 0,
				(float)RequiredSize.X, (float)RequiredSize.Y,
				0.f, 0.f,
				1.f, 1.f,
				FIntPoint(DestTextureWidth, DestTextureHeight),
				FIntPoint(1, 1),
				VertexShader,
				EDRF_Default);
		}
		RHICmdList.EndRenderPass();
	}

	const FIntPoint DownsampleSize = RequiredSize;
	UpsampleRect(RHICmdList, RendererModule, RectParams, DownsampleSize, PointClamp, IntermediateTargets);

#endif
}

void FSlatePostProcessor::ReleaseRenderTargets()
{
	check(IsInGameThread());
	// Only release the resource not delete it.  Deleting it could cause issues on any RHI thread
	for (int32 ui = 0; ui < IntermediateTargetsArray.Num(); ++ui)
	{
		BeginReleaseResource(IntermediateTargetsArray[ui]);
	}
}


void FSlatePostProcessor::DownsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize, FSlatePostProcessResource* IntermediateTargets)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlatePostProcessDownsample);

	// Source is the viewport.  This is the width and height of the viewport backbuffer
	const int32 SrcTextureWidth = Params.SourceTextureSize.X;
	const int32 SrcTextureHeight = Params.SourceTextureSize.Y;

	// Dest is the destination quad for the downsample
	const int32 DestTextureWidth = IntermediateTargets->GetWidth();
	const int32 DestTextureHeight = IntermediateTargets->GetHeight();

	// Rect of the viewport
	const FSlateRect& SourceRect = Params.SourceRect;

	// Rect of the final destination post process effect (not downsample rect).  This is the area we sample from
	const FSlateRect& DestRect = Params.DestRect;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FTexture2DRHIRef DestTexture = IntermediateTargets->GetRenderTarget(0);

	// Downsample and store in intermediate texture
	{
		TShaderMapRef<FSlatePostProcessDownsamplePS> PixelShader(ShaderMap);

		RHICmdList.Transition(FRHITransitionInfo(Params.SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

		const FVector2f InvSrcTextureSize(1.f/SrcTextureWidth, 1.f/SrcTextureHeight);

		// If using a custom output target, sample the source rect for UV's, else do not as the dest rect may be a subsection
		const FSlateRect& UVRect = Params.PostProcessDest == EPostProcessDestination::DestTexture
			? SourceRect
			: DestRect;

		const FVector2f UVStart = FVector2f(UVRect.Left, UVRect.Top) * InvSrcTextureSize;
		const FVector2f UVEnd = FVector2f(UVRect.Right, UVRect.Bottom) * InvSrcTextureSize;
		const FVector2f SizeUV = UVEnd - UVStart;
		
		RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)DestTextureWidth, (float)DestTextureHeight, 0.0f);
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

		FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("DownsampleRect"));
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

			PixelShader->SetShaderParams(BatchedParameters, FShaderParams::MakePixelShaderParams(FVector4f(InvSrcTextureSize.X, InvSrcTextureSize.Y, 0, 0)));
			PixelShader->SetUVBounds(BatchedParameters, FVector4f(UVStart, UVEnd));
			PixelShader->SetTexture(BatchedParameters, Params.SourceTexture, BilinearClamp);

			RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);

			RendererModule.DrawRectangle(
				RHICmdList,
				0.f, 0.f,
				(float)DownsampleSize.X, (float)DownsampleSize.Y,
				UVStart.X, UVStart.Y,
				SizeUV.X, SizeUV.Y,
				FIntPoint(DestTextureWidth, DestTextureHeight),
				FIntPoint(1, 1),
				VertexShader,
				EDRF_Default);
		}
		RHICmdList.EndRenderPass();
	}
	
	// Testing only
#if 0
	UpsampleRect(RHICmdList, RendererModule, Params, DownsampleSize, IntermediateTargets);
#endif
}

void FSlatePostProcessor::UpsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize, FSamplerStateRHIRef& Sampler, FSlatePostProcessResource* IntermediateTargets)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlatePostProcessUpsample);

	const FVector4f Zero(0.f, 0.f, 0.f, 0.f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = Params.CornerRadius == Zero ? TStaticBlendState<>::GetRHI() : TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	// Original source texture is now the destination texture
	FTexture2DRHIRef DestTexture = Params.PostProcessDest == EPostProcessDestination::DestTexture && Params.DestTexture 
		? Params.DestTexture 
		: Params.SourceTexture;
	const int32 DestTextureWidth = DestTexture->GetSizeX();
	const int32 DestTextureHeight = DestTexture->GetSizeY();

	const int32 DownsampledWidth = DownsampleSize.X;
	const int32 DownsampledHeight = DownsampleSize.Y;

	// Source texture is the texture that was originally downsampled
	FTexture2DRHIRef SrcTexture = IntermediateTargets->GetRenderTarget(0);
	const int32 SrcTextureWidth = IntermediateTargets->GetWidth();
	const int32 SrcTextureHeight = IntermediateTargets->GetHeight();

	const FSlateRect& SourceRect = Params.SourceRect;
	const FSlateRect& DestRect = Params.DestRect;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)DestTextureWidth, (float)DestTextureHeight, 0.0f);

	// Perform Writable transitions first

	TArray<FRHITransitionInfo> RHITransitionInfos =
	{
		FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics),
		FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV)
	};

	FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);

	bool bHasMRT = false;
	bool bIsSCRGB = false;

	FRHITexture* UITargetTexture = Params.UITarget.IsValid() ? Params.UITarget->GetRHI() : nullptr;
	UITargetTexture = Params.PostProcessDest == EPostProcessDestination::DestTexture && Params.DestTexture 
		? Params.DestTexture->GetTexture2D() 
		: UITargetTexture;

	if (UITargetTexture != nullptr && DestTexture != UITargetTexture)
	{
		RPInfo.ColorRenderTargets[1].RenderTarget = UITargetTexture;
		RPInfo.ColorRenderTargets[1].ArraySlice = -1;
		RPInfo.ColorRenderTargets[1].Action = ERenderTargetActions::Load_Store;
		bHasMRT = true;
		bIsSCRGB = (DestTexture->GetDesc().Format == PF_FloatRGBA);
		RHITransitionInfos.Add(FRHITransitionInfo(UITargetTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
	}

	RHICmdList.Transition(RHITransitionInfos);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("UpsampleRect"));
	{
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		if (Params.RestoreStateFunc)
		{
			// This can potentially end and restart a renderpass.
			// #todo refactor so that we only start one renderpass here. Right now RestoreStateFunc may call UpdateScissorRect which requires an open renderpass.
			Params.RestoreStateFunc(RHICmdList, GraphicsPSOInit, RPInfo);
		}

		TShaderRef<FSlateElementPS> PixelShader;
		if (bHasMRT)
		{
			if (bIsSCRGB)
			{
				PixelShader = TShaderMapRef<FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::HDR_SCRGB> >(ShaderMap);
			}
			else
			{
				PixelShader = TShaderMapRef<FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::HDR_PQ10> >(ShaderMap);
			}
		}
		else
		{
			PixelShader = TShaderMapRef<FSlatePostProcessUpsamplePS<ESlatePostProcessUpsamplePSPermutation::SDR> >(ShaderMap);
		}

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, Params.StencilRef);

		const FVector2f SizeUV(
			DownsampledWidth == SrcTextureWidth ? 1.0f : (DownsampledWidth / (float)SrcTextureWidth) - (1.0f / (float)SrcTextureWidth),
			DownsampledHeight == SrcTextureHeight ? 1.0f : (DownsampledHeight / (float)SrcTextureHeight) - (1.0f / (float)SrcTextureHeight)
			);

		const FVector2f Size(DestRect.Right - DestRect.Left, DestRect.Bottom - DestRect.Top);
		FShaderParams ShaderParams = FShaderParams::MakePixelShaderParams(FVector4f(Size, SizeUV), Params.CornerRadius);

		FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
		PixelShader->SetShaderParams(BatchedParameters, ShaderParams);
		PixelShader->SetTexture(BatchedParameters, SrcTexture, Sampler);
		RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);


		RendererModule.DrawRectangle(RHICmdList,
			DestRect.Left, DestRect.Top,
			Size.X, Size.Y,
			0, 0,
			SizeUV.X, SizeUV.Y,
			DestTexture->GetSizeXY(),
			FIntPoint(1, 1),
			VertexShader,
			EDRF_Default);
	}
	RHICmdList.EndRenderPass();

}
#define BILINEAR_FILTER_METHOD 1

#if !BILINEAR_FILTER_METHOD

static int32 ComputeWeights(int32 KernelSize, float Sigma, TArray<FVector4f>& OutWeightsAndOffsets)
{
	OutWeightsAndOffsets.AddUninitialized(KernelSize / 2 + 1);

	int32 SampleIndex = 0;
	for (int32 X = 0; X < KernelSize; X += 2)
	{
		float Dist = X;
		FVector4f WeightAndOffset;
		WeightAndOffset.X = (1.0f / FMath::Sqrt(2 * PI*Sigma*Sigma))*FMath::Exp(-(Dist*Dist) / (2 * Sigma*Sigma));
		WeightAndOffset.Y = Dist;

		Dist = X + 1;
		WeightAndOffset.Z = (1.0f / FMath::Sqrt(2 * PI*Sigma*Sigma))*FMath::Exp(-(Dist*Dist) / (2 * Sigma*Sigma));
		WeightAndOffset.W = Dist;

		OutWeightsAndOffsets[SampleIndex] = WeightAndOffset;

		++SampleIndex;
	}

	return KernelSize;
};

#else

static float GetWeight(float Dist, float Strength)
{
	// from https://en.wikipedia.org/wiki/Gaussian_blur
	float Strength2 = Strength*Strength;
	return (1.0f / FMath::Sqrt(2 * PI*Strength2))*FMath::Exp(-(Dist*Dist) / (2 * Strength2));
}

static FVector2f GetWeightAndOffset(float Dist, float Sigma)
{
	float Offset1 = Dist;
	float Weight1 = GetWeight(Offset1, Sigma);

	float Offset2 = Dist + 1;
	float Weight2 = GetWeight(Offset2, Sigma);

	float TotalWeight = Weight1 + Weight2;

	float Offset = 0;
	if (TotalWeight > 0)
	{
		Offset = (Weight1*Offset1 + Weight2*Offset2) / TotalWeight;
	}


	return FVector2f(TotalWeight, Offset);
}

static int32 ComputeWeights(int32 KernelSize, float Sigma, TArray<FVector4f>& OutWeightsAndOffsets)
{
	int32 NumSamples = FMath::DivideAndRoundUp(KernelSize, 2);

	// We need half of the sample count array because we're packing two samples into one float4

	OutWeightsAndOffsets.AddUninitialized(NumSamples%2 == 0 ? NumSamples / 2 : NumSamples/2+1);

	OutWeightsAndOffsets[0] = FVector4f(FVector2f(GetWeight(0,Sigma), 0), GetWeightAndOffset(1, Sigma) );
	int32 SampleIndex = 1;
	for (int32 X = 3; X < KernelSize; X += 4)
	{
		OutWeightsAndOffsets[SampleIndex] = FVector4f(GetWeightAndOffset((float)X, Sigma), GetWeightAndOffset((float)(X + 2), Sigma));

		++SampleIndex;
	}

	return NumSamples;
};

#endif

int32 FSlatePostProcessor::ComputeBlurWeights(int32 KernelSize, float StdDev, TArray<FVector4f>& OutWeightsAndOffsets)
{
	return ComputeWeights(KernelSize, StdDev, OutWeightsAndOffsets);
}

void FSlatePostProcessor::TickPostProcessResources()
{
	if (GSlateEnableDeleteUnusedPostProcess > 0)
	{
		check(IsInRenderingThread());

		for (TArray<FSlatePostProcessResource*>::TIterator It = IntermediateTargetsArray.CreateIterator(); It; ++It)
		{
			if (GFrameCounter - (*It)->GetFrameUsed() > GSlateEnableDeleteUnusedPostProcess)
			{
				(*It)->CleanUp();
				It.RemoveCurrent();
			}
		}
	}
}