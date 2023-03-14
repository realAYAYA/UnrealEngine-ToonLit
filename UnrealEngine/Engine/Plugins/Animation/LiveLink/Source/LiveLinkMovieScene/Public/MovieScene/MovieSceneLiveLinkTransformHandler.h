// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MovieScene/IMovieSceneLiveLinkPropertyHandler.h"
#include "MovieScene/MovieSceneLiveLinkStructProperties.h"
#include "MovieScene/MovieSceneLiveLinkStructPropertyBindings.h"
#include "MovieScene/MovieSceneLiveLinkBufferData.h"


struct FKeyDataOptimizationParams;


class FMovieSceneLiveLinkTransformHandler : public IMovieSceneLiveLinkPropertyHandler
{
public:
	FMovieSceneLiveLinkTransformHandler(const FLiveLinkStructPropertyBindings& InBinding, FLiveLinkPropertyData* PropertyStorage)
		: PropertyStorage(PropertyStorage)
		, PropertyBinding(InBinding)
		, ElementCount(0)
	{
	}

	virtual ~FMovieSceneLiveLinkTransformHandler() = default;

public:

	virtual void CreateChannels(const UScriptStruct& InStruct, int32 InElementCount) override;
	virtual void RecordFrame(const FFrameNumber& InFrameNumber, const UScriptStruct& InStruct, const FLiveLinkBaseFrameData* InFrameData) override;
	virtual void Finalize(bool bInReduceKeys, const FKeyDataOptimizationParams& InOptimizationParams) override;

	virtual void InitializeFromExistingChannels(const UScriptStruct& InStruct) override;
	virtual void FillFrame(int32 InKeyIndex, const FLiveLinkWorldTime& InWorldTime, const TOptional<FQualifiedFrameTime>& InTimecodeTime, const UScriptStruct& InStruct, FLiveLinkBaseFrameData* OutFrame) override;
	virtual void FillFrameInterpolated(const FFrameTime& InFrameTime, const FLiveLinkWorldTime& InWorldTime, const TOptional<FQualifiedFrameTime>& InTimecodeTime, const UScriptStruct& InStruct, FLiveLinkBaseFrameData* OutFrame) override;
protected:

	/** Channel storage for this property */
	FLiveLinkPropertyData* PropertyStorage;

	/** Binding for this property */
	FLiveLinkStructPropertyBindings PropertyBinding;

	/** Number of elements to record each frame */
	int32 ElementCount;

	/** Buffer of transform keys. Keys are inserted into tracks in FinalizeTrack() */
	TArray<FLiveLinkTransformKeys> BufferedTransformsPerElement;
};




