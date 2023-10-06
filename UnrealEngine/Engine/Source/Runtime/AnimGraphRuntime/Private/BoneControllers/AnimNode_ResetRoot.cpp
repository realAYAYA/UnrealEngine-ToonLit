// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_ResetRoot.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimStats.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ResetRoot)

/////////////////////////////////////////////////////
// FAnimNode_ResetRoot

FAnimNode_ResetRoot::FAnimNode_ResetRoot()
{
}

void FAnimNode_ResetRoot::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(")"));
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_ResetRoot::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateSkeletalControl_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(ResetRoot, !IsInGameThread());

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	FCSPose<FCompactPose>& CSPose = Output.Pose;

	// Get all Children of the Root in Component Space. We want to preserve these in Component Space, after the Root has been reset.
	TArray<FTransform> ChildrenTransform;
	ChildrenTransform.Reserve(RootChildren.Num());
	for (int32 Index = 0; Index < RootChildren.Num(); Index++)
	{
		ChildrenTransform.Add(CSPose.GetComponentSpaceTransform(RootChildren[Index]));
	}

	// Reset Root
	const FCompactPoseBoneIndex RootBoneIndex = FCompactPoseBoneIndex(0);
	FTransform RootTransform = BoneContainer.GetRefPoseTransform(RootBoneIndex);
	
	OutBoneTransforms.Add(FBoneTransform(RootBoneIndex, RootTransform));
	for (int32 Index = 0; Index < RootChildren.Num(); Index++)
	{
		OutBoneTransforms.Add(FBoneTransform(RootChildren[Index], ChildrenTransform[Index]));
	}

	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

bool FAnimNode_ResetRoot::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return true;
}

void FAnimNode_ResetRoot::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
}

void FAnimNode_ResetRoot::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(InitializeBoneReferences)
	// Gather all direct Children of the Root.
	RootChildren.Reset();

	const int32 NumCompactPoseBones = RequiredBones.GetCompactPoseNumBones();
	for (int32 Index = 0; Index < NumCompactPoseBones; Index++)
	{
		const FCompactPoseBoneIndex BoneIndex = FCompactPoseBoneIndex(Index);
		const FCompactPoseBoneIndex ParentBoneIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		if (ParentBoneIndex == 0)
		{
			RootChildren.Add(BoneIndex);
		}
	}
}


