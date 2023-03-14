// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"

#include "IRivermaxOutputStream.h"
#include "Misc/FrameRate.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxTypes.h"

#include "RivermaxMediaCapture.generated.h"


namespace UE::RivermaxCore
{
	struct FRivermaxStreamOptions;
}

class URivermaxMediaOutput;

/**
 * Output Media for Rivermax streams.
 */
UCLASS(BlueprintType)
class RIVERMAXMEDIA_API URivermaxMediaCapture : public UMediaCapture, public UE::RivermaxCore::IRivermaxOutputStreamListener
{
	GENERATED_BODY()

public:

	//~ Begin UMediaCapture interface
	virtual bool HasFinishedProcessing() const override;
protected:
	virtual bool ValidateMediaOutput() const override;
	virtual bool InitializeCapture() override;
	virtual bool UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) override;
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;
	virtual bool ShouldCaptureRHIResource() const override;
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow) override;

	/** For custom conversion, methods that need to be overriden */
	virtual FIntPoint GetCustomOutputSize(const FIntPoint& InSize) const override;
	virtual EMediaCaptureResourceType GetCustomOutputResourceType() const override;
	virtual FRDGBufferDesc GetCustomBufferDescription(const FIntPoint& InDesiredSize) const override;
	virtual void OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGBufferRef OutputBuffer, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV) override;
	//~ End UMediaCapture interface

	//~ Begin UObject interface
	virtual bool IsReadyForFinishDestroy();
	//~ End UObject interface

	//~ Begin IRivermaxOutputStreamListener interface
	virtual void OnInitializationCompleted(bool bHasSucceed) override;
	virtual void OnStreamError() override;
	//~ End IRivermaxOutputStreamListener interface

private:
	struct FRivermaxCaptureSyncData;

	/** Initializes capture and launches stream creation */
	bool Initialize(URivermaxMediaOutput* InMediaOutput);

	/** Initializes sync handlers, fences, for gpudirect functionality */
	void InitializeSyncHandlers_RenderThread();

	/** Returns available sync handler. i.e fence not already used */
	TSharedPtr<FRivermaxCaptureSyncData> GetAvailableSyncHandler() const;

	/** Returns true if one of the sync handler is in flight waiting for its fence */
	bool AreSyncHandlersBusy() const;

	/** Configures rivermax stream with desired output options */
	bool ConfigureStream(URivermaxMediaOutput* InMediaOutput, UE::RivermaxCore::FRivermaxStreamOptions& OutOptions) const;

	/** When GPUDirect is involved, we add a pass after the conversion pass to write a fence and know when we can consume the buffer */
	void AddSyncPointPass(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, FRDGBufferRef OutputBuffer);

private:

	/** Instance of the rivermax stream opened for this capture */
	TUniquePtr<UE::RivermaxCore::IRivermaxOutputStream> RivermaxStream;

	/** Set of options used to configure output stream */
	UE::RivermaxCore::FRivermaxStreamOptions Options;

	/** Whether sync handlers have been initialized or not.  */
	bool bAreSyncHandlersInitialized = false;

	/** Whether capture is active. Used to be queried by sync task */
	std::atomic<bool> bIsActive;

	/** Array of sync handlers (fence) to sync when captured buffer is completed */
	TArray<TSharedPtr<FRivermaxCaptureSyncData>> SyncHandlers;
};
