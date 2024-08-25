// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "Templates/SubclassOf.h"
#include "AnimNextSchedulePortTask.generated.h"

class UAnimNextSchedule;
class UAnimNextSchedulePort;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FScheduleTask;
	struct FScheduleTickFunction;
	struct FScheduleInstanceData;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

USTRUCT()
struct FAnimNextSchedulePortTask
{
	GENERATED_BODY()

	friend struct UE::AnimNext::FScheduleTask;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::FScheduleInstanceData;

	FAnimNextSchedulePortTask() = default;

private:
	void RunPort(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	// The type of the port
	UPROPERTY()
	TSubclassOf<UAnimNextSchedulePort> Port;

	// Index of each term in the schedule intermediates
	UPROPERTY()
	TArray<uint32> Terms;
};
