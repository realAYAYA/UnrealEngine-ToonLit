// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Math/RigVMFunction_MathColor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_MathColor)

FRigVMFunction_MathColorMake_Execute()
{
	Result = FLinearColor(R, G, B, A);
}

FRigVMFunction_MathColorFromFloat_Execute()
{
	Result = FLinearColor(Value, Value, Value);
}

FRigVMFunction_MathColorFromDouble_Execute()
{
	Result = FLinearColor((float)Value, (float)Value, (float)Value);
}

FRigVMFunction_MathColorAdd_Execute()
{
	Result = A + B;
}

FRigVMFunction_MathColorSub_Execute()
{
	Result = A - B;
}

FRigVMFunction_MathColorMul_Execute()
{
	Result = A * B;
}

FRigVMFunction_MathColorLerp_Execute()
{
	Result = FMath::Lerp<FLinearColor>(A, B, T);
}

