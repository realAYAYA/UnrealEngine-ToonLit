// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGWorldActor.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "Engine/World.h"

APCGWorldActor::APCGWorldActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
	bDefaultOutlinerExpansionState = false;
#endif

	PartitionGridSize = DefaultPartitionGridSize;
	LandscapeCacheObject = ObjectInitializer.CreateDefaultSubobject<UPCGLandscapeCache>(this, TEXT("LandscapeCache"));
}

#if WITH_EDITOR
void APCGWorldActor::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	LandscapeCacheObject->PrimeCache();
}
#endif

void APCGWorldActor::PostLoad()
{
	Super::PostLoad();
	RegisterToSubsystem();
}

void APCGWorldActor::BeginDestroy()
{
	UnregisterFromSubsystem();
	Super::BeginDestroy();
}

#if WITH_EDITOR
APCGWorldActor* APCGWorldActor::CreatePCGWorldActor(UWorld* InWorld)
{
	check(InWorld);
	APCGWorldActor* PCGActor = InWorld->SpawnActor<APCGWorldActor>();
	PCGActor->RegisterToSubsystem();

	return PCGActor;
}
#endif

void APCGWorldActor::RegisterToSubsystem()
{
	UPCGSubsystem* PCGSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UPCGSubsystem>() : nullptr;
	if (PCGSubsystem)
	{
		PCGSubsystem->RegisterPCGWorldActor(this);
	}
}

void APCGWorldActor::UnregisterFromSubsystem()
{
	UPCGSubsystem* PCGSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UPCGSubsystem>() : nullptr;
	if (PCGSubsystem)
	{
		PCGSubsystem->UnregisterPCGWorldActor(this);
	}
}

#if WITH_EDITOR
void APCGWorldActor::OnPartitionGridSizeChanged()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(APCGWorldActor::OnPartitionGridSizeChanged);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UPCGSubsystem* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
	ULevel* Level = World->GetCurrentLevel();
	if (!PCGSubsystem || !Level)
	{
		return;
	}
	
	bool bAllSafeToDelete = true;

	auto AddPartitionComponentAndCheckIfSafeToDelete = [&bAllSafeToDelete](AActor* Actor) -> bool
	{
		TObjectPtr<APCGPartitionActor> PartitionActor = CastChecked<APCGPartitionActor>(Actor);

		if (!PartitionActor->IsSafeForDeletion())
		{
			bAllSafeToDelete = false;
			return true;
		}

		return true;
	};

	UPCGActorHelpers::ForEachActorInLevel<APCGPartitionActor>(Level, AddPartitionComponentAndCheckIfSafeToDelete);

	// TODO: When we have the capability to stop the generation, we should just do that
	// For now, just throw an error
	if (!bAllSafeToDelete)
	{
		UE_LOG(LogPCG, Error, TEXT("Trying to change the partition grid size while there are partitionned PCGComponents that are refreshing. We cannot stop the refresh for now, so we abort there. You should delete your partition actors manually and regenerate when the refresh is done"));
		return;
	}

	// Then delete all PCGPartitionActors
	PCGSubsystem->DeletePartitionActors(/*bDeleteOnlyUnused=*/false);

	// And finally, refresh all components that are partitioned (registered to the PCGSubsystem)
	// to let them recreate the needed PCG Partition Actors.
	for (UPCGComponent* PCGComponent : PCGSubsystem->GetAllRegisteredComponents())
	{
		check(PCGComponent);
		PCGComponent->DirtyGenerated();
		PCGComponent->Refresh();
	}
}

void APCGWorldActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, PartitionGridSize) 
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, bUse2DGrid))
	{
		OnPartitionGridSizeChanged();
	}
}
#endif // WITH_EDITOR