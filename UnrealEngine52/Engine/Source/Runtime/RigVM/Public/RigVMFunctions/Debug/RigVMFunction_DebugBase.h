// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunction_DebugBase.generated.h"

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct RIGVM_API FRigVMFunction_DebugBase : public FRigVMStruct
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct RIGVM_API FRigVMFunction_DebugBaseMutable : public FRigVMStructMutable
{
	GENERATED_BODY()
};
