// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGActorHelpers.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"
#include "Helpers/PCGHelpers.h"

#include "EngineUtils.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGActorHelpers)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

UInstancedStaticMeshComponent* UPCGActorHelpers::GetOrCreateISMC(AActor* InTargetActor, UPCGComponent* InSourceComponent, uint64 SettingsUID, const FPCGISMCBuilderParameters& InParams)
{
	UPCGManagedISMComponent* MISMC = GetOrCreateManagedISMC(InTargetActor, InSourceComponent, SettingsUID, InParams);
	if (MISMC)
	{
		return MISMC->GetComponent();
	}
	else
	{
		return nullptr;
	}
}

UPCGManagedISMComponent* UPCGActorHelpers::GetOrCreateManagedISMC(AActor* InTargetActor, UPCGComponent* InSourceComponent, uint64 SettingsUID, const FPCGISMCBuilderParameters& InParams)
{
	check(InTargetActor && InSourceComponent);

	const UStaticMesh* StaticMesh = InParams.Descriptor.StaticMesh;
	if (!StaticMesh)
	{
		return nullptr;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorHelpers::GetOrCreateManagedISMC);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorHelpers::GetOrCreateManagedISMC::FindMatchingMISMC);
		UPCGManagedISMComponent* MatchingResource = nullptr;
		InSourceComponent->ForEachManagedResource([&MatchingResource, &InParams, &InTargetActor, SettingsUID](UPCGManagedResource* InResource)
		{
			// Early out if already found a match
			if (MatchingResource)
			{
				return;
			}

			if (UPCGManagedISMComponent* Resource = Cast<UPCGManagedISMComponent>(InResource))
			{
				if (Resource->GetSettingsUID() != SettingsUID || !Resource->CanBeUsed())
				{
					return;
				}

				if (UInstancedStaticMeshComponent* ISMC = Resource->GetComponent())
				{
					if (IsValid(ISMC) &&
						ISMC->GetOwner() == InTargetActor &&
						ISMC->NumCustomDataFloats == InParams.NumCustomDataFloats &&
						Resource->GetDescriptor() == InParams.Descriptor)
					{
						MatchingResource = Resource;
					}
				}
			}
		});

		if (MatchingResource)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorHelpers::GetOrCreateManagedISMC::MarkAsUsed);
			MatchingResource->MarkAsUsed();

			return MatchingResource;
		}
	}

	// No matching ISM component found, let's create a new one
	InTargetActor->Modify(!InSourceComponent->IsInPreviewMode());

	// Done as in InstancedStaticMesh.cpp
#if WITH_EDITOR
	const bool bMeshHasNaniteData = StaticMesh->IsNaniteEnabled();
#else
	const bool bMeshHasNaniteData = StaticMesh->GetRenderData() && StaticMesh->GetRenderData()->HasValidNaniteData();
