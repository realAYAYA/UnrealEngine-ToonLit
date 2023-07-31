// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathMatrix.h"
#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathMatrix)

FRigUnit_MathMatrixFromTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Transform.ToMatrixWithScale();
}

FRigVMStructUpgradeInfo FRigUnit_MathMatrixFromTransform::GetUpgradeInfo() const
{
	FRigUnit_MathMatrixFromTransformV2 NewNode;
	NewNode.Value = Transform;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Transform"), TEXT("Value"));
	return Info;
}

FRigUnit_MathMatrixFromTransformV2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.ToMatrixWithScale();
}

FRigUnit_MathMatrixToTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.SetFromMatrix(Value);
}

FRigUnit_MathMatrixFromVectors_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMatrix(X, Y, Z, FVector::ZeroVector);
	Result.SetOrigin(Origin);
}

FRigUnit_MathMatrixToVectors_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	X = Value.GetScaledAxis(EAxis::X);
	Y = Value.GetScaledAxis(EAxis::Y);
	Z = Value.GetScaledAxis(EAxis::Z);
	Origin = Value.GetOrigin();
}

FRigUnit_MathMatrixMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathMatrixInverse_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.Inverse();
}

