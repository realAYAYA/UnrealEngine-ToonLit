// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/
#include "SceneCaptureRendering.h"
#include "Containers/ArrayView.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/MemStack.h"
#include "EngineDefines.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Engine/Scene.h"
#include "SceneInterface.h"
#include "LegacyScreenPercentageDriver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "Shader.h"
#include "TextureResource.h"
#include "SceneUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "RendererModule.h"
#include "Rendering/MotionVectorSimulation.h"
#include "SceneViewExtension.h"
#include "GenerateMips.h"
#include "RectLightTexture.h"
#include "Materials/MaterialRenderProxy.h"
#include "Rendering/CustomRenderPass.h"

bool GSceneCaptureAllowRenderInMainRenderer = true;
static FAutoConsoleVariableRef CVarSceneCaptureAllowRenderInMainRenderer(
	TEXT("r.SceneCapture.AllowRenderInMainRenderer"),
	GSceneCaptureAllowRenderInMainRenderer,
	TEXT("Whether to allow SceneDepth & DeviceDepth scene capture to render in the main renderer as an optimization.\n")
	TEXT("0: render as an independent renderer.\n")
	TEXT("1: render as part of the main renderer if Render in Main Renderer is enabled on scene capture component.\n"),
	ECVF_Scalability);

#if WITH_EDITOR
// All scene captures on the given render thread frame will be dumped
uint32 GDumpSceneCaptureMemoryFrame = INDEX_NONE;
void DumpSceneCaptureMemory()
{
	ENQUEUE_RENDER_COMMAND(DumpSceneCaptureMemory)(
		[](FRHICommandList& RHICmdList)
		{
			GDumpSceneCaptureMemoryFrame = GFrameNumberRenderThread;
		});
}

FAutoConsoleCommand CmdDumpSceneCaptureViewState(
	TEXT("r.SceneCapture.DumpMemory"),
	TEXT("Editor specific command to dump scene capture memory to log"),
	FConsoleCommandDelegate::CreateStatic(DumpSceneCaptureMemory)
);
#endif  // WITH_EDITOR

/** A pixel shader for capturing a component of the rendered scene for a scene capture.*/
class FSceneCapturePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSceneCapturePS);
	SHADER_USE_PARAMETER_STRUCT(FSceneCapturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	enum class ESourceMode : uint32
	{
		ColorAndOpacity,
		ColorNoAlpha,
		ColorAndSceneDepth,
		SceneDepth,
		DeviceDepth,
		Normal,
		BaseColor,
		MAX
	};

	class FSourceModeDimension : SHADER_PERMUTATION_ENUM_CLASS("SOURCE_MODE", ESourceMode);
	class FEnable128BitRT : SHADER_PERMUTATION_BOOL("ENABLE_128_BIT");
	using FPermutationDomain = TShaderPermutationDomain<FSourceModeDimension, FEnable128BitRT>;

	static FPermutationDomain GetPermutationVector(ESceneCaptureSource CaptureSource, bool bUse128BitRT, bool bIsMobilePlatform)
	{
		ESourceMode SourceMode = ESourceMode::MAX;
		switch (CaptureSource)
		{
		case SCS_SceneColorHDR:
			SourceMode = ESourceMode::ColorAndOpacity;
			break;
		case SCS_SceneColorHDRNoAlpha:
			SourceMode = ESourceMode::ColorNoAlpha;
			break;
		case SCS_SceneColorSceneDepth:
			SourceMode = ESourceMode::ColorAndSceneDepth;
			break;
		case SCS_SceneDepth:
			SourceMode = ESourceMode::SceneDepth;
			break;
		case SCS_DeviceDepth:
			SourceMode = ESourceMode::DeviceDepth;
			break;
		case SCS_Normal:
			SourceMode = ESourceMode::Normal;
			break;
		case SCS_BaseColor:
			SourceMode = ESourceMode::BaseColor;
			break;
		default:
			checkf(false, TEXT("SceneCaptureSource not implemented."));
		}

		if (bIsMobilePlatform && (SourceMode == ESourceMode::Normal || SourceMode == ESourceMode::BaseColor))
		{
			SourceMode = ESourceMode::ColorAndOpacity;
		}
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FSourceModeDimension>(SourceMode);
		PermutationVector.Set<FEnable128BitRT>(bUse128BitRT);
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		auto SourceModeDim = PermutationVector.Get<FSourceModeDimension>();
		bool bPlatformRequiresExplicit128bitRT = FDataDrivenShaderPlatformInfo::GetRequiresExplicit128bitRT(Parameters.Platform);
		return (!PermutationVector.Get<FEnable128BitRT>() || bPlatformRequiresExplicit128bitRT) && (!IsMobilePlatform(Parameters.Platform) || (SourceModeDim != ESourceMode::Normal && SourceModeDim != ESourceMode::BaseColor));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		static const TCHAR* ShaderSourceModeDefineName[] =
		{
			TEXT("SOURCE_MODE_SCENE_COLOR_AND_OPACITY"),
			TEXT("SOURCE_MODE_SCENE_COLOR_NO_ALPHA"),
			TEXT("SOURCE_MODE_SCENE_COLOR_SCENE_DEPTH"),
			TEXT("SOURCE_MODE_SCENE_DEPTH"),
			TEXT("SOURCE_MODE_DEVICE_DEPTH"),
			TEXT("SOURCE_MODE_NORMAL"),
			TEXT("SOURCE_MODE_BASE_COLOR")
		};
		static_assert(UE_ARRAY_COUNT(ShaderSourceModeDefineName) == (uint32)ESourceMode::MAX, "ESourceMode doesn't match define table.");

		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 SourceModeIndex = static_cast<uint32>(PermutationVector.Get<FSourceModeDimension>());
		OutEnvironment.SetDefine(ShaderSourceModeDefineName[SourceModeIndex], 1u);

		if (PermutationVector.Get<FEnable128BitRT>())
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
		}

		if (IsMobilePlatform(Parameters.Platform))
		{
			OutEnvironment.FullPrecisionInPS = 1;
		}

	}
};

IMPLEMENT_GLOBAL_SHADER(FSceneCapturePS, "/Engine/Private/SceneCapturePixelShader.usf", "Main", SF_Pixel);

