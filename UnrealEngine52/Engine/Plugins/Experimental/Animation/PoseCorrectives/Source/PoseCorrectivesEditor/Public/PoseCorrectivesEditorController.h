// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PoseCorrectivesEditor.h"
#include "IPersonaToolkit.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "Input/Reply.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"
#include "Rigs/RigHierarchyDefines.h"


class FCorrectivesEditMode;
class FPoseCorrectivesEditorToolkit;
class IDetailsView;
class UControlRig;
class UControlRigComponent;
class UControlRigSkeletalMeshComponent;
class UPoseCorrectivesAnimInstance;
class UPoseCorrectivesAnimSourceInstance;
class UPoseCorrectivesAsset;


class FPoseCorrectivesEditorController : public TSharedFromThis<FPoseCorrectivesEditorController>, FGCObject
{
public:

	/** Initialize the editor */
	void Initialize(TSharedPtr<FPoseCorrectivesEditorToolkit> InEditorToolkit, UPoseCorrectivesAsset* InAsset);

	void InitializeTargetControlRigBP();
	void UninitializeTargetControlRigBP();
	
	/** Preview scene to be supplied by IHasPersonaToolkit::GetPersonaToolkit */
	TSharedPtr<IPersonaToolkit> PersonaToolkit;

	/** the persona toolkit */
	TWeakPtr<FPoseCorrectivesEditorToolkit> EditorToolkit;

	/** The asset */
	UPoseCorrectivesAsset* Asset = nullptr;	

	/** viewport skeletal mesh */
	UDebugSkelMeshComponent* SourceSkelMeshComponent;
	UDebugSkelMeshComponent* TargetSkelMeshComponent;

	/** viewport anim instance */
	TObjectPtr<UPoseCorrectivesAnimSourceInstance> SourceAnimInstance;
	TObjectPtr<UPoseCorrectivesAnimInstance> TargetAnimInstance;	
	
	/** asset properties tab */
	TSharedPtr<IDetailsView> DetailsView;

	/** get the source skeletal mesh we are copying FROM */
	USkeletalMesh* GetSourceSkeletalMesh() const;
	/** get the target skeletal mesh we are copying TO */
	USkeletalMesh* GetTargetSkeletalMesh() const;

	/** Sequence Browser**/
	void PlayAnimationAsset(UAnimationAsset* AssetToPlay);
	void PlayPreviousAnimationAsset() const;
	UAnimationAsset* PreviousAsset = nullptr;
	/** END Sequence Browser */

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override {return "PoseCorrectivesEditorController";};

	void HandleEditCorrective(const FName& CorrectiveName);
	void HandleNewCorrective();
	void HandleStopEditPose();
	void HandleCancelEditPose();
	void UpdateCorrective();

	bool IsEditingPose() const;
	bool CanAddCorrective() const;

	void HandleTargetMeshChanged();
	void HandleGroupListChanged();
	void HandleGroupEdited(const FName& GroupName);
	bool HandleCorrectiveRenamed(const FName& OldName, const FName& NewName);

	void SetPoseCorrectivesGroupsView(TSharedPtr<class SPoseCorrectivesGroups> InPoseCorrectivesGroupsView);
	void SetCorrectivesViewer(TSharedPtr<class SCorrectivesViewer> InCorrectivesViewer);

	FTransform GetRigElementTransform(const FRigElementKey& InElement, bool bLocal, bool bOnDebugInstance) const;
	void SetRigElementTransform(const FRigElementKey& InElement, const FTransform& InTransform, bool bLocal);

private:
	FName AddPose();
	
	void EnterEditMode(const FName& CorrectiveName);
	void ExitEditMode();

	FCorrectivesEditMode* GetControlRigEditMode();
	void SetRigHierachyToCorrective(const FName& CorrectiveName);
	void EnableControlRigInteraction(bool bEnableInteraction);

	FName CorrectiveInEditMode;

	TSharedPtr<SPoseCorrectivesGroups> PoseCorrectivesGroupsView;
	TSharedPtr<SCorrectivesViewer> CorrectivesViewer;
	
	/* Editor Control rig*/
	UControlRig* ControlRig = nullptr;
};
