// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGHelpers.h"
#include "PCGInputOutputSettings.h"
#include "PCGVolume.h"
#include "PCGManagedResource.h"
#include "Data/PCGDifferenceData.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGLandscapeData.h"
#include "Data/PCGLandscapeSplineData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPrimitiveData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGUnionData.h"
#include "Data/PCGVolumeData.h"
#include "Graph/PCGGraphExecutor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Volume.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "Misc/ScopeLock.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "UPCGComponent"

namespace PCGComponent
{
	const bool bSaveOnCleanupAndGenerate = false;
}

bool UPCGComponent::CanPartition() const
{
	// Support/Force partitioning on non-PCG partition actors in WP worlds.
	return GetOwner() && GetOwner()->GetWorld() && GetOwner()->GetWorld()->GetWorldPartition() != nullptr && Cast<APCGPartitionActor>(GetOwner()) == nullptr;
}

bool UPCGComponent::IsPartitioned() const
{
	return bIsPartitioned && CanPartition();
}

void UPCGComponent::SetIsPartitioned(bool bIsNowPartitioned)
{
	if (bIsNowPartitioned == bIsPartitioned)
	{
		return;
	}

	bool bDoActorMapping = bGenerated || PCGHelpers::IsRuntimeOrPIE();

	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (bGenerated)
		{
			CleanupLocalImmediate(/*bRemoveComponents=*/true);
		}

		if (bIsNowPartitioned)
		{
			bIsPartitioned = bIsNowPartitioned;
			Subsystem->RegisterOrUpdatePCGComponent(this, bDoActorMapping);
		}
		else
		{
			Subsystem->UnregisterPCGComponent(this);
			bIsPartitioned = bIsNowPartitioned;
		}
	}
	else
	{
		bIsPartitioned = false;
	}
}

void UPCGComponent::SetGraph_Implementation(UPCGGraph* InGraph)
{
	SetGraphLocal(InGraph);
}

void UPCGComponent::SetGraphLocal(UPCGGraph* InGraph)
{
	if(Graph == InGraph)
	{
		return;
	}

#if WITH_EDITOR
	if (Graph)
	{
		Graph->OnGraphChangedDelegate.RemoveAll(this);
	}
#endif

	Graph = InGraph;

#if WITH_EDITOR
	if (InGraph)
	{
		Graph->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
	}
#endif

	RefreshAfterGraphChanged(Graph, /*bIsStructural=*/true, /*bDirtyInputs=*/true);
}

void UPCGComponent::AddToManagedResources(UPCGManagedResource* InResource)
{
	if (InResource)
	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		GeneratedResources.Add(InResource);
	}
}

void UPCGComponent::ForEachManagedResource(TFunctionRef<void(UPCGManagedResource*)> Func)
{
	FScopeLock ResourcesLock(&GeneratedResourcesLock);
	check(!GeneratedResourcesInaccessible);
	for (TObjectPtr<UPCGManagedResource> ManagedResource : GeneratedResources)
	{
		Func(ManagedResource);
	}
}

bool UPCGComponent::ShouldGenerate(bool bForce, EPCGComponentGenerationTrigger RequestedGenerationTrigger) const
{
	if (!bActivated || !Graph || !GetSubsystem())
	{
		return false;
	}

#if WITH_EDITOR
	// Always run Generate if we are in editor and partitioned since the original component doesn't know the state of the local one.
	if (IsPartitioned() && !PCGHelpers::IsRuntimeOrPIE())
	{
		return true;
	}
#endif

	// A request is invalid only if it was requested "GenerateOnLoad", but it is "GenerateOnDemand"
	// Meaning that all "GenerateOnDemand" requests are always valid, and "GenerateOnLoad" request is only valid if we want a "GenerateOnLoad" trigger.
	bool bValidRequest = !(RequestedGenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad && GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand);

	return ((!bGenerated && bValidRequest) ||
#if WITH_EDITOR
			bDirtyGenerated || 
#endif
			bForce);
}

void UPCGComponent::SetPropertiesFromOriginal(const UPCGComponent* Original)
{
	check(Original);

	EPCGComponentInput NewInputType = Original->InputType;

	// If we're inheriting properties from another component that would have targeted a "special" actor
	// then we must make sure we update the InputType appropriately
	if (NewInputType == EPCGComponentInput::Actor)
	{
		if(Cast<ALandscapeProxy>(Original->GetOwner()) != nullptr && Cast<ALandscapeProxy>(GetOwner()) == nullptr)
		{
			NewInputType = EPCGComponentInput::Landscape;
		}
	}

#if WITH_EDITOR
	const bool bHasDirtyInput = InputType != NewInputType;
	const bool bHasDirtyExclusions = !(ExcludedTags.Num() == Original->ExcludedTags.Num() && ExcludedTags.Includes(Original->ExcludedTags));
	const bool bIsDirty = bHasDirtyInput || bHasDirtyExclusions || Seed != Original->Seed || Graph != Original->Graph;

	if (bHasDirtyExclusions)
	{
		TeardownTrackingCallbacks();
		ExcludedTags = Original->ExcludedTags;
		SetupTrackingCallbacks();
		RefreshTrackingData();
	}
#else
	ExcludedTags = Original->ExcludedTags;
#endif

	InputType = NewInputType;
	Seed = Original->Seed;
	SetGraphLocal(Original->Graph);

	GenerationTrigger = Original->GenerationTrigger;

#if WITH_EDITOR
	// Note that while we dirty here, we won't trigger a refresh since we don't have the required context
	if (bIsDirty)
	{
		Modify();
		DirtyGenerated((bHasDirtyInput ? EPCGComponentDirtyFlag::Input : EPCGComponentDirtyFlag::None) | (bHasDirtyExclusions ? EPCGComponentDirtyFlag::Exclusions : EPCGComponentDirtyFlag::None));
	}
#endif
}

void UPCGComponent::Generate()
{
	if (IsGenerating())
	{
		return;
	}

#if WITH_EDITOR
	FScopedTransaction Transaction(LOCTEXT("PCGGenerate", "Execute generation on PCG component"));
#endif

	GenerateLocal(/*bForce=*/PCGComponent::bSaveOnCleanupAndGenerate);
}

void UPCGComponent::Generate_Implementation(bool bForce)
{
	GenerateLocal(bForce);
}

void UPCGComponent::GenerateLocal(bool bForce)
{
	GenerateInternal(bForce, EPCGComponentGenerationTrigger::GenerateOnDemand, {});
}

FPCGTaskId UPCGComponent::GenerateInternal(bool bForce, EPCGComponentGenerationTrigger RequestedGenerationTrigger, const TArray<FPCGTaskId>& Dependencies)
{
	if (IsGenerating() || !GetSubsystem() || !ShouldGenerate(bForce, RequestedGenerationTrigger))
	{
		return InvalidPCGTaskId;
	}

	Modify();

	CurrentGenerationTask = GetSubsystem()->ScheduleComponent(this, /*bSave=*/bForce, Dependencies);

	return CurrentGenerationTask;
}

FPCGTaskId UPCGComponent::CreateGenerateTask(bool bForce, const TArray<FPCGTaskId>& Dependencies)
{
	if (IsGenerating())
	{
		return InvalidPCGTaskId;
	}

#if WITH_EDITOR
	// TODO: Have a better way to know when we need to generate a new seed.
	//if (bForce && bGenerated && !bDirtyGenerated)
	//{
	//	++Seed;
	//}
#endif

	// Keep track of all the dependencies
	TArray<FPCGTaskId> AdditionalDependencies;
	const TArray<FPCGTaskId>* AllDependencies = &Dependencies;

	if (IsCleaningUp())
	{
		AdditionalDependencies = Dependencies;
		AdditionalDependencies.Add(CurrentCleanupTask);
		AllDependencies = &AdditionalDependencies;
	}
	else if (bGenerated)
	{
		// Immediate pass to mark all resources unused (and remove the ones that cannot be re-used)
		CleanupLocalImmediate(/*bRemoveComponents=*/false);
	}

	const FBox NewBounds = GetGridBounds();
	if (!NewBounds.IsValid)
	{
		OnProcessGraphAborted();
		return InvalidPCGTaskId;
	}

	return GetSubsystem()->ScheduleGraph(this, *AllDependencies);
}