static bool CaptureNeedsSceneColor(ESceneCaptureSource CaptureSource)
{
	return CaptureSource != SCS_FinalColorLDR && CaptureSource != SCS_FinalColorHDR && CaptureSource != SCS_FinalToneCurveHDR;
}

static TFunction<void(FRHICommandList& RHICmdList)> CopyCaptureToTargetSetViewportFn = [](FRHICommandList& RHICmdList) {};

void CopySceneCaptureComponentToTarget(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef ViewFamilyTexture,
	const FSceneViewFamily& ViewFamily,
	TConstArrayView<FViewInfo> Views)
{
	TArray<const FViewInfo*> ViewPtrArray;
	for (const FViewInfo& View : Views)
	{
		ViewPtrArray.Add(&View);
	}
	CopySceneCaptureComponentToTarget(GraphBuilder, SceneTextures, ViewFamilyTexture, ViewFamily, ViewPtrArray);
}

void CopySceneCaptureComponentToTarget(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef ViewFamilyTexture,
	const FSceneViewFamily& ViewFamily,
	const TArray<const FViewInfo*>& Views)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	const bool bForwardShadingEnabled = IsForwardShadingEnabled(ViewFamily.GetShaderPlatform());
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = *Views[ViewIndex];

		// If view has its own scene capture setting, use it over view family setting
		ESceneCaptureSource SceneCaptureSource = View.CustomRenderPass ? View.CustomRenderPass->GetSceneCaptureSource() : ViewFamily.SceneCaptureSource;
		if (bForwardShadingEnabled && (SceneCaptureSource == SCS_Normal || SceneCaptureSource == SCS_BaseColor))
		{
			SceneCaptureSource = SCS_SceneColorHDR;
		}
		if (!CaptureNeedsSceneColor(SceneCaptureSource))
		{
			continue;
		}

		RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneComponent_View[%d]", SceneCaptureSource);

		bool bIsCompositing = false;
		if (SceneCaptureSource == SCS_SceneColorHDR && ViewFamily.SceneCaptureCompositeMode == SCCM_Composite)
		{
			// Blend with existing render target color. Scene capture color is already pre-multiplied by alpha.
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
			bIsCompositing = true;
		}
		else if (SceneCaptureSource == SCS_SceneColorHDR && ViewFamily.SceneCaptureCompositeMode == SCCM_Additive)
		{
			// Add to existing render target color. Scene capture color is already pre-multiplied by alpha.
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
			bIsCompositing = true;
		}
		else
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		}

		const bool bUse128BitRT = PlatformRequires128bitRT(ViewFamilyTexture->Desc.Format);
		const FSceneCapturePS::FPermutationDomain PixelPermutationVector = FSceneCapturePS::GetPermutationVector(SceneCaptureSource, bUse128BitRT, IsMobilePlatform(ViewFamily.GetShaderPlatform()));

		FSceneCapturePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSceneCapturePS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(ViewFamily.GetFeatureLevel());
		PassParameters->RenderTargets[0] = FRenderTargetBinding(ViewFamilyTexture, bIsCompositing ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::ENoAction);

		TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FSceneCapturePS> PixelShader(View.ShaderMap, PixelPermutationVector);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("View(%d)", ViewIndex),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, GraphicsPSOInit, VertexShader, PixelShader, &View] (FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer LocalGraphicsPSOInit = GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(LocalGraphicsPSOInit);
			SetGraphicsPipelineState(RHICmdList, LocalGraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
			
			CopyCaptureToTargetSetViewportFn(RHICmdList);

			DrawRectangle(
				RHICmdList,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.UnconstrainedViewRect.Size(),
				View.GetSceneTexturesConfig().Extent,
				VertexShader,
				EDRF_UseTriangleOptimization);
		});
	}
}

void CopySceneCaptureComponentToTarget(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef ViewFamilyTexture,
	const FSceneViewFamily& ViewFamily,
	TConstStridedView<FSceneView> Views)
{
	const FSceneView& View = Views[0];

	check(View.bIsViewInfo);
	const FMinimalSceneTextures& SceneTextures = static_cast<const FViewInfo&>(View).GetSceneTextures();

	TConstArrayView<FViewInfo> ViewInfos = MakeArrayView(static_cast<const FViewInfo*>(&Views[0]), Views.Num());

	CopySceneCaptureComponentToTarget(GraphBuilder, SceneTextures, ViewFamilyTexture, ViewFamily, ViewInfos);
}

static void UpdateSceneCaptureContentDeferred_RenderThread(
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderer* SceneRenderer, 
	FRenderTarget* RenderTarget, 
	FTexture* RenderTargetTexture, 
	const FString& EventName, 
	const FRHICopyTextureInfo& CopyInfo,
	bool bGenerateMips,
	const FGenerateMipsParams& GenerateMipsParams,
	bool bClearRenderTarget,
	bool bOrthographicCamera
	)
{
	SceneRenderer->RenderThreadBegin(RHICmdList);

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	const ERHIFeatureLevel::Type FeatureLevel = SceneRenderer->FeatureLevel;

#if WANTS_DRAW_MESH_EVENTS
	SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("SceneCapture %s"), *EventName);
	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SceneCapture %s", *EventName), ERDGBuilderFlags::AllowParallelExecute);
#else
	SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContent_RenderThread);
	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SceneCapture"), ERDGBuilderFlags::AllowParallelExecute);
