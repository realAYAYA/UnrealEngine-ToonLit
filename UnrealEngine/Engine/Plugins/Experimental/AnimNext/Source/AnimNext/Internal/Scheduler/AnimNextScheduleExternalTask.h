// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleExternalTask.generated.h"

class UAnimNextSchedule;
class UAnimNextComponent;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

USTRUCT()
struct FAnimNextScheduleExternalTask
{
	GENERATED_BODY()

	FAnimNextScheduleExternalTask() = default;

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UAnimNextComponent;
	friend struct FAnimNextSchedulerEntry;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct UE::AnimNext::FScheduleInstanceData;

	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	UPROPERTY()
	FName ExternalTask;
};
