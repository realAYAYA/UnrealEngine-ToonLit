// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargeterController.h"

#include "ScopedTransaction.h"
#include "Algo/LevenshteinDistance.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "RigEditor/IKRigController.h"
#include "IKRigEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargeterController)

#define LOCTEXT_NAMESPACE "IKRetargeterController"

UIKRetargeterController* UIKRetargeterController::GetController(const UIKRetargeter* InRetargeterAsset)
{
	if (!InRetargeterAsset)
	{
		return nullptr;
	}

	if (!InRetargeterAsset->Controller)
	{
		UIKRetargeterController* Controller = NewObject<UIKRetargeterController>();
		Controller->Asset = const_cast<UIKRetargeter*>(InRetargeterAsset);
		Controller->Asset->Controller = Controller;
	}

	return Cast<UIKRetargeterController>(InRetargeterAsset->Controller);
}

UIKRetargeter* UIKRetargeterController::GetAsset() const
{
	return Asset;
}

void UIKRetargeterController::CleanAsset() const
{
	CleanChainMapping();
	CleanPoseList(ERetargetSourceOrTarget::Source);
	CleanPoseList(ERetargetSourceOrTarget::Target);
}

void UIKRetargeterController::SetIKRig(const ERetargetSourceOrTarget SourceOrTarget, UIKRigDefinition* IKRig) const
{
	if (SourceOrTarget == ERetargetSourceOrTarget::Source)
	{
		Asset->SourceIKRigAsset = IKRig;
		Asset->SourcePreviewMesh = IKRig ? IKRig->GetPreviewMesh() : nullptr;
	}
	else
	{
		Asset->TargetIKRigAsset = IKRig;
		Asset->TargetPreviewMesh = IKRig ? IKRig->GetPreviewMesh() : nullptr;
	}
	
	// re-ask to fix root height for this mesh
	if (IKRig)
	{
		SetAskedToFixRootHeightForMesh(GetPreviewMesh(SourceOrTarget), false);
	}

	CleanChainMapping();
	
	constexpr bool bForceRemap = false;
	AutoMapChains(EAutoMapChainType::Fuzzy, bForceRemap);

	// update any editors attached to this asset
	BroadcastIKRigReplaced(SourceOrTarget);
	BroadcastPreviewMeshReplaced(SourceOrTarget);
	BroadcastNeedsReinitialized();
}

const UIKRigDefinition* UIKRetargeterController::GetIKRig(const ERetargetSourceOrTarget SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->GetSourceIKRig() : Asset->GetTargetIKRig();
}

UIKRigDefinition* UIKRetargeterController::GetIKRigWriteable(const ERetargetSourceOrTarget SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->GetSourceIKRigWriteable() : Asset->GetTargetIKRigWriteable();
}

void UIKRetargeterController::SetPreviewMesh(
	const ERetargetSourceOrTarget SourceOrTarget,
	USkeletalMesh* PreviewMesh) const
{	
	if (SourceOrTarget == ERetargetSourceOrTarget::Source)
	{
		Asset->SourcePreviewMesh = PreviewMesh;
	}
	else
	{
		Asset->TargetPreviewMesh = PreviewMesh;
	}
	
	// re-ask to fix root height for this mesh
	SetAskedToFixRootHeightForMesh(PreviewMesh, false);
	
	// update any editors attached to this asset
	BroadcastPreviewMeshReplaced(SourceOrTarget);
	BroadcastNeedsReinitialized();
}

USkeletalMesh* UIKRetargeterController::GetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget) const
{
	// can't preview anything if target IK Rig is null
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	if (!IKRig)
	{
		return nullptr;
	}

	// optionally prefer override if one is provided
	const TSoftObjectPtr<USkeletalMesh> PreviewMesh = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->SourcePreviewMesh : Asset->TargetPreviewMesh;
	if (!PreviewMesh.IsNull())
	{
		return PreviewMesh.LoadSynchronous();
	}

	// fallback to preview mesh from IK Rig asset
	return IKRig->GetPreviewMesh();
}

FTargetRootSettings UIKRetargeterController::GetRootSettings() const
{
	return GetAsset()->GetRootSettingsUObject()->Settings;
}

void UIKRetargeterController::SetRootSettings(const FTargetRootSettings& RootSettings) const
{
	GetAsset()->GetRootSettingsUObject()->Settings = RootSettings;
}

FRetargetGlobalSettings UIKRetargeterController::GetGlobalSettings() const
{
	return GetAsset()->GetGlobalSettingsUObject()->Settings;
}

