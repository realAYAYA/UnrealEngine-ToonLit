// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesEditorController.h"

#include "PoseCorrectivesEditorToolkit.h"
#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesAnimInstance.h"
#include "SCorrectivesViewer.h"
#include "SPoseCorrectivesGroups.h"
#include "CorrectivesEditMode.h"

#include "AnimPose.h"
#include "AnimPreviewInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigObjectBinding.h"
#include "EditorModeManager.h"
#include "ScopedTransaction.h"

#include "ControlRig/Private/Units/Execution/RigUnit_InverseExecution.h"


#define LOCTEXT_NAMESPACE "PoseCorrectivesEditorController"

void FPoseCorrectivesEditorController::Initialize(TSharedPtr<FPoseCorrectivesEditorToolkit> InEditorToolkit, UPoseCorrectivesAsset* InAsset)
{
	EditorToolkit = InEditorToolkit;
	Asset = InAsset;
}

void FPoseCorrectivesEditorController::InitializeTargetControlRigBP()
{
	if (!TargetSkelMeshComponent || !TargetSkelMeshComponent->GetSkinnedAsset())
	{
		UninitializeTargetControlRigBP();
		return;
	}

	TObjectPtr<const UControlRigBlueprint> ControlRigBP = Cast<UControlRigBlueprint>(Asset->ControlRigBlueprint);	
	if (ControlRigBP)
	{
		if (const UClass* GeneratedClass = ControlRigBP->GetControlRigBlueprintGeneratedClass())
		{
			if (ControlRig)
			{
				if (ControlRig->GetClass() != GeneratedClass)
				{
					ControlRig = nullptr;
				}
			}

			if (ControlRig == nullptr)
			{
				ControlRig = NewObject<UControlRig>(TargetSkelMeshComponent, GeneratedClass);
				ControlRig->ExecutionType = ERigExecutionType::Editing;				
				ControlRig->Initialize();

				ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());				
				TargetAnimInstance->SetSourceControlRig(ControlRig);
 			}

			FEditorModeTools& EditorModeTools = EditorToolkit.Pin()->GetEditorModeManager();
			FCorrectivesEditMode* EditMode = static_cast<FCorrectivesEditMode*>(EditorModeTools.GetActiveMode(FCorrectivesEditMode::ModeName));
			if (!EditMode)
			{
				EditorModeTools.ActivateMode(FCorrectivesEditMode::ModeName);
				EditMode = static_cast<FCorrectivesEditMode*>(EditorModeTools.GetActiveMode(FCorrectivesEditMode::ModeName));
			}
		
			if (EditMode)
			{
				EditMode->OnGetRigElementTransform() = FOnGetRigElementTransform::CreateSP(this, &FPoseCorrectivesEditorController::GetRigElementTransform);
				EditMode->OnSetRigElementTransform() = FOnSetRigElementTransform::CreateSP(this, &FPoseCorrectivesEditorController::SetRigElementTransform);
				
				EditMode->SetObjects(ControlRig, TargetSkelMeshComponent, nullptr);
			}

			EnableControlRigInteraction(IsEditingPose());
		}
	}
	else
	{
		UninitializeTargetControlRigBP();
	}
}

void FPoseCorrectivesEditorController::UninitializeTargetControlRigBP()
{
	ControlRig = nullptr;

	if (FCorrectivesEditMode* EditMode = GetControlRigEditMode())
	{
		EditorToolkit.Pin()->GetEditorModeManager().DeactivateMode(FCorrectivesEditMode::ModeName);
	}
}

void FPoseCorrectivesEditorController::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ControlRig);
	Collector.AddReferencedObject(SourceAnimInstance);
	Collector.AddReferencedObject(TargetAnimInstance);
};


USkeletalMesh* FPoseCorrectivesEditorController::GetSourceSkeletalMesh() const
{
	if (!Asset)
	{
		return nullptr;
	}

	return Asset->SourcePreviewMesh.Get();
}

USkeletalMesh* FPoseCorrectivesEditorController::GetTargetSkeletalMesh() const
{
	if (!Asset)
	{
		return nullptr;
	}

	return Asset->TargetMesh.Get();
}