bool UPCGComponent::GetActorsFromTags(const TSet<FName>& InTags, TSet<TWeakObjectPtr<AActor>>& OutActors, bool bCullAgainstLocalBounds)
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	FBox LocalBounds = bCullAgainstLocalBounds ? GetGridBounds() : FBox(EForceInit::ForceInit);

	TArray<AActor*> PerTagActors;

	OutActors.Reset();

	bool bHasValidTag = false;
	for (const FName& Tag : InTags)
	{
		if (Tag != NAME_None)
		{
			bHasValidTag = true;
			UGameplayStatics::GetAllActorsWithTag(World, Tag, PerTagActors);

			for (AActor* Actor : PerTagActors)
			{
				if (!bCullAgainstLocalBounds || LocalBounds.Intersect(GetGridBounds(Actor)))
				{
					OutActors.Emplace(Actor);
				}
			}

			PerTagActors.Reset();
		}
	}

	return bHasValidTag;
}

void UPCGComponent::PostProcessGraph(const FBox& InNewBounds, bool bInGenerated, FPCGContext* Context)
{
	LastGeneratedBounds = InNewBounds;

	CleanupUnusedManagedResources();

	if (bInGenerated)
	{
		bGenerated = true;

		CurrentGenerationTask = InvalidPCGTaskId;

#if WITH_EDITOR
		bDirtyGenerated = false;
		OnPCGGraphGeneratedDelegate.Broadcast(this);
#endif
		// After a successful generation, we also want to call PostGenerateFunctions
		// if we have any. We also need a context.

		if (Context)
		{
			// If the original component is partitioned, local components have to forward
			// their inputs, so that they can be gathered by the original component.
			// We don't have the info on the original component here, so forward for all
			// components.
			Context->OutputData = Context->InputData;

			CallPostGenerateFunctions(Context);
		}
	}
}

void UPCGComponent::CallPostGenerateFunctions(FPCGContext* Context) const
{
	check(Context);

	if (AActor* Owner = GetOwner())
	{
		for (const FName& FunctionName : PostGenerateFunctionNames)
		{
			if (UFunction* PostGenerateFunc = Owner->GetClass()->FindFunctionByName(FunctionName))
			{
				// Validate that the function take the right number of arguments
				if (PostGenerateFunc->NumParms != 1)
				{
					UE_LOG(LogPCG, Error, TEXT("[UPCGComponent] PostGenerateFunction \"%s\" from actor \"%s\" doesn't have exactly 1 parameter. Will skip the call."), *FunctionName.ToString(), *Owner->GetFName().ToString());
					continue;
				}

				bool bIsValid = false;
				TFieldIterator<FProperty> PropIterator(PostGenerateFunc);
				while (PropIterator)
				{
					if (!!(PropIterator->PropertyFlags & CPF_Parm))
					{
						if (FStructProperty* Property = CastField<FStructProperty>(*PropIterator))
						{
							if (Property->Struct == FPCGDataCollection::StaticStruct())
							{
								bIsValid = true;
								break;
							}
						}
					}

					++PropIterator;
				}

				if (bIsValid)
				{
					Owner->ProcessEvent(PostGenerateFunc, &Context->InputData);
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("[UPCGComponent] PostGenerateFunction \"%s\" from actor \"%s\" parameter type is not PCGDataCollection. Will skip the call."), *FunctionName.ToString(), *Owner->GetFName().ToString());
				}
			}
			else
			{
				UE_LOG(LogPCG, Error, TEXT("[UPCGComponent] PostGenerateFunction \"%s\" was not found in the component owner \"%s\"."), *FunctionName.ToString(), *Owner->GetFName().ToString());
			}
		}
	}
}

void UPCGComponent::PostCleanupGraph()
{
	bGenerated = false;
	CurrentCleanupTask = InvalidPCGTaskId;

#if WITH_EDITOR
	OnPCGGraphCleanedDelegate.Broadcast(this);
	bDirtyGenerated = false;
#endif
}

void UPCGComponent::OnProcessGraphAborted()
{
	UE_LOG(LogPCG, Warning, TEXT("Process Graph was called but aborted, check for errors in log if you expected a result."));

	CleanupUnusedManagedResources();

	CurrentGenerationTask = InvalidPCGTaskId;

#if WITH_EDITOR
	bDirtyGenerated = false;
#endif
}

void UPCGComponent::Cleanup()
{
	if ((!bGenerated && !IsGenerating()) || !GetSubsystem() || IsCleaningUp())
	{
		return;
	}

#if WITH_EDITOR
	FScopedTransaction Transaction(LOCTEXT("PCGCleanup", "Clean up PCG component"));
#endif

	CleanupLocal(/*bRemoveComponents=*/true, /*bSave=*/PCGComponent::bSaveOnCleanupAndGenerate);
}

void UPCGComponent::Cleanup_Implementation(bool bRemoveComponents, bool bSave)
{
	CleanupLocal(bRemoveComponents, bSave);
}

void UPCGComponent::CleanupLocal(bool bRemoveComponents, bool bSave)
{
	CleanupInternal(bRemoveComponents, bSave, {});
}

FPCGTaskId UPCGComponent::CleanupInternal(bool bRemoveComponents, bool bSave, const TArray<FPCGTaskId>& Dependencies)
{
	if ((!bGenerated && !IsGenerating()) || !GetSubsystem() || IsCleaningUp())
	{
		return InvalidPCGTaskId;
	}

	Modify();

	CurrentCleanupTask = GetSubsystem()->ScheduleCleanup(this, bRemoveComponents, bSave, Dependencies);
	return CurrentCleanupTask;
}

AActor* UPCGComponent::ClearPCGLink(UClass* TemplateActor)
{
	if (!bGenerated || !GetOwner() || !GetWorld())
	{
		return nullptr;
	}

	// TODO: Perhaps remove this part if we want to do it in the PCG Graph.
	if (IsGenerating() || IsCleaningUp())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	// First create a new actor that will be the new owner of all the resources
	AActor* NewActor = UPCGActorHelpers::SpawnDefaultActor(World, TemplateActor ? TemplateActor : AActor::StaticClass(), TEXT("PCGStamp"), GetOwner()->GetTransform());

	// Then move all resources linked to this component to this actor
	bool bHasMovedResources = MoveResourcesToNewActor(NewActor, /*bCreateChild=*/false);

	// And finally, if we are partitioned, we need to do the same for all PCGActors, in Editor only.
	if (IsPartitioned())
	{
#if WITH_EDITOR
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->ClearPCGLink(this, LastGeneratedBounds, NewActor);
		}
#endif // WITH_EDITOR
	}
	else
	{
		if (bHasMovedResources)
		{
			Cleanup(true);
		}
		else
		{
			World->DestroyActor(NewActor);
			NewActor = nullptr;
		}
	}

	return NewActor;
}

bool UPCGComponent::MoveResourcesToNewActor(AActor* InNewActor, bool bCreateChild)
{
	// Don't move resources if we are generating or cleaning up
	if (IsGenerating() || IsCleaningUp())
	{
		return false;
	}

	check(InNewActor);
	AActor* NewActor = InNewActor;

	bool bHasMovedResources = false;

	Modify();

	if (bCreateChild)
	{
		NewActor = UPCGActorHelpers::SpawnDefaultActor(GetWorld(), NewActor->GetClass(), TEXT("PCGStampChild"), GetOwner()->GetTransform(), NewActor);
		check(NewActor);
	}

	// Trying to move all resources for now. Perhaps in the future we won't want that.
	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (TObjectPtr<UPCGManagedResource>& GeneratedResource : GeneratedResources)
		{
			check(GeneratedResource);
			GeneratedResource->MoveResourceToNewActor(NewActor);
			TSet<TSoftObjectPtr<AActor>> Dummy;
			GeneratedResource->ReleaseIfUnused(Dummy);
			bHasMovedResources = true;
		}

		GeneratedResources.Empty();
	}

	if (!bHasMovedResources && bCreateChild)
	{
		// There was no resource moved, delete the newly spawned actor.
		GetWorld()->DestroyActor(NewActor);
		return false;
	}

	return bHasMovedResources;
}

void UPCGComponent::CleanupLocalImmediate(bool bRemoveComponents)
{
	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;

	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
		{
			if (GeneratedResources[ResourceIndex]->Release(bRemoveComponents, ActorsToDelete))
			{
				GeneratedResources.RemoveAtSwap(ResourceIndex);
			}
		}
	}

	UPCGActorHelpers::DeleteActors(GetWorld(), ActorsToDelete.Array());

	// If bRemoveComponents is true, it means we are in a "real" cleanup, not a pre-cleanup before a generate.
	// So call PostCleanup in this case.
	if (bRemoveComponents)
	{
		PostCleanupGraph();
	}
}

