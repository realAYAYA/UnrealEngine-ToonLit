// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecompressionTools.h"
#include "Animation/AnimSequenceDecompressionContext.h"
#include "Animation/AnimCompressionTypes.h"
#include "Animation/AnimBoneCompressionCodec.h"

#include "AnimationRuntime.h"
#include "AnimEncoding.h"
#include "Animation/AnimBoneDecompressionData.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "HAL/ConsoleManager.h"

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);

DECLARE_CYCLE_STAT(TEXT("Build Anim Track Pairs"), STAT_BuildAnimTrackPairs, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("Extract Pose From Anim Data"), STAT_ExtractPoseFromAnimData, STATGROUP_Anim);

DECLARE_CYCLE_STAT(TEXT("AnimSeq GetBonePose"), STAT_AnimSeq_GetBonePose, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("AnimSeq EvalCurveData"), STAT_AnimSeq_EvalCurveData, STATGROUP_Anim);

namespace UE::AnimNext
{

static IConsoleVariable* CVarForceEvalRawData = IConsoleManager::Get().FindConsoleVariable(TEXT("a.ForceEvalRawData"));
static bool GetForceRawData()
{
	return (CVarForceEvalRawData != nullptr) ? CVarForceEvalRawData->GetBool() : false;
}


//****************************************************************************
// This file contains code extracted from AnimSequence and AnimationDecompression,
// in order to be able to decompress an anim sequence using AnimNext format
//****************************************************************************

struct FGetBonePoseScratchArea : public TThreadSingleton<FGetBonePoseScratchArea>
{
	BoneTrackArray RotationScalePairs;
	BoneTrackArray TranslationPairs;
	BoneTrackArray AnimScaleRetargetingPairs;
	BoneTrackArray AnimRelativeRetargetingPairs;
	BoneTrackArray OrientAndScaleRetargetingPairs;
};


static bool CanEvaluateRawAnimationData(const UAnimSequence* AnimSequence)
{
#if WITH_EDITOR
	return AnimSequence->IsDataModelValid();
#else
	return false;
#endif
}

static bool UseRawDataForPoseExtraction(const UAnimSequence* AnimSequence, FLODPose& AnimationPoseData)
{
	return CanEvaluateRawAnimationData(AnimSequence) && (
#if WITH_EDITOR
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AnimSequence->OnlyUseRawData() ||
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
		(AnimSequence->GetSkeletonVirtualBoneGuid() != AnimSequence->GetSkeleton()->GetVirtualBoneGuid()) || AnimationPoseData.GetDisableRetargeting() || AnimationPoseData.ShouldUseRawData() ||
#if WITH_EDITOR
		GetForceRawData() ||
#endif // WITH_EDITOR
		AnimationPoseData.ShouldUseSourceData());
}

// --- ---

/*static*/ void FDecompressionTools::GetAnimationPose(const UAnimSequence* AnimSequence, FLODPose& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext)
{
	if (OutAnimationPoseData.GetRefPose().IsValid() == false)
	{
		return;
	}

	// @todo anim: if compressed and baked in the future, we don't have to do this 
	if (UseRawDataForPoseExtraction(AnimSequence, OutAnimationPoseData) && AnimSequence->IsValidAdditive())
	{
		switch (AnimSequence->GetAdditiveAnimType())
		{
			case AAT_LocalSpaceBase:
			{
				GetBonePose_Additive(AnimSequence, ExtractionContext, OutAnimationPoseData);
			}
			break;

			case AAT_RotationOffsetMeshSpace:
			{
				GetBonePose_AdditiveMeshRotationOnly(AnimSequence, ExtractionContext, OutAnimationPoseData);
			}
			break;

			default:
				break;
		}
	}
	else
	{
		GetBonePose(AnimSequence, ExtractionContext, OutAnimationPoseData);
	}

	// If the sequence has root motion enabled, allow sampling of a root motion delta into the custom attribute container of the outgoing pose
	if (AnimSequence->HasRootMotion())
	{
		if (const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get())
		{
			//RootMotionProvider->SampleRootMotion(ExtractionContext.DeltaTimeRecord, *AnimSequence, ExtractionContext.bLooping, OutAnimationPoseData.GetAttributes());
		}
	}

	// Check that all bone atoms coming from animation are normalized
#if DO_CHECK && WITH_EDITORONLY_DATA
	//check(OutAnimationPoseData.LocalTransforms.IsNormalized());
#endif

}

void FDecompressionTools::GetBonePose(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData, bool bForceUseRawData /*= false*/)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimSeq_GetBonePose);
	CSV_SCOPED_TIMING_STAT(Animation, AnimSeq_GetBonePose);

	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = OutAnimationPoseData.GetLODBoneIndexToSkeletonBoneIndexMap();

	check(!bForceUseRawData || CanEvaluateRawAnimationData(AnimSequence));
	const bool bUseRawDataForPoseExtraction = (CanEvaluateRawAnimationData(AnimSequence) && bForceUseRawData) ||
