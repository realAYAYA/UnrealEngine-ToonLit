// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkFunctionLibrary.h"

#include "BinkMediaPlayerPrivate.h"
#include "Misc/Paths.h"
#include "Rendering/RenderingCommon.h"
#include "RenderingThread.h"
#include "OneColorShader.h"

#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"

#include "binkplugin.h"

extern TSharedPtr<FBinkMovieStreamer, ESPMode::ThreadSafe> MovieStreamer;

// Note: Has to be in a seperate function because you can't do #if inside a render command macro
static void Bink_DrawOverlays_Internal(FRHICommandListImmediate &RHICmdList, const UGameViewportClient* gameViewport) {
	if (!GEngine || !gameViewport || !gameViewport->Viewport) {
		return;
	}

	FVector2D screenSize;
	gameViewport->GetViewportSize(screenSize);
	const FTexture2DRHIRef &backbuffer = gameViewport->Viewport->GetRenderTargetTexture();
	if(!backbuffer.GetReference()) 
	{
		return;
	}

	BINKPLUGINFRAMEINFO FrameInfo = {};
	FrameInfo.screen_resource = backbuffer.GetReference();
	FrameInfo.screen_resource_state = 4; // D3D12_RESOURCE_STATE_RENDER_TARGET; (only used in d3d12)
	FrameInfo.width = screenSize.X;
	FrameInfo.height = screenSize.Y;
	FrameInfo.sdr_or_hdr = backbuffer->GetFormat() == PF_A2B10G10R10 ? 1 : 0;
	FrameInfo.cmdBuf = &RHICmdList;
	BinkPluginSetPerFrameInfo(&FrameInfo);
	BinkPluginAllScheduled();
	BinkPluginDraw(0, 1);
}

void UBinkFunctionLibrary::Bink_DrawOverlays() 
{
	TWeakObjectPtr<UGameViewportClient> gameViewport = (GEngine != nullptr) ? GEngine->GameViewport : nullptr;
	if (!gameViewport.IsValid()) {
		return;
	}

	ENQUEUE_RENDER_COMMAND(BinkOverlays)([gameViewport](FRHICommandListImmediate& RHICmdList) 
	{ 
		if (gameViewport.IsValid())
		{
			Bink_DrawOverlays_Internal(RHICmdList, gameViewport.Get());
		}
	});
}

FTimespan UBinkFunctionLibrary::BinkLoadingMovie_GetDuration() 
{
	double ms = 0;
	if(MovieStreamer.IsValid() && MovieStreamer.Get()->bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(MovieStreamer.Get()->bnk, &bpinfo);
		ms = ((double)bpinfo.Frames) * ((double)bpinfo.FrameRateDiv) * 1000.0 / ((double)bpinfo.FrameRate);
	}
	return FTimespan::FromMilliseconds(ms);
}

FTimespan UBinkFunctionLibrary::BinkLoadingMovie_GetTime() 
{
	double ms = 0;
	if(MovieStreamer.IsValid() && MovieStreamer.Get()->bnk) 
	{
		BINKPLUGININFO bpinfo = {};
		BinkPluginInfo(MovieStreamer.Get()->bnk, &bpinfo);
		ms = ((double)bpinfo.FrameNum) * ((double)bpinfo.FrameRateDiv) * 1000.0 / ((double)bpinfo.FrameRate);
	}
	return FTimespan::FromMilliseconds(ms);
}
