// Copyright Epic Games, Inc. All Rights Reserved.


#include "Solvers/IKRig_LimbSolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRig_LimbSolver)

#define LOCTEXT_NAMESPACE "IKRig_LimbSolver"

UIKRig_LimbSolver::UIKRig_LimbSolver()
{
	Effector = CreateDefaultSubobject<UIKRig_LimbEffector>(TEXT("Effector"));
}

void UIKRig_LimbSolver::Initialize(const FIKRigSkeleton& InSkeleton)
{
	Solver.Reset();
	ChildrenToUpdate.Empty();
	
	if (Effector->GoalName == NAME_None || Effector->BoneName == NAME_None || RootName == NAME_None)
	{
		return;
	}

	int32 BoneIndex = InSkeleton.GetBoneIndexFromName(Effector->BoneName);
	const int32 RootIndex = InSkeleton.GetBoneIndexFromName(RootName);
	if (BoneIndex == INDEX_NONE || RootIndex == INDEX_NONE)
	{
		return;
	}

	// populate indices
	TArray<int32> BoneIndices( {BoneIndex} );
	BoneIndex = InSkeleton.GetParentIndex(BoneIndex);
	while (BoneIndex != INDEX_NONE && BoneIndex >= RootIndex)
	{
		BoneIndices.Add(BoneIndex);
		BoneIndex = InSkeleton.GetParentIndex(BoneIndex);
	};

	// if chain is not long enough
	if (BoneIndices.Num() < 3)
	{
		return;
	}

	// sort the chain from root to end
	Algo::Reverse(BoneIndices);

	// initialize solver
	for (int32 Index: BoneIndices)
	{
		const FVector Location = InSkeleton.CurrentPoseGlobal[Index].GetLocation();
		Solver.AddLink(Location, Index);
	}

	const bool bInitialized = Solver.Initialize();
	if (bInitialized)
	{
		// store children that needs propagation once solved
		TArray<int32> Children;
		for (int32 Index = 0; Index < BoneIndices.Num()-1; ++Index)
		{
			// store children if not already handled by the solver (part if the links)
			InSkeleton.GetChildIndices(BoneIndices[Index], Children);
			const int32 NextIndex = BoneIndices[Index+1];
			for (const int32 ChildIndex: Children)
			{
				if (ChildIndex != NextIndex)
				{
					ChildrenToUpdate.Add(ChildIndex);
					GatherChildren(ChildIndex, InSkeleton, ChildrenToUpdate);
				}
			}
		}
		// store end bone children
		GatherChildren(BoneIndices.Last(), InSkeleton, ChildrenToUpdate);
	}
}

void UIKRig_LimbSolver::Solve(FIKRigSkeleton& InOutRigSkeleton, const FIKRigGoalContainer& InGoals)
{
	if (Solver.NumLinks() < 3)
	{
		return;
	}
	
	const FIKRigGoal* IKGoal = InGoals.FindGoalByName(Effector->GoalName);
	if (!IKGoal)
	{
		return;
	}

	// update settings
	FLimbSolverSettings Settings;
	Settings.ReachPrecision = ReachPrecision;
	Settings.MaxIterations = MaxIterations;
	Settings.bEnableLimit = bEnableLimit;
	Settings.MinRotationAngle = MinRotationAngle;
	Settings.EndBoneForwardAxis = EndBoneForwardAxis; 
	Settings.HingeRotationAxis = HingeRotationAxis;
	Settings.bEnableTwistCorrection = bEnableTwistCorrection;
	Settings.ReachStepAlpha = ReachStepAlpha;
	Settings.PullDistribution = PullDistribution;
	Settings.bAveragePull = bAveragePull;

	// update settings
	const FVector& GoalLocation = IKGoal->FinalBlendedPosition;
	const FQuat& GoalRotation = IKGoal->FinalBlendedRotation;
	const bool bModifiedLimb = Solver.Solve(
		InOutRigSkeleton.CurrentPoseGlobal,
		GoalLocation,
		GoalRotation,
		Settings);

	// propagate if needed
	if (bModifiedLimb)
	{
		// update chain bones local transform
		for (int32 Index = 0; Index < Solver.NumLinks(); Index++)
		{
			InOutRigSkeleton.UpdateLocalTransformFromGlobal(Solver.GetBoneIndex(Index));
		}

		// propagate to children
		for (const int32 ChildIndex: ChildrenToUpdate)
		{
			InOutRigSkeleton.UpdateGlobalTransformFromLocal(ChildIndex);
		}
	}
}

