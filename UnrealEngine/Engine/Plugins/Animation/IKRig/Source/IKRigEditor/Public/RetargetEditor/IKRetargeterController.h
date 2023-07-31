// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "IKRetargeterController.generated.h"

struct FIKRetargetPose;
enum class ERetargetSourceOrTarget : uint8;
class FIKRetargetEditorController;
class URetargetChainSettings;
class UIKRigDefinition;
class UIKRetargeter;
class USkeletalMesh;

/** A singleton (per-asset) class used to make modifications to a UIKRetargeter asset.
* Call the static GetController(UIKRetargeter* Asset) function to get the controller for the asset you want to modify. */ 
UCLASS(config = Engine, hidecategories = UObject)
class IKRIGEDITOR_API UIKRetargeterController : public UObject
{
	GENERATED_BODY()

public:

	/** Use this to get the controller for the given retargeter asset */
	static UIKRetargeterController* GetController(UIKRetargeter* InRetargeterAsset);
	/** Get access to the retargeter asset.
	 *@warning Do not make modifications to the asset directly. Use this API instead. */
	UIKRetargeter* GetAsset() const;

	/** SOURCE / TARGET
	* 
	*/
	/** Set the IK Rig to use as the source (to copy animation FROM) */
	void SetSourceIKRig(UIKRigDefinition* SourceIKRig);
	/** Get the preview skeletal mesh */
	USkeletalMesh* GetPreviewMesh(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get either source or target IK Rig */
	const UIKRigDefinition* GetIKRig(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get either source or target IK Rig */
	UIKRigDefinition* GetIKRigWriteable(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Set the SOURCE or TARGET preview mesh based on the mesh in the corresponding IK Rig asset */
	void OnIKRigChanged(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get if we've already asked to fix the root height for the given skeletal mesh */
	bool GetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh) const;
	/** Set if we've asked to fix the root height for the given skeletal mesh */
	void SetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh, bool InAsked) const;
	/** Get name of the Root bone used for retargeting. */
	FName GetRetargetRootBone(const ERetargetSourceOrTarget& SourceOrTarget) const;

	/** RETARGET CHAIN MAPPING
	* 
	*/
	/** Get names of all the bone chains. */
	void GetChainNames(const ERetargetSourceOrTarget& SourceOrTarget, TArray<FName>& OutNames) const;
	/** Remove invalid chain mappings (no longer existing in currently referenced source/target IK Rig assets) */
	void CleanChainMapping(const bool bForceReinitialization=true) const;
	/** Use fuzzy string search to find "best" Source chain to map to each Target chain */
	void AutoMapChains() const;
	/** Callback when IK Rig chain is added or removed. */
	void OnRetargetChainAdded(UIKRigDefinition* IKRig) const;
	/** Callback when IK Rig chain is renamed. Retains existing mappings using the new name */
	void OnRetargetChainRenamed(UIKRigDefinition* IKRig, FName OldChainName, FName NewChainName) const;
	/** Callback when IK Rig chain is removed. */
	void OnRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const;
	/** Set the source chain to map to a given target chain */
	void SetSourceChainForTargetChain(URetargetChainSettings* ChainMap, FName SourceChainToMapTo) const;
	/** Get read-only access to the list of chain mappings */
	const TArray<TObjectPtr<URetargetChainSettings>>& GetChainMappings() const;
	/** Get read-only acces to the chain settings associated with a given target chain */
	const URetargetChainSettings* GetChainMappingByTargetChainName(const FName& TargetChainName) const;
	/** Get the name of the goal assigned to a chain */
	FName GetChainGoal(const TObjectPtr<URetargetChainSettings> ChainSettings) const;
	/** Get whether the given chain's IK goal is connected to a solver */
	bool IsChainGoalConnectedToASolver(const FName& GoalName) const;
	/** END RETARGET CHAIN MAPPING */

	/** RETARGET POSE EDITING
	 * 
	 */
	/** Remove bones from retarget poses that are no longer in skeleton */
	void CleanPoseLists(const bool bForceReinitialization=true) const;
	/** Remove bones from retarget poses that are no longer in skeleton */
	void CleanPoseList(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Add new retarget pose. */
	void AddRetargetPose(
		const FName& NewPoseName,
		const FIKRetargetPose* ToDuplicate,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Rename current retarget pose. */
	void RenameCurrentRetargetPose(
		const FName& NewPoseName,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Remove a retarget pose. */
	void RemoveRetargetPose(
	const FName& PoseToRemove, 
	const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Reset a retarget pose for the specified bones.
	 *If BonesToReset is Empty, will removes all stored deltas, returning pose to reference pose */
	void ResetRetargetPose(
		const FName& PoseToReset,
		const TArray<FName>& BonesToReset,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get the current retarget pose */
    FName GetCurrentRetargetPoseName(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Change which retarget pose is used by the retargeter at runtime */
	void SetCurrentRetargetPose(FName CurrentPose, const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get access to array of retarget poses */
	TMap<FName, FIKRetargetPose>& GetRetargetPoses(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get the current retarget pose */
	FIKRetargetPose& GetCurrentRetargetPose(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Set a delta rotation for a given bone for the current retarget pose (used in Edit Mode in the retarget editor) */
	void SetRotationOffsetForRetargetPoseBone(
		const FName& BoneName,
		const FQuat& RotationOffset,
		const ERetargetSourceOrTarget& SkeletonMode) const;
	/** Get a delta rotation for a given bone for the current retarget pose (used in Edit Mode in the retarget editor) */
	FQuat GetRotationOffsetForRetargetPoseBone(
		const FName& BoneName,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Add a delta translation to the root bone (used in Edit Mode in the retarget editor) */
	void AddTranslationOffsetToRetargetRootBone(
		const FVector& TranslationOffset,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Add a numbered suffix to the given pose name to make it unique. */
	FName MakePoseNameUnique(
		const FString& PoseName,
		const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** END RETARGET POSE EDITING */

private:
	
	/** Called whenever the retargeter is modified in such a way that would require re-initialization by dependent systems.*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRetargeterNeedsInitialized, UIKRetargeter*);
	FOnRetargeterNeedsInitialized RetargeterNeedsInitialized;
	
public:
	
	FOnRetargeterNeedsInitialized& OnRetargeterNeedsInitialized(){ return RetargeterNeedsInitialized; };

	void BroadcastNeedsReinitialized() const
	{
		RetargeterNeedsInitialized.Broadcast(GetAsset());
	}
	
private:

	URetargetChainSettings* GetChainMap(const FName& TargetChainName) const;

	/** Sort the Asset ChainMapping based on the StartBone of the target chains. */
	void SortChainMapping() const;

	/** The actual asset that this Controller modifies. */
	TObjectPtr<UIKRetargeter> Asset = nullptr;

	/** The editor controller for this asset. */
	TSharedPtr<FIKRetargetEditorController> EditorController = nullptr;
};
