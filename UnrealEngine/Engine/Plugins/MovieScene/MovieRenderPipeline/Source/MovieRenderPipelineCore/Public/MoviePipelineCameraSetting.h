// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineSetting.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineCameraSetting.generated.h"


UCLASS(Blueprintable)
class MOVIERENDERPIPELINECORE_API UMoviePipelineCameraSetting : public UMoviePipelineSetting
{
	GENERATED_BODY()
public:
	UMoviePipelineCameraSetting()
		: ShutterTiming(EMoviePipelineShutterTiming::FrameCenter)
	{}

public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "CameraSettingDisplayName", "Camera"); }
#endif
protected:
	virtual bool IsValidOnShots() const override { return true; }
	virtual bool IsValidOnMaster() const override { return true; }
	
	virtual void GetFormatArguments(FMoviePipelineFormatArgs& InOutFormatArgs) const override
	{
		Super::GetFormatArguments(InOutFormatArgs);

		InOutFormatArgs.FilenameArguments.Add(TEXT("shutter_timing"), StaticEnum<EMoviePipelineShutterTiming>()->GetNameStringByValue((int64)ShutterTiming));
		InOutFormatArgs.FilenameArguments.Add(TEXT("overscan_percentage"), FString::SanitizeFloat(OverscanPercentage));
		InOutFormatArgs.FileMetadata.Add(TEXT("unreal/camera/shutterTiming"), StaticEnum<EMoviePipelineShutterTiming>()->GetNameStringByValue((int64)ShutterTiming));
		InOutFormatArgs.FileMetadata.Add(TEXT("unreal/camera/overscanPercentage"), FString::SanitizeFloat(OverscanPercentage));
	}
public:	
	/**
	* Shutter Timing allows you to bias the timing of your shutter angle to either be before, during, or after
	* a frame. When set to FrameClose, it means that the motion gathered up to produce frame N comes from 
	* before and right up to frame N. When set to FrameCenter, the motion represents half the time before the
	* frame and half the time after the frame. When set to FrameOpen, the motion represents the time from 
	* Frame N onwards.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	EMoviePipelineShutterTiming ShutterTiming;

	/**
	* Overscan percent allows to render additional pixels beyond the set resolution and can be used in conjunction 
	* with EXR file output to add post-processing effects such as lens distortion.
	* Please note that using this feature might affect the results due to auto-exposure and other camera settings.
	* On EXR this will produce a 1080p image with extra pixel data hidden around the outside edges for use 
	* in post production. For all other formats this will increase the final resolution and no pixels will be hidden 
	* (ie: 1080p /w 0.1 overscan will make a 2112x1188 jpg, but a 1080p exr /w 96/54 pixels hidden on each side)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"), Category = "Camera Settings")
	float OverscanPercentage;
	
	/**
	* If true, when a Camera Cut section is found we will also render any other cameras within the same sequence (not parent, nor child sequences though).
	* These cameras are rendered at the same time as the primary camera meaning all cameras capture the same world state. Do note that this multiplies
	* render times and memory requirements!
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
	bool bRenderAllCameras;
};