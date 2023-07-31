// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFrameInterpolationProcessor.h"
#include "LiveLinkBasicFrameInterpolateProcessor.generated.h"

/**
 * Default blending method for any type of frames.
 * It will interpolate numerical properties that are mark with "Interp".
 */
UCLASS(meta = (DisplayName = "Base Interpolation"))
class LIVELINK_API ULiveLinkBasicFrameInterpolationProcessor : public ULiveLinkFrameInterpolationProcessor
{
	GENERATED_BODY()

public:
	class LIVELINK_API FLiveLinkBasicFrameInterpolationProcessorWorker : public ILiveLinkFrameInterpolationProcessorWorker
	{
	public:
		FLiveLinkBasicFrameInterpolationProcessorWorker(bool bInterpolatePropertyValues);

		virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
		
		virtual void Interpolate(double InTime, const FLiveLinkStaticDataStruct& InStaticData, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, FLiveLinkSubjectFrameData& OutBlendedFrame, FLiveLinkInterpolationInfo& OutInterpolationInfo) override;
		virtual void Interpolate(const FQualifiedFrameTime& InTime, const FLiveLinkStaticDataStruct& InStaticData, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, FLiveLinkSubjectFrameData& OutBlendedFrame, FLiveLinkInterpolationInfo& OutInterpolationInfo) override;

		struct FGenericInterpolateOptions
		{
			bool bInterpolatePropertyValues = true;
			bool bCopyClosestFrame = true;
			bool bCopyClosestMetaData = true; // only used if bCopyClosestFrame is false. Does NOT apply to SceneTime, it will always be interpolated.
			bool bInterpolateInterpProperties = true;
		};

		static void GenericInterpolate(double InBlendFactor, const FGenericInterpolateOptions& Options, const FLiveLinkFrameDataStruct& FrameDataA, const FLiveLinkFrameDataStruct& FrameDataB, FLiveLinkFrameDataStruct& OutBlendedFrame);
		static double GetBlendFactor(double InTime, const FLiveLinkFrameDataStruct& FrameDataA, const FLiveLinkFrameDataStruct& FrameDataB);
		static double GetBlendFactor(FQualifiedFrameTime InTime, const FLiveLinkFrameDataStruct& FrameDataA, const FLiveLinkFrameDataStruct& FrameDataB);
		static bool FindInterpolateIndex(double InTime, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, int32& OutFrameIndexA, int32& OutFrameIndexB, FLiveLinkInterpolationInfo& OutInterpolationInfo);
		static bool FindInterpolateIndex(FQualifiedFrameTime InTime, const TArray<FLiveLinkFrameDataStruct>& InSourceFrames, int32& OutFrameIndexA, int32& OutFrameIndexB, FLiveLinkInterpolationInfo& OutInterpolationInfo);

	protected:
		bool bInterpolatePropertyValues = true;
	};

public:
	virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
	virtual ULiveLinkFrameInterpolationProcessor::FWorkerSharedPtr FetchWorker() override;

public:
	UPROPERTY(EditAnywhere, Category = "LiveLink")
	bool bInterpolatePropertyValues = true;

private:
	TSharedPtr<FLiveLinkBasicFrameInterpolationProcessorWorker, ESPMode::ThreadSafe> BaseInstance;
};