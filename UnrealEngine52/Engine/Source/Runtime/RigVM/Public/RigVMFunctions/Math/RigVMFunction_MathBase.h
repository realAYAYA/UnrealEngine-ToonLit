// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunctions/RigVMFunctionDefines.h"
#include "RigVMFunction_MathBase.generated.h"

USTRUCT(meta=(Abstract, NodeColor = "0.05 0.25 0.05"))
struct RIGVM_API FRigVMFunction_MathBase : public FRigVMStruct
{
	GENERATED_BODY()

	virtual void Execute() {};
};

USTRUCT(meta=(Abstract, NodeColor = "0.05 0.25 0.05"))
struct RIGVM_API FRigVMFunction_MathMutableBase : public FRigVMStructMutable
{
	GENERATED_BODY()

	virtual void Execute() {};
};
