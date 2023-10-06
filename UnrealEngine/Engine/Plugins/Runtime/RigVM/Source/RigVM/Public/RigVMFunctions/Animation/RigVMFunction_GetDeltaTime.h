// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_AnimBase.h"
#include "RigVMFunction_GetDeltaTime.generated.h"

/**
 * Returns the time gone by from the previous evaluation
 */
USTRUCT(meta=(DisplayName="Delta Time", Varying))
struct RIGVM_API FRigVMFunction_GetDeltaTime : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_GetDeltaTime()
	{
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Output))
	float Result;
};

