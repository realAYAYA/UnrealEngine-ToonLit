// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Animation/RigUnit_GetWorldTime.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_GetWorldTime)

FRigUnit_GetWorldTime_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FDateTime Now = FDateTime::Now();
	Year = (float)Now.GetYear();
	Month = (float)Now.GetMonth();
	Day = (float)Now.GetDay();
	WeekDay = (float)Now.GetDayOfWeek();
	Hours = (float)Now.GetHour();
	Minutes = (float)Now.GetMinute();
	Seconds = float(Now.GetSecond()) + float(Now.GetMillisecond()) * 0.001f;
	OverallSeconds = (float)FPlatformTime::Seconds();
}


