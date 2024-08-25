// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargeterController.h"

#include "ScopedTransaction.h"
#include "Algo/LevenshteinDistance.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "RigEditor/IKRigController.h"
#include "IKRigEditor.h"
#include "RetargetEditor/IKRetargeterPoseGenerator.h"
#include "Retargeter/IKRetargetOps.h"

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

void UIKRetargeterController::PostInitProperties()
{
	Super::PostInitProperties();
	AutoPoseGenerator = MakeUnique<FRetargetAutoPoseGenerator>(this);
}

UIKRetargeter* UIKRetargeterController::GetAsset() const
{
	return Asset;
}

TObjectPtr<UIKRetargeter>& UIKRetargeterController::GetAssetPtr()
{
	return Asset;
}

void UIKRetargeterController::CleanAsset() const
{
	FScopeLock Lock(&ControllerLock);
	CleanChainMapping();
	CleanPoseList(ERetargetSourceOrTarget::Source);
	CleanPoseList(ERetargetSourceOrTarget::Target);
}

void UIKRetargeterController::SetIKRig(const ERetargetSourceOrTarget SourceOrTarget, UIKRigDefinition* IKRig) const
{
	FScopeLock Lock(&ControllerLock);
	
	FScopedReinitializeIKRetargeter Reinitialize(this);
	
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
	AutoMapChains(EAutoMapChainType::Exact, bForceRemap);

	// update any editors attached to this asset
	BroadcastIKRigReplaced(SourceOrTarget);
	BroadcastPreviewMeshReplaced(SourceOrTarget);
}

const UIKRigDefinition* UIKRetargeterController::GetIKRig(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return Asset->GetIKRig(SourceOrTarget);
}

UIKRigDefinition* UIKRetargeterController::GetIKRigWriteable(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return Asset->GetIKRigWriteable(SourceOrTarget);
}

