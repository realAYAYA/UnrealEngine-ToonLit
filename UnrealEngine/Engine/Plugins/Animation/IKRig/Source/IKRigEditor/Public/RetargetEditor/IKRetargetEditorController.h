// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IKRetargetDetails.h"
#include "IKRetargetPoseExporter.h"
#include "IPersonaToolkit.h"
#include "SIKRetargetAssetBrowser.h"
#include "UObject/ObjectPtr.h"
#include "Input/Reply.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

enum class ERetargetSourceOrTarget : uint8;
class SIKRetargetHierarchy;
class SIKRigOutputLog;
class UIKRetargetProcessor;
class SIKRetargetChainMapList;
class UIKRetargetAnimInstance;
class FIKRetargetEditor;
class UDebugSkelMeshComponent;
class UIKRigDefinition;
class UIKRetargeterController;
class UIKRetargetBoneDetails;
struct FRetargetSkeleton;


// retarget editor modes
enum class ERetargeterOutputMode : uint8
{
	RunRetarget,		// output the retargeted target pose
	ShowRetargetPose,	// output the retarget pose for viewing purposes
	EditRetargetPose,	// allow editing the retarget pose
};

enum class ESelectionEdit : uint8
{
	Add,	// add to selection set
	Remove,	// remove from selection
	Replace	// replace selection entirely
};

enum class ERetargetSelectionType : uint8
{
	BONE,
	CHAIN,
	MESH,
	ROOT,
	NONE
};

struct FBoundIKRig
{
	FBoundIKRig(UIKRigDefinition* InIKRig, const FIKRetargetEditorController& InController);
	void UnBind() const;
	
	UIKRigDefinition* IKRig;
	FDelegateHandle ReInitIKDelegateHandle;
	FDelegateHandle AddedChainDelegateHandle;
	FDelegateHandle RenameChainDelegateHandle;
	FDelegateHandle RemoveChainDelegateHandle;
};

/** a home for cross-widget communication to synchronize state across all tabs and viewport */
class FIKRetargetEditorController : public TSharedFromThis<FIKRetargetEditorController>, FGCObject
{
public:

	virtual ~FIKRetargetEditorController() override {};

	/** Initialize the editor */
	void Initialize(TSharedPtr<FIKRetargetEditor> InEditor, UIKRetargeter* InAsset);
	/** Close the editor */
	void Close();
	/** Bind callbacks to this IK Rig */
	void BindToIKRigAssets(UIKRetargeter* InAsset);
	/** callback when IK Rig asset requires reinitialization */
	void HandleIKRigNeedsInitialized(UIKRigDefinition* ModifiedIKRig) const;
	/** callback when IK Rig asset's retarget chain's have been added or removed*/
	void HandleRetargetChainAdded(UIKRigDefinition* ModifiedIKRig) const;
	/** callback when IK Rig asset's retarget chain has been renamed */
	void HandleRetargetChainRenamed(UIKRigDefinition* ModifiedIKRig, FName OldName, FName NewName) const;
	/** callback when IK Rig asset's retarget chain has been removed */
	void HandleRetargetChainRemoved(UIKRigDefinition* ModifiedIKRig, const FName& InChainRemoved) const;
	/** callback when IK Retargeter asset requires reinitialization */
	void HandleRetargeterNeedsInitialized(UIKRetargeter* Retargeter) const;
	FDelegateHandle RetargeterReInitDelegateHandle;
	
	/** all modifications to the data model should go through this controller */
	TObjectPtr<UIKRetargeterController> AssetController;

	/** the persona toolkit */
	TWeakPtr<FIKRetargetEditor> Editor;

	/** import / export retarget poses */
	TSharedPtr<FIKRetargetPoseExporter> PoseExporter;

	/** viewport skeletal mesh */
	UDebugSkelMeshComponent* GetSkeletalMeshComponent(const ERetargetSourceOrTarget& SourceOrTarget) const;
	UDebugSkelMeshComponent* SourceSkelMeshComponent;
	UDebugSkelMeshComponent* TargetSkelMeshComponent;

	/** viewport anim instance */
	UIKRetargetAnimInstance* GetAnimInstance(const ERetargetSourceOrTarget& SourceOrTarget) const;
	UPROPERTY(transient, NonTransactional)
	TWeakObjectPtr<class UIKRetargetAnimInstance> SourceAnimInstance;
	UPROPERTY(transient, NonTransactional)
	TWeakObjectPtr<class UIKRetargetAnimInstance> TargetAnimInstance;
	
