// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargeterController.h"

#include "ScopedTransaction.h"
#include "Algo/LevenshteinDistance.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "RetargetEditor/IKRetargetEditorController.h"
#include "Retargeter/IKRetargeter.h"
#include "RigEditor/IKRigController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargeterController)

#define LOCTEXT_NAMESPACE "IKRetargeterController"


UIKRetargeterController* UIKRetargeterController::GetController(UIKRetargeter* InRetargeterAsset)
{
	if (!InRetargeterAsset)
	{
		return nullptr;
	}

	if (!InRetargeterAsset->Controller)
	{
		UIKRetargeterController* Controller = NewObject<UIKRetargeterController>();
		Controller->Asset = InRetargeterAsset;
		InRetargeterAsset->Controller = Controller;
	}

	UIKRetargeterController* Controller = Cast<UIKRetargeterController>(InRetargeterAsset->Controller);
	return Controller;
}

UIKRetargeter* UIKRetargeterController::GetAsset() const
{
	return Asset;
}

void UIKRetargeterController::SetSourceIKRig(UIKRigDefinition* SourceIKRig)
{
	Asset->SourceIKRigAsset = SourceIKRig;
	Asset->SourcePreviewMesh = Asset->SourceIKRigAsset->PreviewSkeletalMesh;
}

USkeletalMesh* UIKRetargeterController::GetPreviewMesh(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	// can't preview anything if target IK Rig is null
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	if (!IKRig)
	{
		return nullptr;
	}

	// optionally prefer override if one is provided
	const TSoftObjectPtr<USkeletalMesh> OverrideMesh = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->SourcePreviewMesh : Asset->TargetPreviewMesh;
	if (!OverrideMesh.IsNull())
	{
		return OverrideMesh.LoadSynchronous();
	}

	// fallback to preview mesh from IK Rig asset
	return IKRig->GetPreviewMesh();
}

const UIKRigDefinition* UIKRetargeterController::GetIKRig(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->GetSourceIKRig() : Asset->GetTargetIKRig();
}

UIKRigDefinition* UIKRetargeterController::GetIKRigWriteable(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->GetSourceIKRigWriteable() : Asset->GetTargetIKRigWriteable();
}

void UIKRetargeterController::OnIKRigChanged(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	if (const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget))
	{
		if (SourceOrTarget == ERetargetSourceOrTarget::Source)
		{
			Asset->SourcePreviewMesh = IKRig->PreviewSkeletalMesh;
		}
		else
		{
			Asset->TargetPreviewMesh = IKRig->PreviewSkeletalMesh;
		}

		// re-ask to fix root height for this mesh
		SetAskedToFixRootHeightForMesh(IKRig->PreviewSkeletalMesh.Get(), false);
	}
}

bool UIKRetargeterController::GetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh) const
{
	return GetAsset()->MeshesAskedToFixRootHeightFor.Contains(Mesh);
}

void UIKRetargeterController::SetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh, bool InAsked) const
{
	if (InAsked)
	{
		GetAsset()->MeshesAskedToFixRootHeightFor.Add(Mesh);
	}
	else
	{
		GetAsset()->MeshesAskedToFixRootHeightFor.Remove(Mesh);
	}
}

FName UIKRetargeterController::GetRetargetRootBone(
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	return IKRig ? IKRig->GetRetargetRoot() : FName("None");
}

void UIKRetargeterController::GetChainNames(const ERetargetSourceOrTarget& SourceOrTarget, TArray<FName>& OutNames) const
{
	if (const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget))
	{
		const TArray<FBoneChain>& Chains = IKRig->GetRetargetChains();
		for (const FBoneChain& Chain : Chains)
		{
			OutNames.Add(Chain.ChainName);
		}
	}
}

