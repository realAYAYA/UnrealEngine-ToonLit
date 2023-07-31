// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_AnimBase.h"
#include "RigUnit_GetDeltaTime.generated.h"

/**
 * Returns the time gone by from the previous evaluation
 */
USTRUCT(meta=(DisplayName="Delta Time", Varying))
struct CONTROLRIG_API FRigUnit_GetDeltaTime : public FRigUnit_AnimBase
{
	GENERATED_BODY()
	
	FRigUnit_GetDeltaTime()
	{
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Output))
	float Result;
};

