// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeDelayTask.generated.h"

USTRUCT()
struct STATETREEMODULE_API FStateTreeDelayTaskInstanceData
{
	GENERATED_BODY()
	
	/** Delay before the task ends. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bRunForever", ClampMin="0.0"))
	float Duration = 1.f;
	
	/** Adds random range to the Duration. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bRunForever", ClampMin="0.0"))
	float RandomDeviation = 0.f;
	
	/** If true the task will run forever until a transition stops it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRunForever = false;

	/** Internal countdown in seconds. */
	float RemainingTime = 0.f;
};

/**
 * Simple task to wait indefinitely or for a given time (in seconds) before succeeding.
 */
USTRUCT(meta = (DisplayName = "Delay Task"))
struct STATETREEMODULE_API FStateTreeDelayTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	typedef FStateTreeDelayTaskInstanceData InstanceDataType;
	
	FStateTreeDelayTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return InstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
