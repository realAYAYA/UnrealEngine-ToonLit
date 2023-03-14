// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSignalSubsystem.h"
#include "MassCommandBuffer.h"
#include "MassSignalTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Engine/World.h"

CSV_DEFINE_CATEGORY(MassSignalsCounters, true);

void UMassSignalSubsystem::Tick(float DeltaTime)
{
	for (int i = 0; i < DelayedSignals.Num();)
	{
		FDelayedSignal& DelayedSignal = DelayedSignals[i];
		DelayedSignal.DelayInSeconds -= DeltaTime;
		if(DelayedSignal.DelayInSeconds <= 0.0f)
		{
			SignalEntities(DelayedSignal.SignalName, MakeArrayView(DelayedSignal.Entities));
			DelayedSignals.RemoveAtSwap(i);
		}
		else
		{
			i++;
		}
	}
}

TStatId UMassSignalSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMassSignalSubsystem, STATGROUP_Tickables);
}

void UMassSignalSubsystem::SignalEntity(FName SignalName, const FMassEntityHandle Entity)
{
	checkf(Entity.IsSet(), TEXT("Expecting a valid entity to signal"));
	SignalEntities(SignalName, MakeArrayView(&Entity, 1));
}

void UMassSignalSubsystem::SignalEntities(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
	checkf(Entities.Num() > 0, TEXT("Expecting entities to signal"));
	const UE::MassSignal::FSignalDelegate& SignalDelegate = GetSignalDelegateByName(SignalName);
	SignalDelegate.Broadcast(SignalName, Entities);

#if CSV_PROFILER
	FCsvProfiler::RecordCustomStat(*SignalName.ToString(), CSV_CATEGORY_INDEX(MassSignalsCounters), Entities.Num(), ECsvCustomStatOp::Accumulate);
#endif

	UE_CVLOG(Entities.Num() == 1, this, LogMassSignals, Log, TEXT("Raising signal [%s] to entity [%s]"), *SignalName.ToString(), *Entities[0].DebugGetDescription());
	UE_CVLOG(Entities.Num() > 1, this, LogMassSignals, Log, TEXT("Raising signal [%s] to %d entities"), *SignalName.ToString(), Entities.Num());
}

void UMassSignalSubsystem::DelaySignalEntity(FName SignalName, const FMassEntityHandle Entity, const float DelayInSeconds)
{
	checkf(Entity.IsSet(), TEXT("Expecting a valid entity to signal"));
	DelaySignalEntities(SignalName, MakeArrayView(&Entity, 1), DelayInSeconds);
}

void UMassSignalSubsystem::DelaySignalEntities(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, const float DelayInSeconds)
{
	FDelayedSignal& DelayedSignal = DelayedSignals.Emplace_GetRef();
	DelayedSignal.SignalName = SignalName;
	DelayedSignal.Entities = Entities;
	DelayedSignal.DelayInSeconds = DelayInSeconds;
	UE_CVLOG(Entities.Num() == 1, this, LogMassSignals, Log, TEXT("Delay signal [%s] to entity [%s] in %.2f"), *SignalName.ToString(), *Entities[0].DebugGetDescription(), DelayInSeconds);
	UE_CVLOG(Entities.Num() > 1,this, LogMassSignals, Log, TEXT("Delay signal [%s] to %d entities in %.2f"), *SignalName.ToString(), Entities.Num(), DelayInSeconds);
}

void UMassSignalSubsystem::SignalEntityDeferred(FMassExecutionContext& Context, FName SignalName, const FMassEntityHandle Entity)
{
	checkf(Entity.IsSet(), TEXT("Expecting a valid entity to signal"));
	SignalEntitiesDeferred(Context, SignalName, MakeArrayView(&Entity, 1));
}

void UMassSignalSubsystem::SignalEntitiesDeferred(FMassExecutionContext& Context, FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
	checkf(Entities.Num() > 0, TEXT("Expecting entities to signal"));
	Context.Defer().PushCommand<FMassDeferredSetCommand>([SignalName, Entities](const FMassEntityManager& System)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(System.GetWorld());
		SignalSubsystem->SignalEntities(SignalName, Entities);
	});

	UE_CVLOG(Entities.Num() == 1, this, LogMassSignals, Log, TEXT("Raising deferred signal [%s] to entity [%s]"), *SignalName.ToString(), *Entities[0].DebugGetDescription());
	UE_CVLOG(Entities.Num() > 1, this, LogMassSignals, Log, TEXT("Raising deferred signal [%s] to %d entities"), *SignalName.ToString(), Entities.Num());
}

void UMassSignalSubsystem::DelaySignalEntityDeferred(FMassExecutionContext& Context, FName SignalName, const FMassEntityHandle Entity, const float DelayInSeconds)
{
	checkf(Entity.IsSet(), TEXT("Expecting a valid entity to signal"));
	DelaySignalEntitiesDeferred(Context, SignalName, MakeArrayView(&Entity, 1), DelayInSeconds);
}

void UMassSignalSubsystem::DelaySignalEntitiesDeferred(FMassExecutionContext& Context, FName SignalName, TConstArrayView<FMassEntityHandle> Entities, const float DelayInSeconds)
{
	checkf(Entities.Num() > 0, TEXT("Expecting entities to signal"));
	Context.Defer().PushCommand<FMassDeferredSetCommand>([SignalName, Entities, DelayInSeconds](const FMassEntityManager& System)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(System.GetWorld());
		SignalSubsystem->DelaySignalEntities(SignalName, Entities, DelayInSeconds);
	});

	UE_CVLOG(Entities.Num() == 1, this, LogMassSignals, Log, TEXT("Delay deferred signal [%s] to entity [%s] in %.2f"), *SignalName.ToString(), *Entities[0].DebugGetDescription(), DelayInSeconds);
	UE_CVLOG(Entities.Num() > 1,this, LogMassSignals, Log, TEXT("Delay deferred signal [%s] to %d entities in %.2f"), *SignalName.ToString(), Entities.Num(), DelayInSeconds);
}
