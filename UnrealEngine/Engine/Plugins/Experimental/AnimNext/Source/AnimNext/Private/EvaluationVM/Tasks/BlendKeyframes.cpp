// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/BlendKeyframes.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextBlendTwoKeyframesTask FAnimNextBlendTwoKeyframesTask::Make(float InterpolationAlpha)
{
	FAnimNextBlendTwoKeyframesTask Task;
	Task.InterpolationAlpha = InterpolationAlpha;

	return Task;
}

void FAnimNextBlendTwoKeyframesTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> KeyframeB;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeB))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> KeyframeA;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeA))
	{
		// We have a single input, leave it on top of the stack
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
		return;
	}

	const float Alpha = FMath::Clamp(InterpolationAlpha, 0.0f, 1.0f);
	const float WeightOfPoseA = 1.0f - Alpha;
	const float WeightOfPoseB = Alpha;

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		const FTransformArrayView KeyframeBTransformsView = KeyframeB->Pose.LocalTransforms.GetView();

		BlendOverwriteWithScale(KeyframeBTransformsView, KeyframeB->Pose.LocalTransforms.GetConstView(), WeightOfPoseB);
		BlendAddWithScale(KeyframeBTransformsView, KeyframeA->Pose.LocalTransforms.GetConstView(), WeightOfPoseA);

		// Ensure that all of the resulting rotations are normalized
		NormalizeRotations(KeyframeBTransformsView);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		// Curves cannot blend in place
		FBlendedCurve Result;
		Result.Lerp(KeyframeA->Curves, KeyframeB->Curves, WeightOfPoseB);

		KeyframeB->Curves = MoveTemp(Result);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::BlendAttributes({ KeyframeA->Attributes, KeyframeB->Attributes }, { WeightOfPoseA, WeightOfPoseB }, { 0, 1 }, KeyframeB->Attributes);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
}

FAnimNextBlendOverwriteKeyframeWithScaleTask FAnimNextBlendOverwriteKeyframeWithScaleTask::Make(float ScaleFactor)
{
	FAnimNextBlendOverwriteKeyframeWithScaleTask Task;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendOverwriteKeyframeWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	TUniquePtr<FKeyframeState> Keyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, Keyframe))
	{
		// We have no inputs, nothing to do
		return;
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		BlendOverwriteWithScale(Keyframe->Pose.LocalTransforms.GetView(), Keyframe->Pose.LocalTransforms.GetConstView(), ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		// Curves cannot override in place
		FBlendedCurve Result;
		Result.Override(Keyframe->Curves, ScaleFactor);

		Keyframe->Curves = MoveTemp(Result);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::OverrideAttributes(Keyframe->Attributes, Keyframe->Attributes, ScaleFactor);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Keyframe));
}

FAnimNextBlendAddKeyframeWithScaleTask FAnimNextBlendAddKeyframeWithScaleTask::Make(float ScaleFactor)
{
	FAnimNextBlendAddKeyframeWithScaleTask Task;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendAddKeyframeWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> KeyframeB;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeB))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> KeyframeA;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeA))
	{
		// We have a single input, leave it on top of the stack
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
		return;
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		BlendAddWithScale(KeyframeB->Pose.LocalTransforms.GetView(), KeyframeA->Pose.LocalTransforms.GetConstView(), ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		KeyframeB->Curves.Accumulate(KeyframeA->Curves, ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::AccumulateAttributes(KeyframeA->Attributes, KeyframeB->Attributes, ScaleFactor, AAT_None);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
}