void UIKRetargeterController::SetGlobalSettings(const FRetargetGlobalSettings& GlobalSettings) const
{
	GetAsset()->GetGlobalSettingsUObject()->Settings = GlobalSettings;
}

FTargetChainSettings UIKRetargeterController::GetRetargetChainSettings(const FName& TargetChainName) const
{
	URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName);
	if (!ChainSettings)
	{
		return FTargetChainSettings();
	}

	return ChainSettings->Settings;
}

bool UIKRetargeterController::SetRetargetChainSettings(const FName& TargetChainName, const FTargetChainSettings& Settings) const
{
	if (URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName))
	{
		ChainSettings->Settings = Settings;
		return true;
	}

	return false;
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
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	return IKRig ? IKRig->GetRetargetRoot() : FName("None");
}

void UIKRetargeterController::GetChainNames(const ERetargetSourceOrTarget SourceOrTarget, TArray<FName>& OutNames) const
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

void UIKRetargeterController::CleanChainMapping() const
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
}

void UIKRetargeterController::CleanPoseList(const ERetargetSourceOrTarget SourceOrTarget) const
{
	const UIKRigDefinition* IKRig = GetIKRig(SourceOrTarget);
	if (!IKRig)
	{
		return;
	}
	
	// remove all bone offsets that are no longer part of the skeleton
	const TArray<FName> AllowedBoneNames = IKRig->GetSkeleton().BoneNames;
	TMap<FName, FIKRetargetPose>& RetargetPoses = GetRetargetPoses(SourceOrTarget);
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
		Pose.Value.SortHierarchically(IKRig->GetSkeleton());
	}
}

void UIKRetargeterController::AutoMapChains(const EAutoMapChainType AutoMapType, const bool bForceRemap) const
{
	FScopedTransaction Transaction(LOCTEXT("AutoMapRetargetChains", "Auto-Map Retarget Chains"));
	
	CleanChainMapping();

	// get names of all the potential chains we could map to on the source
	TArray<FName> SourceChainNames;
	GetChainNames(ERetargetSourceOrTarget::Source, SourceChainNames);
	
	// iterate over all the chain mappings and find matching source chain
	for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
	{
		const bool bIsMappedAlready = ChainMap->SourceChain != NAME_None;
		if (bIsMappedAlready && !bForceRemap)
		{
			continue; // already set by user
		}

		ChainMap->Modify();

		// find a source chain to map to
		int32 SourceChainIndexToMapTo = -1;

		switch (AutoMapType)
		{
		case EAutoMapChainType::Fuzzy:
			{
				// auto-map chains using a fuzzy string comparison
				FString TargetNameLowerCase = ChainMap->TargetChain.ToString().ToLower();
				float HighestScore = 0.2f;
				for (int32 ChainIndex=0; ChainIndex<SourceChainNames.Num(); ++ChainIndex)
				{
					FString SourceNameLowerCase = SourceChainNames[ChainIndex].ToString().ToLower();
					float WorstCase = TargetNameLowerCase.Len() + SourceNameLowerCase.Len();
					WorstCase = WorstCase < 1.0f ? 1.0f : WorstCase;
					const float Score = 1.0f - (Algo::LevenshteinDistance(TargetNameLowerCase, SourceNameLowerCase) / WorstCase);
					if (Score > HighestScore)
					{
						HighestScore = Score;
						SourceChainIndexToMapTo = ChainIndex;
					}
				}
				break;
			}
		case EAutoMapChainType::Exact:
			{
				// if no exact match is found, then set to None
				ChainMap->SourceChain = NAME_None;
				
				// auto-map chains with EXACT same name
				for (int32 ChainIndex=0; ChainIndex<SourceChainNames.Num(); ++ChainIndex)
				{
					if (SourceChainNames[ChainIndex] == ChainMap->TargetChain)
					{
						SourceChainIndexToMapTo = ChainIndex;
						break;
					}
				}
				break;
			}
		case EAutoMapChainType::Clear:
			{
				ChainMap->SourceChain = NAME_None;
				break;
			}
		default:
			checkNoEntry();
		}

		// apply source if any decent matches were found
		if (SourceChainNames.IsValidIndex(SourceChainIndexToMapTo))
		{
			ChainMap->SourceChain = SourceChainNames[SourceChainIndexToMapTo];
		}
	}

	// sort them
	SortChainMapping();
}

void UIKRetargeterController::HandleRetargetChainAdded(UIKRigDefinition* IKRig) const
{
	const bool bIsTargetRig = IKRig == Asset->GetTargetIKRig();
	if (!bIsTargetRig)
	{
		// if a source chain is added, it will simply be available as a new option, no need to reinitialize until it's used
		return;
	}

	// add the new chain to the mapping data
	CleanChainMapping();
	BroadcastNeedsReinitialized();
}

