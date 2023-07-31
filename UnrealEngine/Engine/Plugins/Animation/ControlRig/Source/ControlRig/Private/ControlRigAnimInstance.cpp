// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigAnimInstance.h"

#include "Animation/AnimNodeBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigAnimInstance)

////////////////////////////////////////////////////////////////////////////////////////
void FControlRigAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
}

bool FControlRigAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (StoredTransforms.Num() == 0 && StoredCurves.Num() == 0)
	{
		return false;
	}

	FComponentSpacePoseContext CSContext(this);
	CSContext.Pose.InitPose(Output.Pose);

	TArray<FCompactPoseBoneIndex> ModifiedBones;
	for (TPair<int32, FTransform>& StoredTransform : StoredTransforms)
	{
		const int32 BoneIndexToModify = StoredTransform.Key;
		FCompactPoseBoneIndex CompactIndex = Output.Pose.GetBoneContainer().GetCompactPoseIndexFromSkeletonIndex(BoneIndexToModify);
		if (CompactIndex.GetInt() != INDEX_NONE)
		{
			CSContext.Pose.SetComponentSpaceTransform(CompactIndex, StoredTransform.Value);
			ModifiedBones.Add(CompactIndex);
		}
	}

	FCompactPose CompactPose(Output.Pose);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CSContext.Pose, CompactPose);

	// reset to ref pose before setting the pose to ensure if we don't have any missing bones
	Output.ResetToRefPose();

	for (const FCompactPoseBoneIndex& ModifiedBone : ModifiedBones)
	{
		Output.Pose[ModifiedBone] = CompactPose[ModifiedBone];
	}

	for (TPair<SmartName::UID_Type, float> Pair : StoredCurves)
	{
		Output.Curve.Set(Pair.Key, Pair.Value);
	}

	return true;
}

void FControlRigAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	//SnapshotNode.Update_AnyThread(InContext);
	return;
}

FControlRigAnimInstanceProxy::~FControlRigAnimInstanceProxy()
{
}

////////////////////////////////////////////////////////////////////////////////////////

UControlRigAnimInstance::UControlRigAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UControlRigAnimInstance::CreateAnimInstanceProxy()
{
	return new FControlRigAnimInstanceProxy(this);
}


