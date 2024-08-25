// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/PushReferenceKeyframe.h"

#include "EvaluationVM/EvaluationVM.h"

FAnimNextPushReferenceKeyframeTask FAnimNextPushReferenceKeyframeTask::MakeFromSkeleton()
{
	return FAnimNextPushReferenceKeyframeTask();
}

FAnimNextPushReferenceKeyframeTask FAnimNextPushReferenceKeyframeTask::MakeFromAdditiveIdentity()
{
	FAnimNextPushReferenceKeyframeTask Task;
	Task.bIsAdditive = true;

	return Task;
}

void FAnimNextPushReferenceKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(bIsAdditive)));
}
