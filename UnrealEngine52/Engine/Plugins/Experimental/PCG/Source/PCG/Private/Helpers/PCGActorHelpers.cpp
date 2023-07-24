// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGActorHelpers.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"
#include "Helpers/PCGHelpers.h"

#include "EngineUtils.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGActorHelpers)

#if WITH_EDITOR
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
		InSourceComponent->ForEachManagedResource([&MatchingResource, &InParams, SettingsUID](UPCGManagedResource* InResource)
		{
			// Early out if already found a match
			if (MatchingResource)
			{
				return;
			}

			if (UPCGManagedISMComponent* Resource = Cast<UPCGManagedISMComponent>(InResource))
			{
				if (Resource->GetSettingsUID() != SettingsUID)
				{
					return;
				}

				if (UInstancedStaticMeshComponent* ISMC = Resource->GetComponent())
				{
					if (ISMC->NumCustomDataFloats == InParams.NumCustomDataFloats &&
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
	InTargetActor->Modify();

	// Done as in InstancedStaticMesh.cpp
#if WITH_EDITOR
	const bool bMeshHasNaniteData = StaticMesh->NaniteSettings.bEnabled;
#else
	const bool bMeshHasNaniteData = StaticMesh->GetRenderData()->NaniteResources.PageStreamingStates.Num() > 0;
#endif

	FString ComponentName;
	TSubclassOf<UInstancedStaticMeshComponent> ComponentClass;
	if (bMeshHasNaniteData)
	{
		ComponentClass = UInstancedStaticMeshComponent::StaticClass();
		ComponentName += TEXT("ISM");
	}
	else
	{
		ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
		ComponentName += TEXT("HISM");
	}

	ComponentName += TEXT("_") + StaticMesh->GetName();

	UInstancedStaticMeshComponent* ISMC = NewObject<UInstancedStaticMeshComponent>(InTargetActor, ComponentClass, MakeUniqueObjectName(InTargetActor, ComponentClass, FName(ComponentName)));
	InParams.Descriptor.InitComponent(ISMC);
	ISMC->NumCustomDataFloats = InParams.NumCustomDataFloats;

	ISMC->RegisterComponent();
	InTargetActor->AddInstanceComponent(ISMC);

	ISMC->AttachToComponent(InTargetActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
	ISMC->ComponentTags.Add(InSourceComponent->GetFName());
	ISMC->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

	// Create managed resource on source component
	UPCGManagedISMComponent* Resource = NewObject<UPCGManagedISMComponent>(InSourceComponent);
	Resource->SetComponent(ISMC);
	Resource->SetDescriptor(InParams.Descriptor);
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
		// Not in editor, really unlikely to happen but might be slow
		for (const TSoftObjectPtr<AActor>& ManagedActor : ActorsToDelete)
		{
			if (AActor* Actor = ManagedActor.Get())
			{
				if (!ensure(World->DestroyActor(Actor)))
				{
					UE_LOG(LogPCG, Warning, TEXT("Actor %s failed to be destroyed."), *Actor->GetPathName());
				}
			}
		}
	}

	return true;
}

// Note: this is copied verbatim from AIHelpers.h/.cpp
void UPCGActorHelpers::GetActorClassDefaultComponents(const TSubclassOf<AActor>& ActorClass, TArray<UActorComponent*>& OutComponents, const TSubclassOf<UActorComponent>& InComponentClass)
{
	if (!ensure(ActorClass.Get()))
	{
		return;
	}

	UClass* ClassPtr = InComponentClass.Get();
	TArray<UActorComponent*> ResultComponents;

	// Get the components defined on the native class.
	AActor* CDO = ActorClass->GetDefaultObject<AActor>();
	check(CDO);
	if (ClassPtr)
	{
		CDO->GetComponents(InComponentClass, ResultComponents);
	}
	else
	{
		CDO->GetComponents(ResultComponents);
	}

	// Try to get the components off the BP class.
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(*ActorClass);
	if (BPClass)
	{
		// A BlueprintGeneratedClass has a USimpleConstructionScript member. This member has an array of RootNodes
		// which contains the SCSNode for the root SceneComponent and non-SceneComponents. For the SceneComponent
		// hierarchy, each SCSNode knows its children SCSNodes. Each SCSNode stores the component template that will
		// be created when the Actor is spawned.
		//
		// WARNING: This may change in future engine versions!

		TArray<UActorComponent*> Unfiltered;
		// using this semantic to avoid duplicating following loops or adding a filtering check condition inside the loops
		TArray<UActorComponent*>& TmpComponents = ClassPtr ? Unfiltered : ResultComponents;

		// Check added components.
		USimpleConstructionScript* ConstructionScript = BPClass->SimpleConstructionScript;
		if (ConstructionScript)
		{
			for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
			{
				TmpComponents.Add(Node->ComponentTemplate);
			}
		}
		// Check modified inherited components.
		UInheritableComponentHandler* InheritableComponentHandler = BPClass->InheritableComponentHandler;
		if (InheritableComponentHandler)
		{
			for (TArray<FComponentOverrideRecord>::TIterator It = InheritableComponentHandler->CreateRecordIterator(); It; ++It)
			{
				TmpComponents.Add(It->ComponentTemplate);
			}
		}

		// Filter to the ones matching the requested class.
		if (ClassPtr)
		{
			for (UActorComponent* TemplateComponent : Unfiltered)
			{
				if (TemplateComponent->IsA(ClassPtr))
				{
					ResultComponents.Add(TemplateComponent);
				}
			}
		}
	}

	OutComponents = MoveTemp(ResultComponents);
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

AActor* UPCGActorHelpers::SpawnDefaultActor(UWorld* World, TSubclassOf<AActor> ActorClass, FName BaseName, const FTransform& Transform, AActor* Parent)
{
	if (!World || !ActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, ActorClass, BaseName);
	SpawnParams.Owner = Parent;
	AActor* NewActor = World->SpawnActor(*ActorClass, &Transform, SpawnParams);
	
	if (!NewActor)
	{
		return nullptr;
	}

#if WITH_EDITOR
	NewActor->SetActorLabel(SpawnParams.Name.ToString());
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

