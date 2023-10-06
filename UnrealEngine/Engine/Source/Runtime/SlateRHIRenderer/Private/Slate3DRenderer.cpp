// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate3DRenderer.h"
#include "Fonts/FontCache.h"
#include "Materials/MaterialRenderProxy.h"
#include "Widgets/SWindow.h"
#include "SceneUtils.h"
#include "SlateRHIRenderer.h"
#include "Rendering/ElementBatcher.h"
#include "Types/SlateVector2.h"

DECLARE_GPU_STAT_NAMED(Slate3D, TEXT("Slate 3D"));

FSlate3DRenderer::FSlate3DRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, bool bUseGammaCorrection )
	: SlateFontServices( InSlateFontServices )
	, ResourceManager( InResourceManager )
	, bRenderTargetWasCleared(false)
{
	const int32 InitialBufferSize = 200;
	RenderTargetPolicy = MakeShareable( new FSlateRHIRenderingPolicy( SlateFontServices, ResourceManager, InitialBufferSize ) );
	RenderTargetPolicy->SetUseGammaCorrection( bUseGammaCorrection );

	ElementBatcher = MakeUnique<FSlateElementBatcher>(RenderTargetPolicy.ToSharedRef());
}

void FSlate3DRenderer::Cleanup()
{
	if ( RenderTargetPolicy.IsValid() )
	{
		RenderTargetPolicy->ReleaseResources();
	}

	if (IsInGameThread())
	{
		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_RENDER_COMMAND(FSlate3DRenderer_Cleanup)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				DepthStencil.SafeRelease();
			}
		);
	}
	else
	{
		DepthStencil.SafeRelease();
	}

	BeginCleanup(this);
}

void FSlate3DRenderer::SetUseGammaCorrection(bool bUseGammaCorrection)
{
	RenderTargetPolicy->SetUseGammaCorrection(bUseGammaCorrection);
}

void FSlate3DRenderer::SetApplyColorDeficiencyCorrection(bool bApplyColorCorrection)
{
	RenderTargetPolicy->SetApplyColorDeficiencyCorrection(bApplyColorCorrection);
}

FSlateDrawBuffer& FSlate3DRenderer::AcquireDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NUM_DRAW_BUFFERS;
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];

	while (!Buffer->Lock())
	{
		FlushRenderingCommands();

		UE_LOG(LogSlate, Log, TEXT("Slate: Had to block on waiting for a draw buffer"));
		FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;

		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	Buffer->ClearBuffer();

	return *Buffer;
}

void FSlate3DRenderer::ReleaseDrawBuffer(FSlateDrawBuffer& InWindowDrawBuffer)
{
#if DO_CHECK
	bool bFound = false;
	for (int32 Index = 0; Index < NUM_DRAW_BUFFERS; ++Index)
	{
		if (&DrawBuffers[Index] == &InWindowDrawBuffer)
		{
			bFound = true;
			break;
		}
	}
	ensureMsgf(bFound, TEXT("It release a DrawBuffer that is not a member of the Slate3DRenderer"));
#endif

	FSlateDrawBuffer* DrawBuffer = &InWindowDrawBuffer;
	ENQUEUE_RENDER_COMMAND(SlateReleaseDrawBufferCommand)(
		[DrawBuffer](FRHICommandListImmediate& RHICmdList)
		{
			FSlateReleaseDrawBufferCommand::ReleaseDrawBuffer(RHICmdList, DrawBuffer);
		}
	);
}

void FSlate3DRenderer::DrawWindow_GameThread(FSlateDrawBuffer& DrawBuffer)
{
	check( IsInGameThread() );

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetGameThreadFontCache();

	const TArray<TSharedRef<FSlateWindowElementList>>& WindowElementLists = DrawBuffer.GetWindowElementLists();

	for (int32 WindowIndex = 0; WindowIndex < WindowElementLists.Num(); WindowIndex++)
	{
		FSlateWindowElementList& ElementList = *WindowElementLists[WindowIndex];

		SWindow* Window = ElementList.GetPaintWindow();

		if (Window)
		{
			const FVector2D WindowSize = Window->GetSizeInScreen();
			if (WindowSize.X > 0.0 && WindowSize.Y > 0.0)
			{
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements(ElementList);

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();
			}
		}
	}
}

