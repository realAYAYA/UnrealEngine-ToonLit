// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/SimpleTargetingFilterTask.h"

bool USimpleTargetingFilterTask::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	return BP_ShouldFilterTarget(TargetingHandle, TargetData);
}
