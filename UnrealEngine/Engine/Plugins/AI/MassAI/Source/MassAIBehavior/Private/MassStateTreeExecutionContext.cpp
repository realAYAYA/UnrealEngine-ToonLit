// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassEntityView.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CsvProfiler.h"

namespace UE::MassBehavior
{
bool CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTreeProcessorCollectExternalData);
	
	const FMassStateTreeExecutionContext& MassContext = static_cast<const FMassStateTreeExecutionContext&>(Context); 
	const FMassEntityManager& EntityManager = MassContext.GetEntityManager();
	const UWorld* World = MassContext.GetWorld();
	
	bool bFoundAll = true;
	const FMassEntityView EntityView(EntityManager, MassContext.GetEntity());

	check(ExternalDataDescs.Num() == OutDataViews.Num());

	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& DataDesc = ExternalDataDescs[Index];
		if (DataDesc.Struct == nullptr)
		{
			continue;
		}
		
		if (DataDesc.Struct->IsChildOf(FMassFragment::StaticStruct()))
		{
			const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(DataDesc.Struct);
			FStructView Fragment = EntityView.GetFragmentDataStruct(ScriptStruct);
			if (Fragment.IsValid())
			{
				OutDataViews[Index] = FStateTreeDataView(Fragment);
			}
			else
			{
				if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
				{
					// Note: Not breaking here, so that we can validate all missing ones in one go.
					bFoundAll = false;
				}
			}
		}
		else if (DataDesc.Struct->IsChildOf(FMassSharedFragment::StaticStruct()))
		{
			const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(DataDesc.Struct);
			FConstStructView Fragment = EntityView.GetConstSharedFragmentDataStruct(ScriptStruct);
			if (Fragment.IsValid())
			{
				OutDataViews[Index] = FStateTreeDataView(Fragment.GetScriptStruct(), const_cast<uint8*>(Fragment.GetMemory()));
			}
			else
			{
				if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
				{
					// Note: Not breaking here, so that we can validate all missing ones in one go.
					bFoundAll = false;
				}
			}
		}
		else if (DataDesc.Struct && DataDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			const TSubclassOf<UWorldSubsystem> SubClass = Cast<UClass>(const_cast<UStruct*>(ToRawPtr(DataDesc.Struct)));
			UWorldSubsystem* Subsystem = World->GetSubsystemBase(SubClass);
			if (Subsystem)
			{
				OutDataViews[Index] = FStateTreeDataView(Subsystem);
			}
			else
			{
				if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
				{
					// Note: Not breaking here, so that we can validate all missing ones in one go.
					bFoundAll = false;
				}
			}
		}
	}
	
	return bFoundAll;
}

}; // UE::MassBehavior

FMassStateTreeExecutionContext::FMassStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData,
															   FMassEntityManager& InEntityManager, UMassSignalSubsystem& InSignalSubsystem, FMassExecutionContext& InContext)
	: FStateTreeExecutionContext(InOwner, InStateTree, InInstanceData, FOnCollectStateTreeExternalData::CreateStatic(UE::MassBehavior::CollectExternalData))
	, EntityManager(&InEntityManager)
	, SignalSubsystem(&InSignalSubsystem)
	, EntitySubsystemExecutionContext(&InContext)
{
}

void FMassStateTreeExecutionContext::BeginDelayedTransition(const FStateTreeTransitionDelayedState& DelayedState)
{
	if (SignalSubsystem != nullptr && Entity.IsSet())
	{
		// Tick again after the games time has passed to see if the condition still holds true.
		SignalSubsystem->DelaySignalEntity(UE::Mass::Signals::DelayedTransitionWakeup, Entity, DelayedState.TimeLeft + KINDA_SMALL_NUMBER);
	}
}