FPCGTaskId UPCGComponent::CreateCleanupTask(bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies)
{
	if ((!bGenerated && !IsGenerating()) || IsPartitioned() || IsCleaningUp())
	{
		return InvalidPCGTaskId;
	}

	// Keep track of all the dependencies
	TArray<FPCGTaskId> AdditionalDependencies;
	const TArray<FPCGTaskId>* AllDependencies = &Dependencies;

	if (IsGenerating())
	{
		AdditionalDependencies = Dependencies;
		AdditionalDependencies.Add(CurrentGenerationTask);
		AllDependencies = &AdditionalDependencies;
	}

	struct CleanupContext
	{
		bool bIsFirstIteration = true;
		int32 ResourceIndex = -1;
		TSet<TSoftObjectPtr<AActor>> ActorsToDelete;
	};

	TSharedPtr<CleanupContext> Context = MakeShared<CleanupContext>();
	TWeakObjectPtr<UPCGComponent> ThisComponentWeakPtr(this);
	TWeakObjectPtr<UWorld> WorldPtr(GetWorld());

	auto CleanupTask = [Context, ThisComponentWeakPtr, WorldPtr, bRemoveComponents]()
	{
		if (UPCGComponent* ThisComponent = ThisComponentWeakPtr.Get())
		{
			// If the component is not valid anymore, just early out
			if (!IsValid(ThisComponent))
			{
				return true;
			}

			FScopeLock ResourcesLock(&ThisComponent->GeneratedResourcesLock);

			// Safeguard to track illegal modifications of the generated resources array while doing cleanup
			if (Context->bIsFirstIteration)
			{
				check(!ThisComponent->GeneratedResourcesInaccessible);
				ThisComponent->GeneratedResourcesInaccessible = true;
				Context->ResourceIndex = ThisComponent->GeneratedResources.Num() - 1;
				Context->bIsFirstIteration = false;
			}

			// Going backward
			if (Context->ResourceIndex >= 0)
			{
				UPCGManagedResource* Resource = ThisComponent->GeneratedResources[Context->ResourceIndex];
				check(Resource);

				if (Resource->Release(bRemoveComponents, Context->ActorsToDelete))
				{
					ThisComponent->GeneratedResources.RemoveAtSwap(Context->ResourceIndex);
				}

				Context->ResourceIndex--;

				// Returning false means the task is not over
				return false;
			}
			else
			{
				ThisComponent->GeneratedResourcesInaccessible = false;
			}
		}

		if (UWorld* World = WorldPtr.Get())
		{
			UPCGActorHelpers::DeleteActors(World, Context->ActorsToDelete.Array());
		}

		return true;
	};

	return GetSubsystem()->ScheduleGeneric(CleanupTask, this, *AllDependencies);
}

void UPCGComponent::CleanupUnusedManagedResources()
{
	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;

	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
		{
			check(GeneratedResources[ResourceIndex]);
			if (GeneratedResources[ResourceIndex]->ReleaseIfUnused(ActorsToDelete))
			{
				GeneratedResources.RemoveAtSwap(ResourceIndex);
			}
		}
	}

	UPCGActorHelpers::DeleteActors(GetWorld(), ActorsToDelete.Array());
}

void UPCGComponent::BeginPlay()
{
	Super::BeginPlay();

	// First if it is partitioned, register itself to the PCGSubsystem, to map the component to all its corresponding PartitionActors
	if (IsPartitioned() && GetSubsystem())
	{
		GetSubsystem()->RegisterOrUpdatePCGComponent(this);
	}

	if(bActivated && !bGenerated && GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad)
	{
		if (IsPartitioned())
		{
			// If we are partitioned, the responsibility of the generation is to the partition actors.
			// but we still need to know that we are currently generated (even if the state is held by the partition actors)
			// TODO: Will be cleaner when we have dynamic association.
			const FBox NewBounds = GetGridBounds();
			if (NewBounds.IsValid)
			{
				PostProcessGraph(NewBounds, true, nullptr);
			}
		}
		else
		{
			GenerateInternal(/*bForce=*/false, EPCGComponentGenerationTrigger::GenerateOnLoad, {});
			bRuntimeGenerated = true;
		}
	}
}

void UPCGComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Always try to unregister itself, if it doesn't exist, it will early out. 
	// Just making sure that we don't left some resources registered while dead.
	if (GetSubsystem())
	{
		GetSubsystem()->UnregisterPCGComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UPCGComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

#if WITH_EDITOR
	SetupActorCallbacks();
#endif
}

void UPCGComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	// This is inspired by UChildActorComponent::DestroyChildActor()
	// In the case of level change or exit, the subsystem will be null
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (!PCGHelpers::IsRuntimeOrPIE())
		{
			Subsystem->UnregisterPCGComponent(this);
		}
	}
#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPCGComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (!ExclusionTags_DEPRECATED.IsEmpty() && ExcludedTags.IsEmpty())
	{
		ExcludedTags.Append(ExclusionTags_DEPRECATED);
		ExclusionTags_DEPRECATED.Reset();
	}

	/** Deprecation code, should be removed once generated data has been updated */
	if (bGenerated && GeneratedResources.Num() == 0)
	{
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		GetOwner()->GetComponents(ISMCs);

		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			if (ISMC->ComponentTags.Contains(GetFName()))
			{
				UPCGManagedISMComponent* ManagedComponent = NewObject<UPCGManagedISMComponent>(this);
				ManagedComponent->GeneratedComponent = ISMC;
				GeneratedResources.Add(ManagedComponent);
			}
		}

		if (GeneratedActors_DEPRECATED.Num() > 0)
		{
			UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(this);
			ManagedActors->GeneratedActors = GeneratedActors_DEPRECATED;
			GeneratedResources.Add(ManagedActors);
			GeneratedActors_DEPRECATED.Reset();
		}
	}
#endif

#if WITH_EDITOR
	SetupCallbacksOnCreation();
#endif
}

#if WITH_EDITOR
void UPCGComponent::SetupCallbacksOnCreation()
{
	SetupActorCallbacks();
	SetupTrackingCallbacks();

	if(!TrackedLandscapes.IsEmpty())
	{
		SetupLandscapeTracking();
	}
	else
	{
		UpdateTrackedLandscape(/*bBoundsCheck=*/false);
	}

	if (Graph)
	{
		Graph->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
	}
}
#endif

void UPCGComponent::BeginDestroy()
{
#if WITH_EDITOR
	if (Graph)
	{
		Graph->OnGraphChangedDelegate.RemoveAll(this);
		Graph = nullptr;
	}

	if (!IsEngineExitRequested())
	{
		TeardownLandscapeTracking();
		TeardownTrackingCallbacks();
		TeardownActorCallbacks();
	}
#endif

	Super::BeginDestroy();
}

void UPCGComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// We can't register to the subsystem in OnRegister if we are at runtime because
	// the landscape can be not loaded yet.
	// It will be done in BeginPlay at runtime.
	if (!PCGHelpers::IsRuntimeOrPIE() && IsPartitioned() && GetSubsystem())
	{
		if (UWorld* World = GetWorld())
		{
			// We won't be able to spawn any actors if we are currently running a construction script.
			if (!World->bIsRunningConstructionScript)
			{
				GetSubsystem()->RegisterOrUpdatePCGComponent(this, bGenerated);
			}
		}
	}
#endif //WITH_EDITOR
}

TStructOnScope<FActorComponentInstanceData> UPCGComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData = MakeStructOnScope<FActorComponentInstanceData, FPCGComponentInstanceData>(this);
	return InstanceData;
}

void UPCGComponent::OnGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	const bool bIsStructural = ((ChangeType & (EPCGChangeType::Edge | EPCGChangeType::Structural)) != EPCGChangeType::None);
	const bool bDirtyInputs = bIsStructural || ((ChangeType & EPCGChangeType::Input) != EPCGChangeType::None);

	RefreshAfterGraphChanged(InGraph, bIsStructural, bDirtyInputs);
}