#if WITH_EDITOR
		GetForceRawData() ||
#endif // WITH_EDITOR
		UseRawDataForPoseExtraction(AnimSequence, OutAnimationPoseData);

	const bool bIsBakedAdditive = !bUseRawDataForPoseExtraction && AnimSequence->IsValidAdditive();

	const USkeleton* MySkeleton = AnimSequence->GetSkeleton();
	if (!MySkeleton)
	{
		OutAnimationPoseData.SetRefPose(bIsBakedAdditive);
		return;
	}

	const bool bDisableRetargeting = OutAnimationPoseData.GetDisableRetargeting();

	// initialize with ref-pose
	if (bIsBakedAdditive)
	{
		//When using baked additive ref pose is identity
		OutAnimationPoseData.SetRefPose(bIsBakedAdditive);
	}
	else
	{
		// if retargeting is disabled, we initialize pose with 'Retargeting Source' ref pose.
		if (bDisableRetargeting)
		{
			const TArray<FTransform>& AuthoredOnRefSkeleton = AnimSequence->GetRetargetTransforms();

			const int32 NumLODBones = LODBoneIndexToSkeletonBoneIndexMap.Num();
			const int32 NumRawSkeletonBones = AnimSequence->GetSkeleton()->GetReferenceSkeleton().GetRawBoneNum();

			for (int LODBoneIndex = 0; LODBoneIndex < NumLODBones; LODBoneIndex++)
			{
				const int32 SkeletonBoneIndex = LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex];

				// Virtual bones are part of the retarget transform pose, so if the pose has not been updated (recently) there might be a mismatch
				if (SkeletonBoneIndex < NumRawSkeletonBones || AuthoredOnRefSkeleton.IsValidIndex(SkeletonBoneIndex))
				{
					OutAnimationPoseData.LocalTransformsView[LODBoneIndex] = AuthoredOnRefSkeleton[SkeletonBoneIndex];
				}
			}
		}
		else
		{
			OutAnimationPoseData.SetRefPose();
		}
	}

#if WITH_EDITOR
	const int32 NumTracks = bUseRawDataForPoseExtraction ? AnimSequence->GetDataModelInterface()->GetNumBoneTracks() : AnimSequence->CompressedData.CompressedTrackToSkeletonMapTable.Num();
#else
	const int32 NumTracks = AnimSequence->CompressedData.CompressedTrackToSkeletonMapTable.Num();
#endif 
	const bool bTreatAnimAsAdditive = (AnimSequence->IsValidAdditive() && !bUseRawDataForPoseExtraction); // Raw data is never additive
	const FRootMotionReset RootMotionReset(AnimSequence->bEnableRootMotion, AnimSequence->RootMotionRootLock, AnimSequence->bForceRootLock, AnimSequence->ExtractRootTrackTransform(0.0f, /*&RequiredBones*/nullptr), bTreatAnimAsAdditive);

#if WITH_EDITOR
	// Evaluate raw (source) curve and bone data
	if (bUseRawDataForPoseExtraction)
	{
		// TODO : Curves support
		//{
		//	FSkeletonRemappingCurve RemappedCurve(OutAnimationPoseData.GetCurve(), RequiredBones, AnimSequence->GetSkeleton());
		//	FAnimationPoseData RemappedPoseData(OutAnimationPoseData.GetPose(), RemappedCurve.GetCurve(), OutAnimationPoseData.GetAttributes());

		//	const UE::Anim::DataModel::FEvaluationContext EvaluationContext(ExtractionContext.CurrentTime, DataModelInterface->GetFrameRate(), GetRetargetTransformsSourceName(), GetRetargetTransforms(), Interpolation);
		//	DataModelInterface->Evaluate(RemappedPoseData, EvaluationContext);
		//}

		//if ((ExtractionContext.bExtractRootMotion && RootMotionReset.bEnableRootMotion) || RootMotionReset.bForceRootLock)
		//{
		//	RootMotionReset.ResetRootBoneForRootMotion(OutPose[FCompactPoseBoneIndex(0)], RequiredBones);
		//}

		return;
	}
	else