void UIKRetargeterController::CleanChainMapping(const bool bForceReinitialization) const
{
	if (IsValid(Asset->GetTargetIKRig()))
	{
		TArray<FName> TargetChainNames;
		GetChainNames(ERetargetSourceOrTarget::Target, TargetChainNames);

		// remove all target chains that are no longer in the target IK rig asset
		TArray<FName> TargetChainsToRemove;
		for (const URetargetChainSettings* ChainMap : Asset->ChainSettings)
		{
			if (!TargetChainNames.Contains(ChainMap->TargetChain))
			{
				TargetChainsToRemove.Add(ChainMap->TargetChain);
			}
		}
		for (FName TargetChainToRemove : TargetChainsToRemove)
		{
			Asset->ChainSettings.RemoveAll([&TargetChainToRemove](const URetargetChainSettings* Element)
			{
				return Element->TargetChain == TargetChainToRemove;
			});
		}

		// add a mapping for each chain that is in the target IK rig (if it doesn't have one already)
		for (FName TargetChainName : TargetChainNames)
		{
			const bool HasChain = Asset->ChainSettings.ContainsByPredicate([&TargetChainName](const URetargetChainSettings* Element)
			{
				return Element->TargetChain == TargetChainName;
			});
		
			if (!HasChain)
			{
				TObjectPtr<URetargetChainSettings> ChainMap = NewObject<URetargetChainSettings>(Asset, URetargetChainSettings::StaticClass(), NAME_None, RF_Transactional);
				ChainMap->TargetChain = TargetChainName;
				Asset->ChainSettings.Add(ChainMap);
			}
		}
	}

	if (IsValid(Asset->GetSourceIKRig()))
	{
		TArray<FName> SourceChainNames;
		GetChainNames(ERetargetSourceOrTarget::Source,SourceChainNames);
	
		// reset any sources that are no longer present to "None"
		for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
		{
			if (!SourceChainNames.Contains(ChainMap->SourceChain))
			{
				ChainMap->SourceChain = NAME_None;
			}
		}
	}

	// enforce the chain order based on the StartBone index
	SortChainMapping();

	if (bForceReinitialization)
	{
		BroadcastNeedsReinitialized();
	}
}

void UIKRetargeterController::CleanPoseLists(const bool bForceReinitialization) const
{
	CleanPoseList(ERetargetSourceOrTarget::Source);
	CleanPoseList(ERetargetSourceOrTarget::Target);

	if (bForceReinitialization)
	{
		BroadcastNeedsReinitialized();	
	}
}

void UIKRetargeterController::CleanPoseList(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	TMap<FName, FIKRetargetPose>& RetargetPoses = GetRetargetPoses(SourceOrTarget);

	// remove all bone offsets that are no longer part of the skeleton
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	if (IKRig)
	{
		const TArray<FName> AllowedBoneNames = IKRig->Skeleton.BoneNames;
		for (TTuple<FName, FIKRetargetPose>& Pose : RetargetPoses)
		{
			// find bone offsets no longer in target skeleton
			TArray<FName> BonesToRemove;
			for (TTuple<FName, FQuat>& BoneOffset : Pose.Value.BoneRotationOffsets)
			{
				if (!AllowedBoneNames.Contains(BoneOffset.Key))
				{
					BonesToRemove.Add(BoneOffset.Key);
				}
			}
			
			// remove bone offsets
			for (const FName& BoneToRemove : BonesToRemove)
			{
				Pose.Value.BoneRotationOffsets.Remove(BoneToRemove);
			}

			// sort the pose offset from leaf to root
			Pose.Value.SortHierarchically(IKRig->Skeleton);
		}
	}
}

void UIKRetargeterController::AutoMapChains() const
{
	TArray<FName> SourceChainNames;
	GetChainNames(ERetargetSourceOrTarget::Source, SourceChainNames);
	
	// auto-map any chains that have no value using a fuzzy string search
	for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
	{
		if (ChainMap->SourceChain != NAME_None)
		{
			continue; // already set by user
		}

		// find "best match" automatically as a convenience for the user
		FString TargetNameLowerCase = ChainMap->TargetChain.ToString().ToLower();
		float HighestScore = 0.2f;
		int32 HighestScoreIndex = -1;
		for (int32 ChainIndex=0; ChainIndex<SourceChainNames.Num(); ++ChainIndex)
		{
			FString SourceNameLowerCase = SourceChainNames[ChainIndex].ToString().ToLower();
			float WorstCase = TargetNameLowerCase.Len() + SourceNameLowerCase.Len();
			WorstCase = WorstCase < 1.0f ? 1.0f : WorstCase;
			const float Score = 1.0f - (Algo::LevenshteinDistance(TargetNameLowerCase, SourceNameLowerCase) / WorstCase);
			if (Score > HighestScore)
			{
				HighestScore = Score;
				HighestScoreIndex = ChainIndex;
			}
		}

		// apply source if any decent matches were found
		if (SourceChainNames.IsValidIndex(HighestScoreIndex))
		{
			ChainMap->SourceChain = SourceChainNames[HighestScoreIndex];
		}
	}

	// sort them
	SortChainMapping();

	// force update with latest mapping
	BroadcastNeedsReinitialized();
}

