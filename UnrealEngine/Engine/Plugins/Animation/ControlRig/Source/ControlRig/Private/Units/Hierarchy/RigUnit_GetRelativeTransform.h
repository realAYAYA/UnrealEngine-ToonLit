// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_GetRelativeTransform.generated.h"

/**
 * GetRelativeTransform is used to retrieve a single transform from a hierarchy in the space of another transform
 */
USTRUCT(meta=(DisplayName="Get Relative Transform", Category="Transforms", DocumentationPolicy = "Strict", Keywords = "Offset,Local", NodeColor="0.462745, 1,0, 0.329412", Varying))
struct CONTROLRIG_API FRigUnit_GetRelativeTransformForItem : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetRelativeTransformForItem()
		: Child(NAME_None, ERigElementType::Bone)
		, bChildInitial(false)
		, Parent(NAME_None, ERigElementType::Bone)
		, bParentInitial(false)
		, RelativeTransform(FTransform::Identity)
		, CachedChild()
		, CachedParent()
	{}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		return Parent;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The child item to retrieve the transform for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/**
	 * Defines if the child's transform should be retrieved as current (false) or initial (true).
	 * Initial transforms for bones and other elements in the hierarchy represent the reference pose's value.
	 */ 
	UPROPERTY(meta = (Input))
	bool bChildInitial;

	/**
	 * The parent item to use.
	 * The child transform will be retrieve in the space of the parent.
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;

	/**
	 * Defines if the parent's transform should be retrieved as current (false) or initial (true).
	 * Initial transforms for bones and other elements in the hierarchy represent the reference pose's value.
	 */ 
	UPROPERTY(meta = (Input))
	bool bParentInitial;

	// The transform of the given child item relative to the provided parent
	UPROPERTY(meta=(Output))
	FTransform RelativeTransform;

	// Used to cache the child internally
	UPROPERTY()
	FCachedRigElement CachedChild;

	// Used to cache the parent internally
	UPROPERTY()
	FCachedRigElement CachedParent;
};