void UIKRetargeterController::HandleRetargetChainRenamed(UIKRigDefinition* IKRig, FName OldChainName, FName NewChainName) const
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

void UIKRetargeterController::HandleRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const
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

bool UIKRetargeterController::SetSourceChain(FName SourceChainName, FName TargetChainName) const
{
	URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName);
	if (!ChainSettings)
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Could not map target chain. Target chain not found, %s."), *TargetChainName.ToString());
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetRetargetChainSource", "Set Retarget Chain Source"));
	ChainSettings->Modify();
	ChainSettings->SourceChain = SourceChainName;
	
	BroadcastNeedsReinitialized();

	return true;
}

FName UIKRetargeterController::GetSourceChain(const FName& TargetChainName) const
{
	const URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName);
	if (!ChainSettings)
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Could not map target chain. Target chain not found, %s."), *TargetChainName.ToString());
		return NAME_None;
	}
	
	return ChainSettings->SourceChain;
}

const TArray<URetargetChainSettings*>& UIKRetargeterController::GetAllChainSettings() const
{
	return Asset->ChainSettings;
}

FName UIKRetargeterController::GetChainGoal(const TObjectPtr<URetargetChainSettings> ChainSettings) const
{
	if (!ChainSettings)
	{
		return NAME_None;
	}
	
	const UIKRigDefinition* TargetIKRig = GetIKRig(ERetargetSourceOrTarget::Target);
	if (!TargetIKRig)
	{
		return NAME_None;
	}

	const UIKRigController* RigController = UIKRigController::GetController(TargetIKRig);
	return RigController->GetRetargetChainGoal(ChainSettings->TargetChain);
}

bool UIKRetargeterController::IsChainGoalConnectedToASolver(const FName& GoalName) const
{
	const UIKRigDefinition* TargetIKRig = GetIKRig(ERetargetSourceOrTarget::Target);
	if (!TargetIKRig)
	{
		return false;
	}

	const UIKRigController* RigController = UIKRigController::GetController(TargetIKRig);
	return RigController->IsGoalConnectedToAnySolver(GoalName);
}

FName UIKRetargeterController::CreateRetargetPose(const FName& NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopedTransaction Transaction(LOCTEXT("CreateRetargetPose", "Create Retarget Pose"));
	Asset->Modify();

	// create a new pose with a unique name 
	const FName UniqueNewPoseName = MakePoseNameUnique(NewPoseName.ToString(), SourceOrTarget);
	GetRetargetPoses(SourceOrTarget).Add(UniqueNewPoseName);

	// set new pose as the current pose
	FName& CurrentRetargetPoseName = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentRetargetPoseName = UniqueNewPoseName;

	BroadcastNeedsReinitialized();

	return UniqueNewPoseName;
}

bool UIKRetargeterController::RemoveRetargetPose(const FName& PoseToRemove, const ERetargetSourceOrTarget SourceOrTarget) const
{
	if (PoseToRemove == Asset->GetDefaultPoseName())
	{
		return false; // cannot remove default pose
	}

	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(PoseToRemove))
	{
		return false; // cannot remove pose that doesn't exist
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

	return true;
}

FName UIKRetargeterController::DuplicateRetargetPose( const FName PoseToDuplicate, const FName NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const
{
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(PoseToDuplicate))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Trying to duplicate pose that does not exist, %s."), *PoseToDuplicate.ToString());
		return NAME_None; // cannot duplicate pose that doesn't exist
	}
	
	FScopedTransaction Transaction(LOCTEXT("DuplicateRetargetPose", "Duplicate Retarget Pose"));
	Asset->Modify();

	// create a new pose with a unique name
	const FName UniqueNewPoseName = MakePoseNameUnique(NewPoseName.ToString(), SourceOrTarget);
	FIKRetargetPose& NewPose = Poses.Add(UniqueNewPoseName);
	// duplicate the pose data
	NewPose.RootTranslationOffset = Poses[PoseToDuplicate].RootTranslationOffset;
	NewPose.BoneRotationOffsets = Poses[PoseToDuplicate].BoneRotationOffsets;

	// set duplicate to be the current pose
	FName& CurrentRetargetPoseName = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentRetargetPoseName = UniqueNewPoseName;

	BroadcastNeedsReinitialized();

	return UniqueNewPoseName;
}