#endif

	{
		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("SceneCaptureTarget"));
		FRDGTextureRef ShaderResourceTexture = RegisterExternalTexture(GraphBuilder, RenderTargetTexture->TextureRHI, TEXT("SceneCaptureTexture"));

		if (bClearRenderTarget)
		{
			AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, SceneRenderer->Views[0].UnscaledViewRect);
		}

		const FIntRect CopyDestRect = CopyInfo.GetDestRect();

		if (!CopyDestRect.IsEmpty())
		{
			CopyCaptureToTargetSetViewportFn = [CopyDestRect](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				RHICmdList.SetViewport
				(
					float(CopyDestRect.Min.X),
					float(CopyDestRect.Min.Y),
					0.0f,
					float(CopyDestRect.Max.X),
					float(CopyDestRect.Max.Y),
					1.0f
				);
			};
		}
		else
		{
			CopyCaptureToTargetSetViewportFn = [](FRHICommandList& RHICmdList) {};
		}


		// Disable occlusion queries when in orthographic mode
		if (bOrthographicCamera)
		{
			FViewInfo& View = SceneRenderer->Views[0];
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}

		// Render the scene normally
		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderScene);
			SceneRenderer->Render(GraphBuilder);
		}

		if (bGenerateMips)
		{
			FGenerateMips::Execute(GraphBuilder, SceneRenderer->FeatureLevel, TargetTexture, GenerateMipsParams);
		}

		AddCopyTexturePass(GraphBuilder, TargetTexture, ShaderResourceTexture, CopyInfo);

		GraphBuilder.Execute();
	}

	SceneRenderer->RenderThreadEnd(RHICmdList);
}

void UpdateSceneCaptureContentMobile_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	FRenderTarget* RenderTarget,
	FTexture* RenderTargetTexture,
	const FString& EventName,
	const FRHICopyTextureInfo& CopyInfo,
	bool bGenerateMips,
	const FGenerateMipsParams& GenerateMipsParams)
{
	SceneRenderer->RenderThreadBegin(RHICmdList);

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);

#if WANTS_DRAW_MESH_EVENTS
	SCOPED_DRAW_EVENTF(RHICmdList, SceneCaptureMobile, TEXT("SceneCaptureMobile %s"), *EventName);
	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SceneCaptureMobile %s", *EventName));
#else
	SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContentMobile_RenderThread);
	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SceneCaptureMobile"));
#endif

	{
		FViewInfo& View = SceneRenderer->Views[0];

		// Intermediate render target that will need to be flipped (needed on !IsMobileHDR())
		FRDGTextureRef FlippedOutputTexture{};

		const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;

		// We don't support screen percentage in scene capture.
		FIntRect ViewRect = View.UnscaledViewRect;
		FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;

		const FIntRect CopyDestRect = CopyInfo.GetDestRect();

		if (!CopyDestRect.IsEmpty())
		{
			CopyCaptureToTargetSetViewportFn = [CopyDestRect, ViewRect, FlippedOutputTexture](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

				RHICmdList.SetViewport
				(
					float(CopyDestRect.Min.X),
					float(CopyDestRect.Min.Y),
					0.0f,
					float(CopyDestRect.Max.X),
					float(CopyDestRect.Max.Y),
					1.0f
				);
			};
		}
		else
		{
			CopyCaptureToTargetSetViewportFn = [](FRHICommandList& RHICmdList) {};
		}

		// Render the scene normally
		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderScene);
			SceneRenderer->Render(GraphBuilder);
		}

		FRDGTextureRef OutputTexture = RegisterExternalTexture(GraphBuilder, Target->GetRenderTargetTexture(), TEXT("OutputTexture"));
		const FMinimalSceneTextures& SceneTextures = SceneRenderer->GetActiveSceneTextures();

		const FIntPoint TargetSize(UnconstrainedViewRect.Width(), UnconstrainedViewRect.Height());
		{
			// We need to flip this texture upside down (since we depended on tonemapping to fix this on the hdr path)
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				SceneTextures,
				OutputTexture,
				SceneRenderer->ViewFamily,
				SceneRenderer->Views);
		}

		if (bGenerateMips)
		{
			FGenerateMips::Execute(GraphBuilder, SceneRenderer->FeatureLevel, OutputTexture, GenerateMipsParams);
		}

		GraphBuilder.Execute();
	}

	SceneRenderer->RenderThreadEnd(RHICmdList);
}

static void UpdateSceneCaptureContent_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	FRenderTarget* RenderTarget,
	FTexture* RenderTargetTexture,
	const FString& EventName,
	const FRHICopyTextureInfo& CopyInfo,
	bool bGenerateMips,
	const FGenerateMipsParams& GenerateMipsParams,
	bool bClearRenderTarget,
	bool bOrthographicCamera)
{
	FUniformExpressionCacheAsyncUpdateScope AsyncUpdateScope;

	switch (GetFeatureLevelShadingPath(SceneRenderer->Scene->GetFeatureLevel()))
	{
		case EShadingPath::Mobile:
		{
			UpdateSceneCaptureContentMobile_RenderThread(
				RHICmdList,
				SceneRenderer,
				RenderTarget,
				RenderTargetTexture,
				EventName,
				CopyInfo,
				bGenerateMips,
				GenerateMipsParams);
			break;
		}
		case EShadingPath::Deferred:
		{
			UpdateSceneCaptureContentDeferred_RenderThread(
				RHICmdList,
				SceneRenderer,
				RenderTarget,
				RenderTargetTexture,
				EventName,
				CopyInfo,
				bGenerateMips,
				GenerateMipsParams,
				bClearRenderTarget,
				bOrthographicCamera);
			break;
		}
		default:
			checkNoEntry();
			break;
	}

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture->TextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
}

static void BuildOrthoMatrix(FIntPoint InRenderTargetSize, float InOrthoWidth, int32 InTileID, int32 InNumXTiles, int32 InNumYTiles, FMatrix& OutProjectionMatrix)
{
	check((int32)ERHIZBuffer::IsInverted);
	float const XAxisMultiplier = 1.0f;
	float const YAxisMultiplier = InRenderTargetSize.X / float(InRenderTargetSize.Y);

	const float OrthoWidth = InOrthoWidth / 2.0f;
	const float OrthoHeight = InOrthoWidth / 2.0f * XAxisMultiplier / YAxisMultiplier;

	const float NearPlane = 0;
	const float FarPlane = UE_FLOAT_HUGE_DISTANCE / 4.0f;

	const float ZScale = 1.0f / (FarPlane - NearPlane);
	const float ZOffset = -NearPlane;

	if (InTileID == -1)
	{
		OutProjectionMatrix = FReversedZOrthoMatrix(
			OrthoWidth,
			OrthoHeight,
			ZScale,
			ZOffset
		);
		
		return;
	}

#if DO_CHECK
	check(InNumXTiles != 0 && InNumYTiles != 0);
	if (InNumXTiles == 0 || InNumYTiles == 0)
	{
		OutProjectionMatrix = FMatrix(EForceInit::ForceInitToZero);
		return;
	}
#endif

	const float XTileDividerRcp = 1.0f / float(InNumXTiles);
	const float YTileDividerRcp = 1.0f / float(InNumYTiles);

	const float TileX = float(InTileID % InNumXTiles);
	const float TileY = float(InTileID / InNumXTiles);

	float l = -OrthoWidth + TileX * InOrthoWidth * XTileDividerRcp;
	float r = l + InOrthoWidth * XTileDividerRcp;
	float t = OrthoHeight - TileY * InOrthoWidth * YTileDividerRcp;
	float b = t - InOrthoWidth * YTileDividerRcp;

	OutProjectionMatrix = FMatrix(
		FPlane(2.0f / (r-l), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 2.0f / (t-b), 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, -ZScale, 0.0f),
		FPlane(-((r+l)/(r-l)), -((t+b)/(t-b)), 1.0f - ZOffset * ZScale, 1.0f)
	);
}

