// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchMeshComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/MirrorDataTable.h"
#include "Engine/SkinnedAsset.h"
#include "PoseSearch/PoseSearchContext.h"

void UPoseSearchMeshComponent::Initialize(const FTransform& InComponentToWorld)
{
	SetComponentToWorld(InComponentToWorld);
	const FReferenceSkeleton& SkeletalMeshRefSkeleton = GetSkinnedAsset()->GetRefSkeleton();

	// set up bone visibility states as this gets skipped since we allocate the component array before registration
	for (int32 BaseIndex = 0; BaseIndex < 2; ++BaseIndex)
	{
		BoneVisibilityStates[BaseIndex].SetNum(SkeletalMeshRefSkeleton.GetNum());
		for (int32 BoneIndex = 0; BoneIndex < SkeletalMeshRefSkeleton.GetNum(); BoneIndex++)
		{
			BoneVisibilityStates[BaseIndex][BoneIndex] = BVS_ExplicitlyHidden;
		}
	}

	StartingTransform = InComponentToWorld;
	Refresh();
}

void UPoseSearchMeshComponent::Refresh()
{
	// Flip buffers once to copy the directly-written component space transforms
	bNeedToFlipSpaceBaseBuffers = true;
	bHasValidBoneTransform = false;
	FlipEditableSpaceBases();
	bHasValidBoneTransform = true;

	InvalidateCachedBounds();
	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();
	MarkRenderStateDirty();
}

void UPoseSearchMeshComponent::ResetToStart()
{
	SetComponentToWorld(StartingTransform);
	Refresh();
}

void UPoseSearchMeshComponent::UpdatePose(const FUpdateContext& UpdateContext)
{
	FMemMark Mark(FMemStack::Get());

	FCompactPose CompactPose;
	CompactPose.SetBoneContainer(&RequiredBones);
	FBlendedCurve Curve;
	Curve.InitFrom(RequiredBones);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData PoseData(CompactPose, Curve, Attributes);

	if (UpdateContext.SequenceBase)
	{
		float AdvancedTime = UpdateContext.StartTime;

		FAnimationRuntime::AdvanceTime(
			UpdateContext.bLoop,
			UpdateContext.Time - UpdateContext.StartTime,
			AdvancedTime,
			UpdateContext.SequenceBase->GetPlayLength());

		FAnimExtractContext ExtractionCtx;
		ExtractionCtx.CurrentTime = AdvancedTime;

		UpdateContext.SequenceBase->GetAnimationPose(PoseData, ExtractionCtx);
	}
	else if (UpdateContext.BlendSpace)
	{
		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		UpdateContext.BlendSpace->GetSamplesFromBlendInput(UpdateContext.BlendParameters, BlendSamples, TriangulationIndex, true);
		
		float PlayLength = UpdateContext.BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);
		
		float PreviousTime = UpdateContext.StartTime * PlayLength;
		float CurrentTime = UpdateContext.Time * PlayLength;

		float AdvancedTime = PreviousTime;
		FAnimationRuntime::AdvanceTime(
			UpdateContext.bLoop,
			CurrentTime - PreviousTime,
			CurrentTime,
			PlayLength);
		
		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(PreviousTime, AdvancedTime - PreviousTime);
		FAnimExtractContext ExtractionCtx(static_cast<double>(AdvancedTime), true, DeltaTimeRecord, UpdateContext.bLoop);

		for (int32 BlendSampleIdex = 0; BlendSampleIdex < BlendSamples.Num(); BlendSampleIdex++)
		{
			float Scale = BlendSamples[BlendSampleIdex].Animation->GetPlayLength() / PlayLength;

			FDeltaTimeRecord BlendSampleDeltaTimeRecord;
			BlendSampleDeltaTimeRecord.Set(DeltaTimeRecord.GetPrevious() * Scale, DeltaTimeRecord.Delta * Scale);

			BlendSamples[BlendSampleIdex].DeltaTimeRecord = BlendSampleDeltaTimeRecord;
			BlendSamples[BlendSampleIdex].PreviousTime = PreviousTime * Scale;
			BlendSamples[BlendSampleIdex].Time = AdvancedTime * Scale;
		}

		UpdateContext.BlendSpace->GetAnimationPose(BlendSamples, ExtractionCtx, PoseData);
	}
	else
	{
		checkNoEntry();
	}

	LastRootMotionDelta = FTransform::Identity;

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();

	if (ensureMsgf(RootMotionProvider, TEXT("Could not get Root Motion Provider.")))
	{
		if (ensureMsgf(RootMotionProvider->HasRootMotion(Attributes), TEXT("Blend Space had no Root Motion Attribute.")))
		{
			RootMotionProvider->ExtractRootMotion(Attributes, LastRootMotionDelta);
		}
	}

	if (UpdateContext.bMirrored)
	{
		FAnimationRuntime::MirrorPose(
			CompactPose, 
			UpdateContext.MirrorDataTable->MirrorAxis, 
			*UpdateContext.CompactPoseMirrorBones, 
			*UpdateContext.ComponentSpaceRefRotations);
	}

	FCSPose<FCompactPose> ComponentSpacePose;
	ComponentSpacePose.InitPose(CompactPose);

	for (const FBoneIndexType BoneIndex : RequiredBones.GetBoneIndicesArray())
	{
		const FTransform BoneTransform = 
			ComponentSpacePose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex));

		FSkeletonPoseBoneIndex SkeletonBoneIndex =
			RequiredBones.GetSkeletonPoseIndexFromCompactPoseIndex(FCompactPoseBoneIndex(BoneIndex));
		FName BoneName = 
			RequiredBones.GetSkeletonAsset()->GetReferenceSkeleton().GetBoneName(SkeletonBoneIndex.GetInt());
		SetBoneTransformByName(BoneName, BoneTransform, EBoneSpaces::ComponentSpace);
	}

	if (UpdateContext.bMirrored)
	{
		LastRootMotionDelta = UE::PoseSearch::MirrorTransform(LastRootMotionDelta, UpdateContext.MirrorDataTable->MirrorAxis, (*UpdateContext.ComponentSpaceRefRotations)[FCompactPoseBoneIndex(RootBoneIndexType)]);
	}

	const FTransform ComponentTransform = LastRootMotionDelta * StartingTransform;

	SetComponentToWorld(ComponentTransform);
	FillComponentSpaceTransforms();
	Refresh();
}