// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"
#include "AjaMediaOutput.h"
#include "DSP/BufferVectorOperations.h"
#include "HAL/CriticalSection.h"
#include "MediaIOCoreEncodeTime.h"
#include "Misc/FrameRate.h"

#include "AjaMediaCapture.generated.h"


class FEvent;
class UAjaMediaOutput;

namespace AJA
{
	struct AJAOutputFrameBufferData;
}

/**
 * Output Media for AJA streams.
 * The output format could be any of EAjaMediaOutputPixelFormat.
 */
UCLASS(BlueprintType)
class AJAMEDIAOUTPUT_API UAjaMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

public:
	UAjaMediaCapture();
	~UAjaMediaCapture();

	//~ UMediaCapture interface
	virtual bool HasFinishedProcessing() const override;
protected:
	virtual bool ValidateMediaOutput() const override;
	virtual bool InitializeCapture() override;
	virtual bool PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) override;
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;
	virtual bool ShouldCaptureRHIResource() const override;
	
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow) override;
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) override;
	virtual void LockDMATexture_RenderThread(FTextureRHIRef InTexture) override;
	virtual void UnlockDMATexture_RenderThread(FTextureRHIRef InTexture) override;

private:
	struct FAjaOutputCallback;
	friend FAjaOutputCallback;
	struct FAJAOutputChannel;

private:
	bool InitAJA(UAjaMediaOutput* InMediaOutput);
	void WaitForSync_RenderingThread() const;
	void OutputAudio_RenderingThread(const AJA::AJAOutputFrameBufferData& FrameBuffer) const;
	void ApplyViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport);
	void RestoreViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport);
	bool CleanupPreEditorExit();
	void OnEnginePreExit();

	struct FAudioBuffer
	{
		Audio::FAlignedFloatBuffer Data;
		int32 NumChannels = 0;
	};
	
private:
	/** AJA Port for outputting */
	TPimplPtr<FAJAOutputChannel> OutputChannel;
	TPimplPtr<FAjaOutputCallback> OutputCallback;

	/** Name of this output port */
	FString PortName;

	/** Option from MediaOutput */
	bool bWaitForSyncEvent;
	bool bLogDropFrame;
	bool bEncodeTimecodeInTexel;
	EAjaMediaOutputPixelFormat PixelFormat;
	bool UseKey;

	/** Saved IgnoreTextureAlpha flag from viewport */
	bool bSavedIgnoreTextureAlpha;
	bool bIgnoreTextureAlphaChanged;

	/** Selected FrameRate of this output */
	FFrameRate FrameRate;

	/** Critical section for synchronizing access to the OutputChannel */
	FCriticalSection RenderThreadCriticalSection;

	/** Event to wakeup When waiting for sync */
	FEvent* WakeUpEvent;

	/** Holds an audio output that captures audio from the engine. */
	TSharedPtr<class FMediaIOAudioOutput> AudioOutput;

	bool bOutputAudio = false;
	
	/** Textures to release when the capture has stopped. Must be released after GPUTextureTransfer textures have been unregistered. */
	TArray<FTextureRHIRef> TexturesToRelease;

	/** Whether or not GPUTextureTransfer was initialized successfully. */
	bool bGPUTextureTransferAvailable = false;

	/** Handle for the delegate used to clean up AJA on editor shutdown. */
	FDelegateHandle CanCloseEditorDelegateHandle;
};