void BuildProjectionMatrix(FIntPoint InRenderTargetSize, float InFOV, float InNearClippingPlane, FMatrix& OutProjectionMatrix)
{
	float const XAxisMultiplier = 1.0f;
	float const YAxisMultiplier = InRenderTargetSize.X / float(InRenderTargetSize.Y);

	if ((int32)ERHIZBuffer::IsInverted)
	{
		OutProjectionMatrix = FReversedZPerspectiveMatrix(
			InFOV,
			InFOV,
			XAxisMultiplier,
			YAxisMultiplier,
			InNearClippingPlane,
			InNearClippingPlane
			);
	}
	else
	{
		OutProjectionMatrix = FPerspectiveMatrix(
			InFOV,
			InFOV,
			XAxisMultiplier,
			YAxisMultiplier,
			InNearClippingPlane,
			InNearClippingPlane
			);
	}
}

void GetShowOnlyAndHiddenComponents(USceneCaptureComponent* SceneCaptureComponent, TSet<FPrimitiveComponentId>& HiddenPrimitives, TOptional<TSet<FPrimitiveComponentId>>& ShowOnlyPrimitives)
{
	check(SceneCaptureComponent);
	for (auto It = SceneCaptureComponent->HiddenComponents.CreateConstIterator(); It; ++It)
	{
		// If the primitive component was destroyed, the weak pointer will return NULL.
		UPrimitiveComponent* PrimitiveComponent = It->Get();
		if (PrimitiveComponent)
		{
			HiddenPrimitives.Add(PrimitiveComponent->GetPrimitiveSceneId());
		}
	}

	for (auto It = SceneCaptureComponent->HiddenActors.CreateConstIterator(); It; ++It)
	{
		AActor* Actor = *It;

		if (Actor)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
				{
					HiddenPrimitives.Add(PrimComp->GetPrimitiveSceneId());
				}
			}
		}
	}

	if (SceneCaptureComponent->PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList)
	{
		ShowOnlyPrimitives.Emplace();

		for (auto It = SceneCaptureComponent->ShowOnlyComponents.CreateConstIterator(); It; ++It)
		{
			// If the primitive component was destroyed, the weak pointer will return NULL.
			UPrimitiveComponent* PrimitiveComponent = It->Get();
			if (PrimitiveComponent)
			{
				ShowOnlyPrimitives->Add(PrimitiveComponent->GetPrimitiveSceneId());
			}
		}

		for (auto It = SceneCaptureComponent->ShowOnlyActors.CreateConstIterator(); It; ++It)
		{
			AActor* Actor = *It;

			if (Actor)
			{
				for (UActorComponent* Component : Actor->GetComponents())
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
					{
						ShowOnlyPrimitives->Add(PrimComp->GetPrimitiveSceneId());
					}
				}
			}
		}
	}
	else if (SceneCaptureComponent->ShowOnlyComponents.Num() > 0 || SceneCaptureComponent->ShowOnlyActors.Num() > 0)
	{
		static bool bWarned = false;

		if (!bWarned)
		{
			UE_LOG(LogRenderer, Log, TEXT("Scene Capture has ShowOnlyComponents or ShowOnlyActors ignored by the PrimitiveRenderMode setting! %s"), *SceneCaptureComponent->GetPathName());
			bWarned = true;
		}
	}
}

