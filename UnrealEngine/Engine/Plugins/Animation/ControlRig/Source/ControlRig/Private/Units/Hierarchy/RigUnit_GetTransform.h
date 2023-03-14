// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_GetTransform.generated.h"

/**
 * GetTransform is used to retrieve a single transform from a hierarchy.
 */
USTRUCT(meta=(DisplayName="Get Transform", Category="Transforms", DocumentationPolicy = "Strict", Keywords="GetBoneTransform,GetControlTransform,GetInitialTransform,GetSpaceTransform,GetTransform", NodeColor="0.462745, 1,0, 0.329412",Varying))
struct CONTROLRIG_API FRigUnit_GetTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetTransform()
		: Item(NAME_None, ERigElementType::Bone)
		, Space(EBoneGetterSetterMode::GlobalSpace)
		, bInitial(false)
		, Transform(FTransform::Identity)
		, CachedIndex()
	{}

	virtual FString GetUnitLabel() const override;

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if(Space == EBoneGetterSetterMode::LocalSpace)
		{
			if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
			{
				return Hierarchy->GetFirstParent(Item);
			}
		}
		return FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The item to retrieve the transform for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	/**
	 * Defines if the transform should be retrieved in local or global space
	 */ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	/**
	 * Defines if the transform should be retrieved as current (false) or initial (true).
	 * Initial transforms for bones and other elements in the hierarchy represent the reference pose's value.
	 */ 
	UPROPERTY(meta = (Input))
	bool bInitial;

	// The current transform of the given item - or identity in case it wasn't found.
	UPROPERTY(meta=(Output))
	FTransform Transform;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedIndex;
};

/**
* GetTransformArray is used to retrieve an array of transforms from the hierarchy.
*/
USTRUCT(meta=(DisplayName="Get Transform Array", Category="Hierarchy", DocumentationPolicy = "Strict", Keywords="GetBoneTransform,GetControlTransform,GetInitialTransform,GetSpaceTransform,GetTransform", NodeColor="0.462745, 1,0, 0.329412",Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_GetTransformArray : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetTransformArray()
		: Items()
		, Space(EBoneGetterSetterMode::GlobalSpace)
		, bInitial(false)
		, Transforms()
		, CachedIndex()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	* The items to retrieve the transforms for
	*/
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Items;

	/**
	* Defines if the transforms should be retrieved in local or global space
	*/ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	/**
	* Defines if the transforms should be retrieved as current (false) or initial (true).
	* Initial transforms for bones and other elements in the hierarchy represent the reference pose's value.
	*/ 
	UPROPERTY(meta = (Input))
	bool bInitial;

	// The current transform of the given item - or identity in case it wasn't found.
	UPROPERTY(meta=(Output))
	TArray<FTransform> Transforms;

	// Used to cache the internally
	UPROPERTY()
	TArray<FCachedRigElement> CachedIndex;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* GetTransformArray is used to retrieve an array of transforms from the hierarchy.
*/
USTRUCT(meta=(DisplayName="Get Transform Array", Category="Transforms", DocumentationPolicy = "Strict", Keywords="GetBoneTransform,GetControlTransform,GetInitialTransform,GetSpaceTransform,GetTransform", NodeColor="0.462745, 1,0, 0.329412",Varying))
struct CONTROLRIG_API FRigUnit_GetTransformItemArray : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetTransformItemArray()
		: Items()
		, Space(EBoneGetterSetterMode::GlobalSpace)
		, bInitial(false)
		, Transforms()
		, CachedIndex()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	* The items to retrieve the transforms for
	*/
	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	/**
	* Defines if the transforms should be retrieved in local or global space
	*/ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	/**
	* Defines if the transforms should be retrieved as current (false) or initial (true).
	* Initial transforms for bones and other elements in the hierarchy represent the reference pose's value.
	*/ 
	UPROPERTY(meta = (Input))
	bool bInitial;

	// The current transform of the given item - or identity in case it wasn't found.
	UPROPERTY(meta=(Output))
	TArray<FTransform> Transforms;

	// Used to cache the internally
	UPROPERTY()
	TArray<FCachedRigElement> CachedIndex;
};
