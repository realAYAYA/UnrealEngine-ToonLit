// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_SetBoneRotation.generated.h"


/**
 * SetBoneRotation is used to perform a change in the hierarchy by setting a single bone's rotation.
 */
USTRUCT(meta=(DisplayName="Set Rotation", Category="Hierarchy", DocumentationPolicy="Strict", Keywords = "SetBoneRotation", Deprecated="4.25"))
struct CONTROLRIG_API FRigUnit_SetBoneRotation : public FRigUnitMutable
{
	GENERATED_BODY()

		FRigUnit_SetBoneRotation()
		: Rotation(FQuat::Identity)
		, Space(EBoneGetterSetterMode::LocalSpace)
		, Weight(1.f)
		, bPropagateToChildren(true)
		, CachedBone(FCachedRigElement())
	{}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("Rotation")) && Space == EBoneGetterSetterMode::LocalSpace)
		{
			if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
			{
				return Hierarchy->GetFirstParent(FRigElementKey(Bone, ERigElementType::Bone));
			}
		}
		return FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Bone to set the rotation for.
	 */
	UPROPERTY(meta = (Input))
	FName Bone;

	/**
	 * The rotation value to set for the given Bone.
	 */
	UPROPERTY(meta = (Input))
	FQuat Rotation;

	/**
	 * Defines if the bone's rotation should be set
	 * in local or global space.
	 */
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	/**
	 * The weight of the change - how much the change should be applied
	 */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight;

	/**
	 * If set to true all of the global transforms of the children 
	 * of this bone will be recalculated based on their local transforms.
	 * Note: This is computationally more expensive than turning it off.
	 */
	UPROPERTY(meta = (Input, Constant))
	bool bPropagateToChildren;

	// Used to cache the internally used bone index
	UPROPERTY(transient)
	FCachedRigElement CachedBone;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};