void UIKRig_LimbSolver::UpdateSolverSettings(UIKRigSolver* InSettings)
{
	if (UIKRig_LimbSolver* Settings = Cast<UIKRig_LimbSolver>(InSettings))
	{
		ReachPrecision = Settings->ReachPrecision;
		HingeRotationAxis = Settings->HingeRotationAxis;
		MaxIterations = Settings->MaxIterations;
		bEnableLimit = Settings->bEnableLimit;
		MinRotationAngle = Settings->MinRotationAngle;
		bAveragePull = Settings->bAveragePull;
		PullDistribution = Settings->PullDistribution;
		ReachStepAlpha = Settings->ReachStepAlpha;
		bEnableTwistCorrection = Settings->bEnableTwistCorrection;
		EndBoneForwardAxis = Settings->EndBoneForwardAxis; 
	}
}

void UIKRig_LimbSolver::RemoveGoal(const FName& GoalName)
{
	if (Effector->GoalName == GoalName)
	{
		Effector->Modify();
		Effector->GoalName = NAME_None;
		Effector->BoneName = NAME_None;
	}
}

#if WITH_EDITOR

FText UIKRig_LimbSolver::GetNiceName() const
{
	return FText(LOCTEXT("SolverName", "Limb IK"));
}

bool UIKRig_LimbSolver::GetWarningMessage(FText& OutWarningMessage) const
{
	if (Effector->GoalName == NAME_None)
	{
		OutWarningMessage = LOCTEXT("MissingGoal", "Missing goal.");
		return true;
	}

	if (RootName == NAME_None)
	{
		OutWarningMessage = LOCTEXT("MissingRoot", "Missing root.");
		return true;
	}

	if (Solver.NumLinks() < 3)
	{
		OutWarningMessage = LOCTEXT("Requires3BonesChain", "Requires at least 3 bones between root and goal.");
		return true;
	}
	
	return false;
}

bool UIKRig_LimbSolver::IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const
{
	return IKRigSkeleton.IsBoneInDirectLineage(BoneName, RootName);
}

void UIKRig_LimbSolver::AddGoal(const UIKRigEffectorGoal& NewGoal)
{
	Effector->Modify();
	Effector->GoalName = NewGoal.GoalName;
	Effector->BoneName = NewGoal.BoneName;
}

void UIKRig_LimbSolver::RenameGoal(const FName& OldName, const FName& NewName)
{
	if (Effector->GoalName == OldName)
	{
		Effector->Modify();
		Effector->GoalName = NewName;
	}
}

void UIKRig_LimbSolver::SetGoalBone(const FName& GoalName, const FName& NewBoneName)
{
	if (Effector->GoalName == GoalName)
	{
		Effector->Modify();
		Effector->BoneName = NewBoneName;
	}
}

bool UIKRig_LimbSolver::IsGoalConnected(const FName& GoalName) const
{
	return (Effector->GoalName == GoalName);
}

UObject* UIKRig_LimbSolver::GetGoalSettings(const FName& GoalName) const
{
	return (Effector->GoalName == GoalName) ? Effector : nullptr;
}

void UIKRig_LimbSolver::SetRootBone(const FName& RootBoneName)
{
	RootName = RootBoneName;
}

#endif

void UIKRig_LimbSolver::GatherChildren(const int32 BoneIndex, const FIKRigSkeleton& InSkeleton, TArray<int32>& OutChildren)
{
	TArray<int32> Children;
	InSkeleton.GetChildIndices(BoneIndex, Children);
	for (int32 ChildIndex: Children)
	{
		OutChildren.Add(ChildIndex);
		GatherChildren(ChildIndex, InSkeleton, OutChildren);
	}
}

#undef LOCTEXT_NAMESPACE

