// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include <atomic>
#include "Containers/SpscQueue.h"
#include "HAL/CriticalSection.h"
#include "MediaOutput.h"
#include "Misc/Timecode.h"
#include "PixelFormat.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "RHI.h"
#include "RHIResources.h"

#include "MediaCapture.generated.h"

class FSceneViewport;
class FTextureRenderTargetResource;
class UTextureRenderTarget2D;
class FRHICommandListImmediate;
class FRHIGPUTextureReadback;
class FRHIGPUBufferReadback;
class FRDGBuilder;

namespace UE::MediaCaptureData
{
	class FFrameManager;
	class FCaptureFrame;
	class FMediaCaptureHelper;
}

/**
 * Possible states of media capture.
 */
UENUM(BlueprintType)
enum class EMediaCaptureState : uint8
{
	/** Unrecoverable error occurred during capture. */
	Error,

	/** Media is currently capturing. */
	Capturing,

	/** Media is being prepared for capturing. */
	Preparing,

	/** Capture has been stopped but some frames may need to be process. */
	StopRequested,

	/** Capture has been stopped. */
	Stopped,
};

/**
 * Possible resource type the MediaCapture can create based on the conversion operation.
 */
UENUM()
enum class EMediaCaptureResourceType : uint8
{
	/** Texture resources are used for the readback. */
	Texture,

	/** RWBuffer resources are used for the readback. */
	Buffer,
};

/**
 * Base class of additional data that can be stored for each requested capture.
 */
class FMediaCaptureUserData
{

};

/**
 * Type of cropping 
 */
UENUM(BlueprintType)
enum class EMediaCaptureCroppingType : uint8
{
	/** Do not crop the captured image. */
	None,
	/** Keep the center of the captured image. */
	Center,
	/** Keep the top left corner of the captured image. */
	TopLeft,
	/** Use the StartCapturePoint and the size of the MediaOutput to keep of the captured image. */
	Custom,
};

/**
 * Action when overrun occurs
 */
UENUM(BlueprintType)
enum class EMediaCaptureOverrunAction : uint8
{
	/** Flush rendering thread such that all scheduled commands are executed. */
	Flush,
	/** Skip capturing a frame if readback is trailing too much. */
	Skip,
};

/**
 * Description of resource to be captured when in rhi capture / immediate mode.
 */
struct MEDIAIOCORE_API FRHICaptureResourceDescription
{
	FIntPoint ResourceSize = FIntPoint::ZeroValue;
};

/**
 * Base class of additional data that can be stored for each requested capture.
 */
USTRUCT(BlueprintType)
struct MEDIAIOCORE_API FMediaCaptureOptions
{
	GENERATED_BODY()

	FMediaCaptureOptions();

public:

	/** Action to do when game thread overruns render thread and all frames are in flights being captured / readback. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MediaCapture")
	EMediaCaptureOverrunAction OverrunAction = EMediaCaptureOverrunAction::Flush;

	/** Crop the captured SceneViewport or TextureRenderTarget2D to the desired size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	EMediaCaptureCroppingType Crop;

	/**
	 * Crop the captured SceneViewport or TextureRenderTarget2D to the desired size.
	 * @note Only valid when Crop is set to Custom.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	FIntPoint CustomCapturePoint;

	/**
	 * When the capture start, resize the source buffer to the desired size.
	 * @note Only valid when a size is specified by the MediaOutput.
	 * @note For viewport, the window size will not change. Only the viewport will be resized.
	 * @note For RenderTarget, the asset will be modified and resized to the desired size.
	 * @note Not valid for immediate capture of RHI resource
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	bool bResizeSourceBuffer;

	/**
	 * When the application enters responsive mode, skip the frame capture.
	 * The application can enter responsive mode on mouse down, viewport resize, ...
	 * That is to ensure responsiveness in low FPS situations.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	bool bSkipFrameWhenRunningExpensiveTasks;

	/**
	 * Allows to enable/disable pixel format conversion for the cases where render target is not of the desired pixel format. 
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	bool bConvertToDesiredPixelFormat;

	/**
	 * In some cases when we want to stream irregular render targets containing limited number
	 * of channels (for example RG16f), we would like to force Alpha to 1.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	bool bForceAlphaToOneOnConversion;

	/**
	 * Whether to apply a linear to sRGB conversion to the texture before outputting. 
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MediaCapture", meta=(DisplayName="Apply linear to sRGB conversion"))
	bool bApplyLinearToSRGBConversion = false;

	/** Automatically stop capturing after a predetermined number of images. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MediaCapture")
	bool bAutostopOnCapture;

	/** The number of images to capture*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MediaCapture", meta=(editcondition="bAutostopOnCapture"))
	int32 NumberOfFramesToCapture;
};


/** Delegate signatures */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMediaCaptureStateChangedSignature);
DECLARE_MULTICAST_DELEGATE(FMediaCaptureStateChangedSignatureNative);


