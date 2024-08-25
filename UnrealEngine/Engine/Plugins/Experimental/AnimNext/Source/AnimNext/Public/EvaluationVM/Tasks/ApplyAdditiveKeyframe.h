// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "ApplyAdditiveKeyframe.generated.h"

/*
 * Apply Additive Keyframe Task
 *
 * This pops the top two keyframes from the VM keyframe stack, it applies an additive keyframe onto its base, and pushes
 * back the result onto the stack.
 * The top pose should be the additive keyframe and the second to the top the base keyframe.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextApplyAdditiveKeyframeTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextApplyAdditiveKeyframeTask)

	static FAnimNextApplyAdditiveKeyframeTask Make(float BlendWeight);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// How much weight between the additive identity and the additive pose to apply: lerp(identity, additive, weight)
	UPROPERTY()
	float BlendWeight = 0.0f;
};
