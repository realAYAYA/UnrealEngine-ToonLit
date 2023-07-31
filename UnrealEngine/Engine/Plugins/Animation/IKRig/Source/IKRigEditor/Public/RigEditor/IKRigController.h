// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "IKRigController.generated.h"

struct FIKRigInputSkeleton;
struct FReferenceSkeleton;
struct FIKRigSkeleton;
class UIKRigSolver;
class UIKRigDefinition;
class UIKRigEffectorGoal;
class USkeletalMesh;
class USkeleton;
struct FBoneChain;

/** A singleton (per-asset) class used to make modifications to a UIKRigDefinition asset.
 * Call the static GetIKRigController() function to get the controller for the asset you want to modify. */ 
UCLASS(config = Engine, hidecategories = UObject)
class IKRIGEDITOR_API UIKRigController : public UObject
{
	GENERATED_BODY()

public:

	/** Use this to get the controller for the given IKRig */
	static UIKRigController* GetIKRigController(UIKRigDefinition* InIKRigDefinition);
	
	/** Get the asset this controller controls. */
	UIKRigDefinition* GetAsset() const;
	/** Get unique asset integer ID as a name */
	FName GetAssetIDAsName() const;

	/** SKELETON
	 * 
	 */
	/** Sets the preview mesh to use, can optionally reinitialize the skeleton with bReImportBone=true.
	 * Returns true if the mesh was able to be set. False if it was incompatible for any reason. */
	bool SetSkeletalMesh(USkeletalMesh* SkeletalMesh, bool bTransact=false) const;
	/** Get read-access to the IKRig skeleton representation */
	const FIKRigSkeleton& GetIKRigSkeleton() const;
	/** Get the USkeleton asset this rig was initialized with */
	USkeleton* GetSkeleton() const;
	/** Include/exclude a bone from all the solvers. All bones are included by default. */
	void SetBoneExcluded(const FName& BoneName, const bool bExclude) const;
	/** Returns true if the given Bone is excluded, false otherwise. */
	bool GetBoneExcluded(const FName& BoneName) const;
	/** Get the global-space retarget pose transform of the given Bone. Can only be called AFTER skeleton is initialized. */
	FTransform GetRefPoseTransformOfBone(const FName& BoneName) const;
	/** END SKELETON */

	/** SOLVERS
	 * 
	 */
	/** Add a new solver of the given type to the bottom of the stack. Returns the stack index. */
	int32 AddSolver(TSubclassOf<UIKRigSolver> InSolverClass) const;
	/** Remove the solver at the given stack index. */
	void RemoveSolver(const int32 SolverIndex) const;
	/** Move the solver at the given index to the target index. */
	bool MoveSolverInStack(int32 SolverToMoveIndex, int32 TargetSolverIndex) const;
	/** Enabled/disable the given solver. */
	bool SetSolverEnabled(int32 SolverIndex, bool bIsEnabled) const;
	/** Get access to the given solver. */
	UIKRigSolver* GetSolver(int32 Index) const;
	/** Get the number of solvers in the stack. */
	int32 GetNumSolvers() const;
	/** Set the root bone on a given solver. (not all solvers support root bones, checks CanSetRootBone() first) */
	void SetRootBone(const FName& RootBoneName, int32 SolverIndex) const;
	/** Set the end bone on a given solver. (not all solvers require extra end bones, checks CanSetEndBone() first) */
	void SetEndBone(const FName& EndBoneName, int32 SolverIndex) const;
	/** Get read-only access to the array of solvers. */
	const TArray<UIKRigSolver*>& GetSolverArray() const;
	/** Get unique label for a given solver. Returns dash separated index and name like so, "0 - SolverName". */
	FString GetSolverUniqueName(int32 SolverIndex);
	/** END SOLVERS */