void UPCGComponent::RefreshAfterGraphChanged(UPCGGraph* InGraph, bool bIsStructural, bool bDirtyInputs)
{
	if (InGraph != Graph)
	{
		return;
	}

#if WITH_EDITOR
	// In editor, since we've changed the graph, we might have changed the tracked actor tags as well
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		TeardownTrackingCallbacks();
		SetupTrackingCallbacks();
		RefreshTrackingData();
		DirtyCacheForAllTrackedTags();

		if (bIsStructural)
		{
			UpdateTrackedLandscape();
		}

		DirtyGenerated(bDirtyInputs ? (EPCGComponentDirtyFlag::Actor | EPCGComponentDirtyFlag::Landscape) : EPCGComponentDirtyFlag::None);
		if (InGraph)
		{
			Refresh();
		}
		else if (!InGraph)
		{
			// With no graph, we clean up
			CleanupLocal(/*bRemoveComponents=*/true, /*bSave=*/ false);
		}

		InspectionCache.Empty();
		return;
	}
#endif

	// Otherwise, if we are in PIE or runtime, force generate if we have a graph (and were generated). Or cleanup if we have no graph
	if (InGraph && bGenerated)
	{
		GenerateLocal(/*bForce=*/true);
	}
	else if (!InGraph)
	{
		CleanupLocal(/*bRemoveComponents=*/true, /*bSave=*/ false);
	}
}

#if WITH_EDITOR
void UPCGComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange)
	{
		const FName PropName = PropertyAboutToChange->GetFName();

		if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, Graph) && Graph)
		{
			Graph->OnGraphChangedDelegate.RemoveAll(this);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, ExcludedTags))
		{
			TeardownTrackingCallbacks();
		}
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UPCGComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property || !IsValid(this))
	{
		return;
	}

	const FName PropName = PropertyChangedEvent.Property->GetFName();

	// Important note: all property changes already go through the OnObjectPropertyChanged, and will be dirtied here.
	// So where only a Refresh is needed, it goes through the "capture all" else case.
	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, bIsPartitioned))
	{
		if (CanPartition())
		{
			// At this point, bIsPartitioned is already set with the new value.
			// But we need to do some cleanup before
			// So keep this new value, and take its negation for the cleanup.
			bool bIsNowPartitioned = bIsPartitioned;
			bIsPartitioned = !bIsPartitioned;

			// SetIsPartioned cleans up before, so keep track if we were generated or not.
			bool bWasGenerated = bGenerated;
			SetIsPartitioned(bIsNowPartitioned);

			// And finally, re-generate if we were generated and activated
			if (bWasGenerated && bActivated)
			{
				GenerateLocal(/*bForce=*/false);
			}
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, Graph))
	{
		if (Graph)
		{
			Graph->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
		}

		RefreshAfterGraphChanged(Graph, /*bIsStructural=*/true, /*bDirtyInputs=*/true);
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, InputType))
	{
		UpdateTrackedLandscape();
		DirtyGenerated(EPCGComponentDirtyFlag::Input);
		Refresh();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, bParseActorComponents))
	{
		DirtyGenerated(EPCGComponentDirtyFlag::Input);
		Refresh();
	}
	// General properties that don't affect behavior
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, ExcludedTags))
	{
		SetupTrackingCallbacks();
		RefreshTrackingData();

		const bool bHadExclusionData = !CachedExclusionData.IsEmpty();
		const bool bHasExcludedActors = !CachedExcludedActors.IsEmpty();

		if(bHadExclusionData || bHasExcludedActors)
		{
			DirtyGenerated(EPCGComponentDirtyFlag::Exclusions);
			Refresh();
		}
	}
	else
	{
		Refresh();
	}
}

void UPCGComponent::PostEditImport()
{
	Super::PostEditImport();

	SetupCallbacksOnCreation();
}

void UPCGComponent::PreEditUndo()
{
	// Here we will keep a copy of flags that we require to keep through the undo
	// so we can have a consistent state
	LastGeneratedBoundsPriorToUndo = LastGeneratedBounds;

	// We don't know what is changing so remove all callbacks
	if (Graph)
	{
		Graph->OnGraphChangedDelegate.RemoveAll(this);
	}

	if (bGenerated)
	{
		// Cleanup so managed resources are cleaned in all cases
		CleanupLocalImmediate(/*bRemoveComponents=*/true);
		// Put back generated flag to its original value so it is captured properly
		bGenerated = true;
	}	
	
	TeardownTrackingCallbacks();
}

void UPCGComponent::PostEditUndo()
{
	LastGeneratedBounds = LastGeneratedBoundsPriorToUndo;

	if (Graph)
	{
		Graph->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
	}

	SetupTrackingCallbacks();
	RefreshTrackingData();
	UpdateTrackedLandscape();
	DirtyGenerated(EPCGComponentDirtyFlag::All);
	DirtyCacheForAllTrackedTags();

	if (bGenerated)
	{
		Refresh();
	}
}

void UPCGComponent::SetupActorCallbacks()
{
	GEngine->OnActorMoved().AddUObject(this, &UPCGComponent::OnActorMoved);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UPCGComponent::OnObjectPropertyChanged);
}

void UPCGComponent::TeardownActorCallbacks()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	GEngine->OnActorMoved().RemoveAll(this);
}

void UPCGComponent::SetupTrackingCallbacks()
{
	CachedTrackedTagsToSettings.Reset();
	if (Graph)
	{
		CachedTrackedTagsToSettings = Graph->GetTrackedTagsToSettings();
	}

	if(!ExcludedTags.IsEmpty() || !CachedTrackedTagsToSettings.IsEmpty())
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UPCGComponent::OnActorAdded);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UPCGComponent::OnActorDeleted);
	}
}

void UPCGComponent::RefreshTrackingData()
{
	GetActorsFromTags(ExcludedTags, CachedExcludedActors, /*bCullAgainstLocalBounds=*/true);

	TSet<FName> TrackedTags;
	CachedTrackedTagsToSettings.GetKeys(TrackedTags);
	GetActorsFromTags(TrackedTags, CachedTrackedActors, /*bCullAgainstLocalBounds=*/false);
	PopulateTrackedActorToTagsMap(/*bForce=*/true);
}

void UPCGComponent::TeardownTrackingCallbacks()
{
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
}

bool UPCGComponent::ActorHasExcludedTag(AActor* InActor) const
{
	if (!InActor)
	{
		return false;
	}

	bool bHasExcludedTag = false;

	for (const FName& Tag : InActor->Tags)
	{
		if (ExcludedTags.Contains(Tag))
		{
			bHasExcludedTag = true;
			break;
		}
	}

	return bHasExcludedTag;
}

bool UPCGComponent::UpdateExcludedActor(AActor* InActor)
{
	// Dirty data in all cases - the tag or positional changes will be picked up in the test later
	if (CachedExcludedActors.Contains(InActor))
	{
		if (TObjectPtr<UPCGData>* ExclusionData = CachedExclusionData.Find(InActor))
		{
			*ExclusionData = nullptr;
		}

		CachedPCGData = nullptr;
		return true;
	}
	// Dirty only if the impact actor is inside the bounds
	else if (ActorHasExcludedTag(InActor) && GetGridBounds().Intersect(GetGridBounds(InActor)))
	{
		CachedPCGData = nullptr;
		return true;
	}
	else
	{
		return false;
	}
}

bool UPCGComponent::ActorIsTracked(AActor* InActor) const
{
	if (!InActor || !Graph)
	{
		return false;
	}

	bool bIsTracked = false;
	for (const FName& Tag : InActor->Tags)
	{
		if (CachedTrackedTagsToSettings.Contains(Tag))
		{
			bIsTracked = true;
			break;
		}
	}

	return bIsTracked;
}

void UPCGComponent::OnActorAdded(AActor* InActor)
{
	const bool bIsExcluded = UpdateExcludedActor(InActor);
	const bool bIsTracked = AddTrackedActor(InActor);

	if (bIsExcluded || bIsTracked)
	{
		DirtyGenerated(bIsExcluded ? EPCGComponentDirtyFlag::Exclusions : EPCGComponentDirtyFlag::None);
		Refresh();
	}
}

void UPCGComponent::OnActorDeleted(AActor* InActor)
{
	const bool bWasExcluded = UpdateExcludedActor(InActor);
	const bool bWasTracked = RemoveTrackedActor(InActor);

	if (bWasExcluded || bWasTracked)
	{
		DirtyGenerated(bWasExcluded ? EPCGComponentDirtyFlag::Exclusions : EPCGComponentDirtyFlag::None);
		Refresh();
	}
}