void UIKRetargeterController::OnRetargetChainAdded(UIKRigDefinition* IKRig) const
{
	const bool bIsTargetRig = IKRig == Asset->GetTargetIKRig();
	if (!bIsTargetRig)
	{
		// if a source chain is added, it will simply be available as a new option, no need to reinitialize until it's used
		return;
	}

	// add the new chain to the mapping data
	constexpr bool bForceReinitialization = true;
	CleanChainMapping(bForceReinitialization);
}

void UIKRetargeterController::OnRetargetChainRenamed(UIKRigDefinition* IKRig, FName OldChainName, FName NewChainName) const
{
	const bool bIsSourceRig = IKRig == Asset->GetSourceIKRig();
	check(bIsSourceRig || IKRig == Asset->GetTargetIKRig())
	for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
	{
		FName& ChainNameToUpdate = bIsSourceRig ? ChainMap->SourceChain : ChainMap->TargetChain;
		if (ChainNameToUpdate == OldChainName)
		{
			ChainNameToUpdate = NewChainName;
			BroadcastNeedsReinitialized();
			return;
		}
	}
}

void UIKRetargeterController::OnRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const
{
	const bool bIsSourceRig = IKRig == Asset->GetSourceIKRig();
	check(bIsSourceRig || IKRig == Asset->GetTargetIKRig())

	// set source chain name to NONE if it has been deleted 
	if (bIsSourceRig)
	{
		for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
		{
			if (ChainMap->SourceChain == InChainRemoved)
			{
				ChainMap->SourceChain = NAME_None;
				BroadcastNeedsReinitialized();
				return;
			}
		}
		return;
	}
	
	// remove target mapping if the target chain has been removed
	const int32 ChainIndex = Asset->ChainSettings.IndexOfByPredicate([&InChainRemoved](const URetargetChainSettings* ChainMap)
	{
		return ChainMap->TargetChain == InChainRemoved;
	});
	
	if (ChainIndex != INDEX_NONE)
	{
		Asset->ChainSettings.RemoveAt(ChainIndex);
		BroadcastNeedsReinitialized();
	}
}

void UIKRetargeterController::SetSourceChainForTargetChain(URetargetChainSettings* ChainMap, FName SourceChainToMapTo) const
{
	check(ChainMap)
	
	FScopedTransaction Transaction(LOCTEXT("SetRetargetChainSource", "Set Retarget Chain Source"));
	ChainMap->Modify();
	ChainMap->SourceChain = SourceChainToMapTo;
	
	BroadcastNeedsReinitialized();
}

const TArray<TObjectPtr<URetargetChainSettings>>& UIKRetargeterController::GetChainMappings() const
{
	return Asset->ChainSettings;
}

const URetargetChainSettings* UIKRetargeterController::GetChainMappingByTargetChainName(const FName& TargetChainName) const
{
	for (TObjectPtr<URetargetChainSettings> ChainMap : Asset->ChainSettings)
	{
		if (ChainMap->TargetChain == TargetChainName)
		{
			return ChainMap;
		}
	}

	return nullptr;
}

FName UIKRetargeterController::GetChainGoal(const TObjectPtr<URetargetChainSettings> ChainSettings) const
{
	if (!ChainSettings)
	{
		return NAME_None;
	}
	
	UIKRigDefinition* TargetIKRig = GetIKRigWriteable(ERetargetSourceOrTarget::Target);
	if (!TargetIKRig)
	{
		return NAME_None;
	}
	
	const UIKRigController* IKRigController = UIKRigController::GetIKRigController(TargetIKRig);
	return IKRigController->GetRetargetChainGoal(ChainSettings->TargetChain);
}

bool UIKRetargeterController::IsChainGoalConnectedToASolver(const FName& GoalName) const
{
	UIKRigDefinition* TargetIKRig = GetIKRigWriteable(ERetargetSourceOrTarget::Target);
	if (!TargetIKRig)
	{
		return false;
	}
	
	const UIKRigController* IKRigController = UIKRigController::GetIKRigController(TargetIKRig);
	return IKRigController->IsGoalConnectedToAnySolver(GoalName);
}

