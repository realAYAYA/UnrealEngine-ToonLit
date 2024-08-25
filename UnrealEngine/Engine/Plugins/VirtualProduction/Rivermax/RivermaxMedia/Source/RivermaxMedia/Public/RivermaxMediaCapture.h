// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"

#include "IRivermaxOutputStream.h"
#include "Misc/FrameRate.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxTypes.h"

#include "RivermaxMediaCapture.generated.h"


class URivermaxMediaOutput;

/**
 * Output Media for Rivermax streams.
 */
UCLASS(BlueprintType)
class RIVERMAXMEDIA_API URivermaxMediaCapture : public UMediaCapture, public UE::RivermaxCore::IRivermaxOutputStreamListener
{
	GENERATED_BODY()

public:
	/** Rivermax capture specific API to provide stream options access */
	UE::RivermaxCore::FRivermaxOutputStreamOptions GetOutputStreamOptions() const;

	/** Returns information about last presented frame on the output stream */
	void GetLastPresentedFrameInformation(UE::RivermaxCore::FPresentedFrameInfo& OutFrameInfo) const;

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
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer) override;
	virtual void OnRHIResourceCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer) override;
	virtual void OnFrameCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, const FMediaCaptureResourceData& InResourceData) override;
	virtual bool SupportsAnyThreadCapture() const override
	{
		return true;
	}
	virtual bool SupportsAutoRestart() const
	{
		return true;
	}

	/** For custom conversion, methods that need to be overriden */
	virtual FIntPoint GetCustomOutputSize(const FIntPoint& InSize) const override;
	virtual EMediaCaptureResourceType GetCustomOutputResourceType() const override;
	virtual FRDGBufferDesc GetCustomBufferDescription(const FIntPoint& InDesiredSize) const override;
	virtual void OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGBufferRef OutputBuffer, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV) override;
	virtual bool IsOutputSynchronizationSupported() const override { return true; }
	//~ End UMediaCapture interface

	//~ Begin UObject interface
	virtual bool IsReadyForFinishDestroy();
	//~ End UObject interface

	//~ Begin IRivermaxOutputStreamListener interface
	virtual void OnInitializationCompleted(bool bHasSucceed) override;
	virtual void OnStreamError() override;
	virtual void OnPreFrameEnqueue() override;
	//~ End IRivermaxOutputStreamListener interface

private:
	struct FRivermaxCaptureSyncData;

	/** Initializes capture and launches stream creation */
	bool Initialize(URivermaxMediaOutput* InMediaOutput);

	/** Configures rivermax stream with desired output options */
	bool ConfigureStream(URivermaxMediaOutput* InMediaOutput, UE::RivermaxCore::FRivermaxOutputStreamOptions& OutOptions) const;

	/** Enqueues a RHI lambda to reserve a spot for the next frame to capture */
	void AddFrameReservationPass(FRDGBuilder& GraphBuilder);

	/** Common method called for non gpudirect route when a frame is captured, either from render thread or any thread  */
	void OnFrameCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow);

	/** Common method called for gpudirect route when a frame is captured, either from render thread or any thread  */
	void OnRHIResourceCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer);
private:

	/** Instance of the rivermax stream opened for this capture */
	TUniquePtr<UE::RivermaxCore::IRivermaxOutputStream> RivermaxStream;

	/** Set of options used to configure output stream */
	UE::RivermaxCore::FRivermaxOutputStreamOptions Options;

	/** Whether capture is active. Used to be queried by sync task */
	std::atomic<bool> bIsActive;
};
