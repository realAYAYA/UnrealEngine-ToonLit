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
#endif

static TAutoConsoleVariable<bool> CVarForceRegisterOnCookData(
	TEXT("pcg.ForceRegisterOnCookData"),
	true,
	TEXT("Controls whether we will call register/unregister automatically when building the landscape cache."));

static TAutoConsoleVariable<bool> CVarLoadLandscapeInWPOnCookData(
	TEXT("pcg.LoadLandscapeInWPOnCookData"),
	true,
	TEXT("Controls whether we will build the cache from all landscapes in a WP map."));

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGWorldActor)

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

	if (GetWorld())
	{
		// Implementation note: actor references gathered from the world partition helpers will register on creation and unregister on deletion
		// which is why we need to manage this only in the non-WP case.
		TSet<FWorldPartitionReference> ActorRefs;
		UWorldPartition* WorldPartition = GetWorld()->GetWorldPartition();

		if (WorldPartition && CVarLoadLandscapeInWPOnCookData.GetValueOnAnyThread())
		{
			FWorldPartitionHelpers::ForEachActorDesc<ALandscapeProxy>(WorldPartition, [WorldPartition, &ActorRefs](const FWorldPartitionActorDesc* ActorDesc)
			{
				ActorRefs.Add(FWorldPartitionReference(WorldPartition, ActorDesc->GetGuid()));
				return true;
			});
		}
		
		TArray<ALandscapeProxy*> ProxiesToRegisterAndUnregister;
		if (!WorldPartition && CVarForceRegisterOnCookData.GetValueOnAnyThread())
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

		if (!WorldPartition && CVarForceRegisterOnCookData.GetValueOnAnyThread())
		{
			for (ALandscapeProxy* ProxyToUnregister : ProxiesToRegisterAndUnregister)
			{
				ProxyToUnregister->UnregisterAllComponents();
			}
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

void APCGWorldActor::CreateGridGuidsIfNecessary(const PCGHiGenGrid::FSizeArray& InGridSizes)
{
	// Check if any need adding
	ensure(!InGridSizes.IsEmpty());
	PCGHiGenGrid::FSizeArray GridSizesToAdd;
	{
		FReadScopeLock ReadLock(GridGuidsLock);
		for (uint32 GridSize : InGridSizes)
		{
			if (!GridGuids.Contains(GridSize))
			{
				GridSizesToAdd.Push(GridSize);
			}
		}
	}

	if (GridSizesToAdd.Num() > 0)
	{
		FWriteScopeLock WriteLock(GridGuidsLock);

		bool bModified = false;
		for (uint32 GridSize : GridSizesToAdd)
		{
			if (!GridGuids.Contains(GridSize))
			{
				GridGuids.Add(GridSize, FGuid::NewGuid());
				bModified = true;
			}
		}

		if (bModified)
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

void APCGWorldActor::GetGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const
{
	FReadScopeLock ReadLock(GridGuidsLock);
	for (const TPair<uint32, FGuid>& SizeGuid : GridGuids)
	{
		OutSizeToGuidMap.Add(SizeGuid.Key, SizeGuid.Value);
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
		UE_LOG(LogPCG, Error, TEXT("Trying to change the partition grid size while there are partitionned PCGComponents that are refreshing. We cannot stop the refresh for now, so we abort there. You should delete your partition actors manually and regenerate when the refresh is done"));
		return;
	}

	// Then delete all PCGPartitionActors
	PCGSubsystem->DeletePartitionActors(/*bDeleteOnlyUnused=*/false);

	// And finally, refresh all components that are partitioned (registered to the PCGSubsystem)
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
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, PartitionGridSize) 
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APCGWorldActor, bUse2DGrid))
	{
		OnPartitionGridSizeChanged();
	}
}
#endif // WITH_EDITOR
