// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"

#include "MediaIOCoreDefinitions.h"
#include "RivermaxTypes.h"
#include "UObject/ObjectMacros.h"

#include "RivermaxMediaOutput.generated.h"


UENUM()
enum class ERivermaxMediaOutputPixelFormat : uint8
{
	PF_8BIT_YUV422 UMETA(DisplayName = "8bit YUV422"),
	PF_10BIT_YUV422 UMETA(DisplayName = "10bit YUV422"),
	PF_8BIT_RGB UMETA(DisplayName = "8bit RGB"),
	PF_10BIT_RGB UMETA(DisplayName = "10bit RGB"),
	PF_12BIT_RGB UMETA(DisplayName = "12bit RGB"),
	PF_FLOAT16_RGB UMETA(DisplayName = "16bit Float RGB")
};

UENUM()
enum class ERivermaxMediaAlignmentMode : uint8
{
	/** 
	 * Uses NVIDIA Rivermax clock to calculate alignment points based on ST2059 
	 */
	AlignmentPoint,

	/** 
	 * Aligns frame scheduling with frame creation not going faster than frame interval 
	 * In its current shape, useful for a faster stream than frame creation rate
	 */
	FrameCreation,
};

/** 
 * Controls how rivermax capture behaves when there are no buffers available to capture into
 */
UENUM()
enum class ERivermaxFrameLockingMode : uint8
{
	/** If no frame available, continue */
	FreeRun,

	/** Blocks RHI thread prior to capture the current frame if no space is available. */
	BlockOnReservation,
};


/**
 * Output information for a Rivermax media capture.
 */
UCLASS(BlueprintType, meta=(MediaIOCustomLayout="Rivermax", DisplayName = "NVIDIA Rivermax Output"))
class RIVERMAXMEDIA_API URivermaxMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

public:
	
	//~ Begin UMediaOutput interface
	virtual bool Validate(FString& FailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface


public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

	/** Used by frame scheduler to know how to align the output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	ERivermaxMediaAlignmentMode AlignmentMode = ERivermaxMediaAlignmentMode::AlignmentPoint;

	/** 
	 * Whether to produce a continuous output stream repeating last frame if no new frames provided 
	 * Note: Not supported in frame creation mode
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (EditCondition = "AlignmentMode != ERivermaxMediaAlignmentMode::FrameCreation"))
	bool bDoContinuousOutput = true;

	/** For alignment point mode, controls whether we stall engine before capturing if there are no buffer available to capture into */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	ERivermaxFrameLockingMode FrameLockingMode = ERivermaxFrameLockingMode::FreeRun;

	/** Number of frames that can be queued / used in output queue. Frame being sent counts for 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "2", ClampMax = "8", UIMin = "2", UIMax = "8"))
	int32 PresentationQueueSize = 2;

	/** 
	 * Experimental flag to use frame counter instead of using NVIDIA Rivermax clock for timestamping output frames
	 * Meant to be used for UE-UE streams where frame locking is done, e.g. nDisplay.
	 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (EditCondition = "AlignmentMode == ERivermaxMediaAlignmentMode::FrameCreation"))
	bool bDoFrameCounterTimestamping = true;

	/** If false, use the default source buffer size. If true, a specific resolution will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Media", meta = (InlineEditConditionToggle))
	bool bOverrideResolution = false;

	/** Resolution of this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta = (EditCondition = "bOverrideResolution"))
	FIntPoint Resolution = {1920, 1080};
	
	/** Frame rate of this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FFrameRate FrameRate = {24,1};
	
	/** Pixel format for this output stream */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ERivermaxMediaOutputPixelFormat PixelFormat = ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;

	/**
	 * Network card interface to use to send data
	 * Wildcards are supported to match against an interface found on the machine
	 * 192.*.0.110
	 * 192.168.0.1?0
	 * 192.168.0.1*
	 */	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString InterfaceAddress = TEXT("*.*.*.*");

	/** Address of the stream. Can be multicast, i.e. 228.1.1.1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString StreamAddress = UE::RivermaxCore::DefaultStreamAddress;

	/** Port to use for this output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 Port = 50000;

	/** Whether to use GPUDirect if available (Memcopy from GPU to NIC directly bypassing system) if available */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Video")
	bool bUseGPUDirect = true;
};
