// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RetargetEditor/IKRetargeterPoseGenerator.h"
#include "Retargeter/IKRetargeter.h"

#include "IKRetargeterController.generated.h"

struct FRetargetGlobalSettings;
struct FTargetRootSettings;
struct FTargetChainSettings;
struct FIKRetargetPose;
enum class ERetargetSourceOrTarget : uint8;
enum class ERetargetAutoAlignMethod : uint8;
class FIKRetargetEditorController;
class URetargetChainSettings;
class UIKRigDefinition;
class UIKRetargeter;
class USkeletalMesh;

UENUM()
enum class EAutoMapChainType : uint8
{
	Exact, // map chains that have exactly the same name (case insensitive)
	Fuzzy, // map chains to the chain with the closest name (levenshtein distance)
	Clear, // clear all mappings, set them to None
};

// A stateless singleton (1-per-asset) class used to make modifications to a UIKRetargeter asset.
// Use UIKRetargeter.GetController() to get the controller for the asset you want to modify.  
UCLASS(config = Engine, hidecategories = UObject)
class IKRIGEDITOR_API UIKRetargeterController : public UObject
{
	GENERATED_BODY()

public:

	// UObject
	virtual void PostInitProperties() override;
	
	// Get access to the retargeter asset.
	// Warning: Do not make modifications to the asset directly. Use this API instead. 
	UIKRetargeter* GetAsset() const;
	TObjectPtr<UIKRetargeter>& GetAssetPtr();
  
private:
	
	// The actual asset that this Controller modifies. This is the only field this class should have.
	TObjectPtr<UIKRetargeter> Asset = nullptr;

	//
	// GENERAL PUBLIC/SCRIPTING API
	//

public:
	
	/** Use this to get the controller for the given retargeter asset */
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	static UIKRetargeterController* GetController(const UIKRetargeter* InRetargeterAsset);
	
	// Set the IK Rig to use as the source or target (to copy animation FROM/TO) 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	void SetIKRig(const ERetargetSourceOrTarget SourceOrTarget, UIKRigDefinition* IKRig) const;
	
	// Get either source or target IK Rig 
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	const UIKRigDefinition* GetIKRig(const ERetargetSourceOrTarget SourceOrTarget) const;

