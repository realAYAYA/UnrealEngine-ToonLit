// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/DebugCanvas.h"
#include "Engine/World.h"
#include "RenderingThread.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "EngineFontServices.h"
#include "Framework/Application/SlateApplication.h"
#include "IStereoLayers.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "StereoRendering.h"
#include "Slate/SceneViewport.h"
#include "IXRTrackingSystem.h"
#include "ISpectatorScreenController.h"
#include "IHeadMountedDisplay.h"
#include "RenderTargetPool.h"
#include "ViewportClient.h"

/**
 * Simple representation of the backbuffer that the debug canvas renders to
 * This class may only be accessed from the render thread
 */
class FSlateCanvasRenderTarget : public FRenderTarget
{
public:
	/** FRenderTarget interface */
	virtual FIntPoint GetSizeXY() const
	{
		return ViewRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture( FRHITexture* InRHIRef )
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect( const FIntRect& InViewRect ) 
	{ 
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const 
	{
		return ViewRect; 
	}
private:
	FIntRect ViewRect;
};

#define INVALID_LAYER_ID UINT_MAX

FDebugCanvasDrawer::FDebugCanvasDrawer()
	: GameThreadCanvas( NULL )
	, RenderThreadCanvas( NULL )
	, RenderTarget( new FSlateCanvasRenderTarget )
	, LayerID(INVALID_LAYER_ID)
{
	// watch for font cache flushes
	if (FEngineFontServices::IsInitialized())
	{
		FEngineFontServices::Get().OnReleaseResources().AddRaw(this, &FDebugCanvasDrawer::HandleReleaseFontResources);
	}
}

void FDebugCanvasDrawer::ReleaseTexture()
{
	LayerTexture.SafeRelease();
}

void FDebugCanvasDrawer::HandleReleaseFontResources(const class FSlateFontCache& InFontCache)
{
	check(IsInGameThread());

	// If this function is called while we have a pending render Canvas request, then we need to force 
	// a flush on the render thread to clear the pending batches that may reference invalid resources
	if (RenderThreadCanvas)
	{
		ENQUEUE_RENDER_COMMAND(FlushFontResourcesCommand)(
			[this](FRHICommandListImmediate& RHICmdList)
		{
			RenderThreadCanvas->Flush_RenderThread(RHICmdList, true);
		});

		FlushRenderingCommands();
	}

	// If this function is called while the game thread is still prepping a Canvas, then we need to 
	// force clear the pending batches as they may reference invalid resources
	if (GameThreadCanvas)
	{
		GameThreadCanvas->ClearBatchesToRender();
	}
}

void FDebugCanvasDrawer::ReleaseResources()
{
	FDebugCanvasDrawer* const ReleaseMe = this;

	ENQUEUE_RENDER_COMMAND(ReleaseCommand)(
		[ReleaseMe](FRHICommandList& RHICmdList)
		{
			ReleaseMe->ReleaseTexture();
		});
}

FDebugCanvasDrawer::~FDebugCanvasDrawer()
{
	// stop watching for font cache flushes
	if (FEngineFontServices::IsInitialized())
	{
		FEngineFontServices::Get().OnReleaseResources().RemoveAll(this);
	}

	delete RenderTarget;

	// We assume that the render thread is no longer utilizing any canvases
	if( GameThreadCanvas.IsValid() && RenderThreadCanvas != GameThreadCanvas )
	{
		GameThreadCanvas.Reset();
	}

	if( RenderThreadCanvas.IsValid() )
	{
		// Capture a copy of the canvas until the render thread can delete it
		FCanvasPtr RTCanvas = RenderThreadCanvas;
		ENQUEUE_RENDER_COMMAND(DeleteDebugRenderThreadCanvas)(
			[RTCanvas](FRHICommandListImmediate& RHICmdList)
		{
		});

		RenderThreadCanvas = nullptr;
	}

	if (LayerID != INVALID_LAYER_ID && 
		GEngine->StereoRenderingDevice.IsValid() && 
		GEngine->StereoRenderingDevice->GetStereoLayers())
	{
		GEngine->StereoRenderingDevice->GetStereoLayers()->DestroyLayer(LayerID);
		LayerID = INVALID_LAYER_ID;
	}
}

FCanvas* FDebugCanvasDrawer::GetGameThreadDebugCanvas()
{
	return GameThreadCanvas.Get();
}


void FDebugCanvasDrawer::BeginRenderingCanvas( const FIntRect& CanvasRect )
{
	if( CanvasRect.Size().X > 0 && CanvasRect.Size().Y > 0 )
	{
		bCanvasRenderedLastFrame = true;
		FDebugCanvasDrawer* CanvasDrawer = this;
		FCanvasPtr CanvasToRender = GameThreadCanvas;
		ENQUEUE_RENDER_COMMAND(BeginRenderingDebugCanvas)(
			[CanvasDrawer, CanvasToRender, CanvasRect = CanvasRect](FRHICommandListImmediate& RHICmdList)
			{
				FCanvasPtr LocalCanvasToRender = CanvasToRender;
			
				// Delete the old rendering thread canvas
				if( CanvasDrawer->GetRenderThreadCanvas().IsValid() && LocalCanvasToRender.IsValid() )
				{
					CanvasDrawer->DeleteRenderThreadCanvas();
				}

				if (!LocalCanvasToRender.IsValid())
				{
					LocalCanvasToRender = CanvasDrawer->GetRenderThreadCanvas();
				}

				CanvasDrawer->SetRenderThreadCanvas( CanvasRect, LocalCanvasToRender);
			}
		);
		
		// Gave the canvas to the render thread
		GameThreadCanvas = nullptr;
	}
}


void FDebugCanvasDrawer::InitDebugCanvas(FViewportClient* ViewportClient, UWorld* InWorld)
{
	const bool bIsStereoscopic3D = GEngine && GEngine->IsStereoscopic3D();
	IStereoLayers* const StereoLayers = (bIsStereoscopic3D && GEngine && GEngine->StereoRenderingDevice.IsValid()) ? GEngine->StereoRenderingDevice->GetStereoLayers() : nullptr;
	const bool bUseInternalTexture = StereoLayers && bIsStereoscopic3D;

	// If the canvas is not null there is more than one viewport draw call before slate draws.  This can happen on resizes. 
	// We need to delete the old canvas
		// This can also happen if we are debugging a HUD blueprint and in that case we need to continue using
		// the same canvas
	if (FSlateApplication::Get().IsNormalExecution())
	{
		const float DPIScale = bUseInternalTexture ? 1.0f : ViewportClient->GetDPIScale();
		GameThreadCanvas = MakeShared<FCanvas, ESPMode::ThreadSafe>(RenderTarget, nullptr, InWorld, InWorld ? InWorld->GetFeatureLevel() : GMaxRHIFeatureLevel, FCanvas::CDM_DeferDrawing, DPIScale);

		// Do not allow the canvas to be flushed outside of our debug rendering path
		GameThreadCanvas->SetAllowedModes(FCanvas::Allow_DeleteOnRender);
	}

	if (GameThreadCanvas.IsValid())
	{
		GameThreadCanvas->SetUseInternalTexture(bUseInternalTexture);

		if (bUseInternalTexture && LayerTexture)
		{
			if (StereoLayers)
			{
				IStereoLayers::FLayerDesc StereoLayerDesc = StereoLayers->GetDebugCanvasLayerDesc(LayerTexture->GetRHI());
				StereoLayerDesc.Flags |= !bCanvasRenderedLastFrame ? IStereoLayers::LAYER_FLAG_HIDDEN : 0;

				if (LayerID == INVALID_LAYER_ID && bCanvasRenderedLastFrame)
				{
					LayerID = StereoLayers->CreateLayer(StereoLayerDesc);
				}
				else
				{
					StereoLayers->SetLayerDesc(LayerID, StereoLayerDesc);
				}
			}
		}

		bCanvasRenderedLastFrame = false;
	}
}

void FDebugCanvasDrawer::Draw_RenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer, const FSlateCustomDrawParams& Params)
{
	check( IsInRenderingThread() );
	check(RHICmdList.IsOutsideRenderPass());

	SCOPED_DRAW_EVENT(RHICmdList, DrawDebugCanvas);

	QUICK_SCOPE_CYCLE_COUNTER(Stat_DrawDebugCanvas);
	if( RenderThreadCanvas.IsValid() )
	{
		FRHITexture* RT = InWindowBackBuffer != nullptr ? ((FTexture2DRHIRef*)InWindowBackBuffer)->GetReference() : nullptr;
		FTextureRHIRef HMDSwapchain = nullptr;	// Attention: RT could point to HMDSwapchain later.
		if (RenderThreadCanvas->IsUsingInternalTexture())
		{
			if (LayerTexture && RenderThreadCanvas->GetParentCanvasSize() != LayerTexture->GetDesc().Extent)
			{
				LayerTexture.SafeRelease();
			}

			if (!LayerTexture)
			{
				// Set TexCreate_NoFastClear because the fast CMASK clear was not working on ps4.
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(RenderThreadCanvas->GetParentCanvasSize(), PF_B8G8R8A8, FClearValueBinding(), ETextureCreateFlags::None, ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::NoFastClear, false));
				Desc.DebugName = TEXT("DebugCanvasLayerTexture");
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, LayerTexture, TEXT("DebugCanvasLayerTexture"));
				UE_LOG(LogProfilingDebugging, Log, TEXT("Allocated a %d x %d texture for HMD canvas layer"), RenderThreadCanvas->GetParentCanvasSize().X, RenderThreadCanvas->GetParentCanvasSize().Y);
			}

			IStereoLayers* const StereoLayers = (GEngine && GEngine->IsStereoscopic3D() && GEngine->StereoRenderingDevice.IsValid()) ? GEngine->StereoRenderingDevice->GetStereoLayers() : nullptr;

			FTextureRHIRef HMDNull = nullptr;
			if (StereoLayers)
			{
				StereoLayers->GetAllocatedTexture(LayerID, HMDSwapchain, HMDNull);

				// If drawing to a layer tell the spectator screen controller to copy that layer to the spectator screen.
				if (StereoLayers->ShouldCopyDebugLayersToSpectatorScreen() && LayerID != INVALID_LAYER_ID && GEngine && GEngine->XRSystem)
				{
					IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();
					if (HMD)
					{
						ISpectatorScreenController* SpectatorScreenController = HMD->GetSpectatorScreenController();
						if (SpectatorScreenController)
						{
							SpectatorScreenController->QueueDebugCanvasLayerID(LayerID);
						}
					}
				}
			}
			RT = reinterpret_cast<FRHITexture*>(HMDSwapchain == nullptr ? LayerTexture->GetRHI() : HMDSwapchain.GetReference());
		}
		RHICmdList.Transition(FRHITransitionInfo(RT, ERHIAccess::Unknown, ERHIAccess::RTV));
		RenderTarget->SetRenderTargetTexture(RT);

