// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_PropagateTransform.generated.h"

/**
 * Propagate Transform can be used to force a recalculation of a bone's global transform
 * from its local - as well as propagating that change onto the children.
 */
USTRUCT(meta=(DisplayName="Propagate Transform", Category="Hierarchy", DocumentationPolicy = "Strict", Keywords = "PropagateToChildren,RecomputeGlobal,RecalculateGlobal", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_PropagateTransform : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_PropagateTransform()
		: Item(NAME_None, ERigElementType::Bone)
		, bRecomputeGlobal(true)
		, bApplyToChildren(false)
		, bRecursive(false)
		, CachedIndex()
	{}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		return Item;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The item to offset the transform for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	// If set to true the item's global transform will be recomputed from
	// its parent's transform and its local.
	UPROPERTY(meta=(Input))
	bool bRecomputeGlobal;

	// If set to true the direct children of this item will be recomputed as well.
	// Combined with bRecursive all children will be affected recursively.
	UPROPERTY(meta=(Input))
	bool bApplyToChildren;

	// If set to true and with bApplyToChildren enabled
	// all children will be affected recursively.
	UPROPERTY(meta=(Input))
	bool bRecursive;

	// Used to cache the item internally
	UPROPERTY(transient)
	FCachedRigElement CachedIndex;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};
