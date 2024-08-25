// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/SimpleTargetingSortTask.h"

float USimpleTargetingSortTask::GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	return BP_GetScoreForTarget(TargetingHandle, TargetData);
}
