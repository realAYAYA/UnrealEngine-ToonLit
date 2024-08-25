// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRenderingPolicy.h"
#include "UniformBuffer.h"
#include "Shader.h"
#include "ShowFlags.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "EngineGlobals.h"
#include "RHIStaticStates.h"
#include "RHIUtilities.h"
#include "GlobalRenderResources.h"
#include "Interfaces/SlateRHIRenderingPolicyInterface.h"
#include "SceneView.h"
#include "SceneUtils.h"
#include "Engine/Engine.h"
#include "SlateShaders.h"
#include "Rendering/SlateRenderer.h"
#include "SlateRHIRenderer.h"
#include "SlateMaterialShader.h"
#include "SlateUTextureResource.h"
#include "SlateMaterialResource.h"
#include "SlateUpdatableBuffer.h"
#include "SlatePostProcessor.h"
#include "Materials/MaterialRenderProxy.h"
#include "Modules/ModuleManager.h"
#include "PipelineStateCache.h"
#include "Math/RandomStream.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Types/SlateConstants.h"
#include "RenderGraphResources.h"
#include "SceneRenderTargetParameters.h"

extern void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters);

DECLARE_CYCLE_STAT(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Update Buffers RT"), STAT_SlateUpdateBufferRTTimeLambda, STATGROUP_Slate);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Layers"), STAT_SlateNumLayers, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Batches"), STAT_SlateNumBatches, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Vertices"), STAT_SlateVertexCount, STATGROUP_Slate);

DECLARE_DWORD_COUNTER_STAT(TEXT("Clips (Scissor)"), STAT_SlateScissorClips, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Clips (Stencil)"), STAT_SlateStencilClips, STATGROUP_Slate);

#if WITH_SLATE_DEBUGGING
int32 SlateEnableDrawEvents = 1;
#else
int32 SlateEnableDrawEvents = 0;
#endif
static FAutoConsoleVariableRef CVarSlateEnableDrawEvents(TEXT("Slate.EnableDrawEvents"), SlateEnableDrawEvents, TEXT("."), ECVF_Default);

#if WITH_SLATE_DEBUGGING
int32 BatchToDraw = -1;
static FAutoConsoleVariableRef CVarSlateDrawBatchNum(TEXT("Slate.DrawBatchNum"), BatchToDraw, TEXT("."), ECVF_Default);
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define SLATE_DRAW_EVENT(RHICmdList, EventName) SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, EventName, SlateEnableDrawEvents);
	#define SLATE_DRAW_EVENTF(RHICmdList, EventName, Format, ...) SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventName, SlateEnableDrawEvents, Format, ##__VA_ARGS__);
#else
	#define SLATE_DRAW_EVENT(RHICmdList, EventName)
	#define SLATE_DRAW_EVENTF(RHICmdList, EventName, Format, ...);
#endif

TAutoConsoleVariable<int32> CVarSlateAbsoluteIndices(
	TEXT("Slate.AbsoluteIndices"),
	0,
	TEXT("0: Each element first vertex index starts at 0 (default), 1: Use absolute indices, simplifies draw call setup on RHIs that do not support BaseVertex"),
	ECVF_Default
);

FSlateRHIRenderingPolicy::FSlateRHIRenderingPolicy(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, TOptional<int32> InitialBufferSize)
	: FSlateRenderingPolicy(InSlateFontServices, 0)
	, PostProcessor(new FSlatePostProcessor)
	, ResourceManager(InResourceManager)
	, bGammaCorrect(true)
	, bApplyColorDeficiencyCorrection(true)
	, InitialBufferSizeOverride(InitialBufferSize)
	, LastDeviceProfile(nullptr)
{
	InitResources();
}

void FSlateRHIRenderingPolicy::InitResources()
{
	int32 NumVertices = 100;

	if ( InitialBufferSizeOverride.IsSet() )
	{
		NumVertices = InitialBufferSizeOverride.GetValue();
	}
	else if ( GConfig )
	{
		int32 NumVertsInConfig = 0;
		if ( GConfig->GetInt(TEXT("SlateRenderer"), TEXT("NumPreallocatedVertices"), NumVertsInConfig, GEngineIni) )
		{
			NumVertices = NumVertsInConfig;
		}
	}

	// Always create a little space but never allow it to get too high
#if !SLATE_USE_32BIT_INDICES
	NumVertices = FMath::Clamp(NumVertices, 100, 65535);
#else
	NumVertices = FMath::Clamp(NumVertices, 100, 1000000);
#endif

	UE_LOG(LogSlate, Verbose, TEXT("Allocating space for %d vertices"), NumVertices);

	SourceVertexBuffer.Init(NumVertices);
	SourceIndexBuffer.Init(NumVertices);

	BeginInitResource(&StencilVertexBuffer);
}

void FSlateRHIRenderingPolicy::ReleaseResources()
{
	SourceVertexBuffer.Destroy();
	SourceIndexBuffer.Destroy();

	BeginReleaseResource(&StencilVertexBuffer);
}

void FSlateRHIRenderingPolicy::BeginDrawingWindows()
{
	check( IsInRenderingThread() );
}

void FSlateRHIRenderingPolicy::EndDrawingWindows()
{
	check( IsInParallelRenderingThread() );
}

void FSlateRHIRenderingPolicy::BuildRenderingBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateUpdateBufferRTTime);

	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	// Merge together batches for less draw calls
	InBatchData.MergeRenderBatches();

	const FSlateVertexArray& FinalVertexData = InBatchData.GetFinalVertexData();
	const FSlateIndexArray& FinalIndexData = InBatchData.GetFinalIndexData();

	const int32 NumVertices = FinalVertexData.Num();
	const int32 NumIndices = FinalIndexData.Num();

	if (InBatchData.GetRenderBatches().Num() > 0 && NumVertices > 0 && NumIndices > 0)
	{
		bool bShouldShrinkResources = false;
		bool bAbsoluteIndices = CVarSlateAbsoluteIndices.GetValueOnRenderThread() != 0;

		SourceVertexBuffer.PreFillBuffer(NumVertices, bShouldShrinkResources);
		SourceIndexBuffer.PreFillBuffer(NumIndices, bShouldShrinkResources);

		RHICmdList.EnqueueLambda([
			VertexBuffer = SourceVertexBuffer.VertexBufferRHI.GetReference(),
			IndexBuffer = SourceIndexBuffer.IndexBufferRHI.GetReference(),
			&InBatchData,
			bAbsoluteIndices
		](FRHICommandListImmediate& InRHICmdList)
		{
			SCOPE_CYCLE_COUNTER(STAT_SlateUpdateBufferRTTimeLambda);

			// Note: Use "Lambda" prefix to prevent clang/gcc warnings of '-Wshadow' warning
			const FSlateVertexArray& LambdaFinalVertexData = InBatchData.GetFinalVertexData();
			const FSlateIndexArray& LambdaFinalIndexData = InBatchData.GetFinalIndexData();

			const int32 NumBatchedVertices = LambdaFinalVertexData.Num();
			const int32 NumBatchedIndices = LambdaFinalIndexData.Num();

			uint32 RequiredVertexBufferSize = NumBatchedVertices * sizeof(FSlateVertex);
			uint8* VertexBufferData = (uint8*)InRHICmdList.LockBuffer(VertexBuffer, 0, RequiredVertexBufferSize, RLM_WriteOnly);

			uint32 RequiredIndexBufferSize = NumBatchedIndices * sizeof(SlateIndex);
			uint8* IndexBufferData = (uint8*)InRHICmdList.LockBuffer(IndexBuffer, 0, RequiredIndexBufferSize, RLM_WriteOnly);

			FMemory::Memcpy(VertexBufferData, LambdaFinalVertexData.GetData(), RequiredVertexBufferSize);
			FMemory::Memcpy(IndexBufferData, LambdaFinalIndexData.GetData(), RequiredIndexBufferSize);

			InRHICmdList.UnlockBuffer(VertexBuffer);
			InRHICmdList.UnlockBuffer(IndexBuffer);
		});

		RHICmdList.RHIThreadFence(true);
	}

	checkSlow(SourceVertexBuffer.GetBufferUsageSize() <= SourceVertexBuffer.GetBufferSize());
	checkSlow(SourceIndexBuffer.GetBufferUsageSize() <= SourceIndexBuffer.GetBufferSize());

	SET_DWORD_STAT(STAT_SlateNumLayers, InBatchData.GetNumLayers());
	SET_DWORD_STAT(STAT_SlateNumBatches, InBatchData.GetNumFinalBatches());
	SET_DWORD_STAT(STAT_SlateVertexCount, InBatchData.GetFinalVertexData().Num());
}

