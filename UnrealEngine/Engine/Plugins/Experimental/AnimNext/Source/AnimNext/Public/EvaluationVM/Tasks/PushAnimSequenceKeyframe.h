// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "PushAnimSequenceKeyframe.generated.h"

class UAnimSequence;

/*
 * Push Anim Sequence Keyframe Task
 *
 * This pushes an anim sequence keyframe onto the top of the VM keyframe stack.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextAnimSequenceKeyframeTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextAnimSequenceKeyframeTask)

	static FAnimNextAnimSequenceKeyframeTask MakeFromSampleTime(TWeakObjectPtr<UAnimSequence> AnimSequence, double SampleTime, bool bInterpolate);
	static FAnimNextAnimSequenceKeyframeTask MakeFromKeyframeIndex(TWeakObjectPtr<UAnimSequence> AnimSequence, uint32 KeyframeIndex);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// Anim Sequence to grab the keyframe from
	UPROPERTY()
	TWeakObjectPtr<UAnimSequence> AnimSequence;

	// The point in time within the animation sequence at which we sample the keyframe.
	// If negative, the sample time hasn't been provided and we should use the keyframe index.
	UPROPERTY()
	double SampleTime = -1.0;

	// The specific keyframe within the animation sequence to retrieve.
	// If ~0, the keyframe index hasn't been provided and we should use the sample time.
	UPROPERTY()
	uint32 KeyframeIndex = ~0;

	// Whether to interpolate or step the animation sequence.
	// Only used when the sample time is used.
	UPROPERTY()
	bool bInterpolate = false;

	// Whether to extract trajectory or not
	UPROPERTY()
	bool bExtractTrajectory = false;
};