bool UIKRetargeterController::RenameRetargetPose(const FName OldPoseName, const FName NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const
{
	// does the old pose exist?
	if (!GetRetargetPoses(SourceOrTarget).Contains(OldPoseName))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Trying to rename pose that does not exist, %s."), *OldPoseName.ToString());
		return false;
	}

	// do not allow renaming the default pose (this is disallowed from the UI, but must be done here as well for API usage)
	if (OldPoseName == UIKRetargeter::GetDefaultPoseName())
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Trying to rename the default pose. This is not allowed."));
    	return false;
	}

	// check if we're renaming the current pose
	const bool bWasCurrentPose = GetCurrentRetargetPoseName(SourceOrTarget) == OldPoseName;
	
	FScopedTransaction Transaction(LOCTEXT("RenameRetargetPose", "Rename Retarget Pose"));
	Asset->Modify();

	// make sure new name is unique
	const FName UniqueNewPoseName = MakePoseNameUnique(NewPoseName.ToString(), SourceOrTarget);

	// replace key in the map
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	const FIKRetargetPose OldPoseData = Poses[OldPoseName];
	Poses.Remove(OldPoseName);
	Poses.Shrink();
	Poses.Add(UniqueNewPoseName, OldPoseData);

	// make this the current retarget pose, iff the old one was
	if (bWasCurrentPose)
	{
		SetCurrentRetargetPose(UniqueNewPoseName, SourceOrTarget);
	}

	BroadcastNeedsReinitialized();

	return true;
}

void UIKRetargeterController::ResetRetargetPose(
	const FName& PoseToReset,
	const TArray<FName>& BonesToReset,
	const ERetargetSourceOrTarget SourceOrTarget) const
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

FName UIKRetargeterController::GetCurrentRetargetPoseName(const ERetargetSourceOrTarget SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->CurrentSourceRetargetPose : GetAsset()->CurrentTargetRetargetPose;
}

bool UIKRetargeterController::SetCurrentRetargetPose(FName NewCurrentPose, const ERetargetSourceOrTarget SourceOrTarget) const
{
	const TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(NewCurrentPose))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Trying to set current pose to a pose that does not exist, %s."), *NewCurrentPose.ToString());
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetCurrentPose", "Set Current Pose"));
	Asset->Modify();
	FName& CurrentPose = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentPose = NewCurrentPose;
	
	BroadcastNeedsReinitialized();

	return true;
}

TMap<FName, FIKRetargetPose>& UIKRetargeterController::GetRetargetPoses(const ERetargetSourceOrTarget SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->SourceRetargetPoses : GetAsset()->TargetRetargetPoses;
}

FIKRetargetPose& UIKRetargeterController::GetCurrentRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const
{
	return GetRetargetPoses(SourceOrTarget)[GetCurrentRetargetPoseName(SourceOrTarget)];
}

void UIKRetargeterController::SetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const FQuat& RotationOffset,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	const UIKRigDefinition* IKRig = SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->GetSourceIKRig() : GetAsset()->GetTargetIKRig();
	FIKRetargetPose& Pose = GetCurrentRetargetPose(SourceOrTarget);
	Pose.SetDeltaRotationForBone(BoneName, RotationOffset);
	Pose.SortHierarchically(IKRig->GetSkeleton());
}

FQuat UIKRetargeterController::GetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	TMap<FName, FQuat>& BoneOffsets = GetCurrentRetargetPose(SourceOrTarget).BoneRotationOffsets;
	if (!BoneOffsets.Contains(BoneName))
	{
		return FQuat::Identity;
	}
	
	return BoneOffsets[BoneName];
}

void UIKRetargeterController::SetRootOffsetInRetargetPose(
	const FVector& TranslationOffset,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	GetCurrentRetargetPose(SourceOrTarget).AddToRootTranslationDelta(TranslationOffset);
}

FVector UIKRetargeterController::GetRootOffsetInRetargetPose(
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	return GetCurrentRetargetPose(SourceOrTarget).GetRootTranslationDelta();
}

FName UIKRetargeterController::MakePoseNameUnique(const FString& PoseName, const ERetargetSourceOrTarget SourceOrTarget) const
{
	FString UniqueName = PoseName;
	
	if (UniqueName.IsEmpty())
	{
		UniqueName = Asset->GetDefaultPoseName().ToString();
	}
	
	int32 Suffix = 1;
	const TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	while (Poses.Contains(FName(UniqueName)))
	{
		UniqueName = PoseName + "_" + FString::FromInt(Suffix);
		++Suffix;
	}
	return FName(UniqueName);
}

URetargetChainSettings* UIKRetargeterController::GetChainSettings(const FName& TargetChainName) const
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
		const FIKRigSkeleton& TargetSkeleton = TargetIKRig->GetSkeleton();

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