static FSceneView* CreateSceneView( FSceneViewFamilyContext* ViewFamilyContext, FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix, const FIntRect InViewRect)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_CreateSceneView);
	// In loading screens, the engine is NULL, so we skip out.
	if (GEngine == nullptr)
	{
		return nullptr;
	}
	
	FIntRect ViewRect = InViewRect;
	if (ViewRect.IsEmpty())
	{
		ViewRect = FIntRect(FIntPoint(0, 0), BackBuffer.GetSizeXY());
	}

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamilyContext;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = ViewProjectionMatrix;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView( ViewInitOptions );
	ViewFamilyContext->Views.Add( View );

	const FIntPoint BufferSize = BackBuffer.GetSizeXY();

	// Create the view's uniform buffer.
	FViewUniformShaderParameters ViewUniformShaderParameters;
	ViewUniformShaderParameters.VTFeedbackBuffer = GEmptyStructuredBufferWithUAV->UnorderedAccessViewRHI;

	View->SetupCommonViewUniformBufferParameters(
		ViewUniformShaderParameters,
		BufferSize,
		1,
		ViewRect,
		View->ViewMatrices,
		FViewMatrices()
	);

	// TODO LWC
	ViewUniformShaderParameters.WorldViewOriginHigh = (FVector3f)View->ViewMatrices.GetViewOrigin();
	
	// Slate materials need this scale to be positive, otherwise it can fail in querying scene textures (e.g., custom stencil)
	ViewUniformShaderParameters.BufferToSceneTextureScale = FVector2f(1.0f, 1.0f);

	ERHIFeatureLevel::Type RHIFeatureLevel = View->GetFeatureLevel();

	ViewUniformShaderParameters.MobilePreviewMode =
		(GIsEditor &&
		(RHIFeatureLevel == ERHIFeatureLevel::ES3_1) &&
		GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1) ? 1.0f : 0.0f;

	UpdateNoiseTextureParameters(ViewUniformShaderParameters);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_CreateViewUniformBufferImmediate);
		View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformShaderParameters, UniformBuffer_SingleFrame);
	}

	return View;
}

static const FName RendererModuleName("Renderer");

