// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGWorldActor.h"

#include "Grid/PCGLandscapeCache.h"
#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "LandscapeProxy.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "UObject/UObjectHash.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGWorldActor)

bool FPCGPartitionActorRecord::operator==(const FPCGPartitionActorRecord& InOther) const
{
	return GridGuid == InOther.GridGuid && GridSize == InOther.GridSize && GridCoords == InOther.GridCoords;
}

uint32 GetTypeHash(const FPCGPartitionActorRecord& In)
{
	uint32 HashResult = HashCombine(GetTypeHash(In.GridGuid), GetTypeHash(In.GridSize));
	HashResult = HashCombine(HashResult, GetTypeHash(In.GridCoords));
	return HashResult;
}

APCGWorldActor::APCGWorldActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
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
	check(LandscapeCacheObject);

	UWorld* World = GetWorld();

	if (World && LandscapeCacheObject->SerializationMode == EPCGLandscapeCacheSerializationMode::SerializeOnlyAtCook)
	{
		// Implementation note: actor references gathered from the world partition helpers will register on creation and unregister on deletion
		// which is why we need to manage this only in the non-WP case.
		TSet<FWorldPartitionReference> ActorRefs;
		TArray<ALandscapeProxy*> ProxiesToRegisterAndUnregister;

		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeProxy>(WorldPartition, [WorldPartition, &ActorRefs](const FWorldPartitionActorDescInstance* ActorDescInstance)
			{
				check(ActorDescInstance);
				// Create WP references only for actors that aren't currently loaded, otherwise we might end up unloading them
				// if their actor desc ref count isn't setup properly
				if (!ActorDescInstance->GetActor())
				{
					ActorRefs.Add(FWorldPartitionReference(WorldPartition, ActorDescInstance->GetGuid()));
				}
				return true;
			});
		}
		else
		{
			// Since we're not in a WP map, the proxies should be outered to this world.
			// Important note: registering the landscape proxies can create objects, which can and will cause issues with the ForEachWithOuter, hence the second loop in which we do the register
			ForEachObjectWithOuter(GetWorld(), [&ProxiesToRegisterAndUnregister](UObject* Object)
			{
				if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Object))
				{
					bool bHasUnregisteredComponents = false;
					LandscapeProxy->ForEachComponent(/*bIncludeFromChildActors=*/false, [&bHasUnregisteredComponents](const UActorComponent* Component)
					{
						if (Component && !Component->IsRegistered())
						{
							bHasUnregisteredComponents = true;
						}
					});

					if (bHasUnregisteredComponents)
					{
						ProxiesToRegisterAndUnregister.Add(LandscapeProxy);
					}
				}
			});

			for (ALandscapeProxy* ProxyToRegister : ProxiesToRegisterAndUnregister)
			{
				ProxyToRegister->RegisterAllComponents();
			}
		}

		LandscapeCacheObject->PrimeCache();

		for (ALandscapeProxy* ProxyToUnregister : ProxiesToRegisterAndUnregister)
		{
			ProxyToUnregister->UnregisterAllComponents();
		}
	}
}
#endif

void APCGWorldActor::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Commented because it was causing issues with landscape proxies guids not being initialized.
		/*if (LandscapeCacheObject.Get())
		{
			// Make sure landscape cache is ready to provide data immediately.
			LandscapeCacheObject->Initialize();
		}*/
	}
}

void APCGWorldActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterToSubsystem();
}

void APCGWorldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void APCGWorldActor::CreateGridGuidsIfNecessary(const PCGHiGenGrid::FSizeArray& InGridSizes, bool bAreGridsSerialized)
{
	if (InGridSizes.IsEmpty())
	{
		return;
	}

	TMap<uint32, FGuid>& GuidsMap = bAreGridsSerialized ? GridGuids : TransientGridGuids;
	FRWLock& GuidsLock = bAreGridsSerialized ? GridGuidsLock : TransientGridGuidsLock;

	// Check if any need adding
	PCGHiGenGrid::FSizeArray GridSizesToAdd;
	{
		FReadScopeLock ReadLock(GuidsLock);
		for (uint32 GridSize : InGridSizes)
		{
			if (!GuidsMap.Contains(GridSize))
			{
				GridSizesToAdd.Push(GridSize);
			}
		}
	}

	if (GridSizesToAdd.Num() > 0)
	{
		FWriteScopeLock WriteLock(GuidsLock);

		bool bModified = false;
		for (uint32 GridSize : GridSizesToAdd)
		{
			if (!GuidsMap.Contains(GridSize))
			{
				GuidsMap.Add(GridSize, FGuid::NewGuid());
				bModified = true;
			}
		}

		if (bModified && bAreGridsSerialized)
		{
			// Set dirty flag if we added guids. Unfortunately if the guids are not up to date, this will produce save prompts
			// to users. However, this was needed to ensure the guids are saved - without this guids were lost and PAs were leaked.
			// The alternative would be to ensure this never happens automatically, but rather only happens when user clicks Generate
			// or etc. However we take a very proactive approach to creating PAs in editor because they can't be created at runtime.

			// Schedule dirtying rather than do immediately because dirtying is a no-op during level load
			if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GetWorld()))
			{
				PCGSubsystem->ScheduleGeneric([this]()
				{
					this->MarkPackageDirty();
					return true;
				}, nullptr, {});
			}
		}
	}
}

