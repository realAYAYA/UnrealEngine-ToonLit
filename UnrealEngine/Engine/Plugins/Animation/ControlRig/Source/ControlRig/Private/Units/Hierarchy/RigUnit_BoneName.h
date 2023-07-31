// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_BoneName.generated.h"

/**
 * The Item node is used to share a specific item across the graph
 */
USTRUCT(meta=(DisplayName="Item", Category="Hierarchy", NodeColor = "0.4627450108528137 1.0 0.3294120132923126", DocumentationPolicy = "Strict", Constant))
struct CONTROLRIG_API FRigUnit_Item : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_Item()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The item
	 */
	UPROPERTY(meta = (Input, Output, ExpandByDefault))
	FRigElementKey Item;
};

/**
 * BoneName is used to represent a bone name in the graph
 */
USTRUCT(meta=(DisplayName="Bone Name", Category="Hierarchy", DocumentationPolicy = "Strict", Constant, Deprecated = "4.25"))
struct CONTROLRIG_API FRigUnit_BoneName : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_BoneName()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Bone
	 */
	UPROPERTY(meta = (Input, Output))
	FName Bone;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * SpaceName is used to represent a Space name in the graph
 */
USTRUCT(meta=(DisplayName="Space Name", Category="Hierarchy", DocumentationPolicy = "Strict", Deprecated = "4.25"))
struct CONTROLRIG_API FRigUnit_SpaceName : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_SpaceName()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Space
	 */
	UPROPERTY(meta = (Input, Output))
	FName Space;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * ControlName is used to represent a Control name in the graph
 */
USTRUCT(meta=(DisplayName="Control Name", Category="Hierarchy", DocumentationPolicy = "Strict", Deprecated = "4.25"))
struct CONTROLRIG_API FRigUnit_ControlName : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_ControlName()
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Control
	 */
	UPROPERTY(meta = (Input, Output))
	FName Control;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};
