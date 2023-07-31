// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_AnimBase.h"
#include "RigUnit_GetWorldTime.generated.h"

/**
 * Returns the current time (year, month, day, hour, minute)
 */
USTRUCT(meta = (DisplayName = "Now", Keywords = "Time,Clock", Varying))
struct CONTROLRIG_API FRigUnit_GetWorldTime : public FRigUnit_AnimBase
{
	GENERATED_BODY()

	FRigUnit_GetWorldTime()
	{
		Year = Month = Day = WeekDay = Hours = Minutes = Seconds = OverallSeconds = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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

