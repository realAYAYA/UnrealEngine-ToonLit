// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_AnimBase.h"
#include "RigVMFunction_TimeConversion.generated.h"

/**
 * Converts frames to seconds based on the current frame rate
 */
USTRUCT(meta=(DisplayName="Frames to Seconds", Varying))
struct RIGVM_API FRigVMFunction_FramesToSeconds : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_FramesToSeconds()
	{
		Seconds = Frames = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Frames;

	UPROPERTY(meta=(Output))
	float Seconds;
};

/**
 * Converts seconds to frames based on the current frame rate
 */
USTRUCT(meta=(DisplayName="Seconds to Frames", Varying))
struct RIGVM_API FRigVMFunction_SecondsToFrames : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_SecondsToFrames()
	{
		Seconds = Frames = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Seconds;

	UPROPERTY(meta=(Output))
	float Frames;
};