static bool UpdateScissorRect(
	FRHICommandList& RHICmdList, 
#if STATS
	int32& ScissorClips, 
	int32& StencilClips,
#endif
	uint32& StencilRef, 
	uint32& MaskingID,
	FSlateBackBuffer& BackBuffer,
	const FSlateRenderBatch& RenderBatch, 
	FRHIRenderPassInfo& RPInfo,
	FTexture2DRHIRef& DepthStencilTarget,
	const FSlateClippingState*& LastClippingState,
	const FVector2f ViewTranslation2D, 
	FGraphicsPipelineStateInitializer& InGraphicsPSOInit,
	FSlateStencilClipVertexBuffer& StencilVertexBuffer,
	const FMatrix& ViewProjection, 
	bool bForceStateChange)
{
	check(RHICmdList.IsInsideRenderPass());
	bool bDidRestartRenderpass = false;

	if (RenderBatch.ClippingState != LastClippingState || bForceStateChange)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_UpdateScissorRect);

		if (RenderBatch.ClippingState)
		{
			const FSlateClippingState& ClipState = *RenderBatch.ClippingState;
			if (ClipState.GetClippingMethod() == EClippingMethod::Scissor)
			{
#if STATS
				ScissorClips++;
#endif

				if (bForceStateChange && MaskingID > 0)
				{
					// #todo-renderpasses this is very gross. If/when this gets refactored we can detect a simple clear or batch up elements by rendertarget (and other stuff)
					RHICmdList.EndRenderPass();
					bDidRestartRenderpass = true;
					ERenderTargetActions StencilAction = IsMemorylessTexture(DepthStencilTarget) ? ERenderTargetActions::DontLoad_DontStore : ERenderTargetActions::Load_Store;

					RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::DontLoad_DontStore, StencilAction);
					RPInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthStencilTarget;
					RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilWrite;

					RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateUpdateScissorRect"));
				}

				const FSlateClippingZone& ScissorRect = ClipState.ScissorRect.GetValue();

				const FIntPoint SizeXY = BackBuffer.GetSizeXY();
				const FVector2f ViewSize((float) SizeXY.X, (float) SizeXY.Y);

				// Clamp scissor rect to BackBuffer size
				const FVector2f TopLeft     = FVector2f::Min(FVector2f::Max(ScissorRect.TopLeft     + ViewTranslation2D, FVector2f(0.0f, 0.0f)), ViewSize);
				const FVector2f BottomRight = FVector2f::Min(FVector2f::Max(ScissorRect.BottomRight + ViewTranslation2D, FVector2f(0.0f, 0.0f)), ViewSize);
				
				RHICmdList.SetScissorRect(true, TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);

				// Disable depth/stencil testing by default
				InGraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				StencilRef = 0;
			}
			else
			{
#if STATS
				StencilClips++;
#endif

				SLATE_DRAW_EVENT(RHICmdList, StencilClipping);

				check(ClipState.StencilQuads.Num() > 0);

				const TArray<FSlateClippingZone>& StencilQuads = ClipState.StencilQuads;

				// We're going to overflow the masking ID this time, we need to reset the MaskingID to 0.
				// this will cause us to clear the stencil buffer so that we can begin fresh.
				if ((MaskingID + StencilQuads.Num()) > 255)
				{
					MaskingID = 0;
				}

				// We only clear the stencil the first time, and if some how the user draws more than 255 masking quads
				// in a single frame.
				const bool bClearStencil = MaskingID == 0;

				// Don't bother setting the render targets unless we actually need to clear them.
				if (bClearStencil || bForceStateChange)
				{
					// #todo-renderpasses Similar to above this is gross. Would require a refactor to really fix.
					RHICmdList.EndRenderPass();
					bDidRestartRenderpass = true;

					// Clear current stencil buffer, we use ELoad/EStore, because we need to keep the stencil around.
					ERenderTargetLoadAction StencilLoadAction = bClearStencil ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;
					ERenderTargetActions StencilAction = MakeRenderTargetActions(StencilLoadAction, ERenderTargetStoreAction::EStore);
					if (IsMemorylessTexture(DepthStencilTarget))
					{
						// We can't preserve content for memoryless targets
						StencilAction = bClearStencil ? ERenderTargetActions::Clear_DontStore : ERenderTargetActions::DontLoad_DontStore;
					}

					RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::DontLoad_DontStore, StencilAction);
					RPInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthStencilTarget;
					RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilWrite;
					TransitionRenderPassTargets(RHICmdList, RPInfo);
					RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateUpdateScissorRect_ClearStencil"));
				}

				// Setup the scissor rect after starting the render pass, as the RHI does not preserve the scissor state between passes / render targets.

				if (bClearStencil)
				{
					// We don't want there to be any scissor rect when we clear the stencil
					RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				}
				else
				{
					// There might be some large - useless stencils, especially in the first couple of stencils if large
					// widgets that clip also contain render targets, so, by setting the scissor to the AABB of the final
					// stencil, we can cut out a lot of work that can't possibly be useful.
					//
					// NOTE - We also round it, because if we don't it can over-eagerly slice off pixels it shouldn't.
					const FSlateClippingZone& MaskQuad = StencilQuads.Last();
					const FSlateRect LastStencilBoundingBox = MaskQuad.GetBoundingBox().Round();

					FSlateRect ScissorRect = LastStencilBoundingBox.OffsetBy(FVector2D(ViewTranslation2D));

					// Chosen stencil quad might have some coordinates outside the viewport.
					// After turning it into a bounding box, this box must be clamped to the current viewport,
					// as scissors outside the viewport don't make sense (and cause assertions to fail).
					const FIntPoint BackBufferSize = BackBuffer.GetSizeXY();
					ScissorRect.Left = FMath::Clamp(ScissorRect.Left, 0.0f, static_cast<float>(BackBufferSize.X));
					ScissorRect.Top = FMath::Clamp(ScissorRect.Top, 0.0f, static_cast<float>(BackBufferSize.Y));
					ScissorRect.Right = FMath::Clamp(ScissorRect.Right, ScissorRect.Left, static_cast<float>(BackBufferSize.X));
					ScissorRect.Bottom = FMath::Clamp(ScissorRect.Bottom, ScissorRect.Top, static_cast<float>(BackBufferSize.Y));

					RHICmdList.SetScissorRect(true, ScissorRect.Left, ScissorRect.Top, ScissorRect.Right, ScissorRect.Bottom);
				}


				FGlobalShaderMap* MaxFeatureLevelShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);

				// Set the new shaders
				TShaderMapRef<FSlateMaskingVS> VertexShader(MaxFeatureLevelShaderMap);
				TShaderMapRef<FSlateMaskingPS> PixelShader(MaxFeatureLevelShaderMap);

				// Start by setting up the stenciling states so that we can write representations of the clipping zones into the stencil buffer only.
				{
					FGraphicsPipelineStateInitializer WriteMaskPSOInit;
					RHICmdList.ApplyCachedRenderTargets(WriteMaskPSOInit);
					WriteMaskPSOInit.BlendState = TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
					WriteMaskPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					WriteMaskPSOInit.DepthStencilState =
						TStaticDepthStencilState<
						/*bEnableDepthWrite*/ false
						, /*DepthTest*/ CF_Always
						, /*bEnableFrontFaceStencil*/ true
						, /*FrontFaceStencilTest*/ CF_Always
						, /*FrontFaceStencilFailStencilOp*/ SO_Keep
						, /*FrontFaceDepthFailStencilOp*/ SO_Keep
						, /*FrontFacePassStencilOp*/ SO_Replace
						, /*bEnableBackFaceStencil*/ true
						, /*BackFaceStencilTest*/ CF_Always
						, /*BackFaceStencilFailStencilOp*/ SO_Keep
						, /*BackFaceDepthFailStencilOp*/ SO_Keep
						, /*BackFacePassStencilOp*/ SO_Replace
						, /*StencilReadMask*/ 0xFF
						, /*StencilWriteMask*/ 0xFF>::GetRHI();

					WriteMaskPSOInit.BoundShaderState.VertexDeclarationRHI = GSlateMaskingVertexDeclaration.VertexDeclarationRHI;
					WriteMaskPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					WriteMaskPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					WriteMaskPSOInit.PrimitiveType = PT_TriangleStrip;

					// Draw the first stencil using SO_Replace, so that we stomp any pixel with a MaskingID + 1.
					{

						SetGraphicsPipelineState(RHICmdList, WriteMaskPSOInit, MaskingID + 1);

						const FSlateClippingZone& MaskQuad = StencilQuads[0];

						FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

						VertexShader->SetViewProjection(BatchedParameters, FMatrix44f(ViewProjection));
						VertexShader->SetMaskRect(BatchedParameters, MaskQuad.TopLeft, MaskQuad.TopRight, MaskQuad.BottomLeft, MaskQuad.BottomRight);

						RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);

						RHICmdList.SetStreamSource(0, StencilVertexBuffer.VertexBufferRHI, 0);
						RHICmdList.DrawPrimitive(0, 2, 1);
					}

					// Now setup the pipeline to use SO_SaturatedIncrement, since we've established the initial
					// stencil with SO_Replace, we can safely use SO_SaturatedIncrement, to build up the stencil
					// to the required mask of MaskingID + StencilQuads.Num(), thereby ensuring only the union of
					// all stencils will render pixels.
					{
						WriteMaskPSOInit.DepthStencilState =
							TStaticDepthStencilState<
							/*bEnableDepthWrite*/ false
							, /*DepthTest*/ CF_Always
							, /*bEnableFrontFaceStencil*/ true
							, /*FrontFaceStencilTest*/ CF_Always
							, /*FrontFaceStencilFailStencilOp*/ SO_Keep
							, /*FrontFaceDepthFailStencilOp*/ SO_Keep
							, /*FrontFacePassStencilOp*/ SO_SaturatedIncrement
							, /*bEnableBackFaceStencil*/ true
							, /*BackFaceStencilTest*/ CF_Always
							, /*BackFaceStencilFailStencilOp*/ SO_Keep
							, /*BackFaceDepthFailStencilOp*/ SO_Keep
							, /*BackFacePassStencilOp*/ SO_SaturatedIncrement
							, /*StencilReadMask*/ 0xFF
							, /*StencilWriteMask*/ 0xFF>::GetRHI();


						SetGraphicsPipelineState(RHICmdList, WriteMaskPSOInit, 0);

						FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
						VertexShader->SetViewProjection(BatchedParameters, FMatrix44f(ViewProjection));
						RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);
					}
				}

				MaskingID += StencilQuads.Num();

				// Next write the number of quads representing the number of clipping zones have on top of each other.
				for (int32 MaskIndex = 1; MaskIndex < StencilQuads.Num(); MaskIndex++)
				{
					const FSlateClippingZone& MaskQuad = StencilQuads[MaskIndex];

					FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

					VertexShader->SetViewProjection(BatchedParameters, FMatrix44f(ViewProjection));
					VertexShader->SetMaskRect(BatchedParameters, MaskQuad.TopLeft, MaskQuad.TopRight, MaskQuad.BottomLeft, MaskQuad.BottomRight);

					RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);

					RHICmdList.SetStreamSource(0, StencilVertexBuffer.VertexBufferRHI, 0);
					RHICmdList.DrawPrimitive(0, 2, 1);
				}

				// Setup the stenciling state to be read only now, disable depth writes, and restore the color buffer
				// because we're about to go back to rendering widgets "normally", but with the added effect that now
				// we have the stencil buffer bound with a bunch of clipping zones rendered into it.
				{
					FRHIDepthStencilState* DSMaskRead =
						TStaticDepthStencilState<
						/*bEnableDepthWrite*/ false
						, /*DepthTest*/ CF_Always
						, /*bEnableFrontFaceStencil*/ true
						, /*FrontFaceStencilTest*/ CF_Equal
						, /*FrontFaceStencilFailStencilOp*/ SO_Keep
						, /*FrontFaceDepthFailStencilOp*/ SO_Keep
						, /*FrontFacePassStencilOp*/ SO_Keep
						, /*bEnableBackFaceStencil*/ true
						, /*BackFaceStencilTest*/ CF_Equal
						, /*BackFaceStencilFailStencilOp*/ SO_Keep
						, /*BackFaceDepthFailStencilOp*/ SO_Keep
						, /*BackFacePassStencilOp*/ SO_Keep
						, /*StencilReadMask*/ 0xFF
						, /*StencilWriteMask*/ 0xFF>::GetRHI();

					InGraphicsPSOInit.DepthStencilState = DSMaskRead;

					// We set a StencilRef equal to the number of stenciling/clipping masks,
					// so unless the pixel we're rendering two is on top of a stencil pixel with the same number
					// it's going to get rejected, thereby clipping everything except for the cross-section of
					// all the stenciling quads.
					StencilRef = MaskingID;
				}
			}

			RHICmdList.ApplyCachedRenderTargets(InGraphicsPSOInit);
		}
		else
		{
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

			// Disable depth/stencil testing
			InGraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			StencilRef = 0;
		}

		LastClippingState = RenderBatch.ClippingState;
	}

	return bDidRestartRenderpass;
}