/**
 * Abstract base class for media capture.
 *
 * MediaCapture capture the texture of the Render target or the SceneViewport and sends it to an external media device.
 * MediaCapture should be created by a MediaOutput.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, hidecategories = (Object))
class MEDIAIOCORE_API UMediaCapture : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Default constructor / destructor to use forward declared uniqueptr */
	virtual ~UMediaCapture();
	UMediaCapture(FVTableHelper&);

	/**
	 * Stop the actual capture if there is one.
	 * Then start the capture of a SceneViewport.
	 * If the SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @note make sure the size of the SceneViewport doesn't change during capture.
	 * @return True if the capture was successfully started
	 */
	bool CaptureSceneViewport(TSharedPtr<FSceneViewport>& SceneViewport, FMediaCaptureOptions CaptureOptions);

	/**
	 * Stop the actual capture if there is one.
	 * Then start a capture for a RHI resource.
	 * @note For this to work, it is expected that CaptureImmediate_RenderThread is called periodically with the resource to capture.
	 * @return True if the capture was successfully initialize
	 */
	bool CaptureRHITexture(const FRHICaptureResourceDescription& ResourceDescription, FMediaCaptureOptions CaptureOptions);

	/**
	 * Stop the current capture if there is one.
	 * Then find and capture every frame from active SceneViewport.
	 * It can only find a SceneViewport when you play in Standalone or in "New Editor Window PIE".
	 * If the active SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return True if the capture was successfully started
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool CaptureActiveSceneViewport(FMediaCaptureOptions CaptureOptions);

	/**
	 * Stop the actual capture if there is one.
	 * Then capture every frame for a TextureRenderTarget2D.
	 * The TextureRenderTarget2D needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return True if the capture was successfully started
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool CaptureTextureRenderTarget2D(UTextureRenderTarget2D* RenderTarget, FMediaCaptureOptions CaptureOptions);

	/**
	 * Update the current capture with a SceneViewport.
	 * If the SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @note make sure the size of the SceneViewport doesn't change during capture.
	 * @return Return true if the capture was successfully updated. If false is returned, the capture was stopped.
	 */
	bool UpdateSceneViewport(TSharedPtr<FSceneViewport>& SceneViewport);

	/**
	 * Update the current capture with every frame for a TextureRenderTarget2D.
	 * The TextureRenderTarget2D needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return Return true if the capture was successfully updated. If false is returned, the capture was stopped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool UpdateTextureRenderTarget2D(UTextureRenderTarget2D* RenderTarget);
	
	/** Captures a resource immediately from the render thread. Used in RHI_RESOURCE capture mode */
	void CaptureImmediate_RenderThread(FRDGBuilder& GraphBuilder, FRHITexture* InSourceTexture);

	/**
	 * Stop the previous requested capture.
	 * @param bAllowPendingFrameToBeProcess	Keep copying the pending frames asynchronously or stop immediately without copying the pending frames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	void StopCapture(bool bAllowPendingFrameToBeProcess);

	/** Get the current state of the capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	virtual EMediaCaptureState GetState() const { return MediaState; }

	/** Set the media output. Can only be set when the capture is stopped. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	void SetMediaOutput(UMediaOutput* InMediaOutput);

	/** Get the desired size of the current capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	FIntPoint GetDesiredSize() const { return DesiredSize; }

	/** Get the desired pixel format of the current capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	EPixelFormat GetDesiredPixelFormat() const { return DesiredPixelFormat; }

	/** Check whether this capture has any processing left to do. */
	virtual bool HasFinishedProcessing() const;

	/** Called when the state of the capture changed. */
	UPROPERTY(BlueprintAssignable, Category = "Media|Output")
	FMediaCaptureStateChangedSignature OnStateChanged;

	/** Set the valid GPU mask of the source buffer/texture being captured. Can be called from any thread. */
	virtual void SetValidSourceGPUMask(FRHIGPUMask GPUMask);

	/**
	 * Called when the state of the capture changed.
	 * The callback is called on the game thread. Note that the change may occur on the rendering thread.
	 */
	FMediaCaptureStateChangedSignatureNative OnStateChangedNative;