void UPCGComponent::OnActorMoved(AActor* InActor)
{
	const bool bOwnerMoved = (InActor == GetOwner());
	const bool bLandscapeMoved = (InActor && TrackedLandscapes.Contains(InActor));

	if (bOwnerMoved || bLandscapeMoved)
	{
		// TODO: find better metrics to dirty the inputs. 
		// TODO: this should dirty only the actor pcg data.
		{
			UpdateTrackedLandscape();
			DirtyGenerated((bOwnerMoved ? EPCGComponentDirtyFlag::Actor : EPCGComponentDirtyFlag::None) | (bLandscapeMoved ? EPCGComponentDirtyFlag::Landscape : EPCGComponentDirtyFlag::None));
			Refresh();
		}
	}
	else
	{
		bool bDirtyAndRefresh = false;
		bool bDirtyExclusions = false;

		if (UpdateExcludedActor(InActor))
		{
			bDirtyAndRefresh = true;
			bDirtyExclusions = true;
		}

		if (DirtyTrackedActor(InActor))
		{
			bDirtyAndRefresh = true;
		}

		if (bDirtyAndRefresh)
		{
			DirtyGenerated(bDirtyExclusions ? EPCGComponentDirtyFlag::Exclusions : EPCGComponentDirtyFlag::None);
			Refresh();
		}
	}
}

void UPCGComponent::UpdateTrackedLandscape(bool bBoundsCheck)
{
	TeardownLandscapeTracking();
	TrackedLandscapes.Reset();

	if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(GetOwner()))
	{
		TrackedLandscapes.Add(Landscape);
	}
	else if (InputType == EPCGComponentInput::Landscape || GraphUsesLandscapePin())
	{
		if (UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr)
		{
			if (bBoundsCheck)
			{
				UPCGData* ActorData = GetActorPCGData();
				if (const UPCGSpatialData* ActorSpatialData = Cast<const UPCGSpatialData>(ActorData))
				{
					TrackedLandscapes = PCGHelpers::GetLandscapeProxies(World, ActorSpatialData->GetBounds());
				}
			}
			else
			{
				TrackedLandscapes = PCGHelpers::GetAllLandscapeProxies(World);
			}
		}
	}

	SetupLandscapeTracking();
}

void UPCGComponent::SetupLandscapeTracking()
{
	for (TWeakObjectPtr<ALandscapeProxy> LandscapeProxy : TrackedLandscapes)
	{
		if (LandscapeProxy.IsValid())
		{
			LandscapeProxy->OnComponentDataChanged.AddUObject(this, &UPCGComponent::OnLandscapeChanged);
		}
	}
}

void UPCGComponent::TeardownLandscapeTracking()
{
	for (TWeakObjectPtr<ALandscapeProxy> LandscapeProxy : TrackedLandscapes)
	{
		if (LandscapeProxy.IsValid())
		{
			LandscapeProxy->OnComponentDataChanged.RemoveAll(this);
		}
	}
}

void UPCGComponent::OnLandscapeChanged(ALandscapeProxy* Landscape, const FLandscapeProxyComponentDataChangedParams& ChangeParams)
{
	if(TrackedLandscapes.Contains(Landscape))
	{
		// Check if there is an overlap in the changed components vs. the current actor data
		EPCGComponentDirtyFlag DirtyFlag = EPCGComponentDirtyFlag::None;

		if (GetOwner() == Landscape)
		{
			DirtyFlag = EPCGComponentDirtyFlag::Actor;
		}
		// Note: this means that graphs that are interacting with the landscape outside their bounds might not be updated properly
		else if (InputType == EPCGComponentInput::Landscape || GraphUsesLandscapePin())
		{
			UPCGData* ActorData = GetActorPCGData();
			if (const UPCGSpatialData* ActorSpatialData = Cast<const UPCGSpatialData>(ActorData))
			{
				const FBox ActorBounds = ActorSpatialData->GetBounds();
				bool bDirtyLandscape = false;

				ChangeParams.ForEachComponent([&bDirtyLandscape, &ActorBounds](const ULandscapeComponent* LandscapeComponent)
				{
					if(LandscapeComponent && ActorBounds.Intersect(LandscapeComponent->Bounds.GetBox()))
					{
						bDirtyLandscape = true;
					}
				});

				if (bDirtyLandscape)
				{
					DirtyFlag = EPCGComponentDirtyFlag::Landscape;
				}
			}
		}

		if (DirtyFlag != EPCGComponentDirtyFlag::None)
		{
			DirtyGenerated(DirtyFlag);
			Refresh();
		}
	}
}

void UPCGComponent::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	bool bValueNotInteractive = (InEvent.ChangeType != EPropertyChangeType::Interactive);
	// Special exception for actor tags, as we can't track otherwise an actor "losing" a tag
	bool bActorTagChange = (InEvent.Property && InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags));
	if (!bValueNotInteractive && !bActorTagChange)
	{
		return;
	}

	// If the object changed is this PCGComponent, dirty ourselves and exit. It will be picked up by PostEditChangeProperty
	if (InObject == this)
	{
		DirtyGenerated();
		return;
	}

	// First, check if it's an actor
	AActor* Actor = Cast<AActor>(InObject);

	// Otherwise, if it's an actor component, track it as well
	if (!Actor)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(InObject))
		{
			Actor = ActorComponent->GetOwner();
		}
	}

	// Finally, if it's neither an actor or an actor component, it might be a dependency of a tracked actor
	if (!Actor)
	{
		for(const TPair<TWeakObjectPtr<AActor>, TSet<TObjectPtr<UObject>>>& TrackedActor : CachedTrackedActorToDependencies)
		{
			if (TrackedActor.Value.Contains(InObject))
			{
				OnActorChanged(TrackedActor.Key.Get(), InObject, bActorTagChange);
			}
		}
	}
	else
	{
		OnActorChanged(Actor, InObject, bActorTagChange);
	}
}

void UPCGComponent::OnActorChanged(AActor* Actor, UObject* InObject, bool bActorTagChange)
{
	if (Actor == GetOwner())
	{
		// Something has changed on the owner (including properties of this component)
		// In the case of splines, this is where we'd get notified if some component properties (incl. spline vertices) have changed
		// TODO: this should dirty only the actor pcg data.
		DirtyGenerated(EPCGComponentDirtyFlag::Actor);
		Refresh();
	}
	else if(Actor)
	{
		bool bDirtyAndRefresh = false;

		if (UpdateExcludedActor(Actor))
		{
			bDirtyAndRefresh = true;
		}

		if ((bActorTagChange && Actor == InObject && UpdateTrackedActor(Actor)) || DirtyTrackedActor(Actor))
		{
			bDirtyAndRefresh = true;
		}

		if (bDirtyAndRefresh)
		{
			DirtyGenerated();
			Refresh();
		}
	}
}

void UPCGComponent::DirtyGenerated(EPCGComponentDirtyFlag DirtyFlag)
{
	bDirtyGenerated = true;

	// Dirty data as a waterfall from basic values
	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Actor))
	{
		CachedActorData = nullptr;
		
		if (Cast<ALandscapeProxy>(GetOwner()))
		{
			CachedLandscapeData = nullptr;
			CachedLandscapeHeightData = nullptr;
		}

		CachedInputData = nullptr;
		CachedPCGData = nullptr;
	}
	
	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Landscape))
	{
		CachedLandscapeData = nullptr;
		CachedLandscapeHeightData = nullptr;
		if (InputType == EPCGComponentInput::Landscape)
		{
			CachedInputData = nullptr;
			CachedPCGData = nullptr;
		}
	}

	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Input))
	{
		CachedInputData = nullptr;
		CachedPCGData = nullptr;
	}

	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Exclusions))
	{
		CachedExclusionData.Reset();
		CachedPCGData = nullptr;
	}

	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Data))
	{
		CachedPCGData = nullptr;
	}

	// For partitioned graph, we must forward the call to the partition actor
	// Note that we do not need to forward "normal" dirty as these will be picked up by the local PCG components
	// However, input changes / moves of the partitioned object will not be caught
	// It would be possible for partitioned actors to add callbacks to their original component, but that inverses the processing flow
	if (DirtyFlag != EPCGComponentDirtyFlag::None && bActivated && IsPartitioned())
	{
		if (GetSubsystem())
		{
			GetSubsystem()->DirtyGraph(this, LastGeneratedBounds, DirtyFlag);
		}
	}
}

