// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "BlendKeyframes.generated.h"

/*
 * Blend Two Keyframes Task
 *
 * This pops the top two keyframes from the VM keyframe stack, it blends/interpolates them, and pushes
 * back the result onto the stack.
 * Let B be the input keyframe at the top of the stack and A be the second from the top.
 * Then we have:
 *     Result = Interpolate(A, B, Alpha)
 *     Top = Interpolate(Top-1, Top, Alpha)
 * An Alpha of 0.0 returns A while 1.0 returns B
 * 
 * Internally, this breaks down into the following tasks:
 *		* FAnimNextBlendOverwriteKeyframeWithScaleTask(1.0 - InterpolationAlpha)	// todo clarify alpha
 *		* FAnimNextBlendAddKeyframeWithScaleTask(InterpolationAlpha)
 *		* FAnimNextNormalizeKeyframeRotationsTask
 * 
 * In order to blend more than two keyframes together, simply add more FAnimNextBlendAddKeyframeWithScaleTask
 * tasks each with its corresponding weight before normalizing rotations.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextBlendTwoKeyframesTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextBlendTwoKeyframesTask)

	static FAnimNextBlendTwoKeyframesTask Make(float InterpolationAlpha);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The interpolation alpha between the two input keyframes
	UPROPERTY()
	float InterpolationAlpha = 0.0f;
};

/*
 * Blend Overwrite Keyframe With Scale Task
 *
 * This pops the top keyframe from the VM keyframe stack, it scales it by a factor, and pushes
 * back the result onto the stack.
 * Top = Top * ScaleFactor
 * 
 * Note that rotations will not be normalized after this task.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextBlendOverwriteKeyframeWithScaleTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextBlendOverwriteKeyframeWithScaleTask)

	static FAnimNextBlendOverwriteKeyframeWithScaleTask Make(float ScaleFactor);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The scale factor to apply to the keyframe
	UPROPERTY()
	float ScaleFactor = 1.0f;
};

/*
 * Blend Add Keyframe With Scale Task
 *
 * This pops the top two keyframes (A and B) from the VM keyframe stack (let B be at the top).
 * B is our intermediary result that we add on top of; while A is the keyframe we scale.
 * The result is pushed back onto the stack.
 * Top = Top + (Top-1 * ScaleFactor)
 * 
 * Note that rotations will not be normalized after this task.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextBlendAddKeyframeWithScaleTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextBlendAddKeyframeWithScaleTask)

	static FAnimNextBlendAddKeyframeWithScaleTask Make(float ScaleFactor);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The scale factor to apply to the keyframe
	UPROPERTY()
	float ScaleFactor = 0.0f;
};
