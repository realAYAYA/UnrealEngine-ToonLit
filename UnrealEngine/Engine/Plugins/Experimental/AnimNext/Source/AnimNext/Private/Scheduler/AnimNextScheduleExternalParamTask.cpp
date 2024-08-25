// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleExternalParamTask.h"
#include "AnimNextStats.h"
#include "ScheduleContext.h"
#include "ScheduleInstanceData.h"
#include "Param/IParameterSource.h"

DEFINE_STAT(STAT_AnimNext_Task_ExternalParams);

void FAnimNextScheduleExternalParamTask::UpdateExternalParams(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ExternalParams);

	using namespace UE::AnimNext;
	
	FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
	FScheduleInstanceData::FExternalParamCache& ExternalParamCache = InstanceData.ExternalParamCaches[TaskIndex];
	const float DeltaTime = InScheduleContext.GetDeltaTime();
	
	for (TUniquePtr<IParameterSource>& ParameterSource : ExternalParamCache.ParameterSources)
	{
		ParameterSource->Update(DeltaTime);
	}
}