static bool UpdateScissorRect(
	FRHICommandList& RHICmdList,
#if STATS
	int32& ScissorClips,
	int32& StencilClips,
#endif
	uint32& StencilRef,
	uint32& MaskingID,
	FSlateBackBuffer& BackBuffer,
	const FSlateRenderBatch& RenderBatch,
	FTexture2DRHIRef& ColorTarget,
	FTexture2DRHIRef& DepthStencilTarget,
	const FSlateClippingState*& LastClippingState,
	const FVector2f ViewTranslation2D,
	FGraphicsPipelineStateInitializer& InGraphicsPSOInit,
	FSlateStencilClipVertexBuffer& StencilVertexBuffer,
	const FMatrix& ViewProjection,
	bool bForceStateChange)
{
	FRHIRenderPassInfo RPInfo(ColorTarget, ERenderTargetActions::Load_Store);
	return UpdateScissorRect(RHICmdList,
#if STATS
		ScissorClips,
		StencilClips,
#endif
		StencilRef,
		MaskingID,
		BackBuffer,
		RenderBatch,
		RPInfo,
		DepthStencilTarget,
		LastClippingState,
		ViewTranslation2D,
		InGraphicsPSOInit,
		StencilVertexBuffer,
		ViewProjection,
		bForceStateChange);
}

static FRHISamplerState* GetSamplerState(ESlateBatchDrawFlag DrawFlags, ETextureSamplerFilter Filter)
{
	FRHISamplerState* SamplerState = nullptr;

	if (EnumHasAllFlags(DrawFlags, (ESlateBatchDrawFlag::TileU | ESlateBatchDrawFlag::TileV)))
	{
		switch (Filter)
		{
		case ETextureSamplerFilter::Point:
			SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicPoint:
			SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::Trilinear:
			SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicLinear:
			SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		default:
			SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		}
	}
	else if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::TileU))
	{
		switch (Filter)
		{
		case ETextureSamplerFilter::Point:
			SamplerState = TStaticSamplerState<SF_Point, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicPoint:
			SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::Trilinear:
			SamplerState = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicLinear:
			SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
			break;
		default:
			SamplerState = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Clamp, AM_Wrap>::GetRHI();
			break;
		}
	}
	else if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::TileV))
	{
		switch (Filter)
		{
		case ETextureSamplerFilter::Point:
			SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicPoint:
			SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::Trilinear:
			SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicLinear:
			SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		default:
			SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap, AM_Wrap>::GetRHI();
			break;
		}
	}
	else
	{
		switch (Filter)
		{
		case ETextureSamplerFilter::Point:
			SamplerState = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicPoint:
			SamplerState = TStaticSamplerState<SF_AnisotropicPoint, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			break;
		case ETextureSamplerFilter::Trilinear:
			SamplerState = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			break;
		case ETextureSamplerFilter::AnisotropicLinear:
			SamplerState = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			break;
		default:
			SamplerState = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			break;
		}
	}

	return SamplerState;
}

void FSlateRHIRenderingPolicy::DrawElements(
	FRHICommandListImmediate& RHICmdList,
	FSlateBackBuffer& BackBuffer,
	FTexture2DRHIRef& ColorTarget,
	FTexture2DRHIRef& PostProcessTexture,
	FTexture2DRHIRef& DepthStencilTarget,
	int32 FirstBatchIndex,
	const TArray<FSlateRenderBatch>& RenderBatches,
	const FSlateRenderingParams& Params)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());
	check(RHICmdList.IsInsideRenderPass());

	// Cache the TextureLODGroups so that we can look them up for texture filtering.
	if (UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		if (UDeviceProfile* Profile = UDeviceProfileManager::Get().GetActiveProfile())
		{
			if (Profile != LastDeviceProfile)
			{
				TextureLODGroups = Profile->GetTextureLODSettings()->TextureLODGroups;
				LastDeviceProfile = Profile;
			}
		}
	}

	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	static const FEngineShowFlags DefaultShowFlags(ESFIM_Game);

	// Disable gammatization when back buffer is in float 16 format.
	// Note that the final editor rendering won't compare 1:1 with 8/10 bit RGBA since blending
	// of "manually" gammatized values is wrong as there is no de-gammatization of the destination buffer
	// and re-gammatization of the resulting blending operation in the 8/10 bit RGBA path.
	// For Editor running in HDR then the gamma needs to be 2.2 and have a float back buffer format.

	const float EngineGamma = (!GIsEditor && (BackBuffer.GetRenderTargetTexture()->GetFormat() == PF_FloatRGBA) && (Params.bIsHDR==false)) ? 1.0f : GEngine ? GEngine->GetDisplayGamma() : 2.2f;
	const float DisplayGamma = bGammaCorrect ? EngineGamma : 1.0f;
	const float DisplayContrast = GSlateContrast;

	int32 ScissorClips = 0;
	int32 StencilClips = 0;

	// In order to support MaterialParameterCollections, we need to create multiple FSceneViews for 
	// each possible Scene that we encounter. The following code creates these as separate arrays, where the first 
	// N entries map directly to entries from ActiveScenes. The final entry is added to represent the absence of a
	// valid scene, i.e. a -1 in the SceneIndex parameter of the batch.
	int32 NumScenes = ResourceManager->GetSceneCount() + 1;
	TArray<FSceneView*, TInlineAllocator<3> > SceneViews;
	SceneViews.SetNum(NumScenes);
	TArray<FSceneViewFamilyContext*, TInlineAllocator<3> > SceneViewFamilyContexts;
	SceneViewFamilyContexts.SetNum(NumScenes);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_CreateScenes);
		for (int32 i = 0; i < ResourceManager->GetSceneCount(); i++)
		{
			SceneViewFamilyContexts[i] = new FSceneViewFamilyContext
			(
				FSceneViewFamily::ConstructionValues
				(
					&BackBuffer,
					ResourceManager->GetSceneAt(i),
					DefaultShowFlags
				)
				.SetTime(Params.Time)
				.SetRealtimeUpdate(true)
			);
			SceneViews[i] = CreateSceneView(SceneViewFamilyContexts[i], BackBuffer, FMatrix(Params.ViewProjectionMatrix), Params.ViewRect);
		}

		SceneViewFamilyContexts[NumScenes - 1] = new FSceneViewFamilyContext
		(
			FSceneViewFamily::ConstructionValues
			(
				&BackBuffer,
				nullptr,
				DefaultShowFlags
			)
			.SetTime(Params.Time)
			.SetRealtimeUpdate(true)
		);
		SceneViews[NumScenes - 1] = CreateSceneView(SceneViewFamilyContexts[NumScenes - 1], BackBuffer, FMatrix(Params.ViewProjectionMatrix), Params.ViewRect);
	}

	TShaderMapRef<FSlateElementVS> GlobalVertexShader(GetGlobalShaderMap(GMaxRHIShaderPlatform));

	FSamplerStateRHIRef BilinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	TSlateElementVertexBuffer<FSlateVertex>* VertexBufferPtr = &SourceVertexBuffer;
	FSlateElementIndexBuffer* IndexBufferPtr = &SourceIndexBuffer;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	const FSlateRenderDataHandle* LastHandle = nullptr;

	const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);

#if WITH_SLATE_VISUALIZERS
	FRandomStream BatchColors(1337);
#endif

	const bool bAbsoluteIndices = CVarSlateAbsoluteIndices.GetValueOnRenderThread() != 0;

	// This variable tracks the last clipping state, so that if multiple batches have the same clipping state, we don't have to do any work.
	const FSlateClippingState* LastClippingState;

	// This is the stenciling ref variable we set any time we draw, so that any stencil comparisons use the right mask id.
	uint32 StencilRef = 0;
	// This is an accumulating maskID that we use to track the between batch usage of the stencil buffer, when at 0, or over 255
	// this signals that we need to reset the masking ID, and clear the stencil buffer, as we've used up the available scratch range.
	uint32 MaskingID = 0;

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	// Disable depth/stencil testing by default
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	FVector2f ViewTranslation2D = Params.ViewOffset;

	// Draw each element
#if WITH_SLATE_DEBUGGING
	int32 NextRenderBatchIndex = BatchToDraw == -1 ? FirstBatchIndex : BatchToDraw;
#else
	int32 NextRenderBatchIndex = FirstBatchIndex;
