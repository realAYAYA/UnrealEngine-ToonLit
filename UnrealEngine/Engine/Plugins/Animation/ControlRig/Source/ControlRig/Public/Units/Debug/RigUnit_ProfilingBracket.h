// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunctions/Debug/RigVMFunction_DebugBase.h"
#include "RigUnit_ProfilingBracket.generated.h"

/**
 * Starts a profiling timer for debugging, used in conjunction with End Profiling Timer
 */
USTRUCT(meta=(DisplayName="Start Profiling Timer", Keywords="Measure,BeginProfiling,Profile", NodeColor="0.25, 0.25, 0.05000000074505806"))
struct CONTROLRIG_API FRigUnit_StartProfilingTimer : public FRigVMFunction_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_StartProfilingTimer()
	{
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Ends an existing profiling timer for debugging, used in conjunction with Start Profiling Timer
 */
USTRUCT(meta = (DisplayName = "End Profiling Timer", Keywords = "Measure,StopProfiling,Meter,Profile", NodeColor="0.25, 0.25, 0.05000000074505806"))
struct CONTROLRIG_API FRigUnit_EndProfilingTimer : public FRigVMFunction_DebugBaseMutable
{
	GENERATED_BODY()

	FRigUnit_EndProfilingTimer()
	{
		NumberOfMeasurements = 1;
		AccumulatedTime = 0.f;
		MeasurementsLeft = 0;
		Prefix = TEXT("Timer");
		bIsInitialized = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input, Constant))
	int32 NumberOfMeasurements;

	UPROPERTY(meta = (Input, Constant))
	FString Prefix;

	UPROPERTY()
	float AccumulatedTime;

	UPROPERTY()
	int32 MeasurementsLeft;

	UPROPERTY()
	bool bIsInitialized;
};
