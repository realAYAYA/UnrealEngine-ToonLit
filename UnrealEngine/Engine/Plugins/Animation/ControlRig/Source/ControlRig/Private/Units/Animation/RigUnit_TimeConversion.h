// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_AnimBase.h"
#include "RigUnit_TimeConversion.generated.h"

/**
 * Converts frames to seconds based on the current frame rate
 */
USTRUCT(meta=(DisplayName="Frames to Seconds", Varying))
struct CONTROLRIG_API FRigUnit_FramesToSeconds : public FRigUnit_AnimBase
{
	GENERATED_BODY()
	
	FRigUnit_FramesToSeconds()
	{
		Seconds = Frames = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Frames;

	UPROPERTY(meta=(Output))
	float Seconds;
};

/**
 * Converts seconds to frames based on the current frame rate
 */
USTRUCT(meta=(DisplayName="Seconds to Frames", Varying))
struct CONTROLRIG_API FRigUnit_SecondsToFrames : public FRigUnit_AnimBase
{
	GENERATED_BODY()
	
	FRigUnit_SecondsToFrames()
	{
		Seconds = Frames = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Seconds;

	UPROPERTY(meta=(Output))
	float Frames;
};