#endif


	/*
		#todo-renderpasses This loop ends up with ugly logic.
		CustomDrawers will draw in their own renderpass. So we must remember to reopen the renderpass with the passed in Color/DepthStencil targets.
	*/
	while (NextRenderBatchIndex != INDEX_NONE)
	{
		VertexBufferPtr = &SourceVertexBuffer;
		IndexBufferPtr = &SourceIndexBuffer;

		if (!RHICmdList.IsInsideRenderPass())
		{
			// Restart the renderpass since the CustomDrawer or post-process may have changed it in last iteration
			FRHIRenderPassInfo RPInfo(BackBuffer.GetRenderTargetTexture(), ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthStencilTarget;
			if (DepthStencilTarget)
			{
				RPInfo.DepthStencilRenderTarget.Action = IsMemorylessTexture(DepthStencilTarget) ? EDepthStencilTargetActions::DontLoad_DontStore : EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil;
				RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			}
			else
			{
				RPInfo.DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
				RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilNop;
			}
			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("RestartingSlateDrawElements"));

			// Something may have messed with the viewport size so set it back to the full target.
			RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)BackBuffer.GetSizeXY().X, (float)BackBuffer.GetSizeXY().Y, 0.0f);

			// Re-apply render target states to the PSO initializer, since we've changed the depth/stencil target.
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		}
				
#if WITH_SLATE_VISUALIZERS
		FLinearColor BatchColor = FLinearColor(BatchColors.GetUnitVector());
#endif
		const FSlateRenderBatch& RenderBatch = RenderBatches[NextRenderBatchIndex];

		NextRenderBatchIndex = RenderBatch.NextBatchIndex;

#if WITH_SLATE_DEBUGGING
		if (BatchToDraw != -1)
		{
			break;
		}
#endif

		const FSlateShaderResource* ShaderResource = RenderBatch.ShaderResource;
		const ESlateBatchDrawFlag DrawFlags = RenderBatch.DrawFlags;
		const ESlateDrawEffect DrawEffects = RenderBatch.DrawEffects;
		const ESlateShader ShaderType = RenderBatch.ShaderType;
		const FShaderParams& ShaderParams = RenderBatch.ShaderParams;

		if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::Wireframe))
		{
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe>::GetRHI();
		}
		else
		{
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid>::GetRHI();
		}

		if (!RenderBatch.CustomDrawer)
		{
			FMatrix DynamicOffset = FTranslationMatrix::Make(FVector(RenderBatch.DynamicOffset.X, RenderBatch.DynamicOffset.Y, 0));
			const FMatrix ViewProjection = DynamicOffset * FMatrix(Params.ViewProjectionMatrix);

			UpdateScissorRect(
				RHICmdList,
#if STATS
				ScissorClips,
				StencilClips,
#endif
				StencilRef,
				MaskingID,
				BackBuffer,
				RenderBatch,
				ColorTarget,
				DepthStencilTarget,
				LastClippingState,
				ViewTranslation2D,
				GraphicsPSOInit,
				StencilVertexBuffer,
				ViewProjection,
				false);

			const uint32 PrimitiveCount = RenderBatch.DrawPrimitiveType == ESlateDrawPrimitive::LineList ? RenderBatch.NumIndices / 2 : RenderBatch.NumIndices / 3;
			check(ShaderResource == nullptr || !ShaderResource->Debug_IsDestroyed());
			ESlateShaderResource::Type ResourceType = ShaderResource ? ShaderResource->GetType() : ESlateShaderResource::Invalid;
			if (ResourceType != ESlateShaderResource::Material && ShaderType != ESlateShader::PostProcess)
			{
				check(RHICmdList.IsInsideRenderPass());
				check(RenderBatch.NumIndices > 0);
				TShaderRef<FSlateElementPS> PixelShader;

				const bool bUseInstancing = RenderBatch.InstanceCount > 1 && RenderBatch.InstanceData != nullptr;
				check(bUseInstancing == false);

#if WITH_SLATE_VISUALIZERS
				TShaderRef<FSlateDebugBatchingPS> BatchingPixelShader;
				if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
				{
					BatchingPixelShader = TShaderMapRef<FSlateDebugBatchingPS>(ShaderMap);
					PixelShader = BatchingPixelShader;
				}
				else
#endif
				{
					bool bIsVirtualTexture = false;

					// check if texture is using BC4 compression and set shader to render grayscale
					bool bUseTextureGrayscale = false;


					if ((ShaderResource != nullptr) && (ResourceType == ESlateShaderResource::TextureObject))
					{
						FSlateBaseUTextureResource* TextureObjectResource = const_cast<FSlateBaseUTextureResource*>(static_cast<const FSlateBaseUTextureResource*>(ShaderResource));
						
						if (UTexture* TextureObj = TextureObjectResource->GetTextureObject())
						{
							bIsVirtualTexture = TextureObj->IsCurrentlyVirtualTextured();

							if (TextureObj->CompressionSettings == TC_Alpha)
							{
								bUseTextureGrayscale = true;
							}
						}
					}

					PixelShader = GetTexturePixelShader(ShaderMap, ShaderType, DrawEffects, bUseTextureGrayscale, bIsVirtualTexture);
				}

#if WITH_SLATE_VISUALIZERS
				if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
				}
				else if (CVarShowSlateOverdraw.GetValueOnRenderThread() != 0)
				{
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
				}
				else
#endif
				{
					GraphicsPSOInit.BlendState =
						EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::NoBlending)
						? TStaticBlendState<>::GetRHI()
						: (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::PreMultipliedAlpha)
							? TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()
							: TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI())
						;
				}

				if (EnumHasAllFlags(DrawFlags, ESlateBatchDrawFlag::Wireframe) || Params.bWireFrame)
				{
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Wireframe>::GetRHI();

					if (Params.bWireFrame)
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					}
				}
				else
				{
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid>::GetRHI();
				}

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSlateVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GlobalVertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

				FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

#if WITH_SLATE_VISUALIZERS
				if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
				{
					BatchingPixelShader->SetBatchColor(BatchedParameters, BatchColor);
				}