	// Set the preview skeletal mesh for either source or target
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	void SetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget, USkeletalMesh* InPreviewMesh) const;
	
	// Get the preview skeletal mesh
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	USkeletalMesh* GetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget) const;

	//
	// GET/SET SETTINGS PUBLIC/SCRIPTING API
	//
	
	// Get a copy of the retarget root settings for this asset.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	FTargetRootSettings GetRootSettings() const;
	
	// Set the retarget root settings for this asset.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	void SetRootSettings(const FTargetRootSettings& RootSettings) const;

	// Get a copy of the global settings for this asset.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	FRetargetGlobalSettings GetGlobalSettings() const;

	// Get a copy of the global settings for this asset.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	void SetGlobalSettings(const FRetargetGlobalSettings& GlobalSettings) const;

	// Get a copy of the settings for the target chain by name.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	FTargetChainSettings GetRetargetChainSettings(const FName& TargetChainName) const;

	// Set the settings for the target chain by name. Returns true if the chain settings were applied, false otherwise.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	bool SetRetargetChainSettings(const FName& TargetChainName, const FTargetChainSettings& Settings) const;

	//
	// RETARGET OPS PUBLIC/SCRIPTING API
	//
	
	// Add a new retarget op of the given type to the bottom of the stack. Returns the stack index.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=RetargetOps)
	int32 AddRetargetOp(TSubclassOf<URetargetOpBase> InOpClass) const;

	// Remove the retarget op at the given stack index. 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=RetargetOps)
	bool RemoveRetargetOp(const int32 OpIndex) const;

	// Remove all ops in the stack.
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	bool RemoveAllOps() const;

	// Get access to the given retarget operation. 
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	URetargetOpBase* GetRetargetOpAtIndex(int32 Index) const;

	// Get access to the given retarget operation. 
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	int32 GetIndexOfRetargetOp(URetargetOpBase* RetargetOp) const;

	// Get the number of Ops in the stack.
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	int32 GetNumRetargetOps() const;

	// Move the retarget op at the given index to the target index. 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=RetargetOps)
	bool MoveRetargetOpInStack(int32 OpToMoveIndex, int32 TargetIndex) const;

	// Set enabled/disabled status of the given retarget operation. 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=RetargetOps)
	bool SetRetargetOpEnabled(int32 RetargetOpIndex, bool bIsEnabled) const;

	// Get enabled status of the given Op. 
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	bool GetRetargetOpEnabled(int32 RetargetOpIndex) const;

	//
	// GENERAL C++ ONLY API
	//

	// Ensures all internal data is compatible with assigned meshes and ready to edit.
	// - Removes bones from retarget poses that are no longer in skeleton
	// - Removes chain settings for chains that are no longer in target IK Rig
	void CleanAsset() const;
	
	// Get either source or target IK Rig 
	UIKRigDefinition* GetIKRigWriteable(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Get if we've already asked to fix the root height for the given skeletal mesh 
	bool GetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh) const;
	
	// Set if we've asked to fix the root height for the given skeletal mesh 
	void SetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh, bool InAsked) const;
	
	// Get name of the Root bone used for retargeting. 
	FName GetRetargetRootBone(const ERetargetSourceOrTarget SourceOrTarget) const;

	//
	// RETARGET CHAIN MAPPING PUBLIC/SCRIPTING API
	//
	
	// Use fuzzy string search to find "best" Source chain to map to each Target chain
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	void AutoMapChains(const EAutoMapChainType AutoMapType, const bool bForceRemap) const;

	// Assign a source chain to the given target chain. Animation will be copied from the source to the target.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	bool SetSourceChain(FName SourceChainName, FName TargetChainName) const;

	// Get the name of the source chain mapped to a given target chain (the chain animation is copied FROM).
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	FName GetSourceChain(const FName& TargetChainName) const;
	
	// Get read-only access to the list of settings for each target chain
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	const TArray<URetargetChainSettings*>& GetAllChainSettings() const;

	//
	// RETARGET CHAIN MAPPING C++ ONLY API
	//
	
	// Get the name of the goal assigned to a chain 
	FName GetChainGoal(const TObjectPtr<URetargetChainSettings> ChainSettings) const;
	
	// Get whether the given chain's IK goal is connected to a solver 
	bool IsChainGoalConnectedToASolver(const FName& GoalName) const;
	
	// Call this when IK Rig chain is added or removed. 
	void HandleRetargetChainAdded(UIKRigDefinition* IKRig) const;
	
	// Call this when IK Rig chain is renamed. Retains existing mappings using the new name 
	void HandleRetargetChainRenamed(UIKRigDefinition* IKRig, FName OldChainName, FName NewChainName) const;
	
	// Call this when IK Rig chain is removed. 
	void HandleRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const;
	
private:

	// Remove invalid chain mappings (no longer existing in currently referenced source/target IK Rig assets)
	// NOTE: be sure to reinitialize any running processors after chain mappings are cleaned!
	void CleanChainMapping() const;
	
	// Get names of all the bone chains.
	void GetChainNames(const ERetargetSourceOrTarget SourceOrTarget, TArray<FName>& OutNames) const;

	// Sort the Asset ChainMapping based on the StartBone of the target chains. 
	void SortChainMapping() const;

	// convenience to get chain settings UObject by name
	URetargetChainSettings* GetChainSettings(const FName& TargetChainName) const;

	//
	// RETARGET POSE PUBLIC/SCRIPTING API
	//

