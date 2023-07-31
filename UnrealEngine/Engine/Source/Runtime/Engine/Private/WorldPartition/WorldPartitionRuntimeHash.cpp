// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "Engine/World.h"
#include "Engine/LevelScriptBlueprint.h"
#include "ActorReferencesUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeHash)

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#endif

#define LOCTEXT_NAMESPACE "WorldPartition"

UWorldPartitionRuntimeHash::UWorldPartitionRuntimeHash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

#if WITH_EDITOR
void UWorldPartitionRuntimeHash::OnBeginPlay()
{
	// Mark always loaded actors so that the Level will force reference to these actors for PIE.
	// These actor will then be duplicated for PIE during the PIE world duplication process
	ForceExternalActorLevelReference(/*bForceExternalActorLevelReferenceForPIE*/true);
}

void UWorldPartitionRuntimeHash::OnEndPlay()
{
	// Unmark always loaded actors
	ForceExternalActorLevelReference(/*bForceExternalActorLevelReferenceForPIE*/false);

	// Release references (will unload actors that were not already loaded in the Editor)
	AlwaysLoadedActorsForPIE.Empty();

	ModifiedActorDescListForPIE.Empty();
}

// In PIE, Always loaded cell is not generated. Instead, always loaded actors will be added to AlwaysLoadedActorsForPIE.
// This will trigger loading/registration of these actors in the PersistentLevel (if not already loaded).
// Then, duplication of world for PIE will duplicate only these actors. 
// When stopping PIE, WorldPartition will release these FWorldPartitionReferences which 
// will unload actors that were not already loaded in the non PIE world.
bool UWorldPartitionRuntimeHash::ConditionalRegisterAlwaysLoadedActorsForPIE(const FWorldPartitionActorDescView& ActorDescView, bool bIsMainWorldPartition, bool bIsMainContainer, bool bIsCellAlwaysLoaded)
{
	if (bIsMainWorldPartition && bIsMainContainer && bIsCellAlwaysLoaded && !IsRunningCookCommandlet())
	{
		// This will load the actor if it isn't already loaded
		FWorldPartitionReference Reference(GetOuterUWorldPartition(), ActorDescView.GetGuid());

		if (AActor* AlwaysLoadedActor = FindObject<AActor>(nullptr, *ActorDescView.GetActorSoftPath().ToString()))
		{
			AlwaysLoadedActorsForPIE.Emplace(Reference, AlwaysLoadedActor);

			// Handle child actors
			AlwaysLoadedActor->ForEachComponent<UChildActorComponent>(true, [this, &Reference](UChildActorComponent* ChildActorComponent)
			{
				if (AActor* ChildActor = ChildActorComponent->GetChildActor())
				{
					AlwaysLoadedActorsForPIE.Emplace(Reference, ChildActor);
				}
			});
		}

		return true;
	}

	return false;
}

void UWorldPartitionRuntimeHash::DumpStateLog(FHierarchicalLogArchive& Ar)
{
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s - Persistent Level"), *GetWorld()->GetName());
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("Always loaded Actor Count: %d "), GetWorld()->PersistentLevel->Actors.Num());
	Ar.Printf(TEXT(""));
}

void UWorldPartitionRuntimeHash::ForceExternalActorLevelReference(bool bForceExternalActorLevelReferenceForPIE)
{
	// Do this only on non game worlds prior to PIE so that always loaded actors get duplicated with the world
	if (!GetWorld()->IsGameWorld())
	{
		for (const FAlwaysLoadedActorForPIE& AlwaysLoadedActor : AlwaysLoadedActorsForPIE)
		{
			if (AActor* Actor = AlwaysLoadedActor.Actor)
			{
				Actor->SetForceExternalActorLevelReferenceForPIE(bForceExternalActorLevelReferenceForPIE);
			}
		}
	}
}
#endif