#endif

				FRHISamplerState* SamplerState = BilinearClamp;
				FRHITexture* TextureRHI = GWhiteTexture->TextureRHI;
				bool bIsVirtualTexture = false;
				FTextureResource* TextureResource = nullptr;

				if (ShaderResource)
				{
					ETextureSamplerFilter Filter = ETextureSamplerFilter::Bilinear;

					if (ResourceType == ESlateShaderResource::TextureObject)
					{
						FSlateBaseUTextureResource* TextureObjectResource = (FSlateBaseUTextureResource*)ShaderResource;
						if (UTexture* TextureObj = TextureObjectResource->GetTextureObject())
						{
							TextureObjectResource->CheckForStaleResources();

							TextureRHI = TextureObjectResource->AccessRHIResource();

							// This can upset some RHIs, so use transparent black texture until it's valid.
							// these can be temporarily invalid when recreating them / invalidating their streaming
							// state.
							if (TextureRHI == nullptr)
							{
								// We use transparent black here, because it's about to become valid - probably, and flashing white
								// wouldn't be ideal.
								TextureRHI = GTransparentBlackTexture->TextureRHI;
							}

							TextureResource = TextureObj->GetResource();

							Filter = GetSamplerFilter(TextureObj);
							bIsVirtualTexture = TextureObj->IsCurrentlyVirtualTextured();
						}
					}
					else
					{
						FRHITexture* NativeTextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)ShaderResource)->GetTypedResource();
						// Atlas textures that have no content are never initialized but null textures are invalid on many platforms.
						TextureRHI = NativeTextureRHI ? NativeTextureRHI : (FRHITexture*)GWhiteTexture->TextureRHI;
					}

					SamplerState = GetSamplerState(DrawFlags, Filter);
				}

				{
					if (bIsVirtualTexture && (TextureResource != nullptr))
					{
						PixelShader->SetVirtualTextureParameters(BatchedParameters, static_cast<FVirtualTexture2DResource*>(TextureResource));
					}
					else
					{
						PixelShader->SetTexture(BatchedParameters, TextureRHI, SamplerState);
					}
					
					PixelShader->SetShaderParams(BatchedParameters, ShaderParams);
					const float FinalGamma = EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::ReverseGamma) ? (1.0f / EngineGamma) : EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma;
					const float FinalContrast = EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma) ? 1 : DisplayContrast;
					PixelShader->SetDisplayGammaAndInvertAlphaAndContrast(BatchedParameters, FinalGamma, EnumHasAllFlags(DrawEffects, ESlateDrawEffect::InvertAlpha) ? 1.0f : 0.0f, FinalContrast);

					RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);
				}
				{
					GlobalVertexShader->SetViewProjection(BatchedParameters, FMatrix44f(ViewProjection));
					RHICmdList.SetBatchedShaderParameters(GlobalVertexShader.GetVertexShader(), BatchedParameters);
				}

				{
					// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
					RHICmdList.SetStreamSource(0, VertexBufferPtr->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
					RHICmdList.DrawIndexedPrimitive(IndexBufferPtr->IndexBufferRHI, 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, RenderBatch.InstanceCount);
				}
			}
			else if (GEngine && ShaderResource && ShaderResource->GetType() == ESlateShaderResource::Material && ShaderType != ESlateShader::PostProcess)
			{
				check(RHICmdList.IsInsideRenderPass());

				check(RenderBatch.NumIndices > 0);
				// Note: This code is only executed if the engine is loaded (in early loading screens attempting to use a material is unsupported
				int32 ActiveSceneIndex = (int32)RenderBatch.SceneIndex;

				// We are assuming at this point that the SceneIndex from the batch is either -1, meaning no scene or a valid scene.
				// We set up the "no scene" option as the last SceneView in the array above.
				if (RenderBatch.SceneIndex == -1)
				{
					ActiveSceneIndex = NumScenes - 1;
				}
				else if (RenderBatch.SceneIndex >= ResourceManager->GetSceneCount())
				{
					// Ideally we should never hit this scenario, but given that Paragon may be using cached
					// render batches and is running into this daily, for this branch we should
					// just ignore the scene if the index is invalid. Note that the
					// MaterialParameterCollections will not be correct for this scene, should they be
					// used.
					ActiveSceneIndex = NumScenes - 1;
#if UE_BUILD_DEBUG && WITH_EDITOR
					UE_LOG(LogSlate, Error, TEXT("Invalid scene index in batch: %d of %d known scenes!"), RenderBatch.SceneIndex, ResourceManager->GetSceneCount());
#endif
				}

				// Handle the case where we skipped out early above
				if (SceneViews[ActiveSceneIndex] == nullptr)
				{
					continue;
				}

				const FSceneView& ActiveSceneView = *SceneViews[ActiveSceneIndex];

				FSlateMaterialResource* MaterialShaderResource = (FSlateMaterialResource*)ShaderResource;
				if (const FMaterialRenderProxy* MaterialRenderProxy = MaterialShaderResource->GetRenderProxy())
				{
					SLATE_DRAW_EVENTF(RHICmdList, MaterialBatch, TEXT("Slate Material: %s"), *MaterialRenderProxy->GetMaterialName());

					MaterialShaderResource->CheckForStaleResources();

					const bool bUseInstancing = RenderBatch.InstanceCount > 0 && RenderBatch.InstanceData != nullptr;

					TShaderRef<FSlateMaterialShaderVS> VertexShader;
					TShaderRef<FSlateMaterialShaderPS> PixelShader;

					FMaterialShaderTypes ShaderTypesToGet;
					ChooseMaterialShaderTypes(ShaderType, bUseInstancing, ShaderTypesToGet);
					const FMaterial* EffectiveMaterial = nullptr;

					const ERHIFeatureLevel::Type ViewFeatureLevel = ActiveSceneView.GetFeatureLevel();
					while(MaterialRenderProxy)
					{
						const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(ViewFeatureLevel);
						FMaterialShaders Shaders;
						if (Material && Material->TryGetShaders(ShaderTypesToGet, nullptr, Shaders))
						{
							EffectiveMaterial = Material;
							Shaders.TryGetVertexShader(VertexShader);
							Shaders.TryGetPixelShader(PixelShader);
							break;
						}

						MaterialRenderProxy = MaterialRenderProxy->GetFallback(ViewFeatureLevel);
					}

					FRHIUniformBuffer* SceneTextureUniformBuffer = GetSceneTextureExtracts().GetUniformBuffer();

					if (VertexShader.IsValid() && PixelShader.IsValid() && SceneTextureUniformBuffer)
					{
						check(EffectiveMaterial);
						const FUniformBufferStaticBindings StaticUniformBuffers(SceneTextureUniformBuffer);
						SCOPED_UNIFORM_BUFFER_STATIC_BINDINGS(RHICmdList, StaticUniformBuffers);

						FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

#if WITH_SLATE_VISUALIZERS
						if (CVarShowSlateBatching.GetValueOnRenderThread() != 0)
						{
							TShaderMapRef<FSlateDebugBatchingPS> BatchingPixelShader(ShaderMap);

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GlobalVertexShader.GetVertexShader();
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = BatchingPixelShader.GetPixelShader();
							GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();

							BatchingPixelShader->SetBatchColor(BatchedParameters, BatchColor);
						}
						else if (CVarShowSlateOverdraw.GetValueOnRenderThread() != 0)
						{
							TShaderMapRef<FSlateDebugOverdrawPS> OverdrawPixelShader(ShaderMap);

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GlobalVertexShader.GetVertexShader();
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = OverdrawPixelShader.GetPixelShader();
							GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
						}
#endif
						{
							PixelShader->SetBlendState(GraphicsPSOInit, EffectiveMaterial);
							FSlateShaderResource* MaskResource = MaterialShaderResource->GetTextureMaskResource();
							if (MaskResource && IsOpaqueOrMaskedBlendMode(*EffectiveMaterial))
							{
								// Font materials require some form of translucent blending
								GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
							}

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseInstancing ? GSlateInstancedVertexDeclaration.VertexDeclarationRHI : GSlateVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
							GraphicsPSOInit.PrimitiveType = GetRHIPrimitiveType(RenderBatch.DrawPrimitiveType);

							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

							{

								PixelShader->SetParameters(BatchedParameters, ActiveSceneView, MaterialRenderProxy, EffectiveMaterial, ShaderParams);
								const float FinalGamma = EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::ReverseGamma) ? 1.0f / EngineGamma : EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma) ? 1.0f : DisplayGamma;
								const float FinalContrast = EnumHasAnyFlags(DrawFlags, ESlateBatchDrawFlag::NoGamma) ? 1 : DisplayContrast;
								PixelShader->SetDisplayGammaAndContrast(BatchedParameters, FinalGamma, FinalContrast);
								const bool bDrawDisabled = EnumHasAllFlags(DrawEffects, ESlateDrawEffect::DisabledEffect);
								PixelShader->SetDrawFlags(BatchedParameters, bDrawDisabled);

								if (MaskResource)
								{
									FTexture2DRHIRef TextureRHI;
									TextureRHI = ((TSlateTexture<FTexture2DRHIRef>*)MaskResource)->GetTypedResource();

									PixelShader->SetAdditionalTexture(BatchedParameters, TextureRHI, BilinearClamp);
								}

								RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);
							}
							{
								VertexShader->SetViewProjection(BatchedParameters, FMatrix44f(ViewProjection));
								VertexShader->SetMaterialShaderParameters(BatchedParameters, ActiveSceneView, MaterialRenderProxy, EffectiveMaterial);
								RHICmdList.SetBatchedShaderParameters(VertexShader.GetVertexShader(), BatchedParameters);
							}
						}

						{
							if (bUseInstancing)
							{
								uint32 InstanceCount = RenderBatch.InstanceCount;

								RenderBatch.InstanceData->BindStreamSource(RHICmdList, 1, RenderBatch.InstanceOffset);

								// for RHIs that can't handle VertexOffset, we need to offset the stream source each time

								RHICmdList.SetStreamSource(0, VertexBufferPtr->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
								RHICmdList.DrawIndexedPrimitive(IndexBufferPtr->IndexBufferRHI, 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, InstanceCount);
							}
							else
							{
								RHICmdList.SetStreamSource(1, nullptr, 0);

								// for RHIs that can't handle VertexOffset, we need to offset the stream source each time
								RHICmdList.SetStreamSource(0, VertexBufferPtr->VertexBufferRHI, RenderBatch.VertexOffset * sizeof(FSlateVertex));
								RHICmdList.DrawIndexedPrimitive(IndexBufferPtr->IndexBufferRHI, 0, 0, RenderBatch.NumVertices, RenderBatch.IndexOffset, PrimitiveCount, 1);

							}
						}
					}
				}
			}
			else if (ShaderType == ESlateShader::PostProcess)
			{
				SLATE_DRAW_EVENT(RHICmdList, PostProcess);
				RHICmdList.EndRenderPass();

				const FVector4f QuadPositionData = ShaderParams.PixelParams;

				FPostProcessRectParams RectParams;
				RectParams.SourceTexture = PostProcessTexture;
				RectParams.SourceRect = FSlateRect((float)Params.ViewRect.Min.X, (float)Params.ViewRect.Min.Y, (float)Params.ViewRect.Max.X, (float)Params.ViewRect.Max.Y);
				RectParams.DestRect = FSlateRect(QuadPositionData.X, QuadPositionData.Y, QuadPositionData.Z, QuadPositionData.W);
				RectParams.SourceTextureSize = PostProcessTexture->GetSizeXY();
				RectParams.CornerRadius = ShaderParams.PixelParams3;
				RectParams.UITarget = Params.UITarget;
				RectParams.HDRDisplayColorGamut = Params.HDRDisplayColorGamut;

				RectParams.RestoreStateFunc = [&](FRHICommandListImmediate&InRHICmdList, FGraphicsPipelineStateInitializer& InGraphicsPSOInit, FRHIRenderPassInfo& RPInfo) {
					return UpdateScissorRect(
						InRHICmdList,
#if STATS
						ScissorClips,
						StencilClips,
#endif
						RectParams.StencilRef,
						MaskingID,
						BackBuffer,
						RenderBatch,
						RPInfo,
						DepthStencilTarget,
						LastClippingState,
						ViewTranslation2D,
						InGraphicsPSOInit,
						StencilVertexBuffer,
						FMatrix(Params.ViewProjectionMatrix),
						true);
				};

				RectParams.StencilRef = StencilRef;

				FBlurRectParams BlurParams;
				BlurParams.KernelSize = ShaderParams.PixelParams2.X;
				BlurParams.Strength = ShaderParams.PixelParams2.Y;
				BlurParams.DownsampleAmount = ShaderParams.PixelParams2.Z;

				PostProcessor->BlurRect(RHICmdList, RendererModule, BlurParams, RectParams);

				check(RHICmdList.IsOutsideRenderPass());
				// Render pass for slate elements will be restarted on a next loop iteration if any
			}
		}
		else
		{
			ICustomSlateElement* CustomDrawer = RenderBatch.CustomDrawer;
			if (CustomDrawer)
			{
				// CustomDrawers will change the rendertarget. So we must close any outstanding renderpasses.
				// Render pass for slate elements will be restarted on a next loop iteration if any
				RHICmdList.EndRenderPass();	

				SLATE_DRAW_EVENT(RHICmdList, CustomDrawer);

				// Disable scissor rect. A previous draw element may have had one
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				LastClippingState = nullptr;

				ICustomSlateElement::FSlateCustomDrawParams CustomDrawParams = ICustomSlateElement::FSlateCustomDrawParams();
				CustomDrawParams.ViewProjectionMatrix = Params.ViewProjectionMatrix;
				CustomDrawParams.ViewOffset = Params.ViewOffset;
				CustomDrawParams.ViewRect = Params.ViewRect;
				CustomDrawParams.HDRDisplayColorGamut = Params.HDRDisplayColorGamut;
				CustomDrawParams.UsedSlatePostBuffers = Params.UsedSlatePostBuffers;
				CustomDrawParams.bWireFrame = Params.bWireFrame;
				CustomDrawParams.bIsHDR = Params.bIsHDR;

				// This element is custom and has no Slate geometry.  Tell it to render itself now
				if (CustomDrawer->UsesAdditionalRHIParams())
				{
					ICustomSlateElementRHI* CustomDrawerRHI = static_cast<ICustomSlateElementRHI*>(CustomDrawer);
					CustomDrawerRHI->Draw_RHIRenderThread(RHICmdList, BackBuffer.GetRenderTargetTexture(), CustomDrawParams, FSlateRHIRenderingPolicyInterface(this));
				}
				else
				{
					CustomDrawer->Draw_RenderThread(RHICmdList, &BackBuffer.GetRenderTargetTexture(), CustomDrawParams);
				}

				//We reset the maskingID here because otherwise the RT might not get re-set in the lines above see: if (bClearStencil || bForceStateChange)
				MaskingID = 0;
			}
		} // CustomDrawer
	}

	// Don't do color correction on iOS or Android, we don't have the GPU overhead for it.