public:
	
	// Add new retarget pose. Returns the name of the new retarget pose.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	FName CreateRetargetPose(const FName& NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const;

	// Remove a retarget pose. Returns true if the pose was found and removed.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	bool RemoveRetargetPose(const FName& PoseToRemove, const ERetargetSourceOrTarget SourceOrTarget) const;

	// Duplicate a retarget pose. Returns the name of the new, duplicate pose (or NAME_None if PoseToDuplicate is not found).
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	FName DuplicateRetargetPose(const FName PoseToDuplicate, const FName NewName, const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Rename current retarget pose. Returns true if a pose was renamed.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	bool RenameRetargetPose(const FName OldPoseName, const FName NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const;

	// Change which retarget pose is used by the retargeter at runtime
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	bool SetCurrentRetargetPose(FName CurrentPose, const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Get the current retarget pose
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta =(BlueprintThreadSafe))
    FName GetCurrentRetargetPoseName(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Get access to array of retarget poses
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	TMap<FName, FIKRetargetPose>& GetRetargetPoses(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Get the current retarget pose
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	FIKRetargetPose& GetCurrentRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const;

	// Reset a retarget pose for the specified bones.
	// If BonesToReset is Empty, will removes all stored deltas, returning pose to reference pose
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	void ResetRetargetPose(const FName& PoseToReset, const TArray<FName>& BonesToReset, const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Set a delta rotation for a given bone for the current retarget pose
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	void SetRotationOffsetForRetargetPoseBone(
		const FName& BoneName,
		const FQuat& RotationOffset,
		const ERetargetSourceOrTarget SkeletonMode) const;
	
	// Get a delta rotation for a given bone for the current retarget pose
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	FQuat GetRotationOffsetForRetargetPoseBone(
		const FName& BoneName,
		const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Set the translation offset on the retarget root bone for the current retarget pose
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=IKRetargeter, meta =(BlueprintThreadSafe))
	void SetRootOffsetInRetargetPose(
		const FVector& TranslationOffset,
		const ERetargetSourceOrTarget SourceOrTarget) const;

	// Get the translation offset on the retarget root bone for the current retarget pose
	UFUNCTION(BlueprintCallable, Category=IKRetargeter, meta = (BlueprintThreadSafe))
	FVector GetRootOffsetInRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const;

	// Automatically align all bones in mapped chains and store in the current retarget pose.
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	void AutoAlignAllBones(ERetargetSourceOrTarget SourceOrTarget) const;

	// Automatically align an array of bones and store in the current retarget pose. Bones not in a mapped chain will be ignored.
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	void AutoAlignBones(const TArray<FName>& BonesToAlign, const ERetargetAutoAlignMethod Method, ERetargetSourceOrTarget SourceOrTarget) const;

	// Moves the entire skeleton vertically until the specified bone is the same height off the ground as in the reference pose.
	UFUNCTION(BlueprintCallable, Category=RetargetOps)
	void SnapBoneToGround(FName ReferenceBone, ERetargetSourceOrTarget SourceOrTarget);

	//
	// RETARGET POSE C++ ONLY API
	//
	
	// Add a numbered suffix to the given pose name to make it unique. 
	FName MakePoseNameUnique(
		const FString& PoseName,
		const ERetargetSourceOrTarget SourceOrTarget) const;
	
private:
	
	// Remove bones from retarget poses that are no longer in skeleton 
	void CleanPoseList(const ERetargetSourceOrTarget SourceOrTarget) const;


	//
	// DELEGATE CALLBACKS FOR OUTSIDE SYSTEMS
	//
	
private:
	
	DECLARE_MULTICAST_DELEGATE(FOnRetargeterNeedsInitialized);
	FOnRetargeterNeedsInitialized RetargeterNeedsInitialized;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnIKRigReplaced, ERetargetSourceOrTarget);
	FOnIKRigReplaced IKRigReplaced;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewMeshReplaced, ERetargetSourceOrTarget);
	FOnPreviewMeshReplaced PreviewMeshReplaced;

	void BroadcastPreviewMeshReplaced(ERetargetSourceOrTarget SourceOrTarget) const
	{
		PreviewMeshReplaced.Broadcast(SourceOrTarget);
	}

	void BroadcastIKRigReplaced(ERetargetSourceOrTarget SourceOrTarget) const
	{
		IKRigReplaced.Broadcast(SourceOrTarget);
	}
	
	void BroadcastNeedsReinitialized() const
	{
		RetargeterNeedsInitialized.Broadcast();
	}

	// auto pose generator
	TUniquePtr<FRetargetAutoPoseGenerator> AutoPoseGenerator;

	// only allow modifications to data model from one thread at a time
	mutable FCriticalSection ControllerLock;

	// prevent reinitializing from inner operations
	mutable int32 ReinitializeScopeCounter = 0;
	
public:

	// Attach a delegate to be notified whenever either the source or target Preview Mesh asset's are swapped out.
	FOnPreviewMeshReplaced& OnPreviewMeshReplaced(){ return PreviewMeshReplaced; };
	
	// Attach a delegate to be notified whenever either the source or target IK Rig asset's are swapped out.
	FOnIKRigReplaced& OnIKRigReplaced(){ return IKRigReplaced; };
	
	// Attach a delegate to be notified whenever the retargeter is modified in such a way that would require re-initialization of the processor.
	FOnRetargeterNeedsInitialized& OnRetargeterNeedsInitialized(){ return RetargeterNeedsInitialized; };
	
	friend class UIKRetargeter;
	friend struct FScopedReinitializeIKRetargeter;
};

struct FScopedReinitializeIKRetargeter
{
	FScopedReinitializeIKRetargeter(const UIKRetargeterController *InController)
	{
		InController->ReinitializeScopeCounter++;
		Controller = InController;
	}
	~FScopedReinitializeIKRetargeter()
	{
		if (--Controller->ReinitializeScopeCounter == 0)
		{
			Controller->GetAsset()->IncrementVersion();
			Controller->BroadcastNeedsReinitialized();
		}
	};

	const UIKRetargeterController* Controller;
};

