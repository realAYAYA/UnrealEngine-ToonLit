// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"
#include "Slate/SceneViewport.h"
#include "PixelStreamingVideoInput.h"
#include "PixelStreamingMediaCapture.generated.h"

UCLASS(BlueprintType)
class UPixelStreamingMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

	//~ Begin UMediaCapture interface
public:
	virtual void OnRHIResourceCaptured_RenderingThread(
		const FCaptureBaseData& InBaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
		FTextureRHIRef InTexture) override;

	virtual bool InitializeCapture() override;
	virtual bool PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool ShouldCaptureRHIResource() const { return true; }
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;
	//~ End UMediaCapture interface

	TSharedPtr<FSceneViewport> GetViewport() const { return SceneViewport.Pin(); }
	virtual void ViewportResized(FViewport* Viewport, uint32 ResizeCode);
	bool WasViewportResized() const { return bViewportResized; }
	void SetVideoInput(TWeakPtr<FPixelStreamingVideoInput> InVideoInput) { VideoInput = InVideoInput; }

	DECLARE_MULTICAST_DELEGATE(FOnCaptureViewportInitialized);
	FOnCaptureViewportInitialized OnCaptureViewportInitialized;

private:
	TWeakPtr<FSceneViewport> SceneViewport;
	TWeakPtr<FPixelStreamingVideoInput> VideoInput;
	
	/* We track whether the viewport has been resized since we created this capturer as resize means restart capturer. */
	bool bViewportResized = false;
};