#if !(PLATFORM_IOS || PLATFORM_ANDROID)
	if (bApplyColorDeficiencyCorrection && GSlateColorDeficiencyType != EColorVisionDeficiency::NormalVision && GSlateColorDeficiencySeverity > 0)
	{
		if (RHICmdList.IsInsideRenderPass())
		{
			RHICmdList.EndRenderPass();
		}

		FPostProcessRectParams RectParams;
		RectParams.SourceTexture = BackBuffer.GetRenderTargetTexture();
		RectParams.SourceRect = FSlateRect(0, 0, BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y);
		RectParams.DestRect = FSlateRect(0, 0, BackBuffer.GetSizeXY().X, BackBuffer.GetSizeXY().Y);
		RectParams.SourceTextureSize = BackBuffer.GetSizeXY();
		RectParams.CornerRadius = FVector4f(0.f, 0.f, 0.f, 0.f);

		PostProcessor->ColorDeficiency(RHICmdList, RendererModule, RectParams);

		FRHIRenderPassInfo RPInfo(ColorTarget, ERenderTargetActions::Load_Store);
		RPInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthStencilTarget;

		// @todo refactor this.
		// ColorDeficiency has self-contained renderpasses. To avoid starting an empty renderpass we do not
		// restart the renderpass here.
	}
#endif

	// Disable scissor rect we no longer need this.
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	// Disable depth/stencil testing once we're done also.
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	for (int i = 0; i < NumScenes; i++)
	{
		// Don't need to delete SceneViews b/c the SceneViewFamily will delete it when it goes away.
		delete SceneViewFamilyContexts[i];
	}

	SceneViews.Empty();
	SceneViewFamilyContexts.Empty();

	INC_DWORD_STAT_BY(STAT_SlateScissorClips, ScissorClips);
	INC_DWORD_STAT_BY(STAT_SlateStencilClips, StencilClips);

	// Disable scissor rect. 
	// This fixes drawing on Metal when the last drawn element used a valid scissor rect
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
}

ETextureSamplerFilter FSlateRHIRenderingPolicy::GetSamplerFilter(const UTexture* Texture) const
{
	// Default to point filtering.
	ETextureSamplerFilter Filter = ETextureSamplerFilter::Point;

	switch (Texture->Filter)
	{
	case TF_Nearest: 
		Filter = ETextureSamplerFilter::Point; 
		break;
	case TF_Bilinear:
		Filter = ETextureSamplerFilter::Bilinear; 
		break;
	case TF_Trilinear: 
		Filter = ETextureSamplerFilter::Trilinear; 
		break;

		// TF_Default
	default:
		// Use LOD group value to find proper filter setting.
		if (Texture->LODGroup < TextureLODGroups.Num())
		{
			Filter = TextureLODGroups[Texture->LODGroup].Filter;
		}
	}

	return Filter;
}

