// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/BlendKeyframesPerBone.h"

#include "AnimationRuntime.h"
#include "Animation/BlendProfile.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "TransformArrayOperations.h"

FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask::Make(TWeakObjectPtr<const UBlendProfile> BlendProfile, const FBlendSampleData& BlendData, float ScaleFactor)
{
	FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask Task;
	Task.BlendProfile = BlendProfile;
	Task.BlendData = &BlendData;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	const UBlendProfile* BlendProfilePtr = BlendProfile.Get();
	if (BlendProfilePtr == nullptr)
	{
		// If we don't have a blend profile, we blend the whole pose
		Super::Execute(VM);
		return;
	}

	TUniquePtr<FKeyframeState> Keyframe;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, Keyframe))
	{
		// We have no inputs, nothing to do
		return;
	}

	const USkeleton* TargetSkeleton = Keyframe->Pose.GetSkeletonAsset();
	TSharedPtr<IInterpolationIndexProvider::FPerBoneInterpolationData> Data = BlendProfilePtr->GetPerBoneInterpolationData(TargetSkeleton);

	const USkeleton* SourceSkeleton = BlendProfilePtr->OwningSkeleton;
	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(SourceSkeleton, TargetSkeleton);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = Keyframe->Pose.GetLODBoneIndexToSkeletonBoneIndexMap();
	const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();

	TArray<int32> LODBoneIndexToWeightIndexMap;
	LODBoneIndexToWeightIndexMap.AddUninitialized(NumLODBones);

	if (SkeletonRemapping.IsValid())
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex TargetSkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			const FSkeletonPoseBoneIndex SourceSkeletonBoneIndex(SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex.GetInt()));
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = SourceSkeletonBoneIndex.IsValid() ? BlendProfilePtr->GetPerBoneInterpolationIndex(SourceSkeletonBoneIndex, TargetSkeleton, Data.Get()) : INDEX_NONE;
		}
	}
	else
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = BlendProfilePtr->GetPerBoneInterpolationIndex(SkeletonBoneIndex, TargetSkeleton, Data.Get());
		}
	}

	const float BlendWeight = BlendData->GetClampedWeight();

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		BlendOverwritePerBoneWithScale(
			Keyframe->Pose.LocalTransforms.GetView(), Keyframe->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BlendData->PerBoneBlendData, BlendWeight);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		// Curves cannot override in place
		FBlendedCurve Result;
		Result.Override(Keyframe->Curves, BlendWeight);

		Keyframe->Curves = MoveTemp(Result);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::BlendAttributesPerBone({ Keyframe->Attributes }, { LODBoneIndexToWeightIndexMap }, { *BlendData }, { 0 }, { Keyframe->Attributes });
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Keyframe));
}

FAnimNextBlendAddKeyframePerBoneWithScaleTask FAnimNextBlendAddKeyframePerBoneWithScaleTask::Make(TWeakObjectPtr<const UBlendProfile> BlendProfile, const FBlendSampleData& BlendDataA, const FBlendSampleData& BlendDataB, float ScaleFactor)
{
	FAnimNextBlendAddKeyframePerBoneWithScaleTask Task;
	Task.BlendProfile = BlendProfile;
	Task.BlendDataA = &BlendDataA;
	Task.BlendDataB = &BlendDataB;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FAnimNextBlendAddKeyframePerBoneWithScaleTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	const UBlendProfile* BlendProfilePtr = BlendProfile.Get();
	if (BlendProfilePtr == nullptr)
	{
		// If we don't have a blend profile, we blend the whole pose
		Super::Execute(VM);
		return;
	}

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

	const USkeleton* TargetSkeleton = KeyframeB->Pose.GetSkeletonAsset();
	TSharedPtr<IInterpolationIndexProvider::FPerBoneInterpolationData> Data = BlendProfilePtr->GetPerBoneInterpolationData(TargetSkeleton);

	const USkeleton* SourceSkeleton = BlendProfilePtr->OwningSkeleton;
	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(SourceSkeleton, TargetSkeleton);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = KeyframeB->Pose.GetLODBoneIndexToSkeletonBoneIndexMap();
	const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();

	TArray<int32> LODBoneIndexToWeightIndexMap;
	LODBoneIndexToWeightIndexMap.AddUninitialized(NumLODBones);

	if (SkeletonRemapping.IsValid())
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex TargetSkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			const FSkeletonPoseBoneIndex SourceSkeletonBoneIndex(SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex.GetInt()));
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = SourceSkeletonBoneIndex.IsValid() ? BlendProfilePtr->GetPerBoneInterpolationIndex(SourceSkeletonBoneIndex, TargetSkeleton, Data.Get()) : INDEX_NONE;
		}
	}
	else
	{
		for (int32 LODBoneIndex = 0; LODBoneIndex < NumLODBones; ++LODBoneIndex)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIndex(LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]);
			LODBoneIndexToWeightIndexMap[LODBoneIndex] = BlendProfilePtr->GetPerBoneInterpolationIndex(SkeletonBoneIndex, TargetSkeleton, Data.Get());
		}
	}

	const float BlendWeightA = BlendDataA->GetClampedWeight();

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		BlendAddPerBoneWithScale(
			KeyframeB->Pose.LocalTransforms.GetView(), KeyframeA->Pose.LocalTransforms.GetConstView(),
			LODBoneIndexToWeightIndexMap, BlendDataA->PerBoneBlendData, BlendWeightA);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		KeyframeB->Curves.Accumulate(KeyframeA->Curves, BlendWeightA);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		// TODO: Might need to be revisited, might not work as intended
		UE::Anim::Attributes::BlendAttributesPerBone(
			{ KeyframeA->Attributes, KeyframeB->Attributes },
			{ LODBoneIndexToWeightIndexMap },
			{ *BlendDataA, *BlendDataB },
			{ 0, 1 },
			{ KeyframeA->Attributes });
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
}
