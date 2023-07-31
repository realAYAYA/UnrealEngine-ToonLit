// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "IKRigDataTypes.h"

#include "IKRigSolver.generated.h"

class FPrimitiveDrawInterface;
struct FIKRigGoalContainer;
struct FIKRigSkeleton;

// this is the base class for creating your own solver type that integrates into the IK Rig framework/editor.
UCLASS(abstract, hidecategories = UObject)
class IKRIG_API UIKRigSolver : public UObject
{
	GENERATED_BODY()
	
public:

	UIKRigSolver() { SetFlags(RF_Transactional); }

	//** RUNTIME */
	/** override to setup internal data based on ref pose */
	virtual void Initialize(const FIKRigSkeleton& IKRigSkeleton) PURE_VIRTUAL("Init");
	/** override Solve() to evaluate new output pose (InOutGlobalTransform) */
	virtual void Solve(FIKRigSkeleton& IKRigSkeleton, const FIKRigGoalContainer& Goals) PURE_VIRTUAL("Solve");
	//** END RUNTIME */

	//** ROOT BONE (optional, implement if your solver requires a root bone) */
	/** if solver requires a root bone, then override this to return it. */
	virtual FName GetRootBone() const { return NAME_None; };
	/** override to support telling outside systems which bones this solver has setting for.
	* NOTE: This must be overriden on solvers that use bone settings.
	* NOTE: Only ADD to the incoming set, do not remove from it. */
	virtual void GetBonesWithSettings(TSet<FName>& OutBonesWithSettings) const {};
	
	/** get if this solver is enabled */
	bool IsEnabled() const { return bIsEnabled; };
	/** turn solver on/off (will be skipped during execution if disabled) */
	void SetEnabled(const bool bEnabled){ bIsEnabled = bEnabled; };
	
	//** SOLVER SETTINGS */
	/** override to support RECEIVING modified solver settings from outside systems for editing/UI.
	 * Note: you can safely cast this to your own solver type and copy any relevant settings at runtime
	 * This is necessary because at runtime, the IKRigProcessor creates a copy of your solver class
	 * and the copy must be notified of changes made to the class settings in the source asset.*/
	virtual void UpdateSolverSettings(UIKRigSolver* InSettings){};

	/** override to support REMOVING a goal from custom solver */
	virtual void RemoveGoal(const FName& GoalName) PURE_VIRTUAL("RemoveGoal");

#if WITH_EDITORONLY_DATA
	
	/** callback whenever this solver is edited */
	DECLARE_EVENT_OneParam(UIKRigSolver, FIKRigSolverModified, UIKRigSolver*);
	FIKRigSolverModified& OnSolverModified(){ return IKRigSolverModified; };
	
	/** override to give your solver a nice name to display in the UI */
	virtual FText GetNiceName() const { checkNoEntry() return FText::GetEmpty(); };
	/** override to provide warning to user during setup of any missing components. return false if no warnings. */
	virtual bool GetWarningMessage(FText& OutWarningMessage) const { return false; };
	//** END SOLVER SETTINGS */

	//** GOALS */
	/** override to support ADDING a new goal to custom solver */
	virtual void AddGoal(const UIKRigEffectorGoal& NewGoal) PURE_VIRTUAL("AddGoal");
	/** override to support RENAMING an existing goal */
	virtual void RenameGoal(const FName& OldName, const FName& NewName) PURE_VIRTUAL("RenameGoal");
	/** override to support CHANGING BONE for an existing goal */
	virtual void SetGoalBone(const FName& GoalName, const FName& NewBoneName) PURE_VIRTUAL("SetGoalBone");
	/** override to support QUERY for a connected goal */
	virtual bool IsGoalConnected(const FName& GoalName) const {return false;};
	/** override to support supplying goals settings specific to this solver to outside systems for editing/UI */
	virtual UObject* GetGoalSettings(const FName& GoalName) const {return nullptr;};
	//** END GOALS */

	//** ROOT BONE (optional, implement if your solver requires a root bone) */
	/** override to support SETTING ROOT BONE for the solver */
	virtual void SetRootBone(const FName& RootBoneName){};
	virtual bool RequiresRootBone() const { return false; };
	//** END ROOT BONE */

	//** ROOT BONE (optional, implement if your solver requires a end bone) */
	/** override to support SETTING END BONE for the solver */
	virtual void SetEndBone(const FName& EndBoneName){};
	virtual bool RequiresEndBone() const { return false; };
	//** END ROOT BONE */

	//** BONE SETTINGS (optional, implement if your solver supports per-bone settings) */
	/** override to support ADDING PER-BONE settings for this solver */
	virtual void AddBoneSetting(const FName& BoneName){};
	/** override to support ADDING PER-BONE settings for this solver */
	virtual void RemoveBoneSetting(const FName& BoneName){};
	/** override to support supplying per-bone settings to outside systems for editing/UI
	 ** NOTE: This must be overriden on solvers that use bone settings.*/
	virtual UObject* GetBoneSetting(const FName& BoneName) const { ensure(!UsesBoneSettings()); return nullptr; };
	
	/** override to tell systems if this solver supports per-bone settings */
	virtual bool UsesBoneSettings() const { return false;};
	/** todo override to draw custom per-bone settings in the editor viewport */
	virtual void DrawBoneSettings(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton, FPrimitiveDrawInterface* PDI) const {};
	/** return true if the supplied Bone is affected by this solver - this provides UI feedback for user */
	virtual bool IsBoneAffectedBySolver(const FName& BoneName, const FIKRigSkeleton& IKRigSkeleton) const { return false; };
	//** END ROOT BONE */

	/** UObject interface */
	virtual void PostLoad() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	/** END UObject interface */

private:

	/** Register callbacks to update IK Rig when a solver is modified */
	FIKRigSolverModified IKRigSolverModified;
#endif
	
	UPROPERTY()
	bool bIsEnabled = true;
};