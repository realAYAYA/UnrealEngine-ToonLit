// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_GetRelativeBoneTransform.generated.h"

/**
 * GetBoneTransform is used to retrieve a single transform from a hierarchy.
 */
USTRUCT(meta=(DisplayName="Get Relative Transform", Category="Hierarchy", DocumentationPolicy = "Strict", Keywords = "GetRelativeBoneTransform", Deprecated = "4.25", Varying))
struct CONTROLRIG_API FRigUnit_GetRelativeBoneTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetRelativeBoneTransform()
		: CachedBone(FCachedRigElement())
		, CachedSpace(FCachedRigElement())
	{}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("Transform")))
		{
			return FRigElementKey(Space, ERigElementType::Bone);
		}
		return FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Bone to retrieve the transform for.
	 */
	UPROPERTY(meta = (Input))
	FName Bone;

	/**
	 * The name of the Bone to retrieve the transform relative within.
	 */
	UPROPERTY(meta = (Input))
	FName Space;

	// The current transform of the given bone - or identity in case it wasn't found.
	UPROPERTY(meta=(Output))
	FTransform Transform;

	// Used to cache the internally used bone index
	UPROPERTY(transient)
	FCachedRigElement CachedBone;

	// Used to cache the internally used space index
	UPROPERTY(transient)
	FCachedRigElement CachedSpace;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};
