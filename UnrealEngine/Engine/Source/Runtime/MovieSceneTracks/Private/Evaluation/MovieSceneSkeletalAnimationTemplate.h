// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "MovieSceneSkeletalAnimationTemplate.generated.h"

USTRUCT()
struct FMovieSceneSkeletalAnimationSectionTemplateParameters : public FMovieSceneSkeletalAnimationParams
{
	GENERATED_BODY()

	FMovieSceneSkeletalAnimationSectionTemplateParameters() {}
	FMovieSceneSkeletalAnimationSectionTemplateParameters(const FMovieSceneSkeletalAnimationParams& BaseParams, FFrameNumber InSectionStartTime, FFrameNumber InSectionEndTime)
		: FMovieSceneSkeletalAnimationParams(BaseParams)
		, SectionStartTime(InSectionStartTime)
		, SectionEndTime(InSectionEndTime)
	{}

	/**
	Calculates the animation's appropriate position in seconds from the begin by current frame time and frame rate
	@param InPosition	Current frame time in sequencer
	@param InFrameRate	Current frame time of this animation
	@return A double indicates the seconds of the animation we should play at this frame time
	*/
	double MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;

	/**The start time of this animation clip in this track in frames*/
	UPROPERTY()
	FFrameNumber SectionStartTime;

	/**The end time of this animation clip in this track in frames*/
	UPROPERTY()
	FFrameNumber SectionEndTime;
};

USTRUCT()
struct FMovieSceneSkeletalAnimationSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneSkeletalAnimationSectionTemplate() {}
	FMovieSceneSkeletalAnimationSectionTemplate(const UMovieSceneSkeletalAnimationSection& Section);

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneSkeletalAnimationSectionTemplateParameters Params;
};