void SetupViewFamilyForSceneCapture(
	FSceneViewFamily& ViewFamily,
	USceneCaptureComponent* SceneCaptureComponent,
	const TArrayView<const FSceneCaptureViewInfo> Views,
	float MaxViewDistance,
	bool bCaptureSceneColor,
	bool bIsPlanarReflection,
	FPostProcessSettings* PostProcessSettings,
	float PostProcessBlendWeight,
	const AActor* ViewActor,
	int32 CubemapFaceIndex)
{
	check(!ViewFamily.GetScreenPercentageInterface());

	// For cube map capture, CubeMapFaceIndex takes precedence over view index, so we must have only one view for that case
	check(CubemapFaceIndex == INDEX_NONE || Views.Num() == 1);

	// Initialize frame number
	ViewFamily.FrameNumber = ViewFamily.Scene->GetFrameNumber();
	ViewFamily.FrameCounter = GFrameCounter;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FSceneCaptureViewInfo& SceneCaptureViewInfo = Views[ViewIndex];

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.SetViewRectangle(SceneCaptureViewInfo.ViewRect);
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.ViewActor = ViewActor;
		ViewInitOptions.ViewLocation = SceneCaptureViewInfo.ViewLocation;
		ViewInitOptions.ViewRotation = SceneCaptureViewInfo.ViewRotation;
		ViewInitOptions.ViewOrigin = SceneCaptureViewInfo.ViewOrigin;
		ViewInitOptions.ViewRotationMatrix = SceneCaptureViewInfo.ViewRotationMatrix;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverrideFarClippingPlaneDistance = MaxViewDistance;
		ViewInitOptions.StereoPass = SceneCaptureViewInfo.StereoPass;
		ViewInitOptions.StereoViewIndex = SceneCaptureViewInfo.StereoViewIndex;
		ViewInitOptions.SceneViewStateInterface = SceneCaptureComponent->GetViewState(CubemapFaceIndex != INDEX_NONE ? CubemapFaceIndex : ViewIndex);
		ViewInitOptions.ProjectionMatrix = SceneCaptureViewInfo.ProjectionMatrix;
		ViewInitOptions.LODDistanceFactor = FMath::Clamp(SceneCaptureComponent->LODDistanceFactor, .01f, 100.0f);
		ViewInitOptions.bIsSceneCapture = true;
		ViewInitOptions.bIsSceneCaptureCube = SceneCaptureComponent->IsCube();
		ViewInitOptions.bSceneCaptureUsesRayTracing = SceneCaptureComponent->bUseRayTracingIfEnabled;
		ViewInitOptions.bIsPlanarReflection = bIsPlanarReflection;

		if (ViewFamily.Scene->GetWorld() != nullptr && ViewFamily.Scene->GetWorld()->GetWorldSettings() != nullptr)
		{
			ViewInitOptions.WorldToMetersScale = ViewFamily.Scene->GetWorld()->GetWorldSettings()->WorldToMeters;
		}

		if (bCaptureSceneColor)
		{
			ViewFamily.EngineShowFlags.PostProcessing = 0;
			ViewInitOptions.OverlayColor = FLinearColor::Black;
		}

		FSceneView* View = new FSceneView(ViewInitOptions);

		GetShowOnlyAndHiddenComponents(SceneCaptureComponent, View->HiddenPrimitives, View->ShowOnlyPrimitives);

		ViewFamily.Views.Add(View);

		View->StartFinalPostprocessSettings(SceneCaptureViewInfo.ViewOrigin);

		// By default, Lumen is disabled in scene captures, but can be re-enabled with the post process settings in the component.
		View->FinalPostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		View->FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::None;

		// Default surface cache to lower resolution for Scene Capture.  Can be overridden via post process settings.
		View->FinalPostProcessSettings.LumenSurfaceCacheResolution = 0.5f;

		View->OverridePostProcessSettings(*PostProcessSettings, PostProcessBlendWeight);
		View->EndFinalPostprocessSettings(ViewInitOptions);
	}
}

static FSceneRenderer* CreateSceneRendererForSceneCapture(
	FScene* Scene,
	USceneCaptureComponent* SceneCaptureComponent,
	FRenderTarget* RenderTarget,
	FIntPoint RenderTargetSize,
	const FMatrix& ViewRotationMatrix,
	const FVector& ViewLocation,
	const FMatrix& ProjectionMatrix,
	float MaxViewDistance,
	bool bCaptureSceneColor,
	FPostProcessSettings* PostProcessSettings,
	float PostProcessBlendWeight,
	const AActor* ViewActor, 
	int32 CubemapFaceIndex = INDEX_NONE)
{
	FSceneCaptureViewInfo SceneCaptureViewInfo;
	SceneCaptureViewInfo.ViewRotationMatrix = ViewRotationMatrix;
	SceneCaptureViewInfo.ViewOrigin = ViewLocation;
	SceneCaptureViewInfo.ProjectionMatrix = ProjectionMatrix;
	SceneCaptureViewInfo.StereoPass = EStereoscopicPass::eSSP_FULL;
	SceneCaptureViewInfo.StereoViewIndex = INDEX_NONE;
	SceneCaptureViewInfo.ViewRect = FIntRect(0, 0, RenderTargetSize.X, RenderTargetSize.Y);

	// Use camera position correction for ortho scene captures
	if(USceneCaptureComponent2D * SceneCaptureComponent2D = Cast<USceneCaptureComponent2D>(SceneCaptureComponent))
	{
		if (!SceneCaptureViewInfo.IsPerspectiveProjection() && SceneCaptureComponent2D->bUpdateOrthoPlanes)
		{
			SceneCaptureViewInfo.UpdateOrthoPlanes(SceneCaptureComponent2D->bUseCameraHeightAsViewTarget);
		}
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		RenderTarget,
		Scene,
		SceneCaptureComponent->ShowFlags)
		.SetResolveScene(!bCaptureSceneColor)
		.SetRealtimeUpdate(SceneCaptureComponent->bCaptureEveryFrame || SceneCaptureComponent->bAlwaysPersistRenderingState));

	FSceneViewExtensionContext ViewExtensionContext(Scene);
	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
	
	SetupViewFamilyForSceneCapture(
		ViewFamily,
		SceneCaptureComponent,
		MakeArrayView(&SceneCaptureViewInfo, 1),
		MaxViewDistance, 
		bCaptureSceneColor,
		/* bIsPlanarReflection = */ false,
		PostProcessSettings, 
		PostProcessBlendWeight,
		ViewActor,
		CubemapFaceIndex);

	// Scene capture source is used to determine whether to disable occlusion queries inside FSceneRenderer constructor
	ViewFamily.SceneCaptureSource = SceneCaptureComponent->CaptureSource;

	// Screen percentage is still not supported in scene capture.
	ViewFamily.EngineShowFlags.ScreenPercentage = false;
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
		ViewFamily, /* GlobalResolutionFraction = */ 1.0f));

	return FSceneRenderer::CreateSceneRenderer(&ViewFamily, nullptr);
}

class FSceneCapturePass final : public FCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FSceneCapturePass);

	FSceneCapturePass(const FString& InDebugName, ERenderMode InRenderMode, ERenderOutput InRenderOutput, UTextureRenderTarget2D* InRenderTarget)
		: FCustomRenderPassBase(InDebugName, InRenderMode, InRenderOutput, FIntPoint(InRenderTarget->GetSurfaceWidth(), InRenderTarget->GetSurfaceHeight()))
		, SceneCaptureRenderTarget(InRenderTarget->GameThread_GetRenderTargetResource())
	{}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		RenderTargetTexture = SceneCaptureRenderTarget->GetRenderTargetTexture(GraphBuilder);
	}
	
	FRenderTarget* SceneCaptureRenderTarget = nullptr;
};

