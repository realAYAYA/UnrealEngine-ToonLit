// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/Hierarchy/RigUnit_SetTransform.h"
#include "Units/Hierarchy/RigUnit_SetCurveValue.h"
#include "FKControlRig.generated.h"

class USkeletalMesh;
struct FReferenceSkeleton;
struct FSmartNameMapping;
/** Structs used to specify which bones/curves/controls we should have active, since if all controls or active we can't passthrough some previous bone transform*/
struct FFKBoneCheckInfo
{
	FName BoneName;
	int32 BoneID;
	bool  bActive;
};

UENUM()
enum class EControlRigFKRigExecuteMode: uint8
{
	/** Replaces the current pose */
	Replace,

	/** Applies the authored pose as an additive layer */
	Additive,

	/** Sets the current pose without the use of offset transforms */
	Direct,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

/** Rig that allows override editing per joint */
UCLASS(NotBlueprintable, Meta = (DisplayName = "FK Control Rig"))
class CONTROLRIG_API UFKControlRig : public UControlRig
{
	GENERATED_UCLASS_BODY()

public: 

	// BEGIN ControlRig
	virtual void Initialize(bool bInitRigUnits = true) override;
	virtual bool ExecuteUnits(FRigUnitContext& InOutContext, const FName& InEventName) override;
	virtual void SetBoneInitialTransformsFromSkeletalMeshComponent(USkeletalMeshComponent* InSkelMeshComp, bool bUseAnimInstance = false) override;
	// END ControlRig

	// utility function to generate a valid control element name
	static FName GetControlName(const FName& InName, const ERigElementType& InType);
	// utility function to generate a target element name for control
	static FName GetControlTargetName(const FName& InName, const ERigElementType& InType);

	TArray<FName> GetControlNames();
	bool GetControlActive(int32 Index) const;
	void SetControlActive(int32 Index, bool bActive);
	void SetControlActive(const TArray<FFKBoneCheckInfo>& InBoneChecks);

	void SetApplyMode(EControlRigFKRigExecuteMode InMode);
	void ToggleApplyMode();
	bool CanToggleApplyMode() const { return true; }
	bool IsApplyModeAdditive() const { return ApplyMode == EControlRigFKRigExecuteMode::Additive; }

	// Ensures that controls mask is updated according to contained ControlRig (control) elements
	void RefreshActiveControls();

	struct FRigElementInitializationOptions
	{	
		// Flag whether or not to generate a transform control for bones
		bool bGenerateBoneControls = true;
		// Flag whether or not to generate a float control for all curves in the hierarchy
		bool bGenerateCurveControls = true;
		
		// Flag whether or not to import all curves from SmartNameMapping
		bool bImportCurves = true;

		// Set of bone names to generate a transform control for
		TArray<FName> BoneNames;
		// Set of curve names to generate a float control for (requires bImportCurves to be false)
		TArray<FName> CurveNames;
	};
	void SetInitializationOptions(const FRigElementInitializationOptions& Options) { InitializationOptions = Options; }
	
private:

	/** Create RigElements - bone hierarchy and curves - from incoming skeleton */
	void CreateRigElements(const USkeletalMesh* InReferenceMesh);
	void CreateRigElements(const FReferenceSkeleton& InReferenceSkeleton, const FSmartNameMapping* InSmartNameMapping);
	void SetControlOffsetsFromBoneInitials();

	UPROPERTY()
	TArray<bool> IsControlActive;

	UPROPERTY()
	EControlRigFKRigExecuteMode ApplyMode;
	EControlRigFKRigExecuteMode CachedToggleApplyMode;

	FRigElementInitializationOptions InitializationOptions;
	friend class FControlRigInteractionTest;
};