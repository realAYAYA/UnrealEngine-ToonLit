// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h" 
#include "RigUnit_Item.generated.h"

USTRUCT(meta = (Abstract, NodeColor = "0.7 0.05 0.5", Category = "Items"))
struct CONTROLRIG_API FRigUnit_ItemBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta = (Abstract, NodeColor = "0.7 0.05 0.5", Category = "Items"))
struct CONTROLRIG_API FRigUnit_ItemBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};

/**
 * Returns true or false if a given item exists
 */
USTRUCT(meta=(DisplayName="Item Exists", Keywords=""))
struct CONTROLRIG_API FRigUnit_ItemExists : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemExists()
	{
		Item = FRigElementKey();
		Exists = false;
		CachedIndex = FCachedRigElement();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	UPROPERTY(meta = (Output))
	bool Exists;

	// Used to cache the internally used index
	UPROPERTY()
	FCachedRigElement CachedIndex;
};

/**
 * Replaces the text within the name of the item
 */
USTRUCT(meta=(DisplayName="Item Replace", Keywords="Replace,Name"))
struct CONTROLRIG_API FRigUnit_ItemReplace : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemReplace()
	{
		Item = Result = FRigElementKey();
		Old = New = NAME_None;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKey Item;

	UPROPERTY(meta = (Input))
	FName Old;

	UPROPERTY(meta = (Input))
	FName New;

	UPROPERTY(meta = (Output))
	FRigElementKey Result;
};

/**
* Returns true if the two items are equal
*/
USTRUCT(meta=(DisplayName="Item Equals", Keywords="", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_ItemEquals : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemEquals()
	{
		A = B = FRigElementKey();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey A;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey B;

	UPROPERTY(meta = (Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns true if the two items are not equal
*/
USTRUCT(meta=(DisplayName="Item Not Equals", Keywords="", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_ItemNotEquals : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemNotEquals()
	{
		A = B = FRigElementKey();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey A;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey B;

	UPROPERTY(meta = (Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns true if the two items' types are equal
*/
USTRUCT(meta=(DisplayName="Item Type Equals", Keywords=""))
struct CONTROLRIG_API FRigUnit_ItemTypeEquals : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemTypeEquals()
	{
		A = B = FRigElementKey();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey A;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey B;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
* Returns true if the two items's types are not equal
*/
USTRUCT(meta=(DisplayName="Item Type Not Equals", Keywords=""))
struct CONTROLRIG_API FRigUnit_ItemTypeNotEquals : public FRigUnit_ItemBase
{
	GENERATED_BODY()

	FRigUnit_ItemTypeNotEquals()
	{
		A = B = FRigElementKey();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey A;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey B;

	UPROPERTY(meta = (Output))
	bool Result;
};