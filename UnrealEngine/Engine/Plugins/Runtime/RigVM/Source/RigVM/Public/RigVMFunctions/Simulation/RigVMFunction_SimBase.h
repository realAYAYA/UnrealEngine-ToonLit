// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunction_SimBase.generated.h"

USTRUCT(meta=(Abstract, Category = "Simulation", NodeColor = "0.25 0.05 0.05"))
struct RIGVM_API FRigVMFunction_SimBase : public FRigVMStruct
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category = "Simulation", NodeColor = "0.25 0.05 0.05"))
struct RIGVM_API FRigVMFunction_SimBaseMutable : public FRigVMStructMutable
{
	GENERATED_BODY()
};
