// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"
#include "Units/Hierarchy/RigUnit_AddBoneTransform.h"
#include "AdditiveControlRig.generated.h"

class USkeletalMesh;
struct FReferenceSkeleton;
struct FSmartNameMapping;

/** Rig that allows additive layer editing per joint */
UCLASS(NotBlueprintable)
class CONTROLRIG_API UAdditiveControlRig : public UControlRig
{
	GENERATED_UCLASS_BODY()

public: 
	// BEGIN ControlRig
	virtual void Initialize(bool bInitRigUnits = true) override;
	virtual bool ExecuteUnits(FRigUnitContext& InOutContext, const FName& InEventName) override;
	// END ControlRig

	// utility function to 
	static FName GetControlName(const FName& InBoneName);
	static FName GetNullName(const FName& InBoneName);

private:
	// custom units that are used to execute this rig
	TArray<FRigUnit_AddBoneTransform> AddBoneRigUnits;

	/** Create RigElements - bone hierarchy and curves - from incoming skeleton */
	void CreateRigElements(const USkeletalMesh* InReferenceMesh);
	void CreateRigElements(const FReferenceSkeleton& InReferenceSkeleton, const FSmartNameMapping* InSmartNameMapping);
};