		if (RenderThreadCanvas->IsScaledToRenderTarget() && RT) 
		{
			RenderThreadCanvas->SetRenderTargetRect( FIntRect(0, 0, (RT)->GetSizeX(), (RT)->GetSizeY()) );
		}
		else
		{
			RenderThreadCanvas->SetRenderTargetRect( RenderTarget->GetViewRect() );
		}

		RenderThreadCanvas->Flush_RenderThread(RHICmdList, true);
		RenderTarget->ClearRenderTargetTexture();
	}
}

FCanvasPtr FDebugCanvasDrawer::GetRenderThreadCanvas()
{
	check( IsInRenderingThread() );
	return RenderThreadCanvas;
}

void FDebugCanvasDrawer::DeleteRenderThreadCanvas()
{
	check( IsInRenderingThread() );
	RenderThreadCanvas.Reset();
}

void FDebugCanvasDrawer::SetRenderThreadCanvas( const FIntRect& InCanvasRect, FCanvasPtr& Canvas )
{
	check( IsInRenderingThread() );
	if (Canvas->IsUsingInternalTexture())
	{
		RenderTarget->SetViewRect(FIntRect(FIntPoint(0, 0), Canvas->GetParentCanvasSize()));
	}
	else
	{
		RenderTarget->SetViewRect(InCanvasRect);
	}
	RenderThreadCanvas = Canvas;
}

