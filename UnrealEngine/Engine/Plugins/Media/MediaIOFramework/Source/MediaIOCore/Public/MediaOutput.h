// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "PixelFormat.h"

#include "MediaOutput.generated.h"


class UMediaCapture;

enum class EMediaCaptureConversionOperation : int8
{
	NONE,
	RGBA8_TO_YUV_8BIT,
	RGB10_TO_YUVv210_10BIT,
	INVERT_ALPHA,
	SET_ALPHA_ONE,
	CUSTOM, //~ If Custom used, OnCustomCapture_RenderingThread, GetCustomOutputSize and GetCustomOutputPixelFormat on the MediaCapture need to be implemented
};

enum class EMediaCaptureSourceType : int8
{
	RENDER_TARGET,
	SCENE_VIEWPORT,
	RHI_RESOURCE,
};

/**
 * Abstract base class for media output.
 *
 * Media output describe the location and/or settings of media objects that can
 * be used to output the content of Unreal Engine to a target device via a MediaCapture.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, hidecategories = (Object))
class MEDIAIOCORE_API UMediaOutput : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Number of texture used to transfer the texture from the GPU to the system memory.
	 * A smaller number is most likely to block the GPU (wait for the transfer to complete).
	 * A bigger number is most likely to increase latency.
	 * @note Some Capture are not are executed on the GPU. If it's the case then no buffer will be needed and no buffer will be created.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Output", meta=(ClampMin=1, ClampMax=8))
	int32 NumberOfTextureBuffers;

	/** Creates the specific implementation of the MediaCapture for the MediaOutput. */
	UFUNCTION(BlueprintCallable, Category="Media|Output")
	UMediaCapture* CreateMediaCapture();

	/**
	 * Validate the media output settings (must be implemented in child classes).
	 *
	 * @return true if validation passed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	virtual bool Validate(FString& OutFailureReason) const;

public:

	static const FIntPoint RequestCaptureSourceSize;

	/**
	 * The size of the buffer we wish to capture.
	 * The size of the buffer can not change during the capture.
	 * Return UMediaOutput::RequestCaptureSourceSize if you wish to take the buffer size as the requested size.
	 */
	virtual FIntPoint GetRequestedSize() const PURE_VIRTUAL(UMediaOutput::GetRequestedSize, return RequestCaptureSourceSize; );

	/**
	 * The pixel format of the buffer we wish to capture.
	 * Some conversion are available. See EMediaCaptureConversionOperation
	 */
	virtual EPixelFormat GetRequestedPixelFormat() const PURE_VIRTUAL(UMediaOutput::GetRequestedPixelFormat, return EPixelFormat::PF_Unknown; );

	/**
	 * The conversion we wish to accomplish on the GPU before the DMA transfer occurs.
	 */
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const { return EMediaCaptureConversionOperation::NONE; }
	
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnOutputModified, UMediaOutput*);
	FOnOutputModified& OnOutputModified() { return OnOutputModifiedDelegate; }
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() { return nullptr; }

private:
#if WITH_EDITOR
	FOnOutputModified OnOutputModifiedDelegate;
#endif

};