int32 UWorldPartitionRuntimeHash::GetAllStreamingCells(TSet<const UWorldPartitionRuntimeCell*>& Cells, bool bAllDataLayers, bool bDataLayersOnly, const TSet<FName>& InDataLayers) const
{
	ForEachStreamingCells([&Cells, bAllDataLayers, bDataLayersOnly, InDataLayers](const UWorldPartitionRuntimeCell* Cell)
	{
		if (!bDataLayersOnly && !Cell->HasDataLayers())
		{
			Cells.Add(Cell);
		}
		else if (Cell->HasDataLayers() && (bAllDataLayers || Cell->HasAnyDataLayer(InDataLayers)))
		{
			Cells.Add(Cell);
		}
		return true;
	});

	return Cells.Num();
}

bool UWorldPartitionRuntimeHash::GetStreamingCells(const FWorldPartitionStreamingQuerySource& QuerySource, TSet<const UWorldPartitionRuntimeCell*>& OutCells) const
{
	ForEachStreamingCellsQuery(QuerySource, [QuerySource, &OutCells](const UWorldPartitionRuntimeCell* Cell)
	{
		OutCells.Add(Cell);
		return true;
	});

	return !!OutCells.Num();
}

bool UWorldPartitionRuntimeHash::GetStreamingCells(const TArray<FWorldPartitionStreamingSource>& Sources, FStreamingSourceCells& OutActivateCells, FStreamingSourceCells& OutLoadCells) const
{
	ForEachStreamingCellsSources(Sources, [&OutActivateCells, &OutLoadCells](const UWorldPartitionRuntimeCell* Cell, EStreamingSourceTargetState TargetState)
	{
		switch (TargetState)
		{
		case EStreamingSourceTargetState::Loaded:
			OutLoadCells.GetCells().Add(Cell);
			break;
		case EStreamingSourceTargetState::Activated:
			OutActivateCells.GetCells().Add(Cell);
			break;
		}
		return true;
	});

	return !!(OutActivateCells.Num() + OutLoadCells.Num());
}

bool UWorldPartitionRuntimeHash::IsCellRelevantFor(bool bClientOnlyVisible) const
{
	if (bClientOnlyVisible)
	{
		const UWorld* World = GetWorld();
		if (World->IsGameWorld())
		{
			// Dedicated server & listen server without server streaming won't consider client-only visible cells
			const ENetMode NetMode = World->GetNetMode();
			if ((NetMode == NM_DedicatedServer) || ((NetMode == NM_ListenServer) && !GetOuterUWorldPartition()->IsServerStreamingEnabled()))
			{
				return false;
			}
		}
	}
	return true;
}

EWorldPartitionStreamingPerformance UWorldPartitionRuntimeHash::GetStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& CellsToActivate) const
{
	EWorldPartitionStreamingPerformance StreamingPerformance = EWorldPartitionStreamingPerformance::Good;

	if (!CellsToActivate.IsEmpty() && GetWorld()->bMatchStarted)
	{
		UWorld* World = GetWorld();

		for (const UWorldPartitionRuntimeCell* Cell : CellsToActivate)
		{
			if (Cell->GetBlockOnSlowLoading() && !Cell->IsAlwaysLoaded() && Cell->GetStreamingStatus() != LEVEL_Visible)
			{
				EWorldPartitionStreamingPerformance CellPerformance = GetStreamingPerformanceForCell(Cell);
				// Cell Performance is worst than previous cell performance
				if (CellPerformance > StreamingPerformance)
				{
					StreamingPerformance = CellPerformance;
					// Early out performance is critical
					if (StreamingPerformance == EWorldPartitionStreamingPerformance::Critical)
					{
						return StreamingPerformance;
					}
				}
			}
		}
	}

	return StreamingPerformance;
}

void UWorldPartitionRuntimeHash::FStreamingSourceCells::AddCell(const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape)
{
	if (Cell->ShouldResetStreamingSourceInfo())
	{
		Cell->ResetStreamingSourceInfo();
	}

	Cell->AppendStreamingSourceInfo(Source, SourceShape);
	Cells.Add(Cell);
}

#undef LOCTEXT_NAMESPACE