#endif // WITH_EDITOR
		// Only try and evaluate compressed bone data if the animation contains any bone tracks
		if (NumTracks != 0)
		{
			// Evaluate compressed bone data
			FAnimSequenceDecompressionContext DecompContext(AnimSequence->GetSamplingFrameRate()
				, AnimSequence->GetSamplingFrameRate().AsFrameTime(AnimSequence->GetPlayLength()).RoundToFrame().Value
				, AnimSequence->Interpolation
				, AnimSequence->GetRetargetTransformsSourceName()
				, *AnimSequence->CompressedData.CompressedDataStructure
				, AnimSequence->GetSkeleton()->GetRefLocalPoses()
				, AnimSequence->CompressedData.CompressedTrackToSkeletonMapTable
				, AnimSequence->GetSkeleton()
				, AnimSequence->IsValidAdditive()
				, AnimSequence->GetAdditiveAnimType());

			DecompressPose(OutAnimationPoseData, AnimSequence->CompressedData, ExtractionContext, DecompContext, AnimSequence->GetRetargetTransforms(), RootMotionReset);
		}

//	// TODO : Curves support
//	// (Always) evaluate compressed curve data
//	{
//		// Scoped so that the RemappedCurve destructs and isn't kept alive longer than needed.
//		FSkeletonRemappingCurve RemappedCurve(OutAnimationPoseData.GetCurve(), RequiredBones, GetSkeleton());
//#if WITH_EDITOR
//		// When evaluating from raw animation data, UE::Anim::BuildPoseFromModel will populate the curve data
//		if (!bUseRawDataForPoseExtraction)
//#endif // WITH_EDITOR
//		{
//			AnimSequence->EvaluateCurveData(RemappedCurve.GetCurve(), ExtractionContext.CurrentTime, bUseRawDataForPoseExtraction);
//		}
//	}

	// TODO : attributes support
	// Evaluate animation attributes (no compressed format yet)
	//EvaluateAttributes(OutAnimationPoseData, ExtractionContext, false);
}

void FDecompressionTools::GetBonePose_Additive(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData)
{
}

void FDecompressionTools::GetBonePose_AdditiveMeshRotationOnly(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData)
{
}


// --- ---

void FDecompressionTools::DecompressPose(FLODPose& OutAnimationPoseData,
										const FCompressedAnimSequence& CompressedData,
										const FAnimExtractContext& ExtractionContext,
										FAnimSequenceDecompressionContext& DecompressionContext,
										FName RetargetSource,
										const FRootMotionReset& RootMotionReset)
{
	const TArray<FTransform>& RetargetTransforms = DecompressionContext.GetSourceSkeleton()->GetRefLocalPoses(RetargetSource);
	DecompressPose(OutAnimationPoseData, CompressedData, ExtractionContext, DecompressionContext, RetargetTransforms, RootMotionReset);
}

