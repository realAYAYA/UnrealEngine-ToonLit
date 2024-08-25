// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleParamScopeTask.generated.h"

class UAnimNextSchedule;
class UAnimNextParameterBlock;
class UAnimNextSchedulerWorldSubsystem;
class UAnimNextComponent;

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
struct FAnimNextScheduleParamScopeEntryTask
{
	GENERATED_BODY()

	FAnimNextScheduleParamScopeEntryTask() = default;

private:
	void RunParamScopeEntry(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UAnimNextSchedulerWorldSubsystem;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend class UAnimNextComponent;

	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 TickFunctionIndex = MAX_uint32;

	// The name of the scope
	UPROPERTY()
	FName Scope;

	// Parameters to apply in this scope
	UPROPERTY()
	TArray<TObjectPtr<UAnimNextParameterBlock>> ParameterBlocks;
};

USTRUCT()
struct FAnimNextScheduleParamScopeExitTask
{
	GENERATED_BODY()

	FAnimNextScheduleParamScopeExitTask() = default;

private:
	void RunParamScopeExit(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::FScheduleTickFunction;

	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	// The name of the scope
	UPROPERTY()
	FName Scope;
};
