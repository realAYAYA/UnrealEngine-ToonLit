// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBoneUtility.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "ReferenceSkeleton.h"

FTransform FGLTFBoneUtility::GetBindTransform(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
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

void FGLTFBoneUtility::GetFrameTimestamps(const UAnimSequence* AnimSequence, TArray<float>& OutFrameTimestamps)
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

void FGLTFBoneUtility::GetBoneIndices(const USkeleton* Skeleton, TArray<FBoneIndexType>& OutBoneIndices)
{
	const int32 BoneCount = Skeleton->GetReferenceSkeleton().GetNum();
	OutBoneIndices.AddUninitialized(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		OutBoneIndices[BoneIndex] = BoneIndex;
	}
}

void FGLTFBoneUtility::GetBoneTransformsByFrame(const UAnimSequence* AnimSequence, const TArray<float>& FrameTimestamps, const TArray<FBoneIndexType>& BoneIndices, TArray<TArray<FTransform>>& OutBoneTransformsByFrame)
{
#if WITH_EDITOR
	const bool bUseRawData = AnimSequence->OnlyUseRawData();
#else
	const bool bUseRawData = false;
#endif

	// Make sure to free stack allocations made by FCompactPose, FBlendedCurve, and FStackCustomAttributes when end of scope
	FMemMark Mark(FMemStack::Get());

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(BoneIndices, FCurveEvaluationOption(true), *AnimSequence->GetSkeleton());
	BoneContainer.SetUseRAWData(bUseRawData);

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
		const FAnimExtractContext ExtractionContext(FrameTimestamps[Frame]); // TODO: set bExtractRootMotion?
		AnimSequence->GetBonePose(PoseData, ExtractionContext, bUseRawData);
		Pose.CopyBonesTo(OutBoneTransformsByFrame[Frame]);
	}

	// Clear all stack allocations to allow FMemMark to free them
	Pose.Empty();
	Curve.Empty();
	Attributes.Empty();
}

const FMovieSceneDoubleChannel* FGLTFBoneUtility::GetTranslationChannels(const UMovieScene3DTransformSection* TransformSection)
{
	// TODO: fix ugly workaround hack in engine api to access channels without GetChannelProxy (which doesn't work properly in UE5)
	static const FProperty* Property = UMovieScene3DTransformSection::StaticClass()->FindPropertyByName(TEXT("Translation"));
	return Property->ContainerPtrToValuePtr<FMovieSceneDoubleChannel>(TransformSection);
}

const FMovieSceneDoubleChannel* FGLTFBoneUtility::GetRotationChannels(const UMovieScene3DTransformSection* TransformSection)
{
	// TODO: fix ugly workaround hack in engine api to access channels without GetChannelProxy (which doesn't work properly in UE5)
	static const FProperty* Property = UMovieScene3DTransformSection::StaticClass()->FindPropertyByName(TEXT("Rotation"));
	return Property->ContainerPtrToValuePtr<FMovieSceneDoubleChannel>(TransformSection);
}

const FMovieSceneDoubleChannel* FGLTFBoneUtility::GetScaleChannels(const UMovieScene3DTransformSection* TransformSection)
{
	// TODO: fix ugly workaround hack in engine api to access channels without GetChannelProxy (which doesn't work properly in UE5)
	static const FProperty* Property = UMovieScene3DTransformSection::StaticClass()->FindPropertyByName(TEXT("Scale"));
	return Property->ContainerPtrToValuePtr<FMovieSceneDoubleChannel>(TransformSection);
}
