// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayMontageStateTreeTask.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayMontageStateTreeTask)

struct FDataRegistryLookup;
struct FDataRegistryId;
struct FMassEntityHandle;

EStateTreeRunStatus FPlayMontageStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Montage == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	ACharacter* Character = Cast<ACharacter>(InstanceData.Actor);
	if (Character == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	
	InstanceData.Time = 0.f;

	// Grab the task duration from the montage.
	InstanceData.ComputedDuration = Montage->GetPlayLength();

	Character->PlayAnimMontage(Montage);
	// @todo: listen anim completed event

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPlayMontageStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.Time += DeltaTime;
	return InstanceData.ComputedDuration <= 0.0f ? EStateTreeRunStatus::Running : (InstanceData.Time < InstanceData.ComputedDuration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded);
}