void UPCGComponent::ResetLastGeneratedBounds()
{
	LastGeneratedBounds = FBox(EForceInit::ForceInit);
}
void UPCGComponent::DisableInspection()
{
	bIsInspecting = false;
	InspectionCache.Empty();
};

void UPCGComponent::StoreInspectionData(const UPCGNode* InNode, const FPCGDataCollection& InInspectionData)
{
	if (!InNode)
	{
		return;
	}

	if (GetGraph() != InNode->GetGraph())
	{
		return;
	}

	InspectionCache.Add(InNode, InInspectionData);
}

const FPCGDataCollection* UPCGComponent::GetInspectionData(const UPCGNode* InNode) const
{
	return InspectionCache.Find(InNode);
}

void UPCGComponent::Refresh()
{
	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (GetOwner() && GetOwner()->bIsEditorPreviewActor)
	{
		return;
	}

	// Discard any refresh if have already one scheduled.
	if (CurrentRefreshTask != InvalidPCGTaskId)
	{
		return;
	}

	// Before doing a refresh, update the component to the subsystem if we are partitioned
	// Only redo the mapping if we are generated
	if (IsPartitioned() && GetSubsystem())
	{
		GetSubsystem()->RegisterOrUpdatePCGComponent(this, /*bDoActorMapping=*/ bGenerated);
	}

	// Following a change in some properties or in some spatial information related to this component,
	// We need to regenerate/cleanup the graph, depending of the state in the editor.
	if (!bActivated)
	{
		bool bWasGenerated = bGenerated;
		CleanupLocalImmediate(/*bRemoveComponents=*/true);
		bGenerated = bWasGenerated;
	}
	else
	{
		if (bGenerated && bRegenerateInEditor)
		{
			GenerateLocal(/*bForce=*/false);
		}
	}
}
#endif // WITH_EDITOR

UPCGData* UPCGComponent::GetPCGData()
{
	if (!CachedPCGData)
	{
		CachedPCGData = CreatePCGData();
	}

	return CachedPCGData;
}

UPCGData* UPCGComponent::GetInputPCGData()
{
	if (!CachedInputData)
	{
		CachedInputData = CreateInputPCGData();
	}

	return CachedInputData;
}

UPCGData* UPCGComponent::GetActorPCGData()
{
	// Actor PCG Data can be a Landscape data too
	if (!CachedActorData || IsLandscapeCachedDataDirty(CachedActorData))
	{
		CachedActorData = CreateActorPCGData();
	}

	return CachedActorData;
}

UPCGData* UPCGComponent::GetLandscapePCGData()
{
	if (!CachedLandscapeData || IsLandscapeCachedDataDirty(CachedLandscapeData))
	{
		CachedLandscapeData = CreateLandscapePCGData(/*bHeightOnly=*/false);
	}

	return CachedLandscapeData;
}

UPCGData* UPCGComponent::GetLandscapeHeightPCGData()
{
	if (!CachedLandscapeHeightData || IsLandscapeCachedDataDirty(CachedLandscapeHeightData))
	{
		CachedLandscapeHeightData = CreateLandscapePCGData(/*bHeightOnly=*/true);
	}

	return CachedLandscapeHeightData;
}

UPCGData* UPCGComponent::GetOriginalActorPCGData()
{
	if (APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(GetOwner()))
	{
		if (UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(this))
		{
			return OriginalComponent->GetActorPCGData();
		}
	}
	else
	{
		return GetActorPCGData();
	}

	return nullptr;
}

TArray<UPCGData*> UPCGComponent::GetPCGExclusionData()
{
	// TODO: replace with a boolean, unify.
	UpdatePCGExclusionData();

	TArray<typename decltype(CachedExclusionData)::ValueType> ExclusionData;
	CachedExclusionData.GenerateValueArray(ExclusionData);
	return ToRawPtrTArrayUnsafe(ExclusionData);
}

void UPCGComponent::UpdatePCGExclusionData()
{
	const UPCGData* InputData = GetInputPCGData();
	const UPCGSpatialData* InputSpatialData = Cast<const UPCGSpatialData>(InputData);

	// Update the list of cached excluded actors here, since we might not have picked up everything on map load (due to WP)
	GetActorsFromTags(ExcludedTags, CachedExcludedActors, /*bCullAgainstLocalBounds=*/true);

	// Build exclusion data based on the CachedExcludedActors
	decltype(CachedExclusionData) ExclusionData;

	for(TWeakObjectPtr<AActor> ExcludedActorWeakPtr : CachedExcludedActors)
	{
		if(!ExcludedActorWeakPtr.IsValid())
		{
			continue;
		}

		AActor* ExcludedActor = ExcludedActorWeakPtr.Get();

		TObjectPtr<UPCGData>* PreviousExclusionData = CachedExclusionData.Find(ExcludedActor);

		if (PreviousExclusionData && *PreviousExclusionData)
		{
			ExclusionData.Add(ExcludedActor, *PreviousExclusionData);
		}
		else
		{
			// Create the new exclusion data
			UPCGData* ActorData = CreateActorPCGData(ExcludedActor);
			UPCGSpatialData* ActorSpatialData = Cast<UPCGSpatialData>(ActorData);

			if (InputSpatialData && ActorSpatialData)
			{
				// Change the target actor to this - otherwise we could push changes on another actor
				ActorSpatialData->TargetActor = GetOwner();

				// Create intersection or projection depending on the dimension
				// TODO: there's an ambiguity here when it's the same dimension.
				// For volumes, we'd expect an intersection, for surfaces we'd expect a projection
				if (ActorSpatialData->GetDimension() > InputSpatialData->GetDimension())
				{
					ExclusionData.Add(ExcludedActor, ActorSpatialData->IntersectWith(InputSpatialData));
				}
				else
				{
					ExclusionData.Add(ExcludedActor, ActorSpatialData->ProjectOn(InputSpatialData));
				}
			}
		}
	}

	CachedExclusionData = ExclusionData;
}

UPCGData* UPCGComponent::CreateActorPCGData()
{
	return CreateActorPCGData(GetOwner(), bParseActorComponents);
}

UPCGData* UPCGComponent::CreateActorPCGData(AActor* Actor, bool bParseActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreateActorPCGData);

	if (!Actor)
	{
		return nullptr;
	}

	// In this case, we'll build the data type that's closest to known actor types
	// TODO: add factory for extensibility
	if (APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(Actor))
	{
		check(GetOwner() == Actor); // Invalid processing otherwise because of the this usage
		if (UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(this))
		{
			check(OriginalComponent->IsPartitioned());
			// TODO: cache/share the original component's actor pcg data
			if (const UPCGSpatialData* OriginalComponentSpatialData = Cast<const UPCGSpatialData>(OriginalComponent->GetActorPCGData()))
			{
				UPCGVolumeData* Data = NewObject<UPCGVolumeData>();
				Data->Initialize(PartitionActor->GetFixedBounds(), PartitionActor);

				return Data->IntersectWith(OriginalComponentSpatialData);
			}
		}

		// TODO: review this once we support non-spatial data?
		return nullptr;
	}
	else if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Actor))
	{
		UPCGLandscapeData* Data = NewObject<UPCGLandscapeData>();
		Data->Initialize({ Landscape }, GetGridBounds(Actor), /*bHeightOnly=*/false, /*bUseMetadata=*/Graph && Graph->bLandscapeUsesMetadata);

		return Data;
	}
	else if (AVolume* Volume = Cast<AVolume>(Actor))
	{
		UPCGVolumeData* Data = NewObject<UPCGVolumeData>();
		Data->Initialize(Volume);

		return Data;
	}
	else if (bParseActor)// Prepare data on a component basis
	{
		TInlineComponentArray<ULandscapeSplinesComponent*, 1> LandscapeSplines;
		Actor->GetComponents(LandscapeSplines);

		TInlineComponentArray<USplineComponent*, 1> Splines;
		Actor->GetComponents(Splines);

		TInlineComponentArray<UShapeComponent*, 1> Shapes;
		Actor->GetComponents(Shapes);

		// Don't get generic primitives unless it's the only thing we can find.
		TInlineComponentArray<UPrimitiveComponent*, 1> OtherPrimitives;
		if (LandscapeSplines.Num() == 0 && Splines.Num() == 0 && Shapes.Num() == 0)
		{
			Actor->GetComponents(OtherPrimitives);
		}

		UPCGUnionData* Union = nullptr;
		if (LandscapeSplines.Num() + Splines.Num() + Shapes.Num() + OtherPrimitives.Num() > 1)
		{
			Union = NewObject<UPCGUnionData>();
		}

		for (ULandscapeSplinesComponent* SplineComponent : LandscapeSplines)
		{
			UPCGLandscapeSplineData* SplineData = NewObject<UPCGLandscapeSplineData>();
			SplineData->Initialize(SplineComponent);

			if (Union)
			{
				Union->AddData(SplineData);
			}
			else
			{
				return SplineData;
			}
		}

		for (USplineComponent* SplineComponent : Splines)
		{
			UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
			SplineData->Initialize(SplineComponent);

			if (Union)
			{
				Union->AddData(SplineData);
			}
			else
			{
				return SplineData;
			}
		}

		for (UShapeComponent* ShapeComponent : Shapes)
		{
			UPCGPrimitiveData* ShapeData = NewObject<UPCGPrimitiveData>();
			ShapeData->Initialize(ShapeComponent);
			
			if (Union)
			{
				Union->AddData(ShapeData);
			}
			else
			{
				return ShapeData;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : OtherPrimitives)
		{
			UPCGPrimitiveData* PrimitiveData = NewObject<UPCGPrimitiveData>();
			PrimitiveData->Initialize(PrimitiveComponent);

			if (Union)
			{
				Union->AddData(PrimitiveData);
			}
			else
			{
				return PrimitiveData;
			}
		}

		if (Union)
		{
			return Union;
		}
	}

	// Finally, if it's not a special actor and there are not parsed components, then return a single point at the actor position
	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromActor(Actor);
	return Data;
}

