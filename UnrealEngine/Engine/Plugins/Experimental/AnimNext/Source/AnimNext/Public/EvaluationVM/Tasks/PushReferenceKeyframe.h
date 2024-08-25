// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "PushReferenceKeyframe.generated.h"

/*
 * Push Reference Keyframe Task
 *
 * This pushes a reference keyframe onto the top of the VM keyframe stack.
 * This task can be used to push the reference keyframe from a source skeleton
 * or the additive identity.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextPushReferenceKeyframeTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextPushReferenceKeyframeTask)

	static FAnimNextPushReferenceKeyframeTask MakeFromSkeleton();
	static FAnimNextPushReferenceKeyframeTask MakeFromAdditiveIdentity();

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// Whether or not the reference pose comes from the skeleton or is the additive identity
	UPROPERTY()
	bool bIsAdditive = false;
};
