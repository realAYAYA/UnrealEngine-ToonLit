// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBoneUtilities.h"
#include "BonePose.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "ReferenceSkeleton.h"

FTransform FGLTFBoneUtilities::GetBindTransform(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
{
	const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.GetRefBoneInfo();
	const TArray<FTransform>& BonePoses = RefSkeleton.GetRefBonePose();

	int32 CurBoneIndex = BoneIndex;
	FTransform BindTransform = FTransform::Identity;

	do
	{
		BindTransform = BindTransform * BonePoses[CurBoneIndex];
		CurBoneIndex = BoneInfos[CurBoneIndex].ParentIndex;
	} while (CurBoneIndex != INDEX_NONE);

	return BindTransform;
}

void FGLTFBoneUtilities::GetFrameTimestamps(const UAnimSequence* AnimSequence, TArray<float>& OutFrameTimestamps)
{
	const int32 FrameCount = AnimSequence->GetNumberOfSampledKeys();
	OutFrameTimestamps.AddUninitialized(FrameCount);

	const float SequenceLength = AnimSequence->GetPlayLength();
	const float FrameLength = FrameCount > 1 ? SequenceLength / (FrameCount - 1) : 0;

	for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		OutFrameTimestamps[FrameIndex] = FMath::Clamp(FrameIndex * FrameLength, 0.0f, SequenceLength);
	}
}

void FGLTFBoneUtilities::GetBoneIndices(const USkeleton* Skeleton, TArray<FBoneIndexType>& OutBoneIndices)
{
	const int32 BoneCount = Skeleton->GetReferenceSkeleton().GetNum();
	OutBoneIndices.AddUninitialized(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		OutBoneIndices[BoneIndex] = BoneIndex;
	}
}

void FGLTFBoneUtilities::GetBoneTransformsByFrame(const UAnimSequence* AnimSequence, const TArray<float>& FrameTimestamps, const TArray<FBoneIndexType>& BoneIndices, TArray<TArray<FTransform>>& OutBoneTransformsByFrame)
{

	// Make sure to free stack allocations made by FCompactPose, FBlendedCurve, and FStackCustomAttributes when end of scope
	FMemMark Mark(FMemStack::Get());

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(BoneIndices, FCurveEvaluationOption(true), *AnimSequence->GetSkeleton());

	const int32 FrameCount = FrameTimestamps.Num();
	OutBoneTransformsByFrame.AddDefaulted(FrameCount);

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData PoseData(Pose, Curve, Attributes);

	for (int32 Frame = 0; Frame < FrameCount; ++Frame)
	{
		const FAnimExtractContext ExtractionContext(static_cast<double>(FrameTimestamps[Frame])); // TODO: set bExtractRootMotion?
		AnimSequence->GetBonePose(PoseData, ExtractionContext);
		Pose.CopyBonesTo(OutBoneTransformsByFrame[Frame]);
	}

	// Clear all stack allocations to allow FMemMark to free them
	Pose.Empty();
	Curve.Empty();
	Attributes.Empty();
}
