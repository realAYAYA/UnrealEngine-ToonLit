// Copyright Epic Games, Inc. All Rights Reserved.

#include "CopyBonesModifier.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

#define LOCTEXT_NAMESPACE "CopyBonesModifier"

UCopyBonesModifier::UCopyBonesModifier()
	:Super()
{
}

void UCopyBonesModifier::OnApply_Implementation(UAnimSequence* Animation)
{
	if (Animation == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("CopyBonesModifier failed. Reason: Invalid Animation"));
		return;
	}

	IAnimationDataController& Controller = Animation->GetController();
	const UAnimDataModel* Model = Animation->GetDataModel();

	if (Model == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("CopyBonesModifier failed. Reason: Invalid Data Model. Animation: %s"), *GetNameSafe(Animation));
		return;
	}

	// Helper structure to store data for the bones we are going to modify
	struct FCopyBoneData
	{
		FName SourceBoneName = NAME_None;
		FName TargetBoneName = NAME_None;
		int32 SourceBoneIdx = INDEX_NONE;
		int32 TargetBoneIdx = INDEX_NONE;
		FCopyBoneData(const FName& InSourceBoneName, const FName& InTargetBoneName, int32 InSourceBoneIdx, int32 InTargetBoneIdx)
			: SourceBoneName(InSourceBoneName), TargetBoneName(InTargetBoneName), SourceBoneIdx(InSourceBoneIdx), TargetBoneIdx(InTargetBoneIdx) {}
	};

	// Validate input
	TArray<FCopyBoneData> CopyBoneDataContainer;
	CopyBoneDataContainer.Reserve(BonePairs.Num());
	for (const FBoneReferencePair& Pair : BonePairs)
	{
		const int32 SourceBoneIdx = Model->GetBoneTrackIndexByName(Pair.SourceBone.BoneName);
		if (SourceBoneIdx == INDEX_NONE)
		{
			continue;
		}

		const int32 TargetBoneIdx = Model->GetBoneTrackIndexByName(Pair.TargetBone.BoneName);
		if (TargetBoneIdx == INDEX_NONE)
		{
			continue;
		}

		CopyBoneDataContainer.Add(FCopyBoneData(Pair.SourceBone.BoneName, Pair.TargetBone.BoneName, SourceBoneIdx, TargetBoneIdx));
	}
	
	// Sort bones to modify so we always modify parents first
	CopyBoneDataContainer.Sort([](const FCopyBoneData& A, const FCopyBoneData& B) { return A.TargetBoneIdx < B.TargetBoneIdx; });

	// Temporally set ForceRootLock to true so we get the correct transforms regardless of the root motion configuration in the animation
	TGuardValue<bool> ForceRootLockGuard(Animation->bForceRootLock, true);

	// Start editing animation data
	const bool bShouldTransact = false;
	Controller.OpenBracket(LOCTEXT("CopyBonesModifier_Bracket", "Updating bones"), bShouldTransact);

	// Get the transform of all the source bones in the desired space
	const int32 NumKeys = Model->GetNumberOfKeys();
	for (int32 AnimKey = 0; AnimKey < NumKeys; AnimKey++)
	{
		for (FCopyBoneData& Data : CopyBoneDataContainer)
		{
			FAnimPose AnimPose;
			UAnimPoseExtensions::GetAnimPoseAtFrame(Animation, AnimKey, FAnimPoseEvaluationOptions(), AnimPose);

			FTransform BonePose = UAnimPoseExtensions::GetBonePose(AnimPose, Data.SourceBoneName, BonePoseSpace);

			// UAnimDataController::UpdateBoneTrackKeys expects local transforms so we need to convert the source transforms to target bone local transforms first. 
			UAnimPoseExtensions::SetBonePose(AnimPose, BonePose, Data.TargetBoneName, BonePoseSpace);
			FTransform BonePoseTargetLocal = UAnimPoseExtensions::GetBonePose(AnimPose, Data.TargetBoneName, EAnimPoseSpaces::Local);

			const FInt32Range KeyRangeToSet(AnimKey, AnimKey + 1);
			Controller.UpdateBoneTrackKeys(Data.TargetBoneName, KeyRangeToSet, { BonePoseTargetLocal.GetLocation() }, { BonePoseTargetLocal.GetRotation() }, { BonePoseTargetLocal.GetScale3D() });
		}
	}

	// Done editing animation data
	Controller.CloseBracket(bShouldTransact);
}

void UCopyBonesModifier::OnRevert_Implementation(UAnimSequence* Animation)
{
	// This AnimModifier doesn't support Revert operation
}

#undef LOCTEXT_NAMESPACE