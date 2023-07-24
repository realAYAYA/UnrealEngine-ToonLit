// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_AnimBase.h"
#include "RigVMFunction_GetWorldTime.generated.h"

/**
 * Returns the current time (year, month, day, hour, minute)
 */
USTRUCT(meta = (DisplayName = "Now", Keywords = "Time,Clock", Varying))
struct RIGVM_API FRigVMFunction_GetWorldTime : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()

	FRigVMFunction_GetWorldTime()
	{
		Year = Month = Day = WeekDay = Hours = Minutes = Seconds = OverallSeconds = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Output))
	float Year;

	UPROPERTY(meta = (Output))
	float Month;

	UPROPERTY(meta = (Output))
	float Day;

	UPROPERTY(meta = (Output))
	float WeekDay;

	UPROPERTY(meta = (Output))
	float Hours;

	UPROPERTY(meta = (Output))
	float Minutes;

	UPROPERTY(meta = (Output))
	float Seconds;

	UPROPERTY(meta = (Output))
	float OverallSeconds;
};