	/** store pointers to various tabs of UI,
	 * have to manage access to these because they can be null if the tabs are closed */
	void SetDetailsView(TSharedPtr<IDetailsView> InDetailsView) { DetailsView = InDetailsView; };
	void SetChainsView(TSharedPtr<SIKRetargetChainMapList> InChainsView) { ChainsView = InChainsView; };
	void SetAssetBrowserView(TSharedPtr<SIKRetargetAssetBrowser> InAssetBrowserView) { AssetBrowserView = InAssetBrowserView; };
	void SetOutputLogView(TSharedPtr<SIKRigOutputLog> InOutputLogView) { OutputLogView = InOutputLogView; };
	void SetHierarchyView(TSharedPtr<SIKRetargetHierarchy> InHierarchyView) { HierarchyView = InHierarchyView; };
	
	/** force refresh all views in the editor */
	void RefreshAllViews() const;
	void RefreshDetailsView() const;
	void RefreshChainsView() const;
	void RefreshAssetBrowserView() const;
	void RefreshHierarchyView() const;
	void RefreshPoseList() const;
	void SetDetailsObject(UObject* DetailsObject) const;
	void SetDetailsObjects(const TArray<UObject*>& DetailsObjects) const;

	/** clear the output log */
	void ClearOutputLog() const;

	/** get the USkeletalMesh we are transferring animation between (either source or target)*/
	USkeletalMesh* GetSkeletalMesh(const ERetargetSourceOrTarget SourceOrTarget) const;
	/** get the USkeleton we are transferring animation between (either source or target)*/
	const USkeleton* GetSkeleton(const ERetargetSourceOrTarget SourceOrTarget) const;
	/** get currently edited debug skeletal mesh */
	UDebugSkelMeshComponent* GetEditedSkeletalMesh() const;
	/** get the currently edited retarget skeleton */
	const FRetargetSkeleton& GetCurrentlyEditedSkeleton(const UIKRetargetProcessor& Processor) const;
	
	/** get world space pose of a bone (with component scale / offset applied) */
	FTransform GetGlobalRetargetPoseOfBone(
		const ERetargetSourceOrTarget SourceOrTarget,
		const int32& BoneIndex,
		const float& Scale,
		const FVector& Offset) const;
	
	/** get world space positions of all immediate children of bone (with component scale / offset applied) */
	static void GetGlobalRetargetPoseOfImmediateChildren(
		const FRetargetSkeleton& RetargetSkeleton,
		const int32& BoneIndex,
		const float& Scale,
		const FVector& Offset,
		TArray<int32>& OutChildrenIndices,
		TArray<FVector>& OutChildrenPositions);

	/** get the retargeter that is running in the viewport (which is a duplicate of the source asset) */
	UIKRetargetProcessor* GetRetargetProcessor() const;
	/** Reset the planting state of the IK (when scrubbing or animation loops over) */
	void ResetIKPlantingState() const;

	/** Sequence Browser**/
	void PlayAnimationAsset(UAnimationAsset* AssetToPlay);
	void StopPlayback();
	void PausePlayback();
	void ResumePlayback();
	UAnimationAsset* AnimThatWasPlaying = nullptr;
	float TimeWhenPaused = 0.0f;
	bool bWasPlayingAnim = false;
	/** END Sequence Browser */

	/** Set viewport / editor tool mode */
	void SetRetargeterMode(ERetargeterOutputMode Mode);
	ERetargeterOutputMode GetRetargeterMode() const { return OutputMode; }
	ERetargeterOutputMode PreviousMode;
	float GetRetargetPoseAmount() const;
	void SetRetargetPoseAmount(float InValue);
	/** END viewport / editor tool mode */
	
	/** general editor mode can be either viewing/editing source or target */
	ERetargetSourceOrTarget GetSourceOrTarget() const { return CurrentlyEditingSourceOrTarget; };
	void SetSourceOrTargetMode(ERetargetSourceOrTarget SourceOrTarget);

	/** ------------------------- SELECTION -----------------------------*/

	/** SELECTION - BONES (viewport or hierarchy view) */
	void EditBoneSelection(
		const TArray<FName>& InBoneNames,
		ESelectionEdit EditMode,
		const bool bFromHierarchyView = false);
	const TArray<FName>& GetSelectedBones() const {return SelectedBoneNames[CurrentlyEditingSourceOrTarget]; };
	/* END bone selection */
	
	/** SELECTION - MESHES (viewport view) */
	void SetSelectedMesh(UPrimitiveComponent* InComponent);
	UPrimitiveComponent* GetSelectedMesh() const;
	void AddOffsetToMeshComponent(const FVector& Offset, USceneComponent* MeshComponent) const;
	/* END mesh selection */

	/** SELECTION - CHAINS (viewport or chains view) */
	void EditChainSelection(
		const TArray<FName>& InChainNames,
		ESelectionEdit EditMode,
		const bool bFromChainsView);
	const TArray<FName>& GetSelectedChains() const {return SelectedChains; };
	/* END chain selection */