void APCGWorldActor::GetSerializedGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const
{
	FReadScopeLock ReadLock(GridGuidsLock);
	for (const TPair<uint32, FGuid>& SizeGuid : GridGuids)
	{
		OutSizeToGuidMap.Add(SizeGuid.Key, SizeGuid.Value);
	}
}

void APCGWorldActor::GetTransientGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const
{
	FReadScopeLock ReadLock(TransientGridGuidsLock);
	for (const TPair<uint32, FGuid>& SizeGuid : TransientGridGuids)
	{
		OutSizeToGuidMap.Add(SizeGuid.Key, SizeGuid.Value);
	}
}

void APCGWorldActor::MergeFrom(APCGWorldActor* OtherWorldActor)
{
	check(OtherWorldActor && this != OtherWorldActor);
	// TODO: Is this really important to check? It seems it can fail, cf FORT-664546. We might want to do something special about it.
	// ensure(PartitionGridSize == OtherWorldActor->PartitionGridSize && bUse2DGrid == OtherWorldActor->bUse2DGrid && GridGuids.OrderIndependentCompareEqual(OtherWorldActor->GridGuids));
	LandscapeCacheObject->TakeOwnership(OtherWorldActor->LandscapeCacheObject);

	// TODO: We could support this better by somehow auto-collapsing new PAs in the same cell into one PA?
	if (SerializedPartitionActorRecords.Num() > 0 && OtherWorldActor->SerializedPartitionActorRecords.Num() > 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Merged two world actors that both manage serialized PCG partition actors, which is not supported. If you have multiple PCG"
			" partition actors in the same cell, you should delete all serialized partition actors via \"Tools > PCG Framework > Delete all PCG partition actors\""
			" and regenerate the partitioned components."));
	}
}

#if WITH_EDITOR
APCGWorldActor* APCGWorldActor::CreatePCGWorldActor(UWorld* InWorld)
{
	APCGWorldActor* PCGActor = nullptr;

	if (InWorld)
	{
		PCGActor = InWorld->SpawnActor<APCGWorldActor>();

		if (PCGActor)
		{
			PCGActor->RegisterToSubsystem();
		}
	}

	return PCGActor;
}
#endif

void APCGWorldActor::RegisterToSubsystem()
{
	if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GetWorld()))
	{
		PCGSubsystem->RegisterPCGWorldActor(this);
	}
}

void APCGWorldActor::UnregisterFromSubsystem()
{
	if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GetWorld()))
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

	UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(World);
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
		UE_LOG(LogPCG, Error, TEXT("Trying to change the partition grid size while there are partitioned PCGComponents that are refreshing. We cannot stop the refresh for now, so we abort there. You should delete your partition actors manually and regenerate when the refresh is done"));
		return;
	}

	// Then delete all PCGPartitionActors
	PCGSubsystem->DeleteSerializedPartitionActors(/*bDeleteOnlyUnused=*/false);
	SerializedPartitionActorRecords.Reset();

	// And finally, regenerate all components that are partitioned (registered to the PCGSubsystem)
	// to let them recreate the needed PCG Partition Actors.
	for (UPCGComponent* PCGComponent : PCGSubsystem->GetAllRegisteredPartitionedComponents())
	{
		check(PCGComponent);
		PCGComponent->DirtyGenerated();
		PCGComponent->Refresh();
	}
}

void APCGWorldActor::PostLoad()
{
	Super::PostLoad();

	// Deprecation - PAs used to be placed on a grid with guid=0. If no grid guids are registered,
	// register a 0 guid now for the current grid size, and this will result in already existing PAs
	// being reused.
	if (GridGuids.IsEmpty())
	{
		GridGuids.Add(PartitionGridSize, FGuid());
	}
}

void APCGWorldActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, PartitionGridSize)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, bUse2DGrid))
	{
		OnPartitionGridSizeChanged();
	}
}
#endif // WITH_EDITOR

void APCGWorldActor::BeginDestroy()
{
	UnregisterFromSubsystem();
	Super::BeginDestroy();
}
