// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_OffsetTransform.generated.h"

/**
 * Offset Transform is used to add an offset to an existing transform in the hierarchy. The offset is post multiplied.
 */
USTRUCT(meta=(DisplayName="Offset Transform", Category="Transforms", DocumentationPolicy = "Strict", Keywords = "Offset,Relative,AddBoneTransform", NodeColor="0, 0.364706, 1.0", Varying))
struct CONTROLRIG_API FRigUnit_OffsetTransformForItem : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_OffsetTransformForItem()
		: Item(NAME_None, ERigElementType::Bone)
		, OffsetTransform(FTransform::Identity)
		, Weight(1.f)
		, bPropagateToChildren(true)
		, CachedIndex()
	{}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
		{
			return Hierarchy->GetFirstParent(Item);
		}
		return FRigElementKey();
	}

	virtual FTransform DetermineOffsetTransformForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
		{
			// this is similar to RigUnit_ModifyTransform
			return OffsetTransform.Inverse() * Hierarchy->GetLocalTransform(Item);
		}
		
		return FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The item to offset the transform for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	// The transform of the item relative to its previous transform
	UPROPERTY(meta=(Input))
	FTransform OffsetTransform;

	// Defines how much the change will be applied
	UPROPERTY(meta=(Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight;

	// If set to true children of affected items in the hierarchy
	// will follow the transform change - otherwise only the parent will move.
	UPROPERTY(meta=(Input))
	bool bPropagateToChildren;

	// Used to cache the item internally
	UPROPERTY()
	FCachedRigElement CachedIndex;
};