#endif

	FString ComponentName;
	TSubclassOf<UInstancedStaticMeshComponent> ComponentClass = InParams.Descriptor.ComponentClass;

	// If the component class in invalid, default to HISM.
	if (!ComponentClass)
	{
		ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
	}

	// It's potentially less efficient to put nanite meshes inside of HISMs so decay those to ISM in this case.
	// Note the equality here, not a IsA because we do not want to change derived types either
	if (ComponentClass == UHierarchicalInstancedStaticMeshComponent::StaticClass())
	{
		if (bMeshHasNaniteData)
		{
			ComponentClass = UInstancedStaticMeshComponent::StaticClass();
		}
		else
		{
			ComponentName = TEXT("HISM_");
		}
	}

	if (ComponentClass == UInstancedStaticMeshComponent::StaticClass())
	{
		ComponentName = TEXT("ISM_");
	}

	ComponentName += StaticMesh->GetName();

	const EObjectFlags ObjectFlags = (InSourceComponent->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
	UInstancedStaticMeshComponent* ISMC = NewObject<UInstancedStaticMeshComponent>(InTargetActor, ComponentClass, MakeUniqueObjectName(InTargetActor, ComponentClass, FName(ComponentName)), ObjectFlags);
	InParams.Descriptor.InitComponent(ISMC);
	ISMC->SetNumCustomDataFloats(InParams.NumCustomDataFloats);

	ISMC->RegisterComponent();
	InTargetActor->AddInstanceComponent(ISMC);

	ISMC->AttachToComponent(InTargetActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
	ISMC->ComponentTags.Add(InSourceComponent->GetFName());
	ISMC->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

	// Create managed resource on source component
	UPCGManagedISMComponent* Resource = NewObject<UPCGManagedISMComponent>(InSourceComponent);
	Resource->SetComponent(ISMC);
	Resource->SetDescriptor(InParams.Descriptor);
	if (InTargetActor->GetRootComponent())
	{
		Resource->SetRootLocation(InTargetActor->GetRootComponent()->GetComponentLocation());
	}
	
	Resource->SetSettingsUID(SettingsUID);
	InSourceComponent->AddToManagedResources(Resource);

	return Resource;
}

bool UPCGActorHelpers::DeleteActors(UWorld* World, const TArray<TSoftObjectPtr<AActor>>& ActorsToDelete)
{
	if (!World || ActorsToDelete.Num() == 0)
	{
		return true;
	}

#if WITH_EDITOR
	// Remove potential references to to-be deleted objects from the global selection sets.
	/*if (GIsEditor)
	{
		GEditor->ResetAllSelectionSets();
	}*/

	/** Note: the following code block is commented out because this is currently getting hit when partition actors & partitioned components are unloaded.
	* Destroying the actors is fine, but trying to delete them from SCC is a major issue and it can hang UE.
	* There are multiple code paths calling this method that would need to be revisited if we want to control this a bit better
	* or we can centralize these kind of requirements in the subsystem - this could then be taken care of in a single place, knowing the actual setting.
	*/
	//UWorldPartition* WorldPartition = World->GetWorldPartition();
	//if (!PCGHelpers::IsRuntimeOrPIE() && WorldPartition)
	//{
	//	TArray<FString> PackagesToDeleteFromSCC;
	//	TSet<UPackage*> PackagesToCleanup;

	//	for (const TSoftObjectPtr<AActor>& ManagedActor : ActorsToDelete)
	//	{
	//		// If actor is loaded, just remove from world and keep track of package to cleanup
	//		if (AActor* Actor = ManagedActor.Get())
	//		{
	//			if (UPackage* ActorPackage = Actor->GetExternalPackage())
	//			{
	//				PackagesToCleanup.Emplace(ActorPackage);
	//			}
	//			
	//			if (Actor->GetWorld() == World)
	//			{
	//				World->DestroyActor(Actor);
	//			}
	//			else
	//			{
	//				// If we're here and the world is null, then the actor has either been destroyed already or it will be picked up by GC by design.
	//				// Otherwise, we have bigger issues, something is very wrong.
	//				check(Actor->GetWorld() == nullptr);
	//			}
	//		}
	//		// Otherwise, get from World Partition.
	//		// Note that it is possible that some actors don't exist anymore, so a null here is not a critical condition
	//		else if (const FWorldPartitionActorDesc* ActorDesc = WorldPartition->GetActorDesc(ManagedActor.ToSoftObjectPath()))
	//		{
	//			PackagesToDeleteFromSCC.Emplace(ActorDesc->GetActorPackage().ToString());
	//			WorldPartition->RemoveActor(ActorDesc->GetGuid());
	//		}
	//	}

	//	// Save currently loaded packages so they get deleted
	//	if (PackagesToCleanup.Num() > 0)
	//	{
	//		ObjectTools::CleanupAfterSuccessfulDelete(PackagesToCleanup.Array(), /*bPerformReferenceCheck=*/true);
	//	}

	//	// Delete outstanding unloaded packages
	//	if (PackagesToDeleteFromSCC.Num() > 0)
	//	{
	//		FPackageSourceControlHelper PackageHelper;
	//		if (!PackageHelper.Delete(PackagesToDeleteFromSCC))
	//		{
	//			return false;
	//		}
	//	}
	//}
	//else
#endif
	{
#if WITH_EDITOR
		// Create TX so that dirty actor packages are tracked
		// 
		// Without tracking deleted actor packages will get unloaded on the next GC with no chance to save them first 
		// See FWorldPartitionExternalDirtyActorsTracker::OnAddDirtyActor (Since CL 32133290)
		//
		// The Actors not being referenced by the Dirty Tracker will prevent them from being collected in UWorldPartition::AddReferencedObjects
		// then in UWorldPartition::OnGCPostReachabilityAnalysis all unreachable actors will get processed to remove RF_Standalone flags allowing 
		// the next GC to collect those packages
		//
		// The fix here is to create a dummy transaction so that the deleted actors are tracked but since we don't want to actually push a transaction, we cancel it after the DestroyActor calls
		FScopedTransaction DummyTransaction(NSLOCTEXT("PCGActorHelpers", "DummyTransaction", "DummyTransaction"), World && !World->IsGameWorld());
#endif

		// Not in editor, really unlikely to happen but might be slow
		for (const TSoftObjectPtr<AActor>& ManagedActor : ActorsToDelete)
		{
			// @todo_pcg: Revisit this GetWorld() check when fixing UE-215065
			if (AActor* Actor = ManagedActor.Get(); Actor && Actor->GetWorld())
			{
				if (!ensure(World->DestroyActor(Actor)))
				{
					UE_LOG(LogPCG, Warning, TEXT("Actor %s failed to be destroyed."), *Actor->GetPathName());
				}
			}
		}

#if WITH_EDITOR
		// Cancel the Dummy transaction so that it can't be undone.
		DummyTransaction.Cancel();
#endif
	}

	return true;
}

void UPCGActorHelpers::GetActorClassDefaultComponents(const TSubclassOf<AActor>& ActorClass, TArray<UActorComponent*>& OutComponents, const TSubclassOf<UActorComponent>& InComponentClass)
{
	OutComponents.Reset();
	AActor::ForEachComponentOfActorClassDefault(ActorClass, InComponentClass, [&OutComponents](const UActorComponent* TemplateComponent)
	{
		OutComponents.Add(const_cast<UActorComponent*>(TemplateComponent));
		return true;
	});
}

void UPCGActorHelpers::ForEachActorInLevel(ULevel* Level, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(AActor*)> Callback)
{
	if (!Level)
	{
		return;
	}

	for (AActor* Actor : Level->Actors)
	{
		if (Actor && Actor->IsA(ActorClass))
		{
			if (!Callback(Actor))
			{
				break;
			}
		}
	}
}

void UPCGActorHelpers::ForEachActorInWorld(UWorld* World, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(AActor*)> Callback)
{
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
	{
		if (AActor* Actor = *It)
		{
			if (!Callback(Actor))
			{
				break;
			}
		}
	}
}

AActor* UPCGActorHelpers::SpawnDefaultActor(UWorld* World, ULevel* Level, TSubclassOf<AActor> ActorClass, FName BaseName, const FTransform& Transform, AActor* Parent)
{
	if (!World || !ActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(Level ? Level : World->GetCurrentLevel(), ActorClass, BaseName);

	return SpawnDefaultActor(World, Level, ActorClass, Transform, SpawnParams, Parent);
}

AActor* UPCGActorHelpers::SpawnDefaultActor(UWorld* World, ULevel* Level, TSubclassOf<AActor> ActorClass, const FTransform& Transform, const FActorSpawnParameters& InSpawnParams, AActor* Parent)
{
	if (!World || !ActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams = InSpawnParams;
	if (Level)
	{
		SpawnParams.OverrideLevel = Level;
	}

	if (PCGHelpers::IsRuntimeOrPIE())
	{
		SpawnParams.ObjectFlags |= RF_Transient;
	}

	AActor* NewActor = World->SpawnActor(*ActorClass, &Transform, SpawnParams);
	
	if (!NewActor)
	{
		return nullptr;
	}

	// HACK: until UE-62747 is fixed, we have to force set the scale after spawning the actor
	NewActor->SetActorRelativeScale3D(Transform.GetScale3D());

#if WITH_EDITOR
	if (SpawnParams.Name != NAME_None)
	{
		NewActor->SetActorLabel(SpawnParams.Name.ToString());
	}
#endif // WITH_EDITOR

	USceneComponent* RootComponent = NewActor->GetRootComponent();
	if (!RootComponent)
	{
		RootComponent = NewObject<USceneComponent>(NewActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
		RootComponent->SetWorldTransform(Transform);

		NewActor->SetRootComponent(RootComponent);
		NewActor->AddInstanceComponent(RootComponent);

		RootComponent->RegisterComponent();
	}

	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITOR
	RootComponent->bVisualizeComponent = true;
#endif // WITH_EDITOR

	if (Parent)
	{
		NewActor->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
	}

	return NewActor;
}

FIntVector UPCGActorHelpers::GetCellCoord(FVector InPosition, int InGridSize, bool bUse2DGrid)
{
	check(InGridSize > 0);

	FVector Temp = InPosition / InGridSize;

	// In case of 2D grid, Z coordinate is always 0
	return FIntVector(
		FMath::FloorToInt(Temp.X),
		FMath::FloorToInt(Temp.Y),
		bUse2DGrid ? 0 : FMath::FloorToInt(Temp.Z)
	);
}

