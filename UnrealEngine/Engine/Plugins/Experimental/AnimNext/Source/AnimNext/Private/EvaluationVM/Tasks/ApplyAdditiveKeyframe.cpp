// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/ApplyAdditiveKeyframe.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextApplyAdditiveKeyframeTask FAnimNextApplyAdditiveKeyframeTask::Make(float BlendWeight)
{
	FAnimNextApplyAdditiveKeyframeTask Task;
	Task.BlendWeight = BlendWeight;

	return Task;
}

void FAnimNextApplyAdditiveKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> AdditiveKeyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, AdditiveKeyframe))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> BaseKeyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, BaseKeyframe))
	{
		// We have a single input, discard it since it must be the additive pose, either way something went wrong
		// Push the reference pose since we'll expect a non-additive pose
		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false)));
		return;
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(BaseKeyframe->Pose.GetNumBones() == AdditiveKeyframe->Pose.GetNumBones());

		const FTransformArrayView BaseTransformsView = BaseKeyframe->Pose.LocalTransforms.GetView();

#if 0
		// TODO: Read the additive type from the pose itself
		if (AdditiveType == AAT_RotationOffsetMeshSpace)
		{
			AccumulateMeshSpaceRotationAdditiveToLocalPoseInternal(BaseAnimationPoseData.GetPose(), AdditiveAnimationPoseData.GetPose(), Weight);
		}
		else
#endif
		{
			BlendWithIdentityAndAccumulate(BaseTransformsView, AdditiveKeyframe->Pose.LocalTransforms.GetConstView(), BlendWeight);
		}

		NormalizeRotations(BaseTransformsView);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		BaseKeyframe->Curves.Accumulate(AdditiveKeyframe->Curves, BlendWeight);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::AccumulateAttributes(AdditiveKeyframe->Attributes, BaseKeyframe->Attributes, BlendWeight, AAT_None);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(BaseKeyframe));
}