void FScene::UpdateSceneCaptureContents(USceneCaptureComponent2D* CaptureComponent)
{
	check(CaptureComponent);

	if (UTextureRenderTarget2D* TextureRenderTarget = CaptureComponent->TextureTarget)
	{
		FTransform Transform = CaptureComponent->GetComponentToWorld();
		FVector ViewLocation = Transform.GetTranslation();

		// Remove the translation from Transform because we only need rotation.
		Transform.SetTranslation(FVector::ZeroVector);
		Transform.SetScale3D(FVector::OneVector);
		FMatrix ViewRotationMatrix = Transform.ToInverseMatrixWithScale();

		// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
		ViewRotationMatrix = ViewRotationMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));
		const float FOV = CaptureComponent->FOVAngle * (float)PI / 360.0f;
		FIntPoint CaptureSize(TextureRenderTarget->GetSurfaceWidth(), TextureRenderTarget->GetSurfaceHeight());

		const bool bUseSceneColorTexture = CaptureNeedsSceneColor(CaptureComponent->CaptureSource);
		const bool bEnableOrthographicTiling = (CaptureComponent->GetEnableOrthographicTiling() && CaptureComponent->ProjectionType == ECameraProjectionMode::Orthographic && bUseSceneColorTexture);
		if (CaptureComponent->GetEnableOrthographicTiling() && CaptureComponent->ProjectionType == ECameraProjectionMode::Orthographic && !bUseSceneColorTexture)
		{
			UE_LOG(LogRenderer, Warning, TEXT("SceneCapture - Orthographic and tiling with CaptureSource not using SceneColor (i.e FinalColor) not compatible. SceneCapture render will not be tiled"));
		}
		
		const int32 TileID = CaptureComponent->TileID;
		const int32 NumXTiles = CaptureComponent->GetNumXTiles();
		const int32 NumYTiles = CaptureComponent->GetNumYTiles();

		FMatrix ProjectionMatrix;
		if (CaptureComponent->bUseCustomProjectionMatrix)
		{
			ProjectionMatrix = CaptureComponent->CustomProjectionMatrix;
		}
		else
		{
			if (CaptureComponent->ProjectionType == ECameraProjectionMode::Perspective)
			{
				const float ClippingPlane = (CaptureComponent->bOverride_CustomNearClippingPlane) ? CaptureComponent->CustomNearClippingPlane : GNearClippingPlane;
				BuildProjectionMatrix(CaptureSize, FOV, ClippingPlane, ProjectionMatrix);
			}
			else
			{
				if (bEnableOrthographicTiling)
				{
					BuildOrthoMatrix(CaptureSize, CaptureComponent->OrthoWidth, CaptureComponent->TileID, NumXTiles, NumYTiles, ProjectionMatrix);
					CaptureSize /= FIntPoint(NumXTiles, NumYTiles);
				}
				else
				{
					BuildOrthoMatrix(CaptureSize, CaptureComponent->OrthoWidth, -1, 0, 0, ProjectionMatrix);
				}
			}
		}

		// As optimization for depth capture modes, render scene capture as additional render passes inside the main renderer.
		if (GSceneCaptureAllowRenderInMainRenderer && 
			CaptureComponent->bRenderInMainRenderer && 
			(CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneDepth || CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_DeviceDepth)
			)
		{
			FCustomRenderPassRendererInput PassInput;
			PassInput.ViewLocation = ViewLocation;
			PassInput.ViewRotationMatrix = ViewRotationMatrix;
			PassInput.ProjectionMatrix = ProjectionMatrix;
			PassInput.ViewActor = CaptureComponent->GetViewOwner();

			FString DebugName = CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneDepth ? TEXT("SceneCapturePass_SceneDepth") : TEXT("SceneCapturePass_DeviceDepth");
			FCustomRenderPassBase::ERenderOutput RenderOutput = CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneDepth ? FCustomRenderPassBase::ERenderOutput::SceneDepth : FCustomRenderPassBase::ERenderOutput::DeviceDepth;
			FSceneCapturePass* CustomPass = new FSceneCapturePass(DebugName, FCustomRenderPassBase::ERenderMode::DepthPass, RenderOutput, TextureRenderTarget);
			PassInput.CustomRenderPass = CustomPass;

			GetShowOnlyAndHiddenComponents(CaptureComponent, PassInput.HiddenPrimitives, PassInput.ShowOnlyPrimitives);

			// Caching scene capture info to be passed to the scene renderer.
			// #todo: We cannot (yet) guarantee for which ViewFamily this CRP will eventually be rendered since it will just execute the next time the scene is rendered by any FSceneRenderer. This seems quite problematic and could easily lead to unexpected behavior...
			AddCustomRenderPass(nullptr, PassInput);
			return;
		}

		FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(
			this, 
			CaptureComponent, 
			TextureRenderTarget->GameThread_GetRenderTargetResource(), 
			CaptureSize, 
			ViewRotationMatrix, 
			ViewLocation, 
			ProjectionMatrix, 
			CaptureComponent->MaxViewDistanceOverride, 
			bUseSceneColorTexture,
			&CaptureComponent->PostProcessSettings, 
			CaptureComponent->PostProcessBlendWeight,
			CaptureComponent->GetViewOwner());

		check(SceneRenderer != nullptr);

		SceneRenderer->Views[0].bFogOnlyOnRenderedOpaque = CaptureComponent->bConsiderUnrenderedOpaquePixelAsFullyTranslucent;

		SceneRenderer->ViewFamily.SceneCaptureCompositeMode = CaptureComponent->CompositeMode;

		// Need view state interface to be allocated for Lumen, as it requires persistent data.  This means
		// "bCaptureEveryFrame" or "bAlwaysPersistRenderingState" must be enabled.
		FSceneViewStateInterface* ViewStateInterface = CaptureComponent->GetViewState(0);

		if (ViewStateInterface &&
			(SceneRenderer->Views[0].FinalPostProcessSettings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::Lumen ||
			 SceneRenderer->Views[0].FinalPostProcessSettings.ReflectionMethod == EReflectionMethod::Lumen))
		{
			// It's OK to call these every frame -- they are no-ops if the correct data is already there
			ViewStateInterface->AddLumenSceneData(this, SceneRenderer->Views[0].FinalPostProcessSettings.LumenSurfaceCacheResolution);
		}
		else if (ViewStateInterface)
		{
			ViewStateInterface->RemoveLumenSceneData(this);
		}

		// Ensure that the views for this scene capture reflect any simulated camera motion for this frame
		TOptional<FTransform> PreviousTransform = FMotionVectorSimulation::Get().GetPreviousTransform(CaptureComponent);

		// Process Scene View extensions for the capture component
		{
			FSceneViewExtensionContext ViewExtensionContext(SceneRenderer->Scene);

			for (int32 Index = 0; Index < CaptureComponent->SceneViewExtensions.Num(); ++Index)
			{
				TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> Extension = CaptureComponent->SceneViewExtensions[Index].Pin();
				if (Extension.IsValid())
				{
					if (Extension->IsActiveThisFrame(ViewExtensionContext))
					{
						SceneRenderer->ViewFamily.ViewExtensions.Add(Extension.ToSharedRef());
					}
				}
				else
				{
					CaptureComponent->SceneViewExtensions.RemoveAt(Index, 1, EAllowShrinking::No);
					--Index;
				}
			}
		}

		for (const FSceneViewExtensionRef& Extension : SceneRenderer->ViewFamily.ViewExtensions)
		{
			Extension->SetupViewFamily(SceneRenderer->ViewFamily);
		}

		{
			FPlane ClipPlane = FPlane(CaptureComponent->ClipPlaneBase, CaptureComponent->ClipPlaneNormal.GetSafeNormal());

			for (FSceneView& View : SceneRenderer->Views)
			{
				if (PreviousTransform.IsSet())
				{
					View.PreviousViewTransform = PreviousTransform.GetValue();
				}

				View.bCameraCut = CaptureComponent->bCameraCutThisFrame;

				if (CaptureComponent->bEnableClipPlane)
				{
					View.GlobalClippingPlane = ClipPlane;
					// Jitter can't be removed completely due to the clipping plane
					View.bAllowTemporalJitter = false;
				}

				for (const FSceneViewExtensionRef& Extension : SceneRenderer->ViewFamily.ViewExtensions)
				{
					Extension->SetupView(SceneRenderer->ViewFamily, View);
				}
			}
		}

		// Reset scene capture's camera cut.
		CaptureComponent->bCameraCutThisFrame = false;

		FTextureRenderTargetResource* TextureRenderTargetResource = TextureRenderTarget->GameThread_GetRenderTargetResource();

		FString EventName;
		if (!CaptureComponent->ProfilingEventName.IsEmpty())
		{
			EventName = CaptureComponent->ProfilingEventName;
		}
		else if (CaptureComponent->GetOwner())
		{
			CaptureComponent->GetOwner()->GetFName().ToString(EventName);
		}
		FName TargetName = TextureRenderTarget->GetFName();

		const bool bGenerateMips = TextureRenderTarget->bAutoGenerateMips;
		FGenerateMipsParams GenerateMipsParams{TextureRenderTarget->MipsSamplerFilter == TF_Nearest ? SF_Point : (TextureRenderTarget->MipsSamplerFilter == TF_Trilinear ? SF_Trilinear : SF_Bilinear),
			TextureRenderTarget->MipsAddressU == TA_Wrap ? AM_Wrap : (TextureRenderTarget->MipsAddressU == TA_Mirror ? AM_Mirror : AM_Clamp),
			TextureRenderTarget->MipsAddressV == TA_Wrap ? AM_Wrap : (TextureRenderTarget->MipsAddressV == TA_Mirror ? AM_Mirror : AM_Clamp)};

		const bool bOrthographicCamera = CaptureComponent->ProjectionType == ECameraProjectionMode::Orthographic;


		// If capturing every frame, only render to the GPUs that are actually being used
		// this frame. We can only determine this by querying the viewport back buffer on
		// the render thread, so pass that along if it exists.
		FRenderTarget* GameViewportRT = nullptr;
		if (CaptureComponent->bCaptureEveryFrame)
		{
			if (GEngine->GameViewport != nullptr)
			{
				GameViewportRT = GEngine->GameViewport->Viewport;
			}
		}

		UTexture* TexturePtrNotDeferenced = TextureRenderTarget;

		// Compositing feature is only active when using SceneColor as the source
		bool bIsCompositing = (CaptureComponent->CompositeMode != SCCM_Overwrite) && (CaptureComponent->CaptureSource == SCS_SceneColorHDR);