public:
	//~ UObject interface
	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;

protected:
	//~ UMediaCapture interface
	virtual bool ValidateMediaOutput() const;

	UE_DEPRECATED(5.1, "This method has been deprecated. Please override InitializeCapture() and PostInitializeCapture variants if needed instead.")
	virtual bool CaptureSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) final;

	UE_DEPRECATED(5.1, "This method has been deprecated. Please override InitializeCapture() and PostInitializeCapture variants if needed instead.")
	virtual bool CaptureRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) final;

	/** Initialization method to prepare implementation for capture */
	virtual bool InitializeCapture() PURE_VIRTUAL(UMediaCapture::InitializeCapture, return false; );
	
	/** Called after initialize for viewport capture type */
	virtual bool PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport) { return true; }

	/** Called after initialize for render target capture type */
	virtual bool PostInitializeCaptureRenderTarget(UTextureRenderTarget2D* InRenderTarget) { return true; }

	/** Called after initialize for rhi resource capture type */
	virtual bool PostInitializeCaptureRHIResource(const FRHICaptureResourceDescription& InResourceDescription) { return true; }

	virtual bool UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) { return true; }
	virtual bool UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) { return true; }
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) { }

	//~ DMA Functions implemented in child classes, Temporary until AJA and BM middleman are migrated to a module.
	virtual void LockDMATexture_RenderThread(FTextureRHIRef InTexture) {}
	virtual void UnlockDMATexture_RenderThread(FTextureRHIRef InTexture) {}

	friend class UE::MediaCaptureData::FCaptureFrame;
	friend class UE::MediaCaptureData::FMediaCaptureHelper;
	struct FCaptureBaseData
	{
		FCaptureBaseData();

		FTimecode SourceFrameTimecode;
		FFrameRate SourceFrameTimecodeFramerate;
		uint32 SourceFrameNumberRenderThread;
		uint32 SourceFrameNumber;
	};

	/**
	 * Capture the data that will pass along to the callback.
	 * @note The capture is done on the Render Thread but triggered from the Game Thread.
	 */
	virtual TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> GetCaptureFrameUserData_GameThread() { return TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe>(); }

	UE_DEPRECATED(5.1, "ShouldCaptureRHITexture has been deprecated to support both texture and buffer output resource. Please use the ShouldCaptureRHIResource instead.")
	virtual bool ShouldCaptureRHITexture() const { return false; }
	
	/** Should we call OnFrameCaptured_RenderingThread() with a RHI resource -or- readback the memory to CPU ram and call OnFrameCaptured_RenderingThread(). */
	virtual bool ShouldCaptureRHIResource() const { return false; }

	/** Called at the beginning of the Capture_RenderThread call if output resource type is Texture */
	virtual void BeforeFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) {}

	/** Called at the beginning of the Capture_RenderThread call if output resource type is Buffer */
	virtual void BeforeFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer) {}

	/**
	 * Callback when the buffer was successfully copied to CPU ram.
	 * The callback in called from a critical point. If you intend to process the buffer, do so in another thread.
	 * The buffer is only valid for the duration of the callback.
	 */
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow) { }

	UE_DEPRECATED(5.1, "OnRHITextureCaptured_RenderingThread has been deprecated to support texture and buffer output resource. Please use OnRHIResourceCaptured_RenderingThread instead.")
	virtual void OnRHITextureCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) { }

	/**
	 * Callbacks when the buffer was successfully copied on the GPU ram.
	 * The callback in called from a critical point. If you intend to process the texture/buffer, do so in another thread.
	 * The texture is valid for the duration of the callback.
	 */
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) { }
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer) { }

	UE_DEPRECATED(5.1, "OnCustomCapture_RenderingThread has been deprecated while moving to a RDG based pipeline. Please use the RDG version instead.")
	virtual void OnCustomCapture_RenderingThread(FRHICommandListImmediate& RHICmdList, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InSourceTexture, FTextureRHIRef TargetableTexture, FResolveParams& ResolveParams, FVector2D CropU, FVector2D CropV)
	{
	}

	/**
	 * Callback when the source texture is ready to be copied on the GPU. You need to copy yourself. You may use the callback to add passes to the graph builder.
	 * Graph execution and output texture readback will be handled after this call. No need to do it in the override.
	 * The callback in called from a critical point be fast.
	 * The texture is valid for the duration of the callback.
	 * Only called when the conversion operation is custom and the output resource type is Texture.
	 */
	virtual void OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGTextureRef OutputTexture, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV)
	{
	}

	/**
	 * Callback when the source texture is ready to be copied on the GPU. You need to copy yourself. You may use the callback to add passes to the graph builder.
	 * Graph execution and output buffer readback will be handled after this call. No need to do it in the override.
	 * The callback in called from a critical point be fast.
	 * The texture is valid for the duration of the callback.
	 * Only called when the conversion operation is custom and the output resource type is Buffer.
	 */
	virtual void OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGBufferRef OutputBuffer, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV)
	{
	}

	/** Get the size of the buffer when the conversion operation is custom. The function is called once at initialization and the result is cached. */
	virtual FIntPoint GetCustomOutputSize(const FIntPoint& InSize) const 
	{
		return InSize; 
	}

	/** Get the pixel format of the buffer when the conversion operation is custom. The function is called once at initialization and the result is cached. */
	virtual EPixelFormat GetCustomOutputPixelFormat(const EPixelFormat& InPixelFormat) const
	{
		return InPixelFormat;
	}
	
	/** Get the desired output resource type when the conversion is custom. This function is called once at initialization and the result is cached */
	virtual EMediaCaptureResourceType GetCustomOutputResourceType() const 
	{
		return DesiredOutputResourceType; 
	}

	/** Get the desired output buffer description when the conversion is custom and operating in buffer resource type */
	virtual FRDGBufferDesc GetCustomBufferDescription(const FIntPoint& InDesiredSize) const
	{
		return DesiredOutputBufferDescription;
	}



