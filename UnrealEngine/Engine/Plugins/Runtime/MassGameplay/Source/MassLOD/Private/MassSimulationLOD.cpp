// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSimulationLOD.h"

#include "MassCommonFragments.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"
#include "MassLODUtils.h"

//-----------------------------------------------------------------------------
// FMassSimulationLODParameters
//-----------------------------------------------------------------------------
FMassSimulationLODParameters::FMassSimulationLODParameters()
{
	LODDistance[EMassLOD::High] = 0.0f;
	LODDistance[EMassLOD::Medium] = 5000.0f;
	LODDistance[EMassLOD::Low] = 10000.0f;
	LODDistance[EMassLOD::Off] = 30000.0f;

	LODMaxCount[EMassLOD::High] = 100;
	LODMaxCount[EMassLOD::Medium] = 200;
	LODMaxCount[EMassLOD::Low] = 300;
	LODMaxCount[EMassLOD::Off] = INT_MAX;
}

//-----------------------------------------------------------------------------
// FMassSimulationVariableTickParameters
//-----------------------------------------------------------------------------
FMassSimulationVariableTickParameters::FMassSimulationVariableTickParameters()
{
	TickRates[EMassLOD::High] = 0.0f;
	TickRates[EMassLOD::Medium] = 0.5f;
	TickRates[EMassLOD::Low] = 1.0f;
	TickRates[EMassLOD::Off] = 1.5f;
}

//-----------------------------------------------------------------------------
// FMassSimulationLODSharedFragment
//-----------------------------------------------------------------------------
FMassSimulationLODSharedFragment::FMassSimulationLODSharedFragment(const FMassSimulationLODParameters& LODParams)
{
	LODCalculator.Initialize(LODParams.LODDistance, LODParams.BufferHysteresisOnDistancePercentage / 100.0f, LODParams.LODMaxCount);
}

//-----------------------------------------------------------------------------
// FMassSimulationLODSharedFragment
//-----------------------------------------------------------------------------
FMassSimulationVariableTickSharedFragment::FMassSimulationVariableTickSharedFragment(const FMassSimulationVariableTickParameters& TickRateParams)
{
	LODTickRateController.Initialize(TickRateParams.TickRates, TickRateParams.bSpreadFirstSimulationUpdate);
}

//-----------------------------------------------------------------------------
// UMassSimulationLODProcessor
//-----------------------------------------------------------------------------

namespace UE::MassLOD
{
	int32 bDebugSimulationLOD = 0;
	FAutoConsoleVariableRef CVarDebugSimulationLODTest(TEXT("ai.debug.SimulationLOD"), bDebugSimulationLOD, TEXT("Debug Simulation LOD"), ECVF_Cheat);
} // UE::MassLOD

UMassSimulationLODProcessor::UMassSimulationLODProcessor()
	: EntityQuery(*this)
	, EntityQueryCalculateLOD(*this)
	, EntityQueryAdjustDistances(*this)
	, EntityQueryVariableTick(*this)
	, EntityQuerySetLODTag(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LODCollector);
}

void UMassSimulationLODProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassViewerInfoFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassSimulationLODParameters>();
	EntityQuery.AddSharedRequirement<FMassSimulationLODSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddSharedRequirement<FMassSimulationVariableTickSharedFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	EntityQueryCalculateLOD = EntityQuery;
	EntityQueryCalculateLOD.SetChunkFilter(FMassSimulationVariableTickSharedFragment::ShouldCalculateLODForChunk);

	EntityQueryAdjustDistances = EntityQuery;
	EntityQueryAdjustDistances.SetChunkFilter([](const FMassExecutionContext& Context)
	{
		const FMassSimulationLODSharedFragment& LODSharedFragment = Context.GetSharedFragment<FMassSimulationLODSharedFragment>();
		return LODSharedFragment.bHasAdjustedDistancesFromCount && FMassSimulationVariableTickSharedFragment::ShouldAdjustLODFromCountForChunk(Context);
	});

	EntityQueryVariableTick.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQueryVariableTick.AddRequirement<FMassSimulationVariableTickFragment>(EMassFragmentAccess::ReadWrite);
	EntityQueryVariableTick.AddConstSharedRequirement<FMassSimulationVariableTickParameters>();
	EntityQueryVariableTick.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQueryVariableTick.AddSharedRequirement<FMassSimulationVariableTickSharedFragment>(EMassFragmentAccess::ReadOnly);

	// In case where the variableTick isn't enabled, we might need to set LOD tags as if the users still wants them
	EntityQuerySetLODTag.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuerySetLODTag.AddRequirement<FMassSimulationVariableTickFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::None);
	EntityQuerySetLODTag.AddConstSharedRequirement<FMassSimulationLODParameters>();
	EntityQuerySetLODTag.SetChunkFilter([](const FMassExecutionContext& Context)
	{
		const FMassSimulationLODParameters& LODParams = Context.GetConstSharedFragment<FMassSimulationLODParameters>();
		return LODParams.bSetLODTags;
	});

	ProcessorRequirements.AddSubsystemRequirement<UMassLODSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UMassSimulationLODProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("SimulationLOD"))

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("PrepareExecution"));

		const UMassLODSubsystem& LODSubsystem = Context.GetSubsystemChecked<UMassLODSubsystem>(EntityManager.GetWorld());
		const TArray<FViewerInfo>& Viewers = LODSubsystem.GetViewers();

		EntityManager.ForEachSharedFragment<FMassSimulationLODSharedFragment>([&Viewers](FMassSimulationLODSharedFragment& LODSharedFragment)
		{
			LODSharedFragment.LODCalculator.PrepareExecution(Viewers);
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("CalculateLOD"));
		EntityQueryCalculateLOD.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			FMassSimulationLODSharedFragment& LODSharedFragment = Context.GetMutableSharedFragment<FMassSimulationLODSharedFragment>();
			TConstArrayView<FMassViewerInfoFragment> ViewersInfoList = Context.GetFragmentView<FMassViewerInfoFragment>();
			TArrayView<FMassSimulationLODFragment> SimulationLODFragments = Context.GetMutableFragmentView<FMassSimulationLODFragment>();
			LODSharedFragment.LODCalculator.CalculateLOD(Context, ViewersInfoList, SimulationLODFragments);
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AdjustDistancesAndLODFromCount"));
		EntityManager.ForEachSharedFragment<FMassSimulationLODSharedFragment>([](FMassSimulationLODSharedFragment& LODSharedFragment)
		{
			LODSharedFragment.bHasAdjustedDistancesFromCount = LODSharedFragment.LODCalculator.AdjustDistancesFromCount();
		});

		EntityQueryAdjustDistances.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			FMassSimulationLODSharedFragment& LODSharedFragment = Context.GetMutableSharedFragment<FMassSimulationLODSharedFragment>();
			TConstArrayView<FMassViewerInfoFragment> ViewersInfoList = Context.GetFragmentView<FMassViewerInfoFragment>();
			TArrayView<FMassSimulationLODFragment> SimulationLODFragments = Context.GetMutableFragmentView<FMassSimulationLODFragment>();
			LODSharedFragment.LODCalculator.AdjustLODFromCount(Context, ViewersInfoList, SimulationLODFragments);
		});
	}

	UWorld* World = EntityManager.GetWorld();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("VariableTickRates"))
		check(World);
		const float Time = World->GetTimeSeconds();
		EntityQueryVariableTick.ForEachEntityChunk(EntityManager, Context, [Time](FMassExecutionContext& Context)
		{
			FMassSimulationVariableTickSharedFragment& TickRateSharedFragment = Context.GetMutableSharedFragment<FMassSimulationVariableTickSharedFragment>();
			TConstArrayView<FMassSimulationLODFragment> SimulationLODFragments = Context.GetMutableFragmentView<FMassSimulationLODFragment>();
			TArrayView<FMassSimulationVariableTickFragment> SimulationVariableTickFragments = Context.GetMutableFragmentView<FMassSimulationVariableTickFragment>();

			TickRateSharedFragment.LODTickRateController.UpdateTickRateFromLOD(Context, SimulationLODFragments, SimulationVariableTickFragments, Time);
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("SetLODTags"))
		check(World);
		EntityQuerySetLODTag.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			TConstArrayView<FMassSimulationLODFragment> SimulationLODFragments = Context.GetMutableFragmentView<FMassSimulationLODFragment>();
			const int32 NumEntities = Context.GetNumEntities();
			for (int32 Index = 0; Index < NumEntities; ++Index)
			{
				const FMassSimulationLODFragment& EntityLOD = SimulationLODFragments[Index];
				if (EntityLOD.PrevLOD != EntityLOD.LOD)
				{
					const FMassEntityHandle Entity = Context.GetEntity(Index);
					UE::MassLOD::PushSwapTagsCommand(Context.Defer(), Entity, EntityLOD.PrevLOD, EntityLOD.LOD);
				}
			}
		});
	}

	// Optional debug display
	if (UE::MassLOD::bDebugSimulationLOD)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("DebugDisplayLOD"));
		EntityQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext& Context)
		{
			FMassSimulationLODSharedFragment& LODSharedFragment = Context.GetMutableSharedFragment<FMassSimulationLODSharedFragment>();
			TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			TConstArrayView<FMassSimulationLODFragment> SimulationLODList = Context.GetFragmentView<FMassSimulationLODFragment>();
			LODSharedFragment.LODCalculator.DebugDisplayLOD(Context, SimulationLODList, LocationList, World);
		});
	}
}