SDebugCanvas::SDebugCanvas()
{
	SetCanTick(false);
	bCanSupportFocus = false;
}

void SDebugCanvas::Construct(const FArguments& InArgs)
{
	SceneViewport = InArgs._SceneViewport;
}

int32 SDebugCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SlatePaintDebugCanvas);
	const FSceneViewport* Viewport = SceneViewport.Get();
	if (Viewport)
	{
		Viewport->PaintDebugCanvas(AllottedGeometry, OutDrawElements, LayerId);
	}

	return LayerId;
}

FVector2D SDebugCanvas::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const FSceneViewport* Viewport = SceneViewport.Get();
	if (Viewport)
	{
		return Viewport->GetSizeXY();
	}
	else
	{
		return FVector2D::ZeroVector;
	}
}

void SDebugCanvas::SetSceneViewport(FSceneViewport* InSceneViewport)
{
	FSceneViewport* CurrentSceneViewport = SceneViewport.Get();
	if (CurrentSceneViewport)
	{
		// this canvas is moving to another viewport
		CurrentSceneViewport->SetDebugCanvas(nullptr);
	}

	SceneViewport = InSceneViewport;

	if (InSceneViewport)
	{
		// Notify the new viewport of its debug canvas for invalidation purposes
		InSceneViewport->SetDebugCanvas(SharedThis(this));
	}
}
