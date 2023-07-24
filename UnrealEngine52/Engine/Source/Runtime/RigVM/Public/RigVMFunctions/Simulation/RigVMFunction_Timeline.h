// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_SimBase.h"
#include "RigVMFunction_Timeline.generated.h"

/**
 * Simulates a time value - can act as a timeline playing back
 */
USTRUCT(meta=(DisplayName="Accumulated Time", Category = "Simulation|Accumulate", Keywords="Playback,Pause,Timeline"))
struct RIGVM_API FRigVMFunction_Timeline : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_Timeline()
	{
		Speed = 1.f;
		Time = AccumulatedValue = 0.f;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	float Speed;

	UPROPERTY(meta=(Output))
	float Time;

	UPROPERTY()
	float AccumulatedValue;

	UPROPERTY()
	bool bIsInitialized;
};

/**
 * Simulates a time value - and outputs loop information
 */
USTRUCT(meta=(DisplayName="Time Loop", Category = "Simulation|Accumulate", Keywords="Playback,Pause,Timeline"))
struct RIGVM_API FRigVMFunction_TimeLoop : public FRigVMFunction_SimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_TimeLoop()
	{
		Speed = 1.f;
		Duration = 1.f;
		Normalize = false;
		Absolute = Relative = FlipFlop = AccumulatedAbsolute = AccumulatedRelative = 0.f;
		NumIterations = 0;
		Even = false;
		bIsInitialized = false;
	}

	virtual void Initialize() override { bIsInitialized = false; }

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	float Speed;

	// the duration of a single loop in seconds
	UPROPERTY(meta = (Input))
	float Duration;

	// if set to true the output relative and flipflop
	// will be normalized over the duration.
	UPROPERTY(meta = (Input))
	bool Normalize;

	// the overall time in seconds
	UPROPERTY(meta=(Output))
	float Absolute;

	// the relative time in seconds (within the loop)
	UPROPERTY(meta=(Output))
	float Relative;

	// the relative time in seconds (within the loop),
	// going from 0 to duration and then back from duration to 0,
	// or 0 to 1 and 1 to 0 if Normalize is turned on
	UPROPERTY(meta=(Output))
	float FlipFlop;

	// true if the iteration of the loop is even
	UPROPERTY(meta=(Output))
	bool Even;

	UPROPERTY()
	float AccumulatedAbsolute;

	UPROPERTY()
	float AccumulatedRelative;

	UPROPERTY()
	int32 NumIterations;

	UPROPERTY()
	bool bIsInitialized;
};