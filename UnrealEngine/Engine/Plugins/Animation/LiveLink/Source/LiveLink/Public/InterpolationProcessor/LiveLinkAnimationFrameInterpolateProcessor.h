// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterpolationProcessor/LiveLinkBasicFrameInterpolateProcessor.h"
#include "LiveLinkAnimationFrameInterpolateProcessor.generated.h"


/**
 * Default blending method for animation frames
 */
UCLASS(meta=(DisplayName="Animation Interpolation"))
class LIVELINK_API ULiveLinkAnimationFrameInterpolationProcessor : public ULiveLinkBasicFrameInterpolationProcessor
{
	GENERATED_BODY()

public:
	class FLiveLinkAnimationFrameInterpolationProcessorWorker : public ULiveLinkBasicFrameInterpolationProcessor::FLiveLinkBasicFrameInterpolationProcessorWorker
	{
	public:
		FLiveLinkAnimationFrameInterpolationProcessorWorker(bool bInterpolatePropertyValues);

		virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
		virtual void Interpolate(double InTime, const FLiveLinkStaticDataStruct& InStaticData, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, FLiveLinkSubjectFrameData& OutBlendedFrame, FLiveLinkInterpolationInfo& OutInterpolationInfo) override;
		virtual void Interpolate(const FQualifiedFrameTime& InTime, const FLiveLinkStaticDataStruct& InStaticData, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, FLiveLinkSubjectFrameData& OutBlendedFrame, FLiveLinkInterpolationInfo& OutInterpolationInfo) override;
	};

public:
	virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
	virtual ULiveLinkFrameInterpolationProcessor::FWorkerSharedPtr FetchWorker() override;

private:
	TSharedPtr<FLiveLinkAnimationFrameInterpolationProcessorWorker, ESPMode::ThreadSafe> Instance;
};