void FSlate3DRenderer::DrawWindowToTarget_RenderThread(FRHICommandListImmediate& InRHICmdList, const FRenderThreadUpdateContext& Context)
{
	check(IsInRenderingThread());
	QUICK_SCOPE_CYCLE_COUNTER(Stat_Slate_WidgetRendererRenderThread);
	SCOPED_DRAW_EVENT( InRHICmdList, SlateRenderToTarget );
	SCOPED_GPU_STAT(InRHICmdList, Slate3D);

	checkSlow(Context.RenderTarget);

	//Update cached uniforms to avoid an ensure being thrown due to a null shader map (UE-110263)
	//Same fix on UE5 stream - CL16165057 
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	const TArray<TSharedRef<FSlateWindowElementList>>& WindowsToDraw = Context.WindowDrawBuffer->GetWindowElementLists();

	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	RenderTargetPolicy->BeginDrawingWindows();

	// Set render target and clear.
	FRHITexture* RTTextureRHI = Context.RenderTarget->GetRenderTargetTexture();
	InRHICmdList.Transition(FRHITransitionInfo(RTTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));
	
	FRHIRenderPassInfo RPInfo(RTTextureRHI, ERenderTargetActions::Load_Store);
	if (Context.bClearTarget)
	{
		RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
	}

	for (int32 WindowIndex = 0; WindowIndex < WindowsToDraw.Num(); WindowIndex++)
	{
		FSlateWindowElementList& WindowElementList = *WindowsToDraw[WindowIndex];

		FSlateBatchData& BatchData = WindowElementList.GetBatchData();

		if (BatchData.GetRenderBatches().Num() > 0)
		{
			RenderTargetPolicy->BuildRenderingBuffers(InRHICmdList, BatchData);
		
			FVector2D DrawOffset = Context.WindowDrawBuffer->ViewOffset;

			FMatrix ProjectionMatrix = FSlateRHIRenderer::CreateProjectionMatrix(RTTextureRHI->GetSizeX(), RTTextureRHI->GetSizeY());
			FMatrix ViewOffset = FTranslationMatrix::Make(FVector(DrawOffset, 0.0));
			ProjectionMatrix = ViewOffset * ProjectionMatrix;

			FSlateBackBuffer BackBufferTarget(Context.RenderTarget->GetRenderTargetTexture(), FIntPoint(RTTextureRHI->GetSizeX(), RTTextureRHI->GetSizeY()));

			FSlateRenderingParams DrawOptions(ProjectionMatrix, FGameTime::CreateDilated(Context.RealTimeSeconds, Context.DeltaRealTimeSeconds, Context.WorldTimeSeconds, Context.DeltaTimeSeconds));
			// The scene renderer will handle it in this case
			DrawOptions.ViewOffset = UE::Slate::CastToVector2f(DrawOffset);

			FTexture2DRHIRef ColorTarget = Context.RenderTarget->GetRenderTargetTexture();

			if (BatchData.IsStencilClippingRequired())
			{
				if (!DepthStencil.IsValid() || ColorTarget->GetSizeXY() != DepthStencil->GetSizeXY())
				{
					DepthStencil.SafeRelease();

					const FRHITextureCreateDesc Desc =
						FRHITextureCreateDesc::Create2D(TEXT("SlateWindowDepthStencil"))
						.SetExtent(ColorTarget->GetSizeXY())
						.SetFormat(PF_DepthStencil)
						.SetClearValue(FClearValueBinding::DepthZero)
						.SetFlags(ETextureCreateFlags::DepthStencilTargetable | ETextureCreateFlags::ShaderResource);

					DepthStencil = RHICreateTexture(Desc);

					check(IsValidRef(DepthStencil));
				}
			}

			// Ideally we'd have a single render pass for all the windows, but this code reuses a single vertex buffer for each draw, which it updates above in BuildRenderingBuffers(),
			// and we can't upload data during a render pass. We need to rewrite this to upload all the data to the buffer first and use offsets for each draw.
			InRHICmdList.BeginRenderPass(RPInfo, TEXT("Slate3D"));

			RenderTargetPolicy->DrawElements(
				InRHICmdList,
				BackBufferTarget,
				ColorTarget,
				ColorTarget,
				DepthStencil,
				BatchData.GetFirstRenderBatchIndex(),
				BatchData.GetRenderBatches(),
				DrawOptions
			);

			// FSlateRHIRenderingPolicy::DrawElements can close the render pass on its own sometimes, so don't do it again.
			if (InRHICmdList.IsInsideRenderPass())
			{
				InRHICmdList.EndRenderPass();
			}

			// each time we do draw content, we reset the flag
			bRenderTargetWasCleared = false;
		}
		// If we have no render command and it's the first clear, we call Begin End to force the clear of the render target. Otherwise, we end up not updating the buffer when all Widget are invisible.
		else if(!bRenderTargetWasCleared)
		{
			InRHICmdList.BeginRenderPass(RPInfo, TEXT("Slate3D")); 
			if (InRHICmdList.IsInsideRenderPass())
			{
				InRHICmdList.EndRenderPass();
			}
			bRenderTargetWasCleared = true;
		}
	}

	FSlateEndDrawingWindowsCommand::EndDrawingWindows(InRHICmdList, Context.WindowDrawBuffer, *RenderTargetPolicy);
	InRHICmdList.Transition(FRHITransitionInfo(RTTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));

	// Enqueue a command to keep "this" alive.
	InRHICmdList.EnqueueLambda([Self = SharedThis(this)](FRHICommandListImmediate&){});
}