void FPoseCorrectivesEditorController::PlayAnimationAsset(UAnimationAsset* AssetToPlay)
{
	if (AssetToPlay && SourceAnimInstance.Get())
	{
		ExitEditMode();

		SourceAnimInstance->SetAnimationAsset(AssetToPlay);
		PreviousAsset = AssetToPlay;		
	}
}

void FPoseCorrectivesEditorController::PlayPreviousAnimationAsset() const
{
	if (PreviousAsset)
	{
		SourceAnimInstance->SetAnimationAsset(PreviousAsset);
	}
}

void FPoseCorrectivesEditorController::EnterEditMode(const FName& CorrectiveName)
{
	SourceAnimInstance->StopAnim();
	SetRigHierachyToCorrective(CorrectiveName);
	
	CorrectiveInEditMode = CorrectiveName;
	SourceAnimInstance->SetUseCorrectiveSource(CorrectiveName);
	TargetAnimInstance->SetUseCorrectiveSource(CorrectiveName);
	
	TargetAnimInstance->SetUseControlRigInput(true);
	EnableControlRigInteraction(true);
}
	
void FPoseCorrectivesEditorController::ExitEditMode()
{
	if(ControlRig)
		ControlRig->GetHierarchy()->ResetPoseToInitial();

	SourceAnimInstance->StopUseCorrectiveSource();
	TargetAnimInstance->StopUseCorrectiveSource();

	TargetAnimInstance->SetUseControlRigInput(false);
	EnableControlRigInteraction(false);

	Asset->Modify();
}

void FPoseCorrectivesEditorController::HandleEditCorrective(const FName& CorrectiveName)
{
	EnterEditMode(CorrectiveName);
}

void FPoseCorrectivesEditorController::HandleNewCorrective()
{
	FName NewCorrectiveName = AddPose();
	EnterEditMode(NewCorrectiveName);

	CorrectivesViewer->OnCorrectivesAssetModified();
	CorrectivesViewer->HighlightCorrective(NewCorrectiveName);
}

void FPoseCorrectivesEditorController::HandleStopEditPose()
{
	Asset->AddCorrective(SourceSkelMeshComponent, TargetSkelMeshComponent, CorrectiveInEditMode);
	ExitEditMode();
	CorrectivesViewer->ClearHighlightedItems();
}

void FPoseCorrectivesEditorController::HandleCancelEditPose()
{
	ExitEditMode();
	CorrectivesViewer->ClearHighlightedItems();
}

void FPoseCorrectivesEditorController::UpdateCorrective()
{
	Asset->AddCorrective(SourceSkelMeshComponent, TargetSkelMeshComponent, CorrectiveInEditMode);
	SetRigHierachyToCorrective(CorrectiveInEditMode);
}

void FPoseCorrectivesEditorController::SetPoseCorrectivesGroupsView(TSharedPtr<SPoseCorrectivesGroups> InPoseCorrectivesGroupsView)
{
	PoseCorrectivesGroupsView = InPoseCorrectivesGroupsView;
}

void FPoseCorrectivesEditorController::SetCorrectivesViewer(TSharedPtr<SCorrectivesViewer> InCorrectivesViewer)
{
	CorrectivesViewer = InCorrectivesViewer;
}

bool FPoseCorrectivesEditorController::IsEditingPose() const
{
	return TargetAnimInstance->IsUsingControlRigInput();
}

bool FPoseCorrectivesEditorController::CanAddCorrective() const
{
	return !IsEditingPose() && TargetSkelMeshComponent && SourceSkelMeshComponent
		&& TargetSkelMeshComponent->GetSkinnedAsset() && SourceSkelMeshComponent->GetSkinnedAsset();
}

FName FPoseCorrectivesEditorController::AddPose()
{
	FScopedTransaction Transaction(LOCTEXT("Add Corrective", "Add Corrective"));

	// Get unique corrective name
	FName CorrectiveName = "Corrective_0";
	int32 NameIndex = 0;	
	while (Asset->GetCorrectiveNames().Contains(CorrectiveName))
	{
		NameIndex++;
		CorrectiveName = FName("Corrective_" + FString::FromInt(NameIndex));		
	}

	Asset->AddCorrective(SourceSkelMeshComponent, TargetSkelMeshComponent, CorrectiveName);

	auto GroupNames = Asset->GetGroupNames();
	if (!GroupNames.IsEmpty())
	{
		Asset->UpdateGroupForCorrective(CorrectiveName, GroupNames.Last());
	}

	return CorrectiveName;
}

