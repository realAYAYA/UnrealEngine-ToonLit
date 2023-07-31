// Copyright Epic Games, Inc. All Rights Reserved.

#include "Solvers/IKRig_BodyMover.h"
#include "IKRigDataTypes.h"
#include "IKRigSkeleton.h"
#include "Solvers/PointsToRotation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRig_BodyMover)

#define LOCTEXT_NAMESPACE "UIKRig_BodyMover"

void UIKRig_BodyMover::Initialize(const FIKRigSkeleton& IKRigSkeleton)
{
	BodyBoneIndex = IKRigSkeleton.GetBoneIndexFromName(RootBone);
}

void UIKRig_BodyMover::Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals)
{
	// no body bone specified
	if (BodyBoneIndex == INDEX_NONE)
	{
		return;
	}

	// no effectors added
	if (Effectors.IsEmpty())
	{
		return;
	}
	
	// ensure body bone exists
	check(IKRigSkeleton.RefPoseGlobal.IsValidIndex(BodyBoneIndex));
	
	// calculate a "best fit" transform from deformed goal locations
	TArray<FVector> InitialPoints;
	TArray<FVector> CurrentPoints;
	for (UIKRig_BodyMoverEffector* Effector : Effectors)
	{
		const FIKRigGoal* Goal = Goals.FindGoalByName(Effector->GoalName);
		if (!Goal)
		{
			return;
		}

		const int32 BoneIndex = IKRigSkeleton.GetBoneIndexFromName(Effector->BoneName);
		const FTransform InitialEffector = IKRigSkeleton.CurrentPoseGlobal[BoneIndex];

		InitialPoints.Add(InitialEffector.GetTranslation());
		CurrentPoints.Add(Goal->FinalBlendedPosition);
	}
	
	FVector InitialCentroid;
	FVector CurrentCentroid;
	const FQuat RotationOffset = GetRotationFromDeformedPoints(
		InitialPoints,
		CurrentPoints,
		InitialCentroid,
		CurrentCentroid);

	// alpha blend the position offset and add it to the current bone location
	const FVector Offset = (CurrentCentroid - InitialCentroid);
	const FVector Weight(
		Offset.X > 0.f ? PositionPositiveX : PositionNegativeX,
		Offset.Y > 0.f ? PositionPositiveY : PositionNegativeY,
		Offset.Z > 0.f ? PositionPositiveZ : PositionNegativeZ);

	// the bone transform to modify
	FTransform& CurrentBodyTransform = IKRigSkeleton.CurrentPoseGlobal[BodyBoneIndex];
	CurrentBodyTransform.AddToTranslation(Offset * (Weight*PositionAlpha));

	// do per-axis alpha blend
	FVector Euler = RotationOffset.Euler() * FVector(RotateXAlpha, RotateYAlpha, RotateZAlpha);
	FQuat FinalRotationOffset = FQuat::MakeFromEuler(Euler);
	// alpha blend the entire rotation offset
	FinalRotationOffset = FQuat::FastLerp(FQuat::Identity, FinalRotationOffset, RotationAlpha).GetNormalized();
	// add rotation offset to original rotation
	CurrentBodyTransform.SetRotation(FinalRotationOffset * CurrentBodyTransform.GetRotation());

	// do FK update of children
	IKRigSkeleton.PropagateGlobalPoseBelowBone(BodyBoneIndex);
}

void UIKRig_BodyMover::UpdateSolverSettings(UIKRigSolver* InSettings)
{
	if(UIKRig_BodyMover* Settings = Cast<UIKRig_BodyMover>(InSettings))
	{
		// copy solver settings
		PositionAlpha = Settings->PositionAlpha;
		PositionPositiveX = Settings->PositionPositiveX;
		PositionPositiveY = Settings->PositionPositiveY;
		PositionPositiveZ = Settings->PositionPositiveZ;
		PositionNegativeX = Settings->PositionNegativeX;
		PositionNegativeY = Settings->PositionNegativeY;
		PositionNegativeZ = Settings->PositionNegativeZ;
		RotationAlpha = Settings->RotationAlpha;
		RotateXAlpha = Settings->RotateXAlpha;
		RotateYAlpha = Settings->RotateYAlpha;
		RotateZAlpha = Settings->RotateZAlpha;

		// copy effector settings
		for (const UIKRig_BodyMoverEffector* InEffector : Settings->Effectors)
		{
			for (UIKRig_BodyMoverEffector* Effector : Effectors)
			{
				if (Effector->GoalName == InEffector->GoalName)
				{
					Effector->InfluenceMultiplier = InEffector->InfluenceMultiplier;
					break;
				}
			}
		}
	}
}

void UIKRig_BodyMover::RemoveGoal(const FName& GoalName)
{
	// ensure goal even exists in this solver
	const int32 GoalIndex = GetIndexOfGoal(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return;
	}

	// remove it
	Effectors.RemoveAt(GoalIndex);
}

#if WITH_EDITOR

FText UIKRig_BodyMover::GetNiceName() const
{
	return FText(LOCTEXT("SolverName", "Body Mover"));
}

bool UIKRig_BodyMover::GetWarningMessage(FText& OutWarningMessage) const
{
	if (RootBone == NAME_None)
	{
		OutWarningMessage = LOCTEXT("MissingRoot", "Missing root bone.");
		return true;
	}

	if (Effectors.IsEmpty())
	{
		OutWarningMessage = LOCTEXT("MissingGoal", "Missing goals.");
		return true;
	}
	
	return false;
}

void UIKRig_BodyMover::AddGoal(const UIKRigEffectorGoal& NewGoal)
{
	UIKRig_BodyMoverEffector* NewEffector = NewObject<UIKRig_BodyMoverEffector>(this, UIKRig_BodyMoverEffector::StaticClass());
	NewEffector->GoalName = NewGoal.GoalName;
	NewEffector->BoneName = NewGoal.BoneName;
	Effectors.Add(NewEffector);
}

void UIKRig_BodyMover::RenameGoal(const FName& OldName, const FName& NewName)
{
	// ensure goal even exists in this solver
	const int32 GoalIndex = GetIndexOfGoal(OldName);
	if (GoalIndex == INDEX_NONE)
	{
		return;
	}

	// rename
	Effectors[GoalIndex]->GoalName = NewName;
}

void UIKRig_BodyMover::SetGoalBone(const FName& GoalName, const FName& NewBoneName)
{
	// ensure goal even exists in this solver
	const int32 GoalIndex = GetIndexOfGoal(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return;
	}

	// rename
	Effectors[GoalIndex]->Modify();
	Effectors[GoalIndex]->BoneName = NewBoneName;
}

bool UIKRig_BodyMover::IsGoalConnected(const FName& GoalName) const
{
	return GetIndexOfGoal(GoalName) != INDEX_NONE;
}

UObject* UIKRig_BodyMover::GetGoalSettings(const FName& GoalName) const
{
	const int32 GoalIndex = GetIndexOfGoal(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return nullptr;
	}

	return Effectors[GoalIndex];
}

bool UIKRig_BodyMover::IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const
{
	return IKRigSkeleton.IsBoneInDirectLineage(BoneName, RootBone);
}

void UIKRig_BodyMover::SetRootBone(const FName& RootBoneName)
{
	RootBone = RootBoneName;
}

#endif

int32 UIKRig_BodyMover::GetIndexOfGoal(const FName& OldName) const
{
	for (int32 i=0; i<Effectors.Num(); ++i)
	{
		if (Effectors[i]->GoalName == OldName)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE

