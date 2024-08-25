// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleExternalParamTask.generated.h"

class UAnimNextSchedule;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

USTRUCT()
struct FAnimNextScheduleExternalParameterSource
{
	GENERATED_BODY()

	// Parameter source name to use
	UPROPERTY()
	FName ParameterSource;

	// Names of all parameters that are required in this task
	UPROPERTY()
	TArray<FName> Parameters;
};

USTRUCT()
struct FAnimNextScheduleExternalParamTask
{
	GENERATED_BODY()

	FAnimNextScheduleExternalParamTask() = default;

	void UpdateExternalParams(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct FAnimNextSchedulerEntry;

	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	TArray<FAnimNextScheduleExternalParameterSource> ParameterSources;

	UPROPERTY()
	bool bThreadSafe = false;
};
