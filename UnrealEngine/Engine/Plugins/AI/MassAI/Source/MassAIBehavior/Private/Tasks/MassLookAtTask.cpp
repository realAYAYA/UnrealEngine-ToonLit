// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/MassLookAtTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassLookAtFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FMassLookAtTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(LookAtHandle);
	
	return true;
}

EStateTreeRunStatus FMassLookAtTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.Time = 0.f;
	
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassLookAtFragment& LookAtFragment = MassContext.GetExternalData(LookAtHandle);

	LookAtFragment.Reset();
	LookAtFragment.LookAtMode = LookAtMode;
	
	if (LookAtMode == EMassLookAtMode::LookAtEntity)
	{
		if (!InstanceData.TargetEntity.IsSet())
		{
			LookAtFragment.LookAtMode = EMassLookAtMode::LookForward;
			MASSBEHAVIOR_LOG(Error, TEXT("Failed LookAt: invalid target entity"));
		}
		else
		{
			LookAtFragment.LookAtMode = EMassLookAtMode::LookAtEntity;
			LookAtFragment.TrackedEntity = InstanceData.TargetEntity;
		}
	}

	LookAtFragment.RandomGazeMode = RandomGazeMode;
	LookAtFragment.RandomGazeYawVariation = RandomGazeYawVariation;
	LookAtFragment.RandomGazePitchVariation = RandomGazePitchVariation;
	LookAtFragment.bRandomGazeEntities = bRandomGazeEntities;

	// A Duration <= 0 indicates that the task runs until a transition in the state tree stops it.
	// Otherwise we schedule a signal to end the task.
	if (InstanceData.Duration > 0.0f)
	{
		UMassSignalSubsystem& MassSignalSubsystem = MassContext.GetExternalData(MassSignalSubsystemHandle);
		MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::LookAtFinished, MassContext.GetEntity(), InstanceData.Duration);
	}

	return EStateTreeRunStatus::Running;
}

void FMassLookAtTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassLookAtFragment& LookAtFragment = MassContext.GetExternalData(LookAtHandle);
	
	LookAtFragment.Reset();
}

EStateTreeRunStatus FMassLookAtTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.Time += DeltaTime;
	
	return InstanceData.Duration <= 0.0f ? EStateTreeRunStatus::Running : (InstanceData.Time < InstanceData.Duration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded);
}