void UIKRetargeterController::AddRetargetPose(
	const FName& NewPoseName,
	const FIKRetargetPose* ToDuplicate,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	FScopedTransaction Transaction(LOCTEXT("AddRetargetPose", "Add Retarget Pose"));
	Asset->Modify();
	
	const FName UniqueNewPoseName = MakePoseNameUnique(NewPoseName.ToString(), SourceOrTarget);
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	FIKRetargetPose& NewPose = Poses.Add(UniqueNewPoseName);
	if (ToDuplicate)
	{
		NewPose.RootTranslationOffset = ToDuplicate->RootTranslationOffset;
		NewPose.BoneRotationOffsets = ToDuplicate->BoneRotationOffsets;
	}
	
	FName& CurrentRetargetPoseName = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentRetargetPoseName = UniqueNewPoseName;

	BroadcastNeedsReinitialized();
}

void UIKRetargeterController::RenameCurrentRetargetPose(
	const FName& NewPoseName,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	// do we already have a retarget pose with this name?
	
	if (GetRetargetPoses(SourceOrTarget).Contains(NewPoseName))
	{
		return;
	}
	
	FScopedTransaction Transaction(LOCTEXT("RenameRetargetPose", "Rename Retarget Pose"));
	Asset->Modify();

	// replace key in the map
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	const FName CurrentPoseName = GetCurrentRetargetPoseName(SourceOrTarget);
	const FIKRetargetPose CurrentPoseData = GetCurrentRetargetPose(SourceOrTarget);
	Poses.Remove(CurrentPoseName);
	Poses.Shrink();
	Poses.Add(NewPoseName, CurrentPoseData);

	// update current pose name
	SetCurrentRetargetPose(NewPoseName, SourceOrTarget);

	BroadcastNeedsReinitialized();
}

void UIKRetargeterController::RemoveRetargetPose(
	const FName& PoseToRemove,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	if (PoseToRemove == Asset->GetDefaultPoseName())
	{
		return; // cannot remove default pose
	}

	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(PoseToRemove))
	{
		return; // cannot remove pose that doesn't exist
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveRetargetPose", "Remove Retarget Pose"));
	Asset->Modify();

	Poses.Remove(PoseToRemove);

	// did we remove the currently used pose?
	if (GetCurrentRetargetPoseName(SourceOrTarget) == PoseToRemove)
	{
		SetCurrentRetargetPose(UIKRetargeter::GetDefaultPoseName(), SourceOrTarget);
	}

	BroadcastNeedsReinitialized();
}

void UIKRetargeterController::ResetRetargetPose(
	const FName& PoseToReset,
	const TArray<FName>& BonesToReset,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(PoseToReset))
	{
		return; // cannot reset pose that doesn't exist
	}
	
	FIKRetargetPose& PoseToEdit = Poses[PoseToReset];
	
	if (BonesToReset.IsEmpty())
	{
		FScopedTransaction Transaction(LOCTEXT("ResetRetargetPose", "Reset Retarget Pose"));
		Asset->Modify();

		PoseToEdit.BoneRotationOffsets.Reset();
		PoseToEdit.RootTranslationOffset = FVector::ZeroVector;
	}
	else
	{
		FScopedTransaction Transaction(LOCTEXT("ResetRetargetBonePose", "Reset Bone Pose"));
		Asset->Modify();
		
		const FName RootBoneName = GetRetargetRootBone(SourceOrTarget);
		for (const FName& BoneToReset : BonesToReset)
		{
			if (PoseToEdit.BoneRotationOffsets.Contains(BoneToReset))
			{
				PoseToEdit.BoneRotationOffsets.Remove(BoneToReset);
			}

			if (BoneToReset == RootBoneName)
			{
				PoseToEdit.RootTranslationOffset = FVector::ZeroVector;	
			}
		}
	}

	BroadcastNeedsReinitialized();
}

FName UIKRetargeterController::GetCurrentRetargetPoseName(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->CurrentSourceRetargetPose : GetAsset()->CurrentTargetRetargetPose;
}

