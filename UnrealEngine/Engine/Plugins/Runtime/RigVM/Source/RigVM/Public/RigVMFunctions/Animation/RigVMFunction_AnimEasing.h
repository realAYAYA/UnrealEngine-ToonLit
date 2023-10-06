// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_AnimBase.h"
#include "RigVMFunctions/Math/RigVMMathLibrary.h"
#include "RigVMFunction_AnimEasing.generated.h"

/**
 * A constant value of an easing type
 */
USTRUCT(meta = (DisplayName = "EaseType", Keywords = "Constant"))
struct RIGVM_API FRigVMFunction_AnimEasingType : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()

	FRigVMFunction_AnimEasingType()
	{
		Type = ERigVMAnimEasingType::CubicEaseInOut;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input, Output))
	ERigVMAnimEasingType Type;
};

/**
 * Returns the eased version of the input value
 */
USTRUCT(meta=(DisplayName="Ease", Keywords="Easing,Profile,Smooth,Cubic"))
struct RIGVM_API FRigVMFunction_AnimEasing : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AnimEasing()
	{
		Value = Result = 0.f;
		Type = ERigVMAnimEasingType::CubicEaseInOut;
		SourceMinimum = TargetMinimum = 0.f;
		SourceMaximum = TargetMaximum = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Input))
	ERigVMAnimEasingType Type;

	UPROPERTY(meta=(Input))
	float SourceMinimum;

	UPROPERTY(meta=(Input))
	float SourceMaximum;

	UPROPERTY(meta=(Input))
	float TargetMinimum;

	UPROPERTY(meta=(Input))
	float TargetMaximum;

	UPROPERTY(meta=(Output))
	float Result;
};

