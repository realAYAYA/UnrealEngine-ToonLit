// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigController.h"

#include "IKRigDefinition.h"
#include "IKRigProcessor.h"
#include "IKRigSolver.h"

#include "Engine/SkeletalMesh.h"
#include "ScopedTransaction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigController)

#define LOCTEXT_NAMESPACE "IKRigController"

UIKRigController* UIKRigController::GetIKRigController(UIKRigDefinition* InIKRigDefinition)
{
	if (!InIKRigDefinition)
	{
		return nullptr;
	}

	if (!InIKRigDefinition->Controller)
	{
		UIKRigController* Controller = NewObject<UIKRigController>();
		Controller->Asset = InIKRigDefinition;
		InIKRigDefinition->Controller = Controller;
	}

	return Cast<UIKRigController>(InIKRigDefinition->Controller);
}

UIKRigDefinition* UIKRigController::GetAsset() const
{
	return Asset;
}

FName UIKRigController::GetAssetIDAsName() const
{
	if (!Asset)
	{
		return NAME_None;
	}

	return FName(FString::FromInt(Asset->GetUniqueID()));
}

void UIKRigController::AddBoneSetting(const FName& BoneName, int32 SolverIndex) const
{
	check(Asset->Solvers.IsValidIndex(SolverIndex))

	if (!CanAddBoneSetting(BoneName, SolverIndex))
	{
		return; // prerequisites not met
	}

	FScopedTransaction Transaction(LOCTEXT("AddBoneSetting_Label", "Add Bone Setting"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();
	Solver->AddBoneSetting(BoneName);
	BroadcastNeedsReinitialized();
}

bool UIKRigController::CanAddBoneSetting(const FName& BoneName, int32 SolverIndex) const
{
	const UIKRigSolver* Solver = GetSolver(SolverIndex);
	if (!Solver)
	{
		return false; // solver doesn't exist
	}

	if (Asset->Skeleton.GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		return false; // bone doesn't exist
	}

	if (!Asset->Solvers[SolverIndex]->UsesBoneSettings())
	{
		return false; // solver doesn't support per-bone settings
	}

	// returns true if the solver in question does NOT already have a settings object for this bone 
	return !static_cast<bool>(Asset->Solvers[SolverIndex]->GetBoneSetting(BoneName));
}

void UIKRigController::RemoveBoneSetting(const FName& BoneName, int32 SolverIndex) const
{
	check(Asset->Solvers.IsValidIndex(SolverIndex))

	if (Asset->Skeleton.GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		return; // bone doesn't exist
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveBoneSetting_Label", "Remove Bone Setting"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();
	Solver->RemoveBoneSetting(BoneName);
	BroadcastNeedsReinitialized();
}

bool UIKRigController::CanRemoveBoneSetting(const FName& BoneName, int32 SolverIndex) const
{
	const UIKRigSolver* Solver = GetSolver(SolverIndex);
	if (!Solver)
	{
		return false; // solver doesn't exist
	}

	if (!Solver->UsesBoneSettings())
	{
		return false; // solver doesn't use bone settings
	}

	if (Asset->Skeleton.GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		return false; // bone doesn't exist
	}

	if (!Solver->GetBoneSetting(BoneName))
	{
		return false; // solver doesn't have any settings for this bone
	}

	return true;
}

UObject* UIKRigController::GetSettingsForBone(const FName& BoneName, int32 SolverIndex) const
{
	const UIKRigSolver* Solver = GetSolver(SolverIndex);
	if (!Solver)
	{
		return nullptr; // solver doesn't exist
	}
	
	return Solver->GetBoneSetting(BoneName);
}

bool UIKRigController::DoesBoneHaveSettings(const FName& BoneName) const
{
	if (Asset->Skeleton.GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		return false; // bone doesn't exist
	}
	
	for (UIKRigSolver* Solver : Asset->Solvers)
	{
		if (UObject* BoneSetting = Solver->GetBoneSetting(BoneName))
		{
			return true;
		}
	}

	return false;
}

FName UIKRigController::AddRetargetChain(const FBoneChain& BoneChain) const
{
	FBoneChain ChainToAdd = BoneChain;
	ChainToAdd.ChainName = GetUniqueRetargetChainName(BoneChain.ChainName);

	FScopedTransaction Transaction(LOCTEXT("AddRetargetChain_Label", "Add Retarget Chain"));
	Asset->Modify();
	
	const int32 NewChainIndex = Asset->RetargetDefinition.BoneChains.Emplace(ChainToAdd);
	
	RetargetChainAdded.Broadcast(Asset);
	BroadcastNeedsReinitialized();

	return Asset->RetargetDefinition.BoneChains[NewChainIndex].ChainName;
}

bool UIKRigController::RemoveRetargetChain(const FName& ChainName) const
{
	FScopedTransaction Transaction(LOCTEXT("RemoveRetargetChain_Label", "Remove Retarget Chain"));
	Asset->Modify();
	
	auto Pred = [ChainName](const FBoneChain& Element)
	{
		if (Element.ChainName == ChainName)
		{
			return true;
		}

		return false;
	};
	
	if (Asset->RetargetDefinition.BoneChains.RemoveAll(Pred) > 0)
	{
		RetargetChainRemoved.Broadcast(Asset, ChainName);
		BroadcastNeedsReinitialized();
		return true;
	}

	return false;
}

FName UIKRigController::RenameRetargetChain(const FName& ChainName, const FName& NewChainName) const
{
	FBoneChain* Chain = Asset->RetargetDefinition.GetEditableBoneChainByName(ChainName);
	if (!Chain)
	{
		return ChainName; // chain doesn't exist to rename
	}

	// make sure it's unique
	const FName UniqueChainName = GetUniqueRetargetChainName(NewChainName);
	
	FScopedTransaction Transaction(LOCTEXT("RenameRetargetChain_Label", "Rename Retarget Chain"));
	Asset->Modify();
	Chain->ChainName = UniqueChainName;
	RetargetChainRenamed.Broadcast(GetAsset(), ChainName, UniqueChainName);
	BroadcastNeedsReinitialized();
	return UniqueChainName;
}

bool UIKRigController::SetRetargetChainStartBone(const FName& ChainName, const FName& StartBoneName) const
{
	if (FBoneChain* BoneChain = Asset->RetargetDefinition.GetEditableBoneChainByName(ChainName))
	{
		FScopedTransaction Transaction(LOCTEXT("SetRetargetChainStartBone_Label", "Set Retarget Chain Start Bone"));
		Asset->Modify();
		BoneChain->StartBone = StartBoneName;
		BroadcastNeedsReinitialized();
		return true;
	}

	return false; // no bone chain with that name
}

bool UIKRigController::SetRetargetChainEndBone(const FName& ChainName, const FName& EndBoneName) const
{
	if (FBoneChain* BoneChain = Asset->RetargetDefinition.GetEditableBoneChainByName(ChainName))
	{
		FScopedTransaction Transaction(LOCTEXT("SetRetargetChainEndBone_Label", "Set Retarget Chain End Bone"));
		Asset->Modify();
		BoneChain->EndBone = EndBoneName;
		BroadcastNeedsReinitialized();
		return true;
	}

	return false; // no bone chain with that name
}

bool UIKRigController::SetRetargetChainGoal(const FName& ChainName, const FName& GoalName) const
{
	FName GoalNameToUse = GoalName;
	if (!GetGoal(GoalName))
	{
		GoalNameToUse = NAME_None; // no goal with that name	
	}
	
	if (FBoneChain* BoneChain = Asset->RetargetDefinition.GetEditableBoneChainByName(ChainName))
	{
		FScopedTransaction Transaction(LOCTEXT("SetRetargetChainGoal_Label", "Set Retarget Chain Goal"));
		Asset->Modify();
		BoneChain->IKGoalName = GoalNameToUse;
		BroadcastNeedsReinitialized();
		return true;
	}

	return false; // no bone chain with that name
}

FName UIKRigController::GetRetargetChainGoal(const FName& ChainName) const
{
	check(Asset)
	const FBoneChain* Chain = Asset->GetRetargetChainByName(ChainName);
	if (!Chain)
	{
		return NAME_None;
	}
	
	return Chain->IKGoalName;
}

FName UIKRigController::GetRetargetChainStartBone(const FName& ChainName) const
{
	check(Asset)
	const FBoneChain* Chain = Asset->GetRetargetChainByName(ChainName);
	if (!Chain)
	{
		return NAME_None;
	}
	
	return Chain->StartBone.BoneName;
}

FName UIKRigController::GetRetargetChainEndBone(const FName& ChainName) const
{
	check(Asset)
	const FBoneChain* Chain = Asset->GetRetargetChainByName(ChainName);
	if (!Chain)
	{
		return NAME_None;
	}
	
	return Chain->EndBone.BoneName;
}

const FBoneChain* UIKRigController::GetRetargetChainByName(const FName& ChainName) const
{
	return Asset->GetRetargetChainByName(ChainName);
}

const TArray<FBoneChain>& UIKRigController::GetRetargetChains() const
{
	check(Asset)
	return Asset->GetRetargetChains();
}

void UIKRigController::SetRetargetRoot(const FName& RootBoneName) const
{
	check(Asset)

	FScopedTransaction Transaction(LOCTEXT("SetRetargetRootBone_Label", "Set Retarget Root Bone"));
	Asset->Modify();
	
	Asset->RetargetDefinition.RootBone = RootBoneName;

	BroadcastNeedsReinitialized();
}

FName UIKRigController::GetRetargetRoot() const
{
	check(Asset)

	return Asset->RetargetDefinition.RootBone;
}

void UIKRigController::SortRetargetChains() const
{
	Asset->RetargetDefinition.BoneChains.Sort([this](const FBoneChain& A, const FBoneChain& B)
	{
		const int32 IndexA = Asset->Skeleton.GetBoneIndexFromName(A.StartBone.BoneName);
		const int32 IndexB = Asset->Skeleton.GetBoneIndexFromName(B.StartBone.BoneName);
		if (IndexA == IndexB)
		{
			// fallback to sorting alphabetically
			return A.ChainName.LexicalLess(B.ChainName);
		}
		return IndexA < IndexB;
	});
}

FName UIKRigController::GetUniqueRetargetChainName(const FName& NameToMakeUnique) const
{
	auto IsNameBeingUsed = [this](const FName& NameToTry)->bool
	{
		for (const FBoneChain& Chain : Asset->RetargetDefinition.BoneChains)
		{
			if (Chain.ChainName == NameToTry)
			{
				return true;
			}
		}
		return false;
	};

	// check if name is already unique
	if (!IsNameBeingUsed(NameToMakeUnique))
	{
		return NameToMakeUnique; 
	}
	
	// keep concatenating an incremented integer suffix until name is unique
	int32 Number = NameToMakeUnique.GetNumber() + 1;
	while(IsNameBeingUsed(FName(NameToMakeUnique, Number)))
	{
		Number++;
	}

	return FName(NameToMakeUnique, Number);
}

bool UIKRigController::ValidateChain(
	const FName& ChainName,
	const FIKRigSkeleton* OptionalSkeleton,
	TSet<int32>& OutChainIndices) const
{
	const FBoneChain* Chain = Asset->GetRetargetChainByName(ChainName);
	if (!Chain)
	{
		return false; // chain doesn't exist
	}

	const FIKRigSkeleton &Skeleton = OptionalSkeleton ? *OptionalSkeleton : GetIKRigSkeleton();
	const int32 StartBoneIndex = Skeleton.GetBoneIndexFromName(Chain->StartBone.BoneName);
	const int32 EndBoneIndex = Skeleton.GetBoneIndexFromName(Chain->EndBone.BoneName);

	const bool bHasStartBone = StartBoneIndex != INDEX_NONE;
	const bool bHasEndBone = EndBoneIndex != INDEX_NONE;

	// chain has neither start nor end bone
	if (!bHasStartBone && !bHasEndBone)
	{
		return false;
	}

	// has only a start bone, this is a single bone "chain" which is fine
	if (bHasStartBone && !bHasEndBone)
	{
		OutChainIndices.Add(StartBoneIndex);
		return true; 
	}

	// has only a end bone, not valid
	if (!bHasStartBone && bHasEndBone)
	{
		OutChainIndices.Add(EndBoneIndex);
		return false; 
	}
	
	// this chain has a start AND an end bone so we must verify that end bone is child of start bone
	int32 NextBoneIndex = EndBoneIndex;
	while (true)
	{
		OutChainIndices.Add(NextBoneIndex);
		if (StartBoneIndex == NextBoneIndex)
		{
			return true;
		}
		
		NextBoneIndex = Skeleton.GetParentIndex(NextBoneIndex);
		if (NextBoneIndex == INDEX_NONE)
		{
			// oops, we walked all the way past the root without finding the start bone
			OutChainIndices.Reset();
			OutChainIndices.Add(EndBoneIndex);
			OutChainIndices.Add(StartBoneIndex);
			return false;
		}
	}
}

FName UIKRigController::GetRetargetChainFromBone(const FName& BoneName, const FIKRigSkeleton* OptionalSkeleton) const
{
	const FIKRigSkeleton& Skeleton = OptionalSkeleton ? *OptionalSkeleton : GetIKRigSkeleton();
	const int32 BoneIndex = Skeleton.GetBoneIndexFromName(BoneName);

	if (BoneName == GetRetargetRoot())
	{
		return FName("Retarget Root");
	}
	
	const TArray<FBoneChain>& Chains = GetRetargetChains();
	TSet<int32> OutBoneIndices;
	for (const FBoneChain& Chain : Chains)
	{
		OutBoneIndices.Reset();
		if (ValidateChain(Chain.ChainName, OptionalSkeleton, OutBoneIndices))
		{
			if (OutBoneIndices.Contains(BoneIndex))
			{
				return Chain.ChainName;
			}
		}
	}

	return NAME_None;
}

FName UIKRigController::GetRetargetChainFromGoal(const FName& GoalName) const
{
	if (GoalName == NAME_None)
	{
		return NAME_None;
	}
	
	const TArray<FBoneChain>& Chains = GetRetargetChains();
	for (const FBoneChain& Chain : Chains)
	{
		if (Chain.IKGoalName == GoalName)
		{
			return Chain.ChainName;
		}
	}

	return NAME_None;
}

// -------------------------------------------------------
// SKELETON
//

bool UIKRigController::SetSkeletalMesh(USkeletalMesh* SkeletalMesh, bool bTransact) const
{
	if (!SkeletalMesh)
	{
		return false;
	}
	
	// first determine runtime compatibility between the IK Rig asset and the skeleton we're trying to run it on
	const FIKRigInputSkeleton InputSkeleton = FIKRigInputSkeleton(SkeletalMesh);
	if (!UIKRigProcessor::IsIKRigCompatibleWithSkeleton(Asset, InputSkeleton, nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("Trying to initialize IKRig with a Skeleton that is missing required bones. See output log. {0}"), *Asset->GetName());
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetSkeletalMesh_Label", "Set Skeletal Mesh"));
	
	if (bTransact)
	{	
		Asset->Modify();
	}

	// update stored skeletal mesh used for previewing results
	Asset->PreviewSkeletalMesh = SkeletalMesh;
	// copy skeleton data from the actual skeleton we want to run on
	Asset->Skeleton.SetInputSkeleton(InputSkeleton, GetAsset()->Skeleton.ExcludedBones);
	// update goal's initial transforms to reflect new
	for (UIKRigEffectorGoal* Goal : Asset->Goals)
	{
		if (bTransact)
		{
			Goal->Modify();
		}
		
		const FTransform InitialTransform = GetRefPoseTransformOfBone(Goal->BoneName);
		Goal->InitialTransform = InitialTransform;
	}

	BroadcastNeedsReinitialized();

	return true;
}

const FIKRigSkeleton& UIKRigController::GetIKRigSkeleton() const
{
	return Asset->Skeleton;
}

USkeleton* UIKRigController::GetSkeleton() const
{
	if (!Asset->PreviewSkeletalMesh)
    {
        return nullptr;
    }
    
    return Asset->PreviewSkeletalMesh->GetSkeleton();
}

void UIKRigController::SetBoneExcluded(const FName& BoneName, const bool bExclude) const
{
	const bool bIsExcluded = Asset->Skeleton.ExcludedBones.Contains(BoneName);
	if (bIsExcluded == bExclude)
	{
		return; // already in the requested state of exclusion
	}

	FScopedTransaction Transaction(LOCTEXT("SetBoneExcluded_Label", "Set Bone Excluded"));
	Asset->Modify();

	if (bExclude)
	{
		Asset->Skeleton.ExcludedBones.Add(BoneName);
	}
	else
	{
		Asset->Skeleton.ExcludedBones.Remove(BoneName);
	}

	BroadcastNeedsReinitialized();
}

bool UIKRigController::GetBoneExcluded(const FName& BoneName) const
{
	return Asset->Skeleton.ExcludedBones.Contains(BoneName);
}

FTransform UIKRigController::GetRefPoseTransformOfBone(const FName& BoneName) const
{	
	const int32 BoneIndex = Asset->Skeleton.GetBoneIndexFromName(BoneName);
	check(BoneIndex != INDEX_NONE) // must initialize IK Rig before getting here
	return Asset->Skeleton.RefPoseGlobal[BoneIndex];
}

// -------------------------------------------------------
// SOLVERS
//

int32 UIKRigController::AddSolver(TSubclassOf<UIKRigSolver> InIKRigSolverClass) const
{
	check(Asset)

	FScopedTransaction Transaction(LOCTEXT("AddSolver_Label", "Add Solver"));
	Asset->Modify();

	UIKRigSolver* NewSolver = NewObject<UIKRigSolver>(Asset, InIKRigSolverClass, NAME_None, RF_Transactional);
	check(NewSolver);

	const int32 SolverIndex = Asset->Solvers.Add(NewSolver);

	BroadcastNeedsReinitialized();

	return SolverIndex;
}

void UIKRigController::RemoveSolver(const int32 SolverIndex) const
{
	check(Asset)
	
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveSolver_Label", "Remove Solver"));
	Asset->Modify();

	Asset->Solvers.RemoveAt(SolverIndex);

	BroadcastNeedsReinitialized();
}

bool UIKRigController::MoveSolverInStack(int32 SolverToMoveIndex, int32 TargetSolverIndex) const
{
	if (!Asset->Solvers.IsValidIndex(SolverToMoveIndex))
	{
		return false;
	}

	if (!Asset->Solvers.IsValidIndex(TargetSolverIndex))
	{
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("ReorderSolver_Label", "Reorder Solvers"));
	Asset->Modify();

	UIKRigSolver* SolverToMove = Asset->Solvers[SolverToMoveIndex];
	Asset->Solvers.Insert(SolverToMove, TargetSolverIndex + 1);
	const int32 SolverToRemove = TargetSolverIndex > SolverToMoveIndex ? SolverToMoveIndex : SolverToMoveIndex + 1;
	Asset->Solvers.RemoveAt(SolverToRemove);

	BroadcastNeedsReinitialized();
	
	return true;
}

bool UIKRigController::SetSolverEnabled(int32 SolverIndex, bool bIsEnabled) const
{
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetSolverEndabled_Label", "Enable/Disable Solver"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();
	Asset->Modify();

	Solver->SetEnabled(bIsEnabled);

	BroadcastNeedsReinitialized();
	
	return true;
}

void UIKRigController::SetRootBone(const FName& RootBoneName, int32 SolverIndex) const
{
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return; // solver doesn't exist
	}

	if (Asset->Skeleton.GetBoneIndexFromName(RootBoneName) == INDEX_NONE)
	{
		return; // bone doesn't exist
	}

	FScopedTransaction Transaction(LOCTEXT("SetRootBone_Label", "Set Root Bone"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();

	Solver->SetRootBone(RootBoneName);

	BroadcastNeedsReinitialized();
}

void UIKRigController::SetEndBone(const FName& EndBoneName, int32 SolverIndex) const
{
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return; // solver doesn't exist
	}

	if (Asset->Skeleton.GetBoneIndexFromName(EndBoneName) == INDEX_NONE)
	{
		return; // bone doesn't exist
	}

	FScopedTransaction Transaction(LOCTEXT("SetEndBone_Label", "Set End Bone"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();

	Solver->SetEndBone(EndBoneName);

	BroadcastNeedsReinitialized();
}

const TArray<UIKRigSolver*>& UIKRigController::GetSolverArray() const
{
	return Asset->Solvers;
}

FString UIKRigController::GetSolverUniqueName(int32 SolverIndex)
{
	check(Asset)
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		checkNoEntry()
		return "";
	}

	return FString::FromInt(SolverIndex+1) + " - " + Asset->Solvers[SolverIndex]->GetNiceName().ToString();
		
}

int32 UIKRigController::GetNumSolvers() const
{
	check(Asset)
	return Asset->Solvers.Num();
}

UIKRigSolver* UIKRigController::GetSolver(int32 Index) const
{
	check(Asset)

	if (Asset->Solvers.IsValidIndex(Index))
	{
		return Asset->Solvers[Index];
	}
	
	return nullptr;
}

// -------------------------------------------------------
// GOALS
//

UIKRigEffectorGoal* UIKRigController::AddNewGoal(const FName& GoalName, const FName& BoneName) const
{
	if (GetIKRigSkeleton().GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		return nullptr; // bone does not exist in the skeleton
	}
	
	if (GetGoalIndex(GoalName) != INDEX_NONE)
	{
		return nullptr; // goal already exists!
	}
	
	FScopedTransaction Transaction(LOCTEXT("AddNewGoal_Label", "Add New Goal"));
	Asset->Modify();

	UIKRigEffectorGoal* NewGoal = NewObject<UIKRigEffectorGoal>(Asset, UIKRigEffectorGoal::StaticClass(), NAME_None, RF_Transactional);
	NewGoal->BoneName = BoneName;
	NewGoal->GoalName = GoalName;
	Asset->Goals.Add(NewGoal);

	// set initial transform
	NewGoal->InitialTransform = GetRefPoseTransformOfBone(NewGoal->BoneName);
	NewGoal->CurrentTransform = NewGoal->InitialTransform;

	// connect the new goal to all the solvers
	for (int32 SolverIndex=0; SolverIndex<GetNumSolvers(); ++SolverIndex)
	{
		ConnectGoalToSolver(*NewGoal, SolverIndex);
	}
 
	BroadcastNeedsReinitialized();
	BroadcastGoalsChange();
	
	return NewGoal;
}

bool UIKRigController::RemoveGoal(const FName& GoalName) const
{
	const int32 GoalIndex = GetGoalIndex(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return false; // can't remove goal we don't have
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveGoal_Label", "Remove Goal"));

	// remove from all the solvers
	const FName& GoalToRemove = Asset->Goals[GoalIndex]->GoalName;
	for (UIKRigSolver* Solver : Asset->Solvers)
	{
		Solver->Modify();
		Solver->RemoveGoal(GoalToRemove);
	}

	// remove from core system
	Asset->Modify();
	Asset->Goals.RemoveAt(GoalIndex);

	// clean any retarget chains that might reference the missing goal
	for (FBoneChain& BoneChain : Asset->RetargetDefinition.BoneChains)
	{
		if (BoneChain.IKGoalName == GoalName)
		{
			BoneChain.IKGoalName = NAME_None;
		}
	}

	BroadcastNeedsReinitialized();
	BroadcastGoalsChange();
	
	return true;
}

FName UIKRigController::RenameGoal(const FName& OldName, const FName& PotentialNewName) const
{
	if (OldName == PotentialNewName)
	{
		return OldName; // skipping renaming the same name
	}
	
	const int32 GoalIndex = GetGoalIndex(OldName);
	if (GoalIndex == INDEX_NONE)
	{
		return NAME_None; // can't rename goal we don't have
	}
	
	// sanitize the potential new name
	FString CleanName = PotentialNewName.ToString();
	SanitizeGoalName(CleanName);
	// make the name unique
	const FName NewName = GetUniqueGoalName(FName(CleanName));
	
	FScopedTransaction Transaction(LOCTEXT("RenameGoal_Label", "Rename Goal"));
	Asset->Modify();

	// rename in core
	Asset->Goals[GoalIndex]->Modify();
	Asset->Goals[GoalIndex]->GoalName = NewName;

	// update any retarget chains that might reference the goal
	for (FBoneChain& BoneChain : Asset->RetargetDefinition.BoneChains)
	{
		if (BoneChain.IKGoalName == OldName)
		{
			BoneChain.IKGoalName = NewName;
		}
	}

	// rename in solvers
	for (UIKRigSolver* Solver : Asset->Solvers)
	{
		Solver->Modify();
		Solver->RenameGoal(OldName, NewName);
	}

	BroadcastNeedsReinitialized();
	BroadcastGoalsChange();

	return NewName;
}

FName UIKRigController::GetUniqueGoalName(const FName& NameToMakeUnique) const
{
	auto IsNameBeingUsed = [this](const FName& NameToTry) -> bool
	{
		// check if this goal already exists (case sensitive)
		int32 ExistingGoalIndex = GetGoalIndex(NameToTry, ENameCase::IgnoreCase);
		return ExistingGoalIndex != INDEX_NONE;
	};

	// check if name is already unique
	if (!IsNameBeingUsed(NameToMakeUnique))
	{
		return NameToMakeUnique; 
	}
	
	// keep concatenating an incremented integer suffix until name is unique
	int32 Number = NameToMakeUnique.GetNumber() + 1;
	while(IsNameBeingUsed(FName(NameToMakeUnique, Number)))
	{
		Number++;
	}

	return FName(NameToMakeUnique, Number);
}

bool UIKRigController::ModifyGoal(const FName& GoalName) const
{
	if (UIKRigEffectorGoal* Goal = GetGoal(GoalName))
	{
		Goal->Modify();
		return true;
	}
	
	return false;
}

bool UIKRigController::SetGoalBone(const FName& GoalName, const FName& NewBoneName) const
{
	const int32 GoalIndex = GetGoalIndex(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return false; // goal doesn't exist in the rig
	}

	const int32 BoneIndex = GetIKRigSkeleton().GetBoneIndexFromName(NewBoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return false; // bone does not exist in the skeleton
	}

	if (GetBoneForGoal(GoalName) == NewBoneName)
	{
		return false; // goal is already using this bone
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetGoalBone_Label", "Set Goal Bone"));

	// update goal
	Asset->Goals[GoalIndex]->Modify();
	Asset->Goals[GoalIndex]->BoneName = NewBoneName;
	
	// update in solvers
	for (UIKRigSolver* Solver : Asset->Solvers)
	{
		Solver->Modify();
		Solver->SetGoalBone(GoalName, NewBoneName);
	}

	// update initial transforms
	ResetGoalTransforms();

	BroadcastNeedsReinitialized();
	
	return true;
}

FName UIKRigController::GetBoneForGoal(const FName& GoalName) const
{
	for (const UIKRigEffectorGoal* Goal : Asset->Goals)
	{
		if (Goal->GoalName == GoalName)
		{
			return Goal->BoneName;
		}
	}
	
	return NAME_None;
}

UIKRigEffectorGoal* UIKRigController::GetGoalForBone(const FName& BoneName) const
{
	TArray<UIKRigEffectorGoal*>& AllGoals = GetAllGoals();
	for (UIKRigEffectorGoal* Goal : AllGoals)
	{
		if (Goal->BoneName == BoneName)
		{
			return Goal;
		}
	}

	return nullptr;
}

bool UIKRigController::ConnectGoalToSolver(const UIKRigEffectorGoal& Goal, int32 SolverIndex) const
{
	// can't add goal that is not present in the core
	check(GetGoalIndex(Goal.GoalName) != INDEX_NONE);
	// can't add goal to a solver with an invalid index
	check(Asset->Solvers.IsValidIndex(SolverIndex))

	FScopedTransaction Transaction(LOCTEXT("ConnectGoalSolver_Label", "Connect Goal to Solver"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();
	
	Solver->AddGoal(Goal);

	BroadcastNeedsReinitialized();
	return true;
}

bool UIKRigController::DisconnectGoalFromSolver(const FName& GoalToRemove, int32 SolverIndex) const
{
	// can't remove goal that is not present in the core
	if (GetGoalIndex(GoalToRemove) == INDEX_NONE)
	{
		return false;
	}
	
	// can't remove goal from a solver with an invalid index
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return false;
	}

    FScopedTransaction Transaction(LOCTEXT("DisconnectGoalSolver_Label", "Disconnect Goal from Solver"));
	UIKRigSolver* Solver = Asset->Solvers[SolverIndex];
	Solver->Modify();
	
	Solver->RemoveGoal(GoalToRemove);

	BroadcastNeedsReinitialized();
	return true;
}

bool UIKRigController::IsGoalConnectedToSolver(const FName& GoalName, int32 SolverIndex) const
{
	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return false;
	}

	return Asset->Solvers[SolverIndex]->IsGoalConnected(GoalName);
}

bool UIKRigController::IsGoalConnectedToAnySolver(const FName& GoalName) const
{
	for (const TObjectPtr<UIKRigSolver> Solver : Asset->Solvers)
	{
		if (Solver->IsGoalConnected(GoalName))
		{
			return true;
		}
	}

	return false;
}

TArray<UIKRigEffectorGoal*>& UIKRigController::GetAllGoals() const
{
	return Asset->Goals;
}

const UIKRigEffectorGoal* UIKRigController::GetGoal(int32 GoalIndex) const
{
	if (!Asset->Goals.IsValidIndex(GoalIndex))
	{
		return nullptr;
	}

	return Asset->Goals[GoalIndex];
}

UIKRigEffectorGoal* UIKRigController::GetGoal(const FName& GoalName) const
{
	const int32 GoalIndex = GetGoalIndex(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return nullptr;
	}
	
	return Asset->Goals[GoalIndex];
}

UObject* UIKRigController::GetGoalSettingsForSolver(const FName& GoalName, int32 SolverIndex) const
{
	const int32 GoalIndex = GetGoalIndex(GoalName);
	if (GoalIndex == INDEX_NONE)
	{
		return nullptr; // no goal with that index
	}

	if (!Asset->Solvers.IsValidIndex(SolverIndex))
	{
		return nullptr; // no solver with that index
	}

	return Asset->Solvers[SolverIndex]->GetGoalSettings(GoalName);
}

FTransform UIKRigController::GetGoalCurrentTransform(const FName& GoalName) const
{
	if(const UIKRigEffectorGoal* Goal = GetGoal(GoalName))
	{
		return Goal->CurrentTransform;
	}
	
	return FTransform::Identity; // no goal with that name
}

void UIKRigController::SetGoalCurrentTransform(const FName& GoalName, const FTransform& Transform) const
{
	UIKRigEffectorGoal* Goal = GetGoal(GoalName);
	check(Goal);

	Goal->CurrentTransform = Transform;
}

void UIKRigController::ResetGoalTransforms() const
{
	FScopedTransaction Transaction(LOCTEXT("ResetGoalTransforms", "Reset All Goal Transforms"));
	
	for (UIKRigEffectorGoal* Goal : Asset->Goals)
    {
		Goal->Modify();
    	const FTransform InitialTransform = GetRefPoseTransformOfBone(Goal->BoneName);
    	Goal->InitialTransform = InitialTransform;
    	Goal->CurrentTransform = InitialTransform;
    }
}

void UIKRigController::SanitizeGoalName(FString& InOutName)
{
	for (int32 i = 0; i < InOutName.Len(); ++i)
	{
		TCHAR& C = InOutName[i];

		const bool bGoodChar =
            ((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||		// A-Z (upper and lowercase) anytime
            (C == '_') || (C == '-') || (C == '.') ||						// _  - . anytime
            ((i > 0) && (C >= '0') && (C <= '9'));							// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}

	// FIXME magic numbers should actually mean something
	static constexpr int32 MaxNameLength = 100;
	if (InOutName.Len() > MaxNameLength)
	{
		InOutName.LeftChopInline(InOutName.Len() - MaxNameLength);
	}
}

int32 UIKRigController::GetGoalIndex(const FName& InGoalName, const ENameCase CompareMethod) const
{
	return Asset->Goals.IndexOfByPredicate([&](const TObjectPtr<UIKRigEffectorGoal>& Goal)
	{
		return Goal->GoalName.IsEqual(InGoalName, CompareMethod); 
	});
}

FName UIKRigController::GetGoalName(const int32& GoalIndex) const
{
	if (!Asset->Goals.IsValidIndex(GoalIndex))
	{
		return NAME_None;
	}

	return Asset->Goals[GoalIndex]->GoalName;
}

void UIKRigController::BroadcastGoalsChange() const
{
	if (Asset)
	{
		static const FName GoalsPropName = GET_MEMBER_NAME_CHECKED(UIKRigDefinition, Goals);
		if (FProperty* GoalProperty = UIKRigDefinition::StaticClass()->FindPropertyByName(GoalsPropName) )
		{
			FPropertyChangedEvent GoalPropertyChangedEvent(GoalProperty);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Asset, GoalPropertyChangedEvent);
		}
	}
}

#undef LOCTEXT_NAMESPACE