protected:
	UTextureRenderTarget2D* GetTextureRenderTarget() { return CapturingRenderTarget; }
	TSharedPtr<FSceneViewport> GetCapturingSceneViewport() { return CapturingSceneViewport.Pin(); }
	EMediaCaptureConversionOperation GetConversionOperation() const { return ConversionOperation; }
	const FString& GetCaptureSourceType() const;
	void SetState(EMediaCaptureState InNewState);


private:
	void InitializeOutputResources(int32 InNumberOfBuffers);
	
	/** Used by RHI capture to buffer captured frame data dependant on game thread such as timecode */
	void OnBeginFrame_GameThread();
	void OnEndFrame_GameThread();
	void CacheMediaOutput(EMediaCaptureSourceType InSourceType);
	void CacheOutputOptions();
	FIntPoint GetOutputSize(const FIntPoint& InSize, const EMediaCaptureConversionOperation & InConversionOperation) const;
	EPixelFormat GetOutputPixelFormat(const EPixelFormat& InPixelFormat, const EMediaCaptureConversionOperation & InConversionOperation) const;
	EMediaCaptureResourceType GetOutputResourceType(const EMediaCaptureConversionOperation & InConversionOperation) const;
	FRDGBufferDesc GetOutputBufferDescription(EMediaCaptureConversionOperation InConversionOperation) const;
	void BroadcastStateChanged();
	void SetFixedViewportSize(TSharedPtr<FSceneViewport> InSceneViewport);
	void ResetFixedViewportSize(TSharedPtr<FSceneViewport> InViewport, bool bInFlushRenderingCommands);

	bool ProcessCapture_RenderThread(FRDGBuilder& GraphBuilder, UMediaCapture* InMediaCapture, UE::MediaCaptureData::FCaptureFrame* CapturingFrame, FTexture2DRHIRef InResourceToCapture, FIntPoint InDesiredSize);
	bool ProcessReadyFrame_RenderThread(FRHICommandListImmediate& RHICmdList, UMediaCapture* InMediaCapture, UE::MediaCaptureData::FCaptureFrame* ReadyFrame);

	void PrintFrameState();