#if WITH_EDITOR
		if (!CaptureComponent->CaptureMemorySize)
		{
			CaptureComponent->CaptureMemorySize = new FSceneCaptureMemorySize;
		}
		TRefCountPtr<FSceneCaptureMemorySize> CaptureMemorySize = CaptureComponent->CaptureMemorySize;
#else
		void* CaptureMemorySize = nullptr;		// Dummy value for lambda capture argument list
#endif

		for (const FSceneViewExtensionRef& Extension : SceneRenderer->ViewFamily.ViewExtensions)
		{
			Extension->BeginRenderViewFamily(SceneRenderer->ViewFamily);
		}

		UE::RenderCommandPipe::FSyncScope SyncScope;

		ENQUEUE_RENDER_COMMAND(CaptureCommand)(
			[SceneRenderer, TextureRenderTargetResource, TexturePtrNotDeferenced, EventName, TargetName, bGenerateMips, GenerateMipsParams, GameViewportRT, bEnableOrthographicTiling, bIsCompositing, bOrthographicCamera, NumXTiles, NumYTiles, TileID, CaptureMemorySize](FRHICommandListImmediate& RHICmdList)
			{
				if (GameViewportRT != nullptr)
				{
					const FRHIGPUMask GPUMask = GameViewportRT->GetGPUMask(RHICmdList);
					TextureRenderTargetResource->SetActiveGPUMask(GPUMask);
				}
				else
				{
					TextureRenderTargetResource->SetActiveGPUMask(FRHIGPUMask::All());
				}

				FRHICopyTextureInfo CopyInfo;

				if (bEnableOrthographicTiling)
				{
					const uint32 RTSizeX = TextureRenderTargetResource->GetSizeX() / NumXTiles;
					const uint32 RTSizeY = TextureRenderTargetResource->GetSizeY() / NumYTiles;
					const uint32 TileX = TileID % NumXTiles;
					const uint32 TileY = TileID / NumXTiles;
					CopyInfo.DestPosition.X = TileX * RTSizeX;
					CopyInfo.DestPosition.Y = TileY * RTSizeY;
					CopyInfo.Size.X = RTSizeX;
					CopyInfo.Size.Y = RTSizeY;
				}

				RectLightAtlas::FAtlasTextureInvalidationScope Invalidation(TexturePtrNotDeferenced);

#if WITH_EDITOR
				// Scene renderer may be deleted in UpdateSceneCaptureContent_RenderThread, grab view state pointer first
				const FSceneViewState* ViewState = SceneRenderer->Views[0].ViewState;
#endif  // WITH_EDITOR

				// Don't clear the render target when compositing, or in a tiling mode that fills in the render target in multiple passes.
				bool bClearRenderTarget = !bIsCompositing && !bEnableOrthographicTiling;

				UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTargetResource, TextureRenderTargetResource, EventName, CopyInfo, bGenerateMips, GenerateMipsParams, bClearRenderTarget, bOrthographicCamera);

#if WITH_EDITOR
				if (ViewState)
				{
					const bool bLogSizes = GDumpSceneCaptureMemoryFrame == GFrameNumberRenderThread;
					if (bLogSizes)
					{
						UE_LOG(LogRenderer, Log, TEXT("LogSizes\tSceneCapture\t%s\t%s\t%dx%d"), *EventName, *TargetName.ToString(), TextureRenderTargetResource->GetSizeX(), TextureRenderTargetResource->GetSizeY());
					}
					CaptureMemorySize->Size = ViewState->GetGPUSizeBytes(bLogSizes);
				}
				else
				{
					CaptureMemorySize->Size = 0;
				}
#endif  // WITH_EDITOR
			}
		);
	}
}