void FPoseCorrectivesEditorController::HandleTargetMeshChanged()
{
	if (PoseCorrectivesGroupsView)
	{
		PoseCorrectivesGroupsView->HandleMeshChanged();
	}	
}

void FPoseCorrectivesEditorController::HandleGroupListChanged()
{
	CorrectivesViewer->OnCorrectivesAssetModified();
}

void FPoseCorrectivesEditorController::HandleGroupEdited(const FName& GroupName)
{
	Asset->UpdateCorrectivesByGroup(GroupName);
	Asset->Modify();
}

bool FPoseCorrectivesEditorController::HandleCorrectiveRenamed(const FName& OldName, const FName& NewName)
{
	bool ValidName = Asset->RenameCorrective(OldName, NewName);

	if (IsEditingPose() && CorrectiveInEditMode == OldName && ValidName)
	{
		CorrectiveInEditMode = NewName;
		SourceAnimInstance->SetUseCorrectiveSource(CorrectiveInEditMode);
		TargetAnimInstance->SetUseCorrectiveSource(CorrectiveInEditMode);
	}

	return ValidName;
}

FTransform FPoseCorrectivesEditorController::GetRigElementTransform(const FRigElementKey& InElement, bool bLocal, bool bOnDebugInstance) const
{
	if (Asset)
	{
		if (TObjectPtr<const UControlRigBlueprint> ControlRigBP = Cast<UControlRigBlueprint>(Asset->ControlRigBlueprint);
			ControlRigBP != nullptr)
		{
			return ControlRigBP->Hierarchy->GetGlobalTransform(InElement);
		}
	}

	return FTransform();
}

void FPoseCorrectivesEditorController::SetRigElementTransform(const FRigElementKey& InElement, const FTransform& InTransform, bool bLocal)
{
	
}

FCorrectivesEditMode* FPoseCorrectivesEditorController::GetControlRigEditMode()
{
	FEditorModeTools& EditorModeTools = EditorToolkit.Pin()->GetEditorModeManager();
	FCorrectivesEditMode* EditMode = static_cast<FCorrectivesEditMode*>(EditorModeTools.GetActiveMode(FCorrectivesEditMode::ModeName));
	return EditMode;
}

void FPoseCorrectivesEditorController::SetRigHierachyToCorrective(const FName& CorrectiveName)
{
	if (!ControlRig)
	{
		return;
	}

	FPoseCorrective* PoseCorrective = Asset->FindCorrective(CorrectiveName);
	if (PoseCorrective)
	{
		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		Hierarchy->ResetPoseToInitial();
		Hierarchy->ResetCurveValues();

		TArray<FName> BoneNames = Asset->GetBoneNames();
		for (int32 Index = 0; Index < BoneNames.Num(); Index++)
		{
			const FName& BoneName = BoneNames[Index];
			if (FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(FRigElementKey(BoneName, ERigElementType::Bone)))
			{
				FTransform LocalTransform = PoseCorrective->CorrectivePoseLocal[Index];
				Hierarchy->SetLocalTransform(BoneElement->GetIndex(), LocalTransform, true, false);
			}
		}

		TArray<FName> CurveNames = Asset->GetCurveNames();
		for (int32 Index = 0; Index < CurveNames.Num(); Index++)
		{
			float CurveValue = PoseCorrective->CorrectiveCurvesDelta[Index] + PoseCorrective->CurveData[Index];
			Hierarchy->SetCurveValue(FRigElementKey(CurveNames[Index], ERigElementType::Curve), CurveValue);
		}

		ControlRig->Execute(FRigUnit_InverseExecution::EventName);
	}
}

void FPoseCorrectivesEditorController::EnableControlRigInteraction(bool bEnableInteraction)
{
	if (!ControlRig)
	{
		return;
	}

	if (bEnableInteraction)
	{
		ControlRig->SetControlsVisible(true);
	}
	else
	{
		if (FCorrectivesEditMode* EditMode = GetControlRigEditMode())		
		{
			EditMode->ClearSelection();
		}

		ControlRig->SetControlsVisible(false);
	}
}
#undef LOCTEXT_NAMESPACE