protected:

	/** MediaOutput associated with this capture */
	UPROPERTY(Transient)
	TObjectPtr<UMediaOutput> MediaOutput;
	
	/** Output size of the media capture. If a conversion is done, this resolution might be different than source resolution. i.e. 1080p RGB might be 960x1080 in YUV */
	FIntPoint DesiredOutputSize = FIntPoint(1920, 1080);
	
	/** Valid source gpu mask of the source. */
	std::atomic<FRHIGPUMask> ValidSourceGPUMask;

	/** Type of capture currently being done. i.e Viewport, RT, RHI Resource */
	EMediaCaptureSourceType CaptureSourceType = EMediaCaptureSourceType::SCENE_VIEWPORT;

private:

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CapturingRenderTarget;
	
	TUniquePtr<UE::MediaCaptureData::FFrameManager> FrameManager;
	int32 NumberOfCaptureFrame = 2;
	int32 CaptureRequestCount = 0;
	EMediaCaptureState MediaState = EMediaCaptureState::Stopped;

	TWeakPtr<FSceneViewport> CapturingSceneViewport;
	FCriticalSection AccessingCapturingSource;

	FIntPoint DesiredSize = FIntPoint(1920, 1080);
	EPixelFormat DesiredPixelFormat = EPixelFormat::PF_A2B10G10R10;
	EPixelFormat DesiredOutputPixelFormat = EPixelFormat::PF_A2B10G10R10;
	FRDGBufferDesc DesiredOutputBufferDescription;
	FMediaCaptureOptions DesiredCaptureOptions;
	EMediaCaptureConversionOperation ConversionOperation = EMediaCaptureConversionOperation::NONE;
	EMediaCaptureResourceType DesiredOutputResourceType = EMediaCaptureResourceType::Texture;
	FString MediaOutputName = FString(TEXT("[undefined]"));
	bool bUseRequestedTargetSize = false;

	bool bViewportHasFixedViewportSize = false;
	std::atomic<bool> bOutputResourcesInitialized;
	std::atomic<bool> bShouldCaptureRHIResource;
	std::atomic<int32> WaitingForRenderCommandExecutionCounter;
	std::atomic<int32> PendingFrameCount;

	struct FQueuedCaptureData
	{
		FCaptureBaseData BaseData;
		TSharedPtr<FMediaCaptureUserData> UserData;
	};
	TSpscQueue<FQueuedCaptureData> CaptureDataQueue;
};
