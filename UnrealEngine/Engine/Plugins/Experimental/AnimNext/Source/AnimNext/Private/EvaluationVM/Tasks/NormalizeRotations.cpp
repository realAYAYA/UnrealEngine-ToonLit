// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/NormalizeRotations.h"

#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

void FAnimNextNormalizeKeyframeRotationsTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		TUniquePtr<FKeyframeState> Keyframe;
		if (!VM.PopValue(KEYFRAME_STACK_NAME, Keyframe))
		{
			// We have no inputs, nothing to do
			return;
		}

		NormalizeRotations(Keyframe->Pose.LocalTransforms.GetView());

		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Keyframe));
	}
}