void UIKRetargeterController::SetCurrentRetargetPose(FName NewCurrentPose, const ERetargetSourceOrTarget& SourceOrTarget) const
{
	const TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	check(Poses.Contains(NewCurrentPose));

	FScopedTransaction Transaction(LOCTEXT("SetCurrentPose", "Set Current Pose"));
	Asset->Modify();
	FName& CurrentPose = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentPose = NewCurrentPose;
	
	BroadcastNeedsReinitialized();
}

TMap<FName, FIKRetargetPose>& UIKRetargeterController::GetRetargetPoses(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->SourceRetargetPoses : GetAsset()->TargetRetargetPoses;
}

FIKRetargetPose& UIKRetargeterController::GetCurrentRetargetPose(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return GetRetargetPoses(SourceOrTarget)[GetCurrentRetargetPoseName(SourceOrTarget)];
}

void UIKRetargeterController::SetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const FQuat& RotationOffset,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	const UIKRigDefinition* IKRig = SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->GetSourceIKRig() : GetAsset()->GetTargetIKRig();
	FIKRetargetPose& Pose = GetCurrentRetargetPose(SourceOrTarget);
	Pose.SetDeltaRotationForBone(BoneName, RotationOffset);
	Pose.SortHierarchically(IKRig->Skeleton);
}

FQuat UIKRetargeterController::GetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	TMap<FName, FQuat>& BoneOffsets = GetCurrentRetargetPose(SourceOrTarget).BoneRotationOffsets;
	if (!BoneOffsets.Contains(BoneName))
	{
		return FQuat::Identity;
	}
	
	return BoneOffsets[BoneName];
}

void UIKRetargeterController::AddTranslationOffsetToRetargetRootBone(
	const FVector& TranslationOffset,
	const ERetargetSourceOrTarget& SourceOrTarget) const
{
	GetCurrentRetargetPose(SourceOrTarget).AddToRootTranslationDelta(TranslationOffset);
}

FName UIKRetargeterController::MakePoseNameUnique(const FString& PoseName, const ERetargetSourceOrTarget& SourceOrTarget) const
{
	FString UniqueName = PoseName;
	int32 Suffix = 1;
	const TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	while (Poses.Contains(FName(UniqueName)))
	{
		UniqueName = PoseName + "_" + FString::FromInt(Suffix);
		++Suffix;
	}
	return FName(UniqueName);
}

URetargetChainSettings* UIKRetargeterController::GetChainMap(const FName& TargetChainName) const
{
	for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
	{
		if (ChainMap->TargetChain == TargetChainName)
		{
			return ChainMap;
		}
	}

	return nullptr;
}

void UIKRetargeterController::SortChainMapping() const
{
	const UIKRigDefinition* TargetIKRig = Asset->GetTargetIKRig();
	if (!IsValid(TargetIKRig))
	{
		return;
	}
	
	Asset->ChainSettings.Sort([TargetIKRig](const URetargetChainSettings& A, const URetargetChainSettings& B)
	{
		const TArray<FBoneChain>& BoneChains = TargetIKRig->GetRetargetChains();
		const FIKRigSkeleton& TargetSkeleton = TargetIKRig->Skeleton;

		// look for chains
		const int32 IndexA = BoneChains.IndexOfByPredicate([&A](const FBoneChain& Chain)
		{
			return A.TargetChain == Chain.ChainName;
		});

		const int32 IndexB = BoneChains.IndexOfByPredicate([&B](const FBoneChain& Chain)
		{
			return B.TargetChain == Chain.ChainName;
		});

		// compare their StartBone Index 
		if (IndexA > INDEX_NONE && IndexB > INDEX_NONE)
		{
			const int32 StartBoneIndexA = TargetSkeleton.GetBoneIndexFromName(BoneChains[IndexA].StartBone.BoneName);
			const int32 StartBoneIndexB = TargetSkeleton.GetBoneIndexFromName(BoneChains[IndexB].StartBone.BoneName);

			if (StartBoneIndexA == StartBoneIndexB)
			{
				// fallback to sorting alphabetically
				return BoneChains[IndexA].ChainName.LexicalLess(BoneChains[IndexB].ChainName);
			}
				
			return StartBoneIndexA < StartBoneIndexB;	
		}

		// sort them according to the target ik rig if previously failed 
		return IndexA < IndexB;
	});
}

#undef LOCTEXT_NAMESPACE