void FDecompressionTools::DecompressPose(FLODPose& OutAnimationPoseData,
										const FCompressedAnimSequence& CompressedData,
										const FAnimExtractContext& ExtractionContext,
										FAnimSequenceDecompressionContext& DecompressionContext,
										const TArray<FTransform>& RetargetTransforms,
										const FRootMotionReset& RootMotionReset)
{
	const FReferencePose& ReferencePose = OutAnimationPoseData.GetRefPose();
	const TArrayView<const FBoneIndexType> LODBoneIndexToSkeletonBoneIndexMap = OutAnimationPoseData.GetLODBoneIndexToMeshBoneIndexMap();
	const TArrayView<const FBoneIndexType> SkeletonToLODBoneIndexes = ReferencePose.GetSkeletonBoneIndexToLODBoneIndexMap(0); // Full list of Skeleton to LOD conversion
	const int32 NumLODBoneIndexes = LODBoneIndexToSkeletonBoneIndexMap.Num();

	const int32 NumTracks = CompressedData.CompressedTrackToSkeletonMapTable.Num();

	const USkeleton* TargetSkeleton = OutAnimationPoseData.GetSkeletonAsset();
	const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(DecompressionContext.GetSourceSkeleton(), TargetSkeleton);

	BoneTrackArray& RotationScalePairs = FGetBonePoseScratchArea::Get().RotationScalePairs;
	BoneTrackArray& TranslationPairs = FGetBonePoseScratchArea::Get().TranslationPairs;
	BoneTrackArray& AnimScaleRetargetingPairs = FGetBonePoseScratchArea::Get().AnimScaleRetargetingPairs;
	BoneTrackArray& AnimRelativeRetargetingPairs = FGetBonePoseScratchArea::Get().AnimRelativeRetargetingPairs;
	BoneTrackArray& OrientAndScaleRetargetingPairs = FGetBonePoseScratchArea::Get().OrientAndScaleRetargetingPairs;

	// build a list of desired bones
	RotationScalePairs.Reset();
	TranslationPairs.Reset();
	AnimScaleRetargetingPairs.Reset();
	AnimRelativeRetargetingPairs.Reset();
	OrientAndScaleRetargetingPairs.Reset();

	// Optimization: assuming first index is root bone. That should always be the case in Skeletons.
	checkSlow((LODBoneIndexToSkeletonBoneIndexMap[0] == FMeshPoseBoneIndex(0).GetInt()));
	// this is not guaranteed for AnimSequences though... If Root is not animated, Track will not exist.
	const bool bFirstTrackIsRootBone = (CompressedData.GetSkeletonIndexFromTrackIndex(0) == 0);

	{
		SCOPE_CYCLE_COUNTER(STAT_BuildAnimTrackPairs);

		// Handle root bone separately if it is track 0. so we start w/ Index 1.
		for (int32 TrackIndex = (bFirstTrackIsRootBone ? 1 : 0); TrackIndex < NumTracks; TrackIndex++)
		{
			const int32 SourceSkeletonBoneIndex = CompressedData.GetSkeletonIndexFromTrackIndex(TrackIndex);
			const int32 TargetSkeletonBoneIndex = SkeletonRemapping.IsValid() ? SkeletonRemapping.GetTargetSkeletonBoneIndex(SourceSkeletonBoneIndex) : SourceSkeletonBoneIndex;

			if (TargetSkeletonBoneIndex != INDEX_NONE)
			{
				const int32 LODBoneIndex = TargetSkeletonBoneIndex < SkeletonToLODBoneIndexes.Num() ? SkeletonToLODBoneIndexes[TargetSkeletonBoneIndex] : INDEX_NONE;

				if (LODBoneIndex != INDEX_NONE && LODBoneIndex < NumLODBoneIndexes) // skip bones not in current LOD
				{
					RotationScalePairs.Add(BoneTrackPair(LODBoneIndex, TrackIndex));

					// Skip extracting translation component for EBoneTranslationRetargetingMode::Skeleton.
					switch (TargetSkeleton->GetBoneTranslationRetargetingMode(TargetSkeletonBoneIndex, OutAnimationPoseData.GetDisableRetargeting()))
					{
					case EBoneTranslationRetargetingMode::Animation:
						TranslationPairs.Add(BoneTrackPair(LODBoneIndex, TrackIndex));
						break;
					case EBoneTranslationRetargetingMode::AnimationScaled:
						TranslationPairs.Add(BoneTrackPair(LODBoneIndex, TrackIndex));
						AnimScaleRetargetingPairs.Add(BoneTrackPair(LODBoneIndex, SourceSkeletonBoneIndex));
						break;
					case EBoneTranslationRetargetingMode::AnimationRelative:
						TranslationPairs.Add(BoneTrackPair(LODBoneIndex, TrackIndex));

						// With baked additives, we can skip 'AnimationRelative' tracks, as the relative transform gets canceled out.
						// (A1 + Rel) - (A2 + Rel) = A1 - A2.
						if (!DecompressionContext.IsAdditiveAnimation())
						{
							AnimRelativeRetargetingPairs.Add(BoneTrackPair(LODBoneIndex, SourceSkeletonBoneIndex));
						}
						break;
					case EBoneTranslationRetargetingMode::OrientAndScale:
						TranslationPairs.Add(BoneTrackPair(LODBoneIndex, TrackIndex));

						// Additives remain additives, they're not retargeted.
						if (!DecompressionContext.IsAdditiveAnimation())
						{
							OrientAndScaleRetargetingPairs.Add(BoneTrackPair(LODBoneIndex, SourceSkeletonBoneIndex));
						}
						break;
					}
				}
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ExtractPoseFromAnimData);
		CSV_SCOPED_TIMING_STAT(Animation, ExtractPoseFromAnimData);
		CSV_CUSTOM_STAT(Animation, NumberOfExtractedAnimations, 1, ECsvCustomStatOp::Accumulate);

		DecompressionContext.Seek(ExtractionContext.CurrentTime);

		// Handle Root Bone separately
		if (bFirstTrackIsRootBone)
		{
			const int32 TrackIndex = 0;
			const int32 LODRootBone = 0;
			FTransform RootAtom = OutAnimationPoseData.LocalTransformsView[0];

			CompressedData.BoneCompressionCodec->DecompressBone(DecompressionContext, TrackIndex, RootAtom);

			// Retarget the root onto the target skeleton (correcting for differences in rest poses)
			if (SkeletonRemapping.RequiresReferencePoseRetarget())
			{
				// Root bone does not require fix-up for additive animations as there is no parent delta rotation to account for
				if (!DecompressionContext.IsAdditiveAnimation())
				{
					const int32 TargetSkeletonBoneIndex = 0;

					RootAtom.SetRotation(SkeletonRemapping.RetargetBoneRotationToTargetSkeleton(TargetSkeletonBoneIndex, RootAtom.GetRotation()));
					if (TargetSkeleton->GetBoneTranslationRetargetingMode(TargetSkeletonBoneIndex, OutAnimationPoseData.GetDisableRetargeting()) != EBoneTranslationRetargetingMode::Skeleton)
					{
						RootAtom.SetTranslation(SkeletonRemapping.RetargetBoneTranslationToTargetSkeleton(TargetSkeletonBoneIndex, RootAtom.GetTranslation()));
					}
				}
			}

			// @laurent - we should look into splitting rotation and translation tracks, so we don't have to process translation twice.
			RetargetBoneTransform(OutAnimationPoseData.GetRefPose()
				, DecompressionContext.GetSourceSkeleton()
				, OutAnimationPoseData.GetSkeletonAsset()
				, DecompressionContext.AnimName
				, RetargetTransforms
				, RootAtom
				, 0
				, LODRootBone
				, DecompressionContext.IsAdditiveAnimation()
				, OutAnimationPoseData.GetDisableRetargeting());

			OutAnimationPoseData.LocalTransformsView[0] = RootAtom;
		}

		if (RotationScalePairs.Num() > 0)
		{
#if DEFAULT_SOA
			// get the remaining bone atoms
			CompressedData.BoneCompressionCodec->DecompressPose(DecompressionContext, UE::Anim::FAnimPoseDecompressionData(RotationScalePairs, TranslationPairs, RotationScalePairs, OutAnimationPoseData.LocalTransformsView.Rotations, OutAnimationPoseData.LocalTransformsView.Translations, OutAnimationPoseData.LocalTransformsView.Scales3D));
#else
			// get the remaining bone atoms
			TArrayView<FTransform> OutPoseBones = OutAnimationPoseData.LocalTransforms.Transforms;
			CompressedData.BoneCompressionCodec->DecompressPose(DecompressionContext, RotationScalePairs, TranslationPairs, RotationScalePairs, OutPoseBones);
#endif
		}
	}

	// Retarget the pose onto the target skeleton (correcting for differences in rest poses)
	if (SkeletonRemapping.RequiresReferencePoseRetarget())
	{
		const int32 LODNumBones = OutAnimationPoseData.GetNumBones();

		if (DecompressionContext.IsAdditiveAnimation())
		{
			for (int32 LODBoneIndex = (bFirstTrackIsRootBone ? 1 : 0); LODBoneIndex < LODNumBones; ++LODBoneIndex)
			{
				const int32 TargetSkeletonBoneIndex = ReferencePose.GetSkeletonBoneIndexFromLODBoneIndex(LODBoneIndex);

				// Mesh space additives do not require fix-up
				if (DecompressionContext.GetAdditiveType() == AAT_LocalSpaceBase)
				{
					OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetRotation(SkeletonRemapping.RetargetAdditiveRotationToTargetSkeleton(TargetSkeletonBoneIndex, OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetRotation()));
				}

				if (TargetSkeleton->GetBoneTranslationRetargetingMode(TargetSkeletonBoneIndex, OutAnimationPoseData.GetDisableRetargeting()) != EBoneTranslationRetargetingMode::Skeleton)
				{
					OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetTranslation(SkeletonRemapping.RetargetAdditiveTranslationToTargetSkeleton(TargetSkeletonBoneIndex, OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetTranslation()));
				}
			}
		}
		else
		{
			for (int32 LODBoneIndex = (bFirstTrackIsRootBone ? 1 : 0); LODBoneIndex < LODNumBones; ++LODBoneIndex)
			{
				const int32 TargetSkeletonBoneIndex = LODBoneIndexToSkeletonBoneIndexMap[LODBoneIndex]; // ReferencePose.GetSkeletonBoneIndexFromLODBoneIndex(LODBoneIndex);
				OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetRotation(SkeletonRemapping.RetargetBoneRotationToTargetSkeleton(TargetSkeletonBoneIndex, OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetRotation()));
				if (TargetSkeleton->GetBoneTranslationRetargetingMode(TargetSkeletonBoneIndex, OutAnimationPoseData.GetDisableRetargeting()) != EBoneTranslationRetargetingMode::Skeleton)
				{
					OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetTranslation(SkeletonRemapping.RetargetBoneTranslationToTargetSkeleton(TargetSkeletonBoneIndex, OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetTranslation()));
				}
			}
		}
	}

	// Once pose has been extracted, snap root bone back to first frame if we are extracting root motion.
	if ((ExtractionContext.bExtractRootMotion && RootMotionReset.bEnableRootMotion) || RootMotionReset.bForceRootLock)
	{
		FTransform RootTransform = OutAnimationPoseData.LocalTransformsView[0];
		RootMotionReset.ResetRootBoneForRootMotion(RootTransform, ReferencePose.GetRefPoseTransform(0));
		OutAnimationPoseData.LocalTransformsView[0] = RootTransform;
	}

	// Anim Scale Retargeting
	int32 const NumBonesToScaleRetarget = AnimScaleRetargetingPairs.Num();
	if (NumBonesToScaleRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = RetargetTransforms;

		for (const BoneTrackPair& BonePair : AnimScaleRetargetingPairs)
		{
			const int32 LODBoneIndex(BonePair.AtomIndex);
			const int32 SourceSkeletonBoneIndex = BonePair.TrackIndex;

			// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
			float const SourceTranslationLength = AuthoredOnRefSkeleton[SourceSkeletonBoneIndex].GetTranslation().Size();
			if (SourceTranslationLength > UE_KINDA_SMALL_NUMBER)
			{
				float const TargetTranslationLength = ReferencePose.GetRefPoseTranslation(LODBoneIndex).Size();
				OutAnimationPoseData.LocalTransformsView[LODBoneIndex].ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
			}
		}
	}

	// Anim Relative Retargeting
	int32 const NumBonesToRelativeRetarget = AnimRelativeRetargetingPairs.Num();
	if (NumBonesToRelativeRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = RetargetTransforms;

		for (const BoneTrackPair& BonePair : AnimRelativeRetargetingPairs)
		{
			const int32 LODBoneIndex(BonePair.AtomIndex);
			const int32 SourceSkeletonBoneIndex = BonePair.TrackIndex;

			const FTransform& RefPoseTransform = ReferencePose.GetRefPoseTransform(LODBoneIndex);

			// Remap the base pose onto the target skeleton so that we are working entirely in target space
			FTransform BaseTransform = AuthoredOnRefSkeleton[SourceSkeletonBoneIndex];
			if (SkeletonRemapping.RequiresReferencePoseRetarget())
			{
				const int32 TargetSkeletonBoneIndex = SkeletonRemapping.GetTargetSkeletonBoneIndex(SourceSkeletonBoneIndex);
				BaseTransform = SkeletonRemapping.RetargetBoneTransformToTargetSkeleton(TargetSkeletonBoneIndex, BaseTransform);
			}

			// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
			OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetRotation(OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetRotation() * BaseTransform.GetRotation().Inverse() * RefPoseTransform.GetRotation());
			OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetTranslation(OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetTranslation() + (RefPoseTransform.GetTranslation() - BaseTransform.GetTranslation()));
			OutAnimationPoseData.LocalTransformsView[LODBoneIndex].SetScale3D(OutAnimationPoseData.LocalTransformsView[LODBoneIndex].GetScale3D() * (RefPoseTransform.GetScale3D() * BaseTransform.GetSafeScaleReciprocal(BaseTransform.GetScale3D())));
			OutAnimationPoseData.LocalTransformsView[LODBoneIndex].NormalizeRotation();
		}
	}

	// TODO : Have to recreate GetRetargetSourceCachedData
	// Translation 'Orient and Scale' Translation Retargeting
	//const int32 NumBonesToOrientAndScaleRetarget = OrientAndScaleRetargetingPairs.Num();
	//if (NumBonesToOrientAndScaleRetarget > 0)
	//{
	//	const FRetargetSourceCachedData& RetargetSourceCachedData = RequiredBones.GetRetargetSourceCachedData(DecompressionContext.AnimName, SkeletonRemapping, RetargetTransforms);
	//	const TArray<FOrientAndScaleRetargetingCachedData>& OrientAndScaleDataArray = RetargetSourceCachedData.OrientAndScaleData;
	//	const TArray<int32>& CompactPoseIndexToOrientAndScaleIndex = RetargetSourceCachedData.CompactPoseIndexToOrientAndScaleIndex;

	//	// If we have any cached retargeting data.
	//	if ((OrientAndScaleDataArray.Num() > 0) && (CompactPoseIndexToOrientAndScaleIndex.Num() == RequiredBones.GetCompactPoseNumBones()))
	//	{
	//		for (int32 Index = 0; Index < NumBonesToOrientAndScaleRetarget; Index++)
	//		{
	//			const BoneTrackPair& BonePair = OrientAndScaleRetargetingPairs[Index];
	//			const FCompactPoseBoneIndex CompactPoseBoneIndex(BonePair.AtomIndex);
	//			const int32 OrientAndScaleIndex = CompactPoseIndexToOrientAndScaleIndex[CompactPoseBoneIndex.GetInt()];
	//			if (OrientAndScaleIndex != INDEX_NONE)
	//			{
	//				const FOrientAndScaleRetargetingCachedData& OrientAndScaleData = OrientAndScaleDataArray[OrientAndScaleIndex];
	//				FTransform& BoneTransform = OutAnimationPoseData.LocalTransforms[CompactPoseBoneIndex];
	//				const FVector AnimatedTranslation = BoneTransform.GetTranslation();

	//				// If Translation is not animated, we can just copy the TargetTranslation. No retargeting needs to be done.
	//				const FVector NewTranslation = (AnimatedTranslation - OrientAndScaleData.SourceTranslation).IsNearlyZero(BONE_TRANS_RT_ORIENT_AND_SCALE_PRECISION) ?
	//					OrientAndScaleData.TargetTranslation :
	//					OrientAndScaleData.TranslationDeltaOrient.RotateVector(AnimatedTranslation) * OrientAndScaleData.TranslationScale;

	//				BoneTransform.SetTranslation(NewTranslation);
	//			}
	//		}
	//	}
	//}
}

void FDecompressionTools::RetargetBoneTransform(const FReferencePose& ReferencePose
	, const USkeleton* SourceSkeleton
	, const USkeleton* TargetSkeleton
	, const FName& RetargetSource
	, FTransform& BoneTransform
	, const int32 SkeletonBoneIndex
	, const int32 LODBoneIndex
	, const bool bIsBakedAdditive
	, const bool bDisableRetargeting)
{
	if (SourceSkeleton)
	{
		const TArray<FTransform>& RetargetTransforms = SourceSkeleton->GetRefLocalPoses(RetargetSource);
		RetargetBoneTransform(ReferencePose, SourceSkeleton, TargetSkeleton, RetargetSource, RetargetTransforms, BoneTransform, SkeletonBoneIndex, LODBoneIndex, bIsBakedAdditive, bDisableRetargeting);
	}
}

void FDecompressionTools::RetargetBoneTransform(const FReferencePose& ReferencePose
	, const USkeleton* SourceSkeleton
	, const USkeleton* TargetSkeleton
	, const FName& SourceName
	, const TArray<FTransform>& RetargetTransforms
	, FTransform& BoneTransform
	, const int32 SkeletonBoneIndex
	, const int32 LODBoneIndex
	, const bool bIsBakedAdditive
	, const bool bDisableRetargeting)
{
	check(!RetargetTransforms.IsEmpty());
	if (SourceSkeleton)
	{
		//const USkeleton* TargetSkeleton = RequiredBones.GetSkeletonAsset();
		const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(SourceSkeleton, TargetSkeleton);

		const int32 TargetSkeletonBoneIndex = ReferencePose.GetSkeletonBoneIndexFromLODBoneIndex(LODBoneIndex);
		const int32 SourceSkeletonBoneIndex = SkeletonRemapping.IsValid() ? SkeletonRemapping.GetSourceSkeletonBoneIndex(TargetSkeletonBoneIndex) : SkeletonBoneIndex;

		switch (TargetSkeleton->GetBoneTranslationRetargetingMode(TargetSkeletonBoneIndex, bDisableRetargeting))
		{
		case EBoneTranslationRetargetingMode::AnimationScaled:
		{
			// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
			const TArray<FTransform>& SkeletonRefPoseArray = RetargetTransforms;
			const float SourceTranslationLength = SkeletonRefPoseArray[SourceSkeletonBoneIndex].GetTranslation().Size();
			if (SourceTranslationLength > UE_KINDA_SMALL_NUMBER)
			{
				const float TargetTranslationLength = ReferencePose.GetRefPoseTranslation(LODBoneIndex).Size();
				BoneTransform.ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
			}
			break;
		}

		case EBoneTranslationRetargetingMode::Skeleton:
		{
			BoneTransform.SetTranslation(bIsBakedAdditive ? FVector::ZeroVector : ReferencePose.GetRefPoseTranslation(LODBoneIndex));
			break;
		}

		case EBoneTranslationRetargetingMode::AnimationRelative:
		{
			// With baked additive animations, Animation Relative delta gets canceled out, so we can skip it.
			// (A1 + Rel) - (A2 + Rel) = A1 - A2.
			if (!bIsBakedAdditive)
			{
				const TArray<FTransform>& AuthoredOnRefSkeleton = RetargetTransforms;
				const FTransform& RefPoseTransform = ReferencePose.GetRefPoseTransform(LODBoneIndex);

				// Remap the base pose onto the target skeleton so that we are working entirely in target space
				FTransform BaseTransform = AuthoredOnRefSkeleton[SourceSkeletonBoneIndex];
				if (SkeletonRemapping.RequiresReferencePoseRetarget())
				{
					BaseTransform = SkeletonRemapping.RetargetBoneTransformToTargetSkeleton(TargetSkeletonBoneIndex, BaseTransform);
				}

				// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
				BoneTransform.SetRotation(BoneTransform.GetRotation() * BaseTransform.GetRotation().Inverse() * RefPoseTransform.GetRotation());
				BoneTransform.SetTranslation(BoneTransform.GetTranslation() + (RefPoseTransform.GetTranslation() - BaseTransform.GetTranslation()));
				BoneTransform.SetScale3D(BoneTransform.GetScale3D() * (RefPoseTransform.GetScale3D() * BaseTransform.GetSafeScaleReciprocal(BaseTransform.GetScale3D())));
				BoneTransform.NormalizeRotation();
			}
			break;
		}

		case EBoneTranslationRetargetingMode::OrientAndScale:
		{
			if (!bIsBakedAdditive)
			{
				//const FRetargetSourceCachedData& RetargetSourceCachedData = RequiredBones.GetRetargetSourceCachedData(SourceName, SkeletonRemapping, RetargetTransforms);
				//const TArray<FOrientAndScaleRetargetingCachedData>& OrientAndScaleDataArray = RetargetSourceCachedData.OrientAndScaleData;
				//const TArray<int32>& CompactPoseIndexToOrientAndScaleIndex = RetargetSourceCachedData.CompactPoseIndexToOrientAndScaleIndex;

				//// If we have any cached retargeting data.
				//if ((OrientAndScaleDataArray.Num() > 0) && (CompactPoseIndexToOrientAndScaleIndex.Num() == RequiredBones.GetCompactPoseNumBones()))
				//{
				//	const int32 OrientAndScaleIndex = CompactPoseIndexToOrientAndScaleIndex[BoneIndex.GetInt()];
				//	if (OrientAndScaleIndex != INDEX_NONE)
				//	{
				//		const FOrientAndScaleRetargetingCachedData& OrientAndScaleData = OrientAndScaleDataArray[OrientAndScaleIndex];
				//		const FVector AnimatedTranslation = BoneTransform.GetTranslation();

				//		// If Translation is not animated, we can just copy the TargetTranslation. No retargeting needs to be done.
				//		const FVector NewTranslation = (AnimatedTranslation - OrientAndScaleData.SourceTranslation).IsNearlyZero(BONE_TRANS_RT_ORIENT_AND_SCALE_PRECISION) ?
				//			OrientAndScaleData.TargetTranslation :
				//			OrientAndScaleData.TranslationDeltaOrient.RotateVector(AnimatedTranslation) * OrientAndScaleData.TranslationScale;

				//		BoneTransform.SetTranslation(NewTranslation);
				//	}
				//}
			}
			break;
		}
		}
	}
}

} // end namespace UE::AnimNext