UPCGData* UPCGComponent::CreatePCGData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreatePCGData);
	UPCGData* InputData = GetInputPCGData();
	UPCGSpatialData* SpatialInput = Cast<UPCGSpatialData>(InputData);
	
	// Early out: incompatible data
	if (!SpatialInput)
	{
		return InputData;
	}

	UPCGDifferenceData* Difference = nullptr;
	TArray<UPCGData*> ExclusionData = GetPCGExclusionData();

	for (UPCGData* Exclusion : ExclusionData)
	{
		if (UPCGSpatialData* SpatialExclusion = Cast<UPCGSpatialData>(Exclusion))
		{
			if (!Difference)
			{
				Difference = SpatialInput->Subtract(SpatialExclusion);
			}
			else
			{
				Difference->AddDifference(SpatialExclusion);
			}
		}
	}

	return Difference ? Difference : InputData;
}

UPCGData* UPCGComponent::CreateLandscapePCGData(bool bHeightOnly)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreateLandscapePCGData);
	AActor* Actor = GetOwner();

	if (!Actor)
	{
		return nullptr;
	}

	UPCGData* ActorData = GetActorPCGData();

	if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Actor))
	{
		return ActorData;
	}

	const UPCGSpatialData* ActorSpatialData = Cast<const UPCGSpatialData>(ActorData);

	FBox ActorBounds;

	if (ActorSpatialData)
	{
		ActorBounds = ActorSpatialData->GetBounds();
	}
	else
	{
		FVector Origin;
		FVector Extent;
		Actor->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
		ActorBounds = FBox::BuildAABB(Origin, Extent);
	}

	TArray<TWeakObjectPtr<ALandscapeProxy>> Landscapes = PCGHelpers::GetLandscapeProxies(Actor->GetWorld(), ActorBounds);

	if (Landscapes.IsEmpty())
	{
		// No landscape found
		return nullptr;
	}

	FBox LandscapeBounds(EForceInit::ForceInit);

	for (TWeakObjectPtr<ALandscapeProxy> Landscape : Landscapes)
	{
		if (Landscape.IsValid())
		{
			LandscapeBounds += GetGridBounds(Landscape.Get());
		}
	}

	// TODO: we're creating separate landscape data instances here so we can do some tweaks on it (such as storing the right target actor) but this probably should change
	UPCGLandscapeData* LandscapeData = NewObject<UPCGLandscapeData>();
	LandscapeData->Initialize(Landscapes, LandscapeBounds, bHeightOnly, /*bUseMetadata=*/Graph && Graph->bLandscapeUsesMetadata);
	// Need to override target actor for this one, not the landscape
	LandscapeData->TargetActor = Actor;

	return LandscapeData;
}

UPCGData* UPCGComponent::CreateInputPCGData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreateInputPCGData);
	AActor* Actor = GetOwner();
	check(Actor);

	// Construct proper input based on input type
	if (InputType == EPCGComponentInput::Actor)
	{
		return GetActorPCGData();
	}
	else if (InputType == EPCGComponentInput::Landscape)
	{
		UPCGData* ActorData = GetActorPCGData();

		const UPCGSpatialData* ActorSpatialData = Cast<const UPCGSpatialData>(ActorData);

		if (!ActorSpatialData)
		{
			// TODO ? support non-spatial data on landscape?
			return nullptr;
		}

		const UPCGSpatialData* LandscapeData = Cast<const UPCGSpatialData>(GetLandscapePCGData());

		if (!LandscapeData)
		{
			return nullptr;
		}

		if (LandscapeData == ActorSpatialData)
		{
			return ActorData;
		}

		// Decide whether to intersect or project
		// Currently, it makes sense to intersect only for volumes;
		// Note that we don't currently check for a volume object but only on dimension
		// so intersections (such as volume X partition actor) get picked up properly
		if (ActorSpatialData->GetDimension() >= 3)
		{
			return LandscapeData->IntersectWith(ActorSpatialData);
		}
		else
		{
			return ActorSpatialData->ProjectOn(LandscapeData);
		}
	}
	else
	{
		// In this case, the input data will be provided in some other form,
		// Most likely to be stored in the PCG data grid.
		return nullptr;
	}
}

bool UPCGComponent::IsLandscapeCachedDataDirty(const UPCGData* Data) const
{
	bool IsCacheDirty = false;

	if (const UPCGLandscapeData* CachedData = Cast<UPCGLandscapeData>(Data))
	{
		IsCacheDirty = Graph && (CachedData->IsUsingMetadata() != Graph->bLandscapeUsesMetadata);
	}

	return IsCacheDirty;
}

FBox UPCGComponent::GetGridBounds() const
{
	return GetGridBounds(GetOwner());
}

FBox UPCGComponent::GetGridBounds(AActor* Actor) const
{
	check(Actor);

	FBox Bounds(EForceInit::ForceInit);

	if (APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(Actor))
	{
		// First, get the bounds from the partition actor
		Bounds = PartitionActor->GetFixedBounds();

		// Then intersect with the original component's bounds.
		if (const UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(this))
		{
			if (OriginalComponent->GetOwner() != PartitionActor)
			{
				Bounds = Bounds.Overlap(OriginalComponent->GetGridBounds());
			}
		}
	}
	// TODO: verify this works as expected in non-editor builds
	else if (ALandscapeProxy* LandscapeActor = Cast<ALandscape>(Actor))
	{
		Bounds = PCGHelpers::GetLandscapeBounds(LandscapeActor);
	}
	else
	{
		Bounds = PCGHelpers::GetActorBounds(Actor);
	}

	return Bounds;
}

UPCGSubsystem* UPCGComponent::GetSubsystem() const
{
	return (GetOwner() && GetOwner()->GetWorld()) ? GetOwner()->GetWorld()->GetSubsystem<UPCGSubsystem>() : nullptr;
}

#if WITH_EDITOR
bool UPCGComponent::PopulateTrackedActorToTagsMap(bool bForce)
{
	if (bActorToTagsMapPopulated && !bForce)
	{
		return false;
	}

	CachedTrackedActorToTags.Reset();
	CachedTrackedActorToDependencies.Reset();
	for (TWeakObjectPtr<AActor> Actor : CachedTrackedActors)
	{
		if (Actor.IsValid())
		{
			AddTrackedActor(Actor.Get(), /*bForce=*/true);
		}
	}

	bActorToTagsMapPopulated = true;
	return true;
}

