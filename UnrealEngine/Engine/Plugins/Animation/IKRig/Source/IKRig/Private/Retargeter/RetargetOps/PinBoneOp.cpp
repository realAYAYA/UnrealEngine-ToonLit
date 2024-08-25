// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/PinBoneOp.h"

#define LOCTEXT_NAMESPACE "UPinBoneOp"

bool UPinBoneOp::Initialize(
	const UIKRetargetProcessor* Processor,
	const FRetargetSkeleton& SourceSkeleton,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	bool bFullyInitialized = true;
	for (FPinBoneData& BoneToPin : BonesToPin)
	{
		BoneToPin.BoneToPinIndex = TargetSkeleton.FindBoneIndexByName(BoneToPin.BoneToPin);
		if (PinTo == ERetargetSourceOrTarget::Source)
		{
			BoneToPin.BoneToPinToIndex = SourceSkeleton.FindBoneIndexByName(BoneToPin.BoneToPinTo);
		}
		else
		{
			BoneToPin.BoneToPinToIndex = TargetSkeleton.FindBoneIndexByName(BoneToPin.BoneToPinTo);
		}

		const bool bFoundBoneToPin = BoneToPin.BoneToPinIndex != INDEX_NONE;
		const bool bFoundBoneToPinTo = BoneToPin.BoneToPinToIndex != INDEX_NONE;
		if (!bFoundBoneToPin)
		{
			bFullyInitialized = false;
			Log.LogWarning(FText::Format(
				LOCTEXT("MissingSourceBone", "Pin Bone retarget op refers to non-existant bone to pin, {0}."),
				FText::FromName(BoneToPin.BoneToPin)));
		}

		if (!bFoundBoneToPinTo)
		{
			bFullyInitialized = false;
			Log.LogWarning(FText::Format(
				LOCTEXT("MissingTargetBone", "Pin Bone retarget op refers to non-existant bone to pin to, {0}."),
				FText::FromName(BoneToPin.BoneToPinTo)));
		}

		if (bFoundBoneToPin && bFoundBoneToPinTo)
		{
			const FTransform& BoneToPinTransform = TargetSkeleton.RetargetGlobalPose[BoneToPin.BoneToPinIndex];
			const TArray<FTransform>& PinToPose = PinTo == ERetargetSourceOrTarget::Source ? SourceSkeleton.RetargetGlobalPose : TargetSkeleton.RetargetGlobalPose;
			const FTransform& BoneToPinToTransform = PinToPose[BoneToPin.BoneToPinToIndex];
			BoneToPin.OffsetInRefPose =  BoneToPinToTransform.GetRelativeTransform(BoneToPinTransform);
		}
	}

#if WITH_EDITOR
	if (bFullyInitialized)
	{
		Message = FText::Format(LOCTEXT("ReadyToRun", "Pinning {0} bones."), FText::AsNumber(BonesToPin.Num()));
	}
	else
	{
		Message = FText(LOCTEXT("MissingBonesWarning", "Bone(s) not found. See output log."));
	}
#endif

	// always treat this op as "initialized", individual pins will only execute if their prerequisites are met
	return true;
}

void UPinBoneOp::Run(
	const UIKRetargetProcessor* Processor,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	for (const FPinBoneData& BoneToPin : BonesToPin)
	{
		if (BoneToPin.BoneToPinIndex == INDEX_NONE || BoneToPin.BoneToPinToIndex == INDEX_NONE)
		{
			continue; // disabled or not successfully initialized
		}

		// calculate new transform for bone to pin
		const TArray<FTransform>& PinToPose = PinTo == ERetargetSourceOrTarget::Source ? InSourceGlobalPose : OutTargetGlobalPose;
		const FTransform& BoneToPinToTransform = PinToPose[BoneToPin.BoneToPinToIndex];
		const FTransform OffsetToMaintain = bMaintainOffset ? BoneToPin.OffsetInRefPose : FTransform::Identity;
		FTransform Result = LocalOffset * ((OffsetToMaintain * BoneToPinToTransform) * GlobalOffset);

		// apply result
		FTransform& BoneToPinTransform = OutTargetGlobalPose[BoneToPin.BoneToPinIndex];
		switch (PinType)
		{
		case EPinBoneType::FullTransform:
			BoneToPinTransform = Result;
			break;
		case EPinBoneType::TranslateOnly:
			BoneToPinTransform.SetTranslation(Result.GetLocation());
			break;
		case EPinBoneType::RotateOnly:
			BoneToPinTransform.SetRotation(Result.GetRotation());
			break;
		case EPinBoneType::ScaleOnly:
			BoneToPinTransform.SetScale3D(Result.GetScale3D());
			break;
		default:
			checkNoEntry();
		}
	}
}

#undef LOCTEXT_NAMESPACE