	/** GOALS
	 * 
	 */
	/** Add a new Goal associated with the given Bone. GoalName must be unique. Bones can have multiple Goals (rare). */
	UIKRigEffectorGoal* AddNewGoal(const FName& GoalName, const FName& BoneName) const;
	/** Remove the Goal by name. */
	bool RemoveGoal(const FName& GoalName) const;
	/** Rename a Goal. Returns new name, which may be different after being sanitized. Returns NAME_None if this fails.*/
	FName RenameGoal(const FName& OldName, const FName& PotentialNewName) const;
	/** Modify a Goal for a transaction. Returns true if Goal found.*/
	bool ModifyGoal(const FName& GoalName) const;
	/** The the Bone that the given Goal should be parented to / associated with. */
	bool SetGoalBone(const FName& GoalName, const FName& NewBoneName) const;
	/** The the Bone associated with the given Goal. */
	FName GetBoneForGoal(const FName& GoalName) const;
	/** The the Goal associated with the given Bone (may be null. */
	UIKRigEffectorGoal* GetGoalForBone(const FName& BoneName) const;
	/** Connect the given Goal to the given Solver. This creates an "Effector" with settings specific to this Solver.*/
	bool ConnectGoalToSolver(const UIKRigEffectorGoal& Goal, int32 SolverIndex) const;
	/** Disconnect the given Goal from the given Solver. This removes the Effector that associates the Goal with the Solver.*/
	bool DisconnectGoalFromSolver(const FName& GoalToRemove, int32 SolverIndex) const;
	/** Returns true if the given Goal is connected to the given Solver. False otherwise. */
	bool IsGoalConnectedToSolver(const FName& GoalName, int32 SolverIndex) const;
	/** Returns true if the given Goal is connected to ANY solver. False otherwise. */
	bool IsGoalConnectedToAnySolver(const FName& GoalName) const;
	/** Get the index of the given Goal in the list of Goals. */
	int32 GetGoalIndex(const FName& InGoalName, const ENameCase CompareMethod = ENameCase::IgnoreCase) const;
	/** Get the name of Goal at the given index. */
	FName GetGoalName(const int32& GoalIndex) const;
	/** Get access to the list of Goals. */
	TArray<UIKRigEffectorGoal*>& GetAllGoals() const;
	/** Get read-only access to the Goal at the given index. */
	const UIKRigEffectorGoal* GetGoal(int32 GoalIndex) const;
	/** Get read-write access to the Goal with the given name. */
	UIKRigEffectorGoal* GetGoal(const FName& GoalName) const;
	/** Get the UObject for the settings associated with the given Goal in the given Solver.
	 ** Solvers can define their own per-Goal settings depending on their needs. These are termed "Effectors". */
	UObject* GetGoalSettingsForSolver(const FName& GoalName, int32 SolverIndex) const;
	/** Get the global-space transform of the given Goal. This may be set by the user in the editor, or at runtime. */
	FTransform GetGoalCurrentTransform(const FName& GoalName) const;
	/** Set the Goal to the given transform. */
	void SetGoalCurrentTransform(const FName& GoalName, const FTransform& Transform) const;
	/** Reset all Goals back to their initial transforms. */
	void ResetGoalTransforms() const;
	/** Ensure that the given name adheres to required standards for Goal names (no special characters etc..)*/
	static void SanitizeGoalName(FString& InOutName);
	/** Add a suffix as needed to ensure the Goal name is unique */
	FName GetUniqueGoalName(const FName& NameToMakeUnique) const;
	
	/** END Goals */

	/** Bone Settings
	 * 
	 */
	/** Add settings to the given Bone/Solver. Does nothing if Bone already has settings in this Solver.*/
	void AddBoneSetting(const FName& BoneName, int32 SolverIndex) const;
	/** Remove settings for the given Bone/Solver. Does nothing if Bone doesn't have setting in this Solver.*/
	void RemoveBoneSetting(const FName& BoneName, int32 SolverIndex) const;
	/** Returns true if this Bone can have settings in the given Solver. */
	bool CanAddBoneSetting(const FName& BoneName, int32 SolverIndex) const;
	/** Returns true if settings for this Bone can be removed from the given Solver. */
	bool CanRemoveBoneSetting(const FName& BoneName, int32 SolverIndex) const;
	/** Get the generic (Solver-specific) Bone settings UObject for this Bone in the given Solver.*/
	UObject* GetSettingsForBone(const FName& BoneName, int32 SolverIndex) const;
	/** Returns true if the given Bone has any settings in any Solver. */
	bool DoesBoneHaveSettings(const FName& BoneName) const;
	/** END Bone Settings */

	/** Retargeting Options and Retarget Bone Chains
	 * 
	 */
	/** Add a Chain with the given Name and Start/End bones. Returns newly created chain name. */
	FName AddRetargetChain(const FBoneChain& BoneChain) const;
	/** Remove a Chain with the given name. Returns true if a Chain was removed. */
	bool RemoveRetargetChain(const FName& ChainName) const;
	/** Renamed the given Chain. Returns the new name (same as old if unsuccessful). */
	FName RenameRetargetChain(const FName& ChainName, const FName& NewChainName) const;
	/** Set the Start Bone for the given Chain. Returns true if operation was successful. */
	bool SetRetargetChainStartBone(const FName& ChainName, const FName& StartBoneName) const;
	/** Set the End Bone for the given Chain. Returns true if operation was successful. */
	bool SetRetargetChainEndBone(const FName& ChainName, const FName& EndBoneName) const;
	/** Set the Goal for the given Chain. Returns true if operation was successful. */
	bool SetRetargetChainGoal(const FName& ChainName, const FName& GoalName) const;
	/** Get the Goal name for the given Chain. */
	FName GetRetargetChainGoal(const FName& ChainName) const;
	/** Get the End Bone name for the given Chain. */
    FName GetRetargetChainStartBone(const FName& ChainName) const;
    /** Get the Start Bone name for the given Chain. */
    FName GetRetargetChainEndBone(const FName& ChainName) const;
	/** Get read-only access to a single retarget chain with the given name */
	const FBoneChain* GetRetargetChainByName(const FName& ChainName) const;
	/** Get read-only access to the list of Chains. */
	const TArray<FBoneChain>& GetRetargetChains() const;
	/** Set the Root Bone of the retargeting (can only be one). */
	void SetRetargetRoot(const FName& RootBoneName) const;
	/** Get the name of the Root Bone of the retargeting (can only be one). */
	FName GetRetargetRoot() const;
	/** Sorts the Chains from Root to tip based on the Start Bone of each Chain. */
	void SortRetargetChains() const;
	/** Make unique name for a retargeting bone chain. Adds a numbered suffix to make it unique.*/
	FName GetUniqueRetargetChainName(const FName& NameToMakeUnique) const;
	/** Returns true if this is a valid chain. Produces array of bone indices between start and end (inclusive).
	 * Optionally provide a runtime skeleton from an IKRigProcessor to get indices for a running instance (otherwise uses stored hierarchy in asset)*/
	bool ValidateChain(
		const FName& ChainName,
		const FIKRigSkeleton* OptionalSkeleton,
		TSet<int32>& OutChainIndices) const;
	/** Get the name of the retarget chain that contains the given Bone. Returns NAME_None if Bone not in a Chain. */
	FName GetRetargetChainFromBone(const FName& BoneName, const FIKRigSkeleton* OptionalSkeleton) const;
	/** Get the name of the retarget chain that contains the given Goal. Returns NAME_None if Goal not in a Chain. */
	FName GetRetargetChainFromGoal(const FName& GoalName) const;
	/** END retarget chains */

	// force all currently connected processors to reinitialize using latest asset state
	void BroadcastNeedsReinitialized() const
	{
		IKRigNeedsInitialized.Broadcast(GetAsset());
	}

private:

	/** Called whenever the rig is modified in such a way that would require re-initialization by dependent systems.*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnIKRigNeedsInitialized, UIKRigDefinition*);
	FOnIKRigNeedsInitialized IKRigNeedsInitialized;

	/** Called whenever a retarget chain is added.*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRetargetChainAdded, UIKRigDefinition*);
	FOnRetargetChainAdded RetargetChainAdded;

	/** Called whenever a retarget chain is removed.*/
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRetargetChainRemoved, UIKRigDefinition*, const FName& /*chain name*/);
	FOnRetargetChainRemoved RetargetChainRemoved;

	/** Called whenever a retarget chain is renamed.*/
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRetargetChainRenamed, UIKRigDefinition*, FName /*old name*/, FName /*new name*/);
	FOnRetargetChainRenamed RetargetChainRenamed;

public:
	
	FOnIKRigNeedsInitialized& OnIKRigNeedsInitialized(){ return IKRigNeedsInitialized; };
	FOnRetargetChainAdded& OnRetargetChainAdded(){ return RetargetChainAdded; };
	FOnRetargetChainRemoved& OnRetargetChainRemoved(){ return RetargetChainRemoved; };
	FOnRetargetChainRenamed& OnRetargetChainRenamed(){ return RetargetChainRenamed; };

private:

	/** The actual IKRigDefinition asset that this Controller modifies. */
	UPROPERTY(transient)
	TObjectPtr<UIKRigDefinition> Asset = nullptr;

	/** Lazy-generated map of Controllers to IK Rig Assets. Avoids duplicate controllers. */
	static TMap<UIKRigDefinition*, UIKRigController*> AssetToControllerMap;

	// broadcast changes within the asset goals array
	void BroadcastGoalsChange() const;
	
	friend class UIKRigDefinition;
};
