// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreRangeTimeSource.h"

void UPropertyAnimatorCoreRangeTimeSource::SetUseStartTime(bool bInUse)
{
	if (bUseStartTime == bInUse)
	{
		return;
	}

	bUseStartTime = bInUse;
}

void UPropertyAnimatorCoreRangeTimeSource::SetStartTime(double InStartTime)
{
	if (FMath::IsNearlyEqual(StartTime, InStartTime))
	{
		return;
	}

	StartTime = InStartTime;
}

void UPropertyAnimatorCoreRangeTimeSource::SetUseStopTime(bool bInUse)
{
	if (bUseStopTime == bInUse)
	{
		return;
	}

	bUseStopTime = bInUse;
}

void UPropertyAnimatorCoreRangeTimeSource::SetStopTime(double InStopTime)
{
	if (FMath::IsNearlyEqual(StopTime, InStopTime))
	{
		return;
	}

	StopTime = InStopTime;
}

bool UPropertyAnimatorCoreRangeTimeSource::IsValidTimeElapsed(double InTimeElapsed) const
{
	if (bUseStartTime && InTimeElapsed < StartTime)
	{
		return false;
	}

	if (bUseStopTime && InTimeElapsed > StopTime)
	{
		return false;
	}

	return true;
}