void FScene::UpdateSceneCaptureContents(USceneCaptureComponentCube* CaptureComponent)
{
	struct FLocal
	{
		/** Creates a transformation for a cubemap face, following the D3D cubemap layout. */
		static FMatrix CalcCubeFaceTransform(ECubeFace Face)
		{
			static const FVector XAxis(1.f, 0.f, 0.f);
			static const FVector YAxis(0.f, 1.f, 0.f);
			static const FVector ZAxis(0.f, 0.f, 1.f);

			// vectors we will need for our basis
			FVector vUp(YAxis);
			FVector vDir;
			switch (Face)
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
			FVector vRight(vUp ^ vDir);
			// create matrix from the 3 axes
			return FBasisVectorMatrix(vRight, vUp, vDir, FVector::ZeroVector);
		}
	} ;

	check(CaptureComponent);

	FTransform Transform = CaptureComponent->GetComponentToWorld();
	const FVector ViewLocation = Transform.GetTranslation();

	if (CaptureComponent->bCaptureRotation)
	{
		// Remove the translation from Transform because we only need rotation.
		Transform.SetTranslation(FVector::ZeroVector);
		Transform.SetScale3D(FVector::OneVector);
	}

	UTextureRenderTargetCube* const TextureTarget = CaptureComponent->TextureTarget;

	if (TextureTarget)
	{
		const float FOV = 90 * (float)PI / 360.0f;
		for (int32 faceidx = 0; faceidx < (int32)ECubeFace::CubeFace_MAX; faceidx++)
		{
			const ECubeFace TargetFace = (ECubeFace)faceidx;
			const FVector Location = CaptureComponent->GetComponentToWorld().GetTranslation();

			FMatrix ViewRotationMatrix;

			if (CaptureComponent->bCaptureRotation)
			{
				ViewRotationMatrix = Transform.ToInverseMatrixWithScale() * FLocal::CalcCubeFaceTransform(TargetFace);
			}
			else
			{
				ViewRotationMatrix = FLocal::CalcCubeFaceTransform(TargetFace);
			}
			FIntPoint CaptureSize(TextureTarget->GetSurfaceWidth(), TextureTarget->GetSurfaceHeight());
			FMatrix ProjectionMatrix;
			BuildProjectionMatrix(CaptureSize, FOV, GNearClippingPlane, ProjectionMatrix);
			FPostProcessSettings PostProcessSettings;

			bool bCaptureSceneColor = CaptureNeedsSceneColor(CaptureComponent->CaptureSource);

			FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(this, CaptureComponent,
				TextureTarget->GameThread_GetRenderTargetResource(), CaptureSize, ViewRotationMatrix,
				Location, ProjectionMatrix, CaptureComponent->MaxViewDistanceOverride,
				bCaptureSceneColor, &PostProcessSettings, 0, CaptureComponent->GetViewOwner(), faceidx);

			for (const FSceneViewExtensionRef& Extension : SceneRenderer->ViewFamily.ViewExtensions)
			{
				Extension->SetupViewFamily(SceneRenderer->ViewFamily);

				for (FSceneView& View : SceneRenderer->Views)
				{
					Extension->SetupView(SceneRenderer->ViewFamily, View);
				}
			}

			FTextureRenderTargetCubeResource* TextureRenderTarget = static_cast<FTextureRenderTargetCubeResource*>(TextureTarget->GameThread_GetRenderTargetResource());
			FString EventName;
			if (!CaptureComponent->ProfilingEventName.IsEmpty())
			{
				EventName = CaptureComponent->ProfilingEventName;
			}
			else if (CaptureComponent->GetOwner())
			{
				CaptureComponent->GetOwner()->GetFName().ToString(EventName);
			}

			for (const FSceneViewExtensionRef& Extension : SceneRenderer->ViewFamily.ViewExtensions)
			{
				Extension->BeginRenderViewFamily(SceneRenderer->ViewFamily);
			}

			UE::RenderCommandPipe::FSyncScope SyncScope;

			ENQUEUE_RENDER_COMMAND(CaptureCommand)(
				[SceneRenderer, TextureRenderTarget, EventName, TargetFace](FRHICommandListImmediate& RHICmdList)
				{
					FRHICopyTextureInfo CopyInfo;
					CopyInfo.DestSliceIndex = TargetFace;
					UpdateSceneCaptureContent_RenderThread(RHICmdList, SceneRenderer, TextureRenderTarget, TextureRenderTarget, EventName, CopyInfo, false, FGenerateMipsParams(), true, false);
				}
			);
		}
	}
}