bool UPCGComponent::AddTrackedActor(AActor* InActor, bool bForce)
{
	if (!bForce)
	{
		PopulateTrackedActorToTagsMap();
	}	

	check(InActor);
	bool bAppliedChange = false;

	for (const FName& Tag : InActor->Tags)
	{
		if (!CachedTrackedTagsToSettings.Contains(Tag))
		{
			continue;
		}

		bAppliedChange = true;
		CachedTrackedActorToTags.FindOrAdd(InActor).Add(Tag);
		PCGHelpers::GatherDependencies(InActor, CachedTrackedActorToDependencies.FindOrAdd(InActor));

		if (!bForce)
		{
			DirtyCacheFromTag(Tag);
		}
	}

	return bAppliedChange;
}

bool UPCGComponent::RemoveTrackedActor(AActor* InActor)
{
	PopulateTrackedActorToTagsMap();

	check(InActor);
	bool bAppliedChange = false;

	if(CachedTrackedActorToTags.Contains(InActor))
	{
		for (const FName& Tag : CachedTrackedActorToTags[InActor])
		{
			DirtyCacheFromTag(Tag);
		}

		CachedTrackedActorToTags.Remove(InActor);
		CachedTrackedActorToDependencies.Remove(InActor);
		bAppliedChange = true;
	}

	return bAppliedChange;
}

bool UPCGComponent::UpdateTrackedActor(AActor* InActor)
{
	check(InActor);
	// If the tracked data wasn't initialized before, then it is not possible to know if we need to update or not - take no chances
	bool bAppliedChange = PopulateTrackedActorToTagsMap();

	// Update the contents of the tracked actor vs. its current tags, and dirty accordingly
	if (CachedTrackedActorToTags.Contains(InActor))
	{
		// Any tags that aren't on the actor and were in the cached actor to tags -> remove & dirty
		TSet<FName> CachedTags = CachedTrackedActorToTags[InActor];
		for (const FName& CachedTag : CachedTags)
		{
			if (!InActor->Tags.Contains(CachedTag))
			{
				CachedTrackedActorToTags[InActor].Remove(CachedTag);
				DirtyCacheFromTag(CachedTag);
				bAppliedChange = true;
			}
		}
	}
		
	// Any tags that are new on the actor and not in the cached actor to tags -> add & dirty
	for (const FName& Tag : InActor->Tags)
	{
		if (!CachedTrackedTagsToSettings.Contains(Tag))
		{
			continue;
		}

		if (!CachedTrackedActorToTags.FindOrAdd(InActor).Find(Tag))
		{
			CachedTrackedActorToTags[InActor].Add(Tag);
			PCGHelpers::GatherDependencies(InActor, CachedTrackedActorToDependencies.FindOrAdd(InActor));
			DirtyCacheFromTag(Tag);
			bAppliedChange = true;
		}
	}

	// Finally, if the current has no tag anymore, we can remove it from the map
	if (CachedTrackedActorToTags.Contains(InActor) && CachedTrackedActorToTags[InActor].IsEmpty())
	{
		CachedTrackedActorToTags.Remove(InActor);
		CachedTrackedActorToDependencies.Remove(InActor);
	}

	return bAppliedChange;
}

bool UPCGComponent::DirtyTrackedActor(AActor* InActor)
{
	PopulateTrackedActorToTagsMap();

	check(InActor);
	bool bAppliedChange = false;

	if (CachedTrackedActorToTags.Contains(InActor))
	{
		for (const FName& Tag : CachedTrackedActorToTags[InActor])
		{
			DirtyCacheFromTag(Tag);
		}

		bAppliedChange = true;
	}
	else if (AddTrackedActor(InActor))
	{
		bAppliedChange = true;
	}

	return bAppliedChange;
}

void UPCGComponent::DirtyCacheFromTag(const FName& InTag)
{
	if (CachedTrackedTagsToSettings.Contains(InTag))
	{
		for (TWeakObjectPtr<const UPCGSettings> Settings : CachedTrackedTagsToSettings[InTag])
		{
			if (Settings.IsValid() && GetSubsystem())
			{
				GetSubsystem()->CleanFromCache(Settings->GetElement().Get());
			}
		}
	}
}

void UPCGComponent::DirtyCacheForAllTrackedTags()
{
	for (const auto& TagToSettings : CachedTrackedTagsToSettings)
	{
		for (TWeakObjectPtr<const UPCGSettings> Settings : TagToSettings.Value)
		{
			if (Settings.IsValid() && GetSubsystem())
			{
				GetSubsystem()->CleanFromCache(Settings->GetElement().Get());
			}
		}
	}
}

bool UPCGComponent::GraphUsesLandscapePin() const
{
	return Graph && Graph->GetInputNode()->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeLabel);
}

#endif // WITH_EDITOR

void UPCGComponent::SetManagedResources(const TArray<TObjectPtr<UPCGManagedResource>>& Resources)
{
	FScopeLock ResourcesLock(&GeneratedResourcesLock);
	check(GeneratedResources.IsEmpty());
	GeneratedResources = Resources;
}

void UPCGComponent::GetManagedResources(TArray<TObjectPtr<UPCGManagedResource>>& Resources) const
{
	FScopeLock ResourcesLock(&GeneratedResourcesLock);
	Resources = GeneratedResources;
}

FPCGComponentInstanceData::FPCGComponentInstanceData(const UPCGComponent* InSourceComponent)
	: FActorComponentInstanceData(InSourceComponent)
	, SourceComponent(InSourceComponent)
{
	if (SourceComponent)
	{
		SourceComponent->GetManagedResources(GeneratedResources);
	}
}

bool FPCGComponentInstanceData::ContainsData() const
{
	return GeneratedResources.Num() > 0 || Super::ContainsData();
}

void FPCGComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	Super::ApplyToComponent(Component, CacheApplyPhase);

	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		UPCGComponent* PCGComponent = CastChecked<UPCGComponent>(Component);

		// Duplicate generated resources + retarget them
		TArray<TObjectPtr<UPCGManagedResource>> DuplicatedResources;
		for (TObjectPtr<UPCGManagedResource> Resource : GeneratedResources)
		{
			if (Resource)
			{
				UPCGManagedResource* DuplicatedResource = CastChecked<UPCGManagedResource>(StaticDuplicateObject(Resource, PCGComponent, FName()));
				DuplicatedResource->PostApplyToComponent();
				DuplicatedResources.Add(DuplicatedResource);
			}
		}

		if (DuplicatedResources.Num() > 0)
		{
			PCGComponent->SetManagedResources(DuplicatedResources);
		}

		// Also remap if we are partitioned
		UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem();
		if (Subsystem && PCGComponent->IsPartitioned() && SourceComponent)
		{
			Subsystem->RemapPCGComponent(SourceComponent, PCGComponent);
		}

#if WITH_EDITOR
		// Finally, start a delayed refresh task (if there is not one already), in editor only
		// It is important to be delayed, because we cannot spawn Partition Actors within this scope,
		// because we are in a construction script.
		DelayedRefresh(PCGComponent);
#endif // WITH_EDITOR
	}
}

#if WITH_EDITOR
void FPCGComponentInstanceData::DelayedRefresh(UPCGComponent* PCGComponent)
{
	if (UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem())
	{
		if (PCGComponent->CurrentRefreshTask == InvalidPCGTaskId)
		{
			TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);
			PCGComponent->CurrentRefreshTask = Subsystem->ScheduleGeneric([ComponentPtr]()
				{
					UPCGComponent* Component = ComponentPtr.Get();
					if (Component && IsValid(Component))
					{
						bool bWasGenerated = Component->bGenerated;
						if (UPCGSubsystem* Subsystem = Component->GetSubsystem())
						{
							// Register/Unregister depending on the partition flag.
							if (Component->IsPartitioned())
							{
								// If we are partitioned but we have resources, we need to force a cleanup
								if (!Component->GeneratedResources.IsEmpty())
								{
									Component->CleanupLocalImmediate(true);
								}

								Subsystem->RegisterOrUpdatePCGComponent(Component, /*bDoActorMapping=*/bWasGenerated);
							}
							else
							{
								Subsystem->UnregisterPCGComponent(Component);
							}
						}

						// Mark the refresh task invalid (since we check in Refresh if there is already a task scheduled)
						Component->CurrentRefreshTask = InvalidPCGTaskId;

						// If we had a cleanup, just generate.
						if (bWasGenerated && !Component->bGenerated)
						{
							Component->GenerateLocal(/*bForce=*/false);
						}
						// Otherwise, only refresh if we are dirty.
						else if (Component->bDirtyGenerated)
						{
							Component->Refresh();
						}
					}

					return true;
				}, PCGComponent, {});
		}
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
