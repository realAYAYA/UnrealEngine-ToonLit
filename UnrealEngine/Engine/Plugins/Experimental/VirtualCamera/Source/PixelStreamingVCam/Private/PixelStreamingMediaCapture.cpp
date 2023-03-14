// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaCapture.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelCaptureInputFrameRHI.h"
#include "Slate/SceneViewport.h"
#include "PixelStreamingVCamLog.h"

void UPixelStreamingMediaCapture::OnRHIResourceCaptured_RenderingThread(
	const FCaptureBaseData& InBaseData,
	TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
	FTextureRHIRef InTexture)
{
	TSharedPtr<FPixelStreamingVideoInput> VideoInputPtr = VideoInput.Pin();
	if (VideoInputPtr)
	{
		VideoInputPtr->OnFrame(FPixelCaptureInputFrameRHI(InTexture));
	}
}

bool UPixelStreamingMediaCapture::InitializeCapture()
{
	UE_LOG(LogPixelStreamingVCam, Log, TEXT("Initializing media capture for Pixel Streaming VCam."));
	bViewportResized = false;
	SetState(EMediaCaptureState::Capturing);
	return true;
}

void UPixelStreamingMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	// Todo: Any cleanup on capture stop should happen here.
}

bool UPixelStreamingMediaCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	SceneViewport = TWeakPtr<FSceneViewport>(InSceneViewport);
	OnCaptureViewportInitialized.Broadcast();

	// Listen for viewport resize events as resizes invalidate media capture, so we want to know when to reset capture
	InSceneViewport->ViewportResizedEvent.AddUObject(this, &UPixelStreamingMediaCapture::ViewportResized);

	return true;
}

void UPixelStreamingMediaCapture::ViewportResized(FViewport* Viewport, uint32 ResizeCode)
{
	bViewportResized = true;
}