	void SetRootSelected(const bool bIsSelected);
	bool GetRootSelected() const { return bIsRootSelected; };
	
	void ClearSelection(const bool bKeepBoneSelection=false);
	ERetargetSelectionType GetLastSelectedItemType() const { return LastSelectedItem; };

	/** ------------------------- END SELECTION -----------------------------*/

	/** determine if bone in the specified skeleton is part of the retarget (in a mapped chain) */
	bool IsBoneRetargeted(const FName& BoneName, ERetargetSourceOrTarget SourceOrTarget) const;
	/** get the name of the chain that contains this bone */
	FName GetChainNameFromBone(const FName& BoneName, ERetargetSourceOrTarget SourceOrTarget) const;

	/** factory to get/create bone details UObject */
	TObjectPtr<UIKRetargetBoneDetails> GetOrCreateBoneDetailsObject(const FName& BoneName);

	/** ------------------------- RETARGET POSES -----------------------------*/
	
	/** go to retarget pose */
	FReply HandleShowRetargetPose();
	bool CanShowRetargetPose() const;
	bool IsShowingRetargetPose() const;
	
	/** toggle current retarget pose */
	TArray<TSharedPtr<FName>> PoseNames;
	FText GetCurrentPoseName() const;
	void OnPoseSelected(TSharedPtr<FName> InPoseName, ESelectInfo::Type SelectInfo) const;
	
	/** edit retarget poses */
	void HandleEditPose();
	bool CanEditPose() const;
	bool IsEditingPose() const;

	/** reset retarget pose */
	void HandleResetAllBones() const;
	void HandleResetSelectedBones() const;
	void HandleResetSelectedAndChildrenBones() const;
	bool CanResetSelected() const;

	/** create new retarget pose */
	void HandleNewPose();
	bool CanCreatePose() const;
	FReply CreateNewPose() const;
	TSharedPtr<SWindow> NewPoseWindow;
	TSharedPtr<SEditableTextBox> NewPoseEditableText;

	/** duplicate current retarget pose */
	void HandleDuplicatePose();
	FReply CreateDuplicatePose() const;

	/** delete retarget pose */
	void HandleDeletePose();
	bool CanDeletePose() const;

	/** rename retarget pose */
	void HandleRenamePose();
	FReply RenamePose() const;
	bool CanRenamePose() const;
	TSharedPtr<SWindow> RenamePoseWindow;
	TSharedPtr<SEditableTextBox> NewNameEditableText;

	/* detect and auto-fix retarget pose that causes root height to be on the ground */
	void FixZeroHeightRetargetRoot(ERetargetSourceOrTarget SourceOrTarget) const;
	
	/** ------------------------- END RETARGET POSES -----------------------------*/

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("Retarget Editor"); };
	/** END FGCObject interface */

	/** render the skeleton's in the viewport (either source or target) */
	void RenderSkeleton(FPrimitiveDrawInterface* PDI, ERetargetSourceOrTarget SourceOrTarget) const;

private:

	/** modal dialog to ask user if they want to fix root bones that are "on the ground" */
	bool PromptToFixRootHeight(ERetargetSourceOrTarget SourceOrTarget) const;
	bool bAskedToFixRoot = false;

	/** asset properties tab */
	TSharedPtr<IDetailsView> DetailsView;
	/** chain list view */
	TSharedPtr<SIKRetargetChainMapList> ChainsView;
	/** asset browser view */
	TSharedPtr<SIKRetargetAssetBrowser> AssetBrowserView;
	/** output log view */
	TSharedPtr<SIKRigOutputLog> OutputLogView;
	/** hierarchy view */
	TSharedPtr<SIKRetargetHierarchy> HierarchyView;

	/** the current output mode of the retargeter */
	ERetargeterOutputMode OutputMode;
	/** slider value to blend between reference pose and retarget pose */
	float RetargetPosePreviewBlend = 1.0f;
	
	/** which skeleton are we editing / viewing? */
	ERetargetSourceOrTarget CurrentlyEditingSourceOrTarget;

	/** current selection set */
	bool bIsRootSelected;
	UPrimitiveComponent* SelectedMesh;
	TArray<FName> SelectedChains;
	TMap<ERetargetSourceOrTarget, TArray<FName>> SelectedBoneNames;
	ERetargetSelectionType LastSelectedItem;
	UPROPERTY()
	TMap<FName,TObjectPtr<UIKRetargetBoneDetails>> AllBoneDetails;

	/** ik rigs bound to this editor (will receive callbacks when requiring reinitialization*/
	TArray<FBoundIKRig> BoundIKRigs;
};