void UIKRetargeterController::SetPreviewMesh(
	const ERetargetSourceOrTarget SourceOrTarget,
	USkeletalMesh* PreviewMesh) const
{
	FScopeLock Lock(&ControllerLock);

	FScopedTransaction Transaction(LOCTEXT("SetPreviewMesh_Transaction", "Set Preview Mesh"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	
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
}

USkeletalMesh* UIKRetargeterController::GetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	
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
	FScopeLock Lock(&ControllerLock);
	return GetAsset()->GetRootSettingsUObject()->Settings;
}

void UIKRetargeterController::SetRootSettings(const FTargetRootSettings& RootSettings) const
{
	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("SetRootSettings_Transaction", "Set Root Settings"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	GetAsset()->Modify();
	GetAsset()->GetRootSettingsUObject()->Settings = RootSettings;
}

FRetargetGlobalSettings UIKRetargeterController::GetGlobalSettings() const
{
	FScopeLock Lock(&ControllerLock);
	return GetAsset()->GetGlobalSettingsUObject()->Settings;
}

void UIKRetargeterController::SetGlobalSettings(const FRetargetGlobalSettings& GlobalSettings) const
{
	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("SetGlobalSettings_Transaction", "Set Global Settings"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	GetAsset()->Modify();
	GetAsset()->GetGlobalSettingsUObject()->Settings = GlobalSettings;
}

FTargetChainSettings UIKRetargeterController::GetRetargetChainSettings(const FName& TargetChainName) const
{
	FScopeLock Lock(&ControllerLock);
	
	const URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName);
	if (!ChainSettings)
	{
		return FTargetChainSettings();
	}

	return ChainSettings->Settings;
}

bool UIKRetargeterController::SetRetargetChainSettings(const FName& TargetChainName, const FTargetChainSettings& Settings) const
{
	FScopeLock Lock(&ControllerLock);

	FScopedTransaction Transaction(LOCTEXT("SetChainSettings_Transaction", "Set Chain Settings"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	
	if (URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName))
	{
		ChainSettings->Modify();
		ChainSettings->Settings = Settings;
		return true;
	}

	return false;
}

int32 UIKRetargeterController::AddRetargetOp(TSubclassOf<URetargetOpBase> InOpClass) const
{
	check(Asset)

	if (!InOpClass)
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Could not add Op to stack. Invalid Op class specified."));
		return INDEX_NONE;
	}

	FScopedTransaction Transaction(LOCTEXT("AddRetargetOp_Label", "Add Retarget Op"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->OpStack->Modify();
	URetargetOpBase* NewOp = NewObject<URetargetOpBase>(Asset, InOpClass, NAME_None, RF_Transactional);
	NewOp->OnAddedToStack(GetAsset());
	return Asset->OpStack->RetargetOps.Add(NewOp);
}

bool UIKRetargeterController::RemoveRetargetOp(const int32 OpIndex) const
{
	check(Asset)
	
	if (!Asset->OpStack->RetargetOps.IsValidIndex(OpIndex))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Retarget Op not removed. Invalid index, %d."), OpIndex);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveRetargetOp_Label", "Remove Retarget Op"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->OpStack->Modify();
	Asset->OpStack->RetargetOps.RemoveAt(OpIndex);
	return true;
}

bool UIKRetargeterController::RemoveAllOps() const
{
	check(Asset)

	FScopedTransaction Transaction(LOCTEXT("RemoveAllRetargetOps_Label", "Remove All Retarget Ops"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->OpStack->Modify();
	Asset->OpStack->RetargetOps.Empty();
	return true;
}

URetargetOpBase* UIKRetargeterController::GetRetargetOpAtIndex(int32 Index) const
{
	check(Asset)

	if (Asset->OpStack->RetargetOps.IsValidIndex(Index))
	{
		return Asset->OpStack->RetargetOps[Index];
	}
	
	return nullptr;
}

int32 UIKRetargeterController::GetIndexOfRetargetOp(URetargetOpBase* RetargetOp) const
{
	check(Asset)
	return Asset->OpStack->RetargetOps.Find(RetargetOp);
}

int32 UIKRetargeterController::GetNumRetargetOps() const
{
	check(Asset)
	return Asset->OpStack->RetargetOps.Num();
}

bool UIKRetargeterController::MoveRetargetOpInStack(int32 OpToMoveIndex, int32 TargetIndex) const
{
	TArray<TObjectPtr<URetargetOpBase>>& RetargetOps = Asset->OpStack->RetargetOps;
	
	if (!RetargetOps.IsValidIndex(OpToMoveIndex))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Retarget Op not moved. Invalid source index, %d."), OpToMoveIndex);
		return false;
	}

	if (!RetargetOps.IsValidIndex(TargetIndex))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Retarget Op not moved. Invalid target index, %d."), TargetIndex);
		return false;
	}

	if (OpToMoveIndex == TargetIndex)
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Retarget Op not moved. Source and target index cannot be the same."));
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("ReorderRetargetOps_Label", "Reorder Retarget Ops"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->OpStack->Modify();
	URetargetOpBase* OpToMove = RetargetOps[OpToMoveIndex];
	RetargetOps.Insert(OpToMove, TargetIndex + 1);
	const int32 OpToRemove = TargetIndex > OpToMoveIndex ? OpToMoveIndex : OpToMoveIndex + 1;
	RetargetOps.RemoveAt(OpToRemove);
	return true;
}

bool UIKRetargeterController::SetRetargetOpEnabled(int32 RetargetOpIndex, bool bIsEnabled) const
{
	if (!Asset->OpStack->RetargetOps.IsValidIndex(RetargetOpIndex))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Retarget op not found. Invalid index, %d."), RetargetOpIndex);
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetRetargetOpEnabled_Label", "Enable/Disable Op"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	URetargetOpBase* OpToMove = Asset->OpStack->RetargetOps[RetargetOpIndex];
	OpToMove->Modify();
	OpToMove->bIsEnabled = bIsEnabled;
	return true;
}

bool UIKRetargeterController::GetRetargetOpEnabled(int32 RetargetOpIndex) const
{
	if (!Asset->OpStack->RetargetOps.IsValidIndex(RetargetOpIndex))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Invalid retarget op index, %d."), RetargetOpIndex);
		return false;
	}

	return Asset->OpStack->RetargetOps[RetargetOpIndex]->bIsEnabled;
}

bool UIKRetargeterController::GetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh) const
{
	return GetAsset()->MeshesAskedToFixRootHeightFor.Contains(Mesh);
}

void UIKRetargeterController::SetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh, bool InAsked) const
{
	FScopeLock Lock(&ControllerLock);
	if (InAsked)
	{
		GetAsset()->MeshesAskedToFixRootHeightFor.Add(Mesh);
	}
	else
	{
		GetAsset()->MeshesAskedToFixRootHeightFor.Remove(Mesh);
	}
}

FName UIKRetargeterController::GetRetargetRootBone(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
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
	if (IsValid(Asset->GetIKRig(ERetargetSourceOrTarget::Target)))
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

	if (IsValid(Asset->GetIKRig(ERetargetSourceOrTarget::Source)))
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
	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("AutoMapRetargetChains", "Auto-Map Retarget Chains"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	
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
	const bool bIsTargetRig = IKRig == Asset->GetIKRig(ERetargetSourceOrTarget::Target);
	if (!bIsTargetRig)
	{
		// if a source chain is added, it will simply be available as a new option, no need to reinitialize until it's used
		return;
	}

	// add the new chain to the mapping data
	CleanChainMapping();
	FScopedReinitializeIKRetargeter Reinitialize(this);
}

void UIKRetargeterController::HandleRetargetChainRenamed(UIKRigDefinition* IKRig, FName OldChainName, FName NewChainName) const
{
	const bool bIsSourceRig = IKRig == Asset->GetIKRig(ERetargetSourceOrTarget::Source);
	const bool bIsTargetRig = IKRig == Asset->GetIKRig(ERetargetSourceOrTarget::Target);
	check(bIsSourceRig || bIsTargetRig)
	for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
	{
		FName& ChainNameToUpdate = bIsSourceRig ? ChainMap->SourceChain : ChainMap->TargetChain;
		if (ChainNameToUpdate == OldChainName)
		{
			FScopedTransaction Transaction(LOCTEXT("RetargetChainRenamed_Label", "Retarget Chain Renamed"));
			ChainMap->Modify();
			ChainNameToUpdate = NewChainName;
			FScopedReinitializeIKRetargeter Reinitialize(this);
			return;
		}
	}
}

void UIKRetargeterController::HandleRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const
{
	FScopedTransaction Transaction(LOCTEXT("RetargetChainRemoved_Label", "Retarget Chain Removed"));
	Asset->Modify();
	
	const bool bIsSourceRig = IKRig == Asset->GetIKRig(ERetargetSourceOrTarget::Source);
	const bool bIsTargetRig = IKRig == Asset->GetIKRig(ERetargetSourceOrTarget::Target);
	check(bIsSourceRig || bIsTargetRig)
	
	// set source chain name to NONE if it has been deleted 
	if (bIsSourceRig)
	{
		for (URetargetChainSettings* ChainMap : Asset->ChainSettings)
		{
			if (ChainMap->SourceChain == InChainRemoved)
			{
				ChainMap->SourceChain = NAME_None;
				FScopedReinitializeIKRetargeter Reinitialize(this);
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
		FScopedReinitializeIKRetargeter Reinitialize(this);
	}
}

bool UIKRetargeterController::SetSourceChain(FName SourceChainName, FName TargetChainName) const
{
	FScopeLock Lock(&ControllerLock);
	URetargetChainSettings* ChainSettings = GetChainSettings(TargetChainName);
	if (!ChainSettings)
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Could not map target chain. Target chain not found, %s."), *TargetChainName.ToString());
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetRetargetChainSource", "Set Retarget Chain Source"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	ChainSettings->Modify();
	ChainSettings->SourceChain = SourceChainName;
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
	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("CreateRetargetPose", "Create Retarget Pose"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->Modify();

	// create a new pose with a unique name 
	const FName UniqueNewPoseName = MakePoseNameUnique(NewPoseName.ToString(), SourceOrTarget);
	GetRetargetPoses(SourceOrTarget).Add(UniqueNewPoseName);

	// set new pose as the current pose
	FName& CurrentRetargetPoseName = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentRetargetPoseName = UniqueNewPoseName;

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

	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("RemoveRetargetPose", "Remove Retarget Pose"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->Modify();

	Poses.Remove(PoseToRemove);

	// did we remove the currently used pose?
	if (GetCurrentRetargetPoseName(SourceOrTarget) == PoseToRemove)
	{
		SetCurrentRetargetPose(UIKRetargeter::GetDefaultPoseName(), SourceOrTarget);
	}
	
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

	FScopeLock Lock(&ControllerLock);
	FScopedTransaction Transaction(LOCTEXT("DuplicateRetargetPose", "Duplicate Retarget Pose"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
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
	return UniqueNewPoseName;
}

bool UIKRetargeterController::RenameRetargetPose(const FName OldPoseName, const FName NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	
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
	FScopedReinitializeIKRetargeter Reinitialize(this);
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
	return true;
}

void UIKRetargeterController::ResetRetargetPose(
	const FName& PoseToReset,
	const TArray<FName>& BonesToReset,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	
	TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(PoseToReset))
	{
		return; // cannot reset pose that doesn't exist
	}
	
	FIKRetargetPose& PoseToEdit = Poses[PoseToReset];

	FScopedReinitializeIKRetargeter Reinitialize(this);
	
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
}

FName UIKRetargeterController::GetCurrentRetargetPoseName(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->CurrentSourceRetargetPose : GetAsset()->CurrentTargetRetargetPose;
}

bool UIKRetargeterController::SetCurrentRetargetPose(FName NewCurrentPose, const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	
	const TMap<FName, FIKRetargetPose>& Poses = GetRetargetPoses(SourceOrTarget);
	if (!Poses.Contains(NewCurrentPose))
	{
		UE_LOG(LogIKRigEditor, Warning, TEXT("Trying to set current pose to a pose that does not exist, %s."), *NewCurrentPose.ToString());
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("SetCurrentPose", "Set Current Pose"));
	FScopedReinitializeIKRetargeter Reinitialize(this);
	Asset->Modify();
	FName& CurrentPose = SourceOrTarget == ERetargetSourceOrTarget::Source ? Asset->CurrentSourceRetargetPose : Asset->CurrentTargetRetargetPose;
	CurrentPose = NewCurrentPose;
	return true;
}

TMap<FName, FIKRetargetPose>& UIKRetargeterController::GetRetargetPoses(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? GetAsset()->SourceRetargetPoses : GetAsset()->TargetRetargetPoses;
}

FIKRetargetPose& UIKRetargeterController::GetCurrentRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return GetRetargetPoses(SourceOrTarget)[GetCurrentRetargetPoseName(SourceOrTarget)];
}

void UIKRetargeterController::SetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const FQuat& RotationOffset,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	FIKRetargetPose& Pose = GetCurrentRetargetPose(SourceOrTarget);
	Pose.SetDeltaRotationForBone(BoneName, RotationOffset);
	const UIKRigDefinition* IKRig = GetAsset()->GetIKRig(SourceOrTarget);
	Pose.SortHierarchically(IKRig->GetSkeleton());
}

FQuat UIKRetargeterController::GetRotationOffsetForRetargetPoseBone(
	const FName& BoneName,
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	
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
	FScopeLock Lock(&ControllerLock);
	GetCurrentRetargetPose(SourceOrTarget).AddToRootTranslationDelta(TranslationOffset);
}

FVector UIKRetargeterController::GetRootOffsetInRetargetPose(
	const ERetargetSourceOrTarget SourceOrTarget) const
{
	FScopeLock Lock(&ControllerLock);
	return GetCurrentRetargetPose(SourceOrTarget).GetRootTranslationDelta();
}

void UIKRetargeterController::AutoAlignAllBones(ERetargetSourceOrTarget SourceOrTarget) const
{
	// undo transaction
	constexpr bool bShouldTransact = true;
	FScopedTransaction Transaction(LOCTEXT("AutoAlignAllBones", "Auto Align All Bones"), bShouldTransact);
	Asset->Modify();
	
	FScopedReinitializeIKRetargeter Reinitialize(this);

	// first reset the entire retarget pose
	ResetRetargetPose(GetCurrentRetargetPoseName(SourceOrTarget), TArray<FName>(), SourceOrTarget);
	
	// suppress warnings about bones that cannot be aligned when aligning ALL bones
	constexpr bool bSuppressWarnings = true;
	AutoPoseGenerator.Get()->AlignAllBones(SourceOrTarget, bSuppressWarnings);
}

void UIKRetargeterController::AutoAlignBones(
	const TArray<FName>& BonesToAlign,
	const ERetargetAutoAlignMethod Method,
	ERetargetSourceOrTarget SourceOrTarget) const
{
	// undo transaction
	constexpr bool bShouldTransact = true;
	FScopedTransaction Transaction(LOCTEXT("AutoAlignSelectedBones", "Auto Align Selected Bones"), bShouldTransact);
	Asset->Modify();

	FScopedReinitializeIKRetargeter Reinitialize(this);
	
	// allow warnings about bones that cannot be aligned when bones are explicitly specified by user
	constexpr bool bSuppressWarnings = false;
	AutoPoseGenerator.Get()->AlignBones(
		BonesToAlign,
		Method,
		SourceOrTarget,
		bSuppressWarnings);
}

void UIKRetargeterController::SnapBoneToGround(FName ReferenceBone, ERetargetSourceOrTarget SourceOrTarget)
{
	// undo transaction
	constexpr bool bShouldTransact = true;
	FScopedTransaction Transaction(LOCTEXT("SnapBoneToGround", "Snap Bone to Ground"), bShouldTransact);
	Asset->Modify();

	AutoPoseGenerator.Get()->SnapToGround(ReferenceBone, SourceOrTarget);
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
	const UIKRigDefinition* TargetIKRig = Asset->GetIKRig(ERetargetSourceOrTarget::Target);
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

