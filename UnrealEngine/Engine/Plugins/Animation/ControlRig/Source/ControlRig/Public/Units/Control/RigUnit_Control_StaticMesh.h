// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_Control.h"
#include "RigUnit_Control_StaticMesh.generated.h"

class UStaticMesh;
class UMaterialInterface;

/** A control unit used to drive a transform from an external source */
USTRUCT(meta=(DisplayName="Static Mesh Control", Category="Controls", ShowVariableNameInTitle, Deprecated="5.0"))
struct CONTROLRIG_API FRigUnit_Control_StaticMesh : public FRigUnit_Control
{
	GENERATED_BODY()

	FRigUnit_Control_StaticMesh();

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** The the transform the mesh will be rendered with (applied on top of the control's transform in the viewport) */
	UPROPERTY(meta=(Input))
	FTransform MeshTransform;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};