TShaderRef<FSlateElementPS> FSlateRHIRenderingPolicy::GetTexturePixelShader(FGlobalShaderMap* ShaderMap, ESlateShader ShaderType, ESlateDrawEffect DrawEffects, bool bUseTextureGrayscale, bool bIsVirtualTexture)
{
	TShaderRef<FSlateElementPS> PixelShader;

#if WITH_SLATE_VISUALIZERS
	if ( CVarShowSlateOverdraw.GetValueOnRenderThread() != 0 )
	{
		PixelShader = TShaderMapRef<FSlateDebugOverdrawPS>(ShaderMap);
	}
	else
#endif
	{
	const bool bDrawDisabled = EnumHasAllFlags( DrawEffects, ESlateDrawEffect::DisabledEffect );
	const bool bUseTextureAlpha = !EnumHasAllFlags( DrawEffects, ESlateDrawEffect::IgnoreTextureAlpha );

	if ( bDrawDisabled )
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				if ( bIsVirtualTexture )
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true, true, true> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true, false, true> >(ShaderMap);
					}
				}
				else
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true, true, false> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, true, false, false> >(ShaderMap);
					}
				}
			}
			else
			{
				if ( bIsVirtualTexture )
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false, true, true> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false, false, true> >(ShaderMap);
					}
				}
				else
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false, true, false> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, true, false, false, false> >(ShaderMap);
					}
				}
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, true> >(ShaderMap);
			}
			else
			{
				PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Border, true, false> >(ShaderMap);
			}
			break;
		case ESlateShader::GrayscaleFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::GrayscaleFont, true> >(ShaderMap);
			break;
		case ESlateShader::ColorFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::ColorFont, true> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, true> >(ShaderMap);
			break;
		case ESlateShader::RoundedBox:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::RoundedBox, true> >(ShaderMap);
			break;
		case ESlateShader::SdfFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::SdfFont, true> >(ShaderMap);
			break;
		case ESlateShader::MsdfFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::MsdfFont, true> >(ShaderMap);
			break;
		}
	}
	else
	{
		switch ( ShaderType )
		{
		default:
		case ESlateShader::Default:
			if ( bUseTextureAlpha )
			{
				if ( bIsVirtualTexture )
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true, true, true> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true, false, true> >(ShaderMap);
					}
				}
				else
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true, true, false> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, true, false, false> >(ShaderMap);
					}
				}
			}
			else
			{
				if ( bIsVirtualTexture )
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false, true, true> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false, false, true> >(ShaderMap);
					}
				}
				else
				{
					if (bUseTextureGrayscale)
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false, true, false> >(ShaderMap);
					}
					else
					{
						PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Default, false, false, false, false> >(ShaderMap);
					}
				}
			}
			break;
		case ESlateShader::Border:
			if ( bUseTextureAlpha )
			{
				PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, true> >(ShaderMap);
			}
			else
			{
				PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::Border, false, false> >(ShaderMap);
			}
			break;
		case ESlateShader::GrayscaleFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::GrayscaleFont, false> >(ShaderMap);
			break;
		case ESlateShader::ColorFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::ColorFont, false> >(ShaderMap);
			break;
		case ESlateShader::LineSegment:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::LineSegment, false> >(ShaderMap);
			break;
		case ESlateShader::RoundedBox:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::RoundedBox, false> >(ShaderMap);
			break;
		case ESlateShader::SdfFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::SdfFont, false> >(ShaderMap);
			break;
		case ESlateShader::MsdfFont:
			PixelShader = TShaderMapRef<TSlateElementPS<ESlateShader::MsdfFont, false> >(ShaderMap);
			break;
		}
	}
	}

#undef PixelShaderLookupTable

	return PixelShader;
}

void FSlateRHIRenderingPolicy::ChooseMaterialShaderTypes(ESlateShader ShaderType, bool bUseInstancing, FMaterialShaderTypes& OutShaderTypes)
{
	switch (ShaderType)
	{
	case ESlateShader::Default:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::Default>>();
		break;
	case ESlateShader::Border:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::Border>>();
		break;
	case ESlateShader::GrayscaleFont:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::GrayscaleFont>>();
		break;
	case ESlateShader::ColorFont:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::ColorFont>>();
		break;
	case ESlateShader::Custom:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::Custom>>();
		break;
	case ESlateShader::RoundedBox:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::RoundedBox>>();
		break;
	case ESlateShader::SdfFont:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::SdfFont>>();
		break;
	case ESlateShader::MsdfFont:
		OutShaderTypes.AddShaderType<TSlateMaterialShaderPS<ESlateShader::MsdfFont>>();
		break;
	default:
		checkf(false, TEXT("Unsupported Slate shader type for use with materials"));
		break;
	}

	if (bUseInstancing)
	{
		OutShaderTypes.AddShaderType<TSlateMaterialShaderVS<true>>();
	}
	else
	{
		OutShaderTypes.AddShaderType<TSlateMaterialShaderVS<false>>();
	}
}

EPrimitiveType FSlateRHIRenderingPolicy::GetRHIPrimitiveType(ESlateDrawPrimitive SlateType)
{
	switch(SlateType)
	{
	case ESlateDrawPrimitive::LineList:
		return PT_LineList;
	case ESlateDrawPrimitive::TriangleList:
	default:
		return PT_TriangleList;
	}

};


void FSlateRHIRenderingPolicy::AddSceneAt(FSceneInterface* Scene, int32 Index)
{
	ResourceManager->AddSceneAt(Scene, Index);
}

void FSlateRHIRenderingPolicy::ClearScenes()
{
	ResourceManager->ClearScenes();
}

void FSlateRHIRenderingPolicy::FlushGeneratedResources()
{
	PostProcessor->ReleaseRenderTargets();
}

void FSlateRHIRenderingPolicy::TickPostProcessResources()
{
	PostProcessor->TickPostProcessResources();
}

void FSlateRHIRenderingPolicy::BlurRectExternal(FRHICommandListImmediate& RHICmdList, FRHITexture* BlurSrc, FRHITexture* BlurDst, FIntRect SrcRect, FIntRect DstRect, float BlurStrength) const
{
	SLATE_DRAW_EVENT(RHICmdList, PostProcess);

	FIntPoint BlurDstExtent = FIntPoint(DstRect.Width(), DstRect.Height());

	// If the radius isn't set, auto-compute it based on the strength
	int32 OutKernelSize = FMath::RoundToInt(BlurStrength * 3.f);

	// Downsample if needed
	int32 OutDownsampleAmount = 0;
	if (OutKernelSize > 9)
	{
		OutDownsampleAmount = OutKernelSize >= 64 ? 4 : 2;
		OutKernelSize /= OutDownsampleAmount;
	}

	// Kernel sizes must be odd
	if (OutKernelSize % 2 == 0)
	{
		++OutKernelSize;
	}

	float ComputedStrength = FMath::Max(.5f, BlurStrength);

	int32 RenderTargetWidth = BlurDstExtent.X;
	int32 RenderTargetHeight = BlurDstExtent.Y;
	
	if (OutDownsampleAmount > 0)
	{
		RenderTargetWidth = FMath::DivideAndRoundUp(RenderTargetWidth, OutDownsampleAmount);
		RenderTargetHeight = FMath::DivideAndRoundUp(RenderTargetHeight, OutDownsampleAmount);
		ComputedStrength /= OutDownsampleAmount;
	}

	OutKernelSize = FMath::Clamp(OutKernelSize, 3, 255 /*MaxKernelSize*/);

	FVector4f PostProcessData = FVector4f((float)OutKernelSize, ComputedStrength, (float)RenderTargetWidth, (float)RenderTargetHeight);

	FVector2f TopLeft = FVector2f::ZeroVector;
	FVector2f BotRight = FVector2f(BlurDstExtent.X, BlurDstExtent.Y);

	FPostProcessRectParams RectParams;
	RectParams.SourceTexture = BlurSrc;
	RectParams.SourceRect = FSlateRect((float)SrcRect.Min.X, (float)SrcRect.Min.Y, (float)SrcRect.Max.X, (float)SrcRect.Max.Y);
	RectParams.DestRect = FSlateRect(TopLeft.X, TopLeft.Y, BotRight.X, BotRight.Y);
	RectParams.SourceTextureSize = BlurSrc->GetSizeXY();
	RectParams.CornerRadius = FVector4f(0, 0, 0, 0);
	RectParams.DestTexture = BlurDst;
	RectParams.PostProcessDest = EPostProcessDestination::DestTexture;

	FBlurRectParams BlurParams;
	BlurParams.KernelSize = PostProcessData.X;
	BlurParams.Strength = PostProcessData.Y;
	BlurParams.DownsampleAmount = OutDownsampleAmount;

	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);
	PostProcessor->BlurRect(RHICmdList, RendererModule, BlurParams, RectParams);

	RHICmdList.Transition(FRHITransitionInfo(BlurDst, ERHIAccess::RTV, ERHIAccess::SRVGraphics));
}
	
