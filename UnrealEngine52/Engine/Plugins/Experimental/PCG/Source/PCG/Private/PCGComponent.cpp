// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComponent.h"

#include "PCGContext.h"
#include "PCGEngineSettings.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGManagedResource.h"
#include "PCGParamData.h"
#include "PCGSubsystem.h"
#include "Data/PCGCollisionShapeData.h"
#include "Data/PCGDifferenceData.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGLandscapeData.h"
#include "Data/PCGLandscapeSplineData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPrimitiveData.h"
#include "Data/PCGProjectionData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGUnionData.h"
#include "Data/PCGVolumeData.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGGeneratedResourcesLogging.h"

#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Algo/Transform.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Volume.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeSplinesComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComponent)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "UPCGComponent"

namespace PCGComponent
{
	const bool bSaveOnCleanupAndGenerate = false;
}

UPCGComponent::UPCGComponent(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	GraphInstance = InObjectInitializer.CreateDefaultSubobject<UPCGGraphInstance>(this, TEXT("PCGGraphInstance"));

#if WITH_EDITOR
	// If we are in Editor, and we are a BP template (no owner), we will mark this component to force a generate when added to world.
	if (!PCGHelpers::IsRuntimeOrPIE() && !GetOwner() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		bForceGenerateOnBPAddedToWorld = true;
	}
#endif // WITH_EDITOR
}

bool UPCGComponent::CanPartition() const
{
	// Support/Force partitioning on non-PCG partition actors in WP worlds.
	return GetOwner() && GetOwner()->GetWorld() && GetOwner()->GetWorld()->GetWorldPartition() != nullptr && Cast<APCGPartitionActor>(GetOwner()) == nullptr;
}

bool UPCGComponent::IsPartitioned() const
{
	return bIsComponentPartitioned && CanPartition();
}

void UPCGComponent::SetIsPartitioned(bool bIsNowPartitioned)
{
	if (bIsNowPartitioned == bIsComponentPartitioned)
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

		// Update the component on the subsystem
		bIsComponentPartitioned = bIsNowPartitioned;
		Subsystem->RegisterOrUpdatePCGComponent(this, bDoActorMapping);
	}
	else
	{
		bIsComponentPartitioned = false;
	}
}

void UPCGComponent::SetGraph_Implementation(UPCGGraphInterface* InGraph)
{
	SetGraphInterfaceLocal(InGraph);
}

UPCGGraph* UPCGComponent::GetGraph() const
{
	return (GraphInstance ? GraphInstance->GetGraph() : nullptr);
}

void UPCGComponent::SetGraphLocal(UPCGGraphInterface* InGraph)
{
	SetGraphInterfaceLocal(InGraph);
}

void UPCGComponent::SetGraphInterfaceLocal(UPCGGraphInterface* InGraphInterface)
{
	if (ensure(GraphInstance))
	{
		GraphInstance->SetGraph(InGraphInterface);
		RefreshAfterGraphChanged(GraphInstance, /*bIsStructural=*/true, /*bDirtyInputs=*/true);
	}
}

void UPCGComponent::AddToManagedResources(UPCGManagedResource* InResource)
{
	PCGGeneratedResourcesLogging::LogAddToManagedResources(InResource);

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
		if (ManagedResource)
		{
			Func(ManagedResource);
		}
	}
}

bool UPCGComponent::ShouldGenerate(bool bForce, EPCGComponentGenerationTrigger RequestedGenerationTrigger) const
{
	if (!bActivated || !GetGraph() || !GetSubsystem())
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

	const bool bGraphInstanceIsDifferent = !GraphInstance->IsEquivalent(Original->GraphInstance);

#if WITH_EDITOR
	const bool bHasDirtyInput = InputType != NewInputType;

	TSet<FName> TrackedTags;
	TSet<FName> OriginalTrackedTags;
	CachedTrackedTagsToSettings.GetKeys(TrackedTags);
	Original->CachedTrackedTagsToSettings.GetKeys(OriginalTrackedTags);
	const bool bHasDirtyTracking = !(TrackedTags.Num() == OriginalTrackedTags.Num() && TrackedTags.Includes(OriginalTrackedTags));

	const bool bIsDirty = bHasDirtyInput || bHasDirtyTracking || bGraphInstanceIsDifferent;
#endif // WITH_EDITOR

	InputType = NewInputType;
	Seed = Original->Seed;
	GenerationTrigger = Original->GenerationTrigger;

	UPCGGraph* OriginalGraph = Original->GraphInstance ? Original->GraphInstance->GetGraph() : nullptr;
	if (OriginalGraph != GraphInstance->GetGraph())
	{
		GraphInstance->SetGraph(OriginalGraph);
	}

	if (bGraphInstanceIsDifferent && OriginalGraph)
	{
		GraphInstance->CopyParameterOverrides(Original->GraphInstance);
	}

#if WITH_EDITOR
	if (bHasDirtyTracking)
	{
		TeardownTrackingCallbacks();
		SetupTrackingCallbacks();
		RefreshTrackingData();
	}

	// Note that while we dirty here, we won't trigger a refresh since we don't have the required context
	if (bIsDirty)
	{
		Modify();
		DirtyGenerated(bHasDirtyInput ? EPCGComponentDirtyFlag::Input : EPCGComponentDirtyFlag::None);
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

bool UPCGComponent::GetActorsFromTags(const TMap<FName, bool>& InTagsAndCulling, TSet<TWeakObjectPtr<AActor>>& OutActors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::GetActorsFromTags::Tracked);
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const FBox LocalBounds = GetGridBounds();

	TArray<AActor*> PerTagActors;

	OutActors.Reset();

	bool bHasValidTag = false;
	for (const TPair<FName, bool>& TagAndCulling : InTagsAndCulling)
	{
		const FName& Tag = TagAndCulling.Key;
		const bool bCullAgainstLocalBounds = TagAndCulling.Value;

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
	PCGGeneratedResourcesLogging::LogPostProcessGraph();

	LastGeneratedBounds = InNewBounds;

	const bool bHadGeneratedOutputBefore = GeneratedGraphOutput.TaggedData.Num() > 0;

	CleanupUnusedManagedResources();

	GeneratedGraphOutput.Reset();

	if (bInGenerated)
	{
		bGenerated = true;

		CurrentGenerationTask = InvalidPCGTaskId;

#if WITH_EDITOR
		// Reset this flag to avoid re-generating on further refreshes.
		bForceGenerateOnBPAddedToWorld = false;

		bDirtyGenerated = false;
		OnPCGGraphGeneratedDelegate.Broadcast(this);
#endif
		// After a successful generation, we also want to call PostGenerateFunctions
		// if we have any. We also need a context.

		if (Context)
		{
			// TODO: should we filter based on supported serialized types here?
			// TOOD: should reouter the contained data to this component
			// .. and also remove it from the rootset information in the graph executor
			//GeneratedGraphOutput = Context->InputData;
			for (const FPCGTaggedData& TaggedData : Context->InputData.TaggedData)
			{
				FPCGTaggedData& DuplicatedTaggedData = GeneratedGraphOutput.TaggedData.Add_GetRef(TaggedData);
				// TODO: outering the first layer might not be sufficient here - might need to expose
				// some methods in the data to traverse all the data to outer everything for serialization
				DuplicatedTaggedData.Data = Cast<UPCGData>(StaticDuplicateObject(TaggedData.Data, this));

				UPCGMetadata* DuplicatedMetadata = nullptr;
				if (UPCGSpatialData* DuplicatedSpatialData = Cast<UPCGSpatialData>(DuplicatedTaggedData.Data))
				{
					DuplicatedMetadata = DuplicatedSpatialData->Metadata;
				}
				else if (UPCGParamData* DuplicatedParamData = Cast<UPCGParamData>(DuplicatedTaggedData.Data))
				{
					DuplicatedMetadata = DuplicatedParamData->Metadata;
				}

				// Make sure the metadata can be serialized independently
				if (DuplicatedMetadata)
				{
					DuplicatedMetadata->Flatten();
				}
			}

			// If the original component is partitioned, local components have to forward
			// their inputs, so that they can be gathered by the original component.
			// We don't have the info on the original component here, so forward for all
			// components.
			Context->OutputData = Context->InputData;

			CallPostGenerateFunctions(Context);
		}
	}

	// Trigger notification - will be used by other tracking mechanisms
#if WITH_EDITOR
	const bool bHasGeneratedOutputAfter = GeneratedGraphOutput.TaggedData.Num() > 0;

	if (bHasGeneratedOutputAfter || bHadGeneratedOutputBefore)
	{
		FProperty* GeneratedOutputProperty = FindFProperty<FProperty>(UPCGComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPCGComponent, GeneratedGraphOutput));
		check(GeneratedOutputProperty);
		FPropertyChangedEvent GeneratedOutputChangedEvent(GeneratedOutputProperty, EPropertyChangeType::ValueSet);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, GeneratedOutputChangedEvent);
	}
#endif
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

	const bool bHadGeneratedGraphOutput = GeneratedGraphOutput.TaggedData.Num() > 0;
	GeneratedGraphOutput.Reset();

#if WITH_EDITOR
	OnPCGGraphCleanedDelegate.Broadcast(this);
	bDirtyGenerated = false;

	if (bHadGeneratedGraphOutput)
	{
		FProperty* GeneratedOutputProperty = FindFProperty<FProperty>(UPCGComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPCGComponent, GeneratedGraphOutput));
		check(GeneratedOutputProperty);
		FPropertyChangedEvent GeneratedOutputChangedEvent(GeneratedOutputProperty, EPropertyChangeType::ValueSet);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, GeneratedOutputChangedEvent);
	}
#endif
}

void UPCGComponent::OnProcessGraphAborted(bool bQuiet)
{
	if (!bQuiet)
	{
		UE_LOG(LogPCG, Warning, TEXT("Process Graph was called but aborted, check for errors in log if you expected a result."));
	}

	CleanupUnusedManagedResources();

	CurrentGenerationTask = InvalidPCGTaskId;
	CurrentCleanupTask = InvalidPCGTaskId; // this is needed to support cancellation

#if WITH_EDITOR
	CurrentRefreshTask = InvalidPCGTaskId;
	bDirtyGenerated = false;
#endif
}

void UPCGComponent::Cleanup()
{
	if (!GetSubsystem() || IsCleaningUp())
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
	if (!GetSubsystem() || IsCleaningUp())
	{
		return InvalidPCGTaskId;
	}

	PCGGeneratedResourcesLogging::LogCleanupInternal(bRemoveComponents);

	Modify();

#if WITH_EDITOR
	ExtraCapture.ResetTimers();
	ExtraCapture.ResetCapturedMessages();
#endif

	CurrentCleanupTask = GetSubsystem()->ScheduleCleanup(this, bRemoveComponents, bSave, Dependencies);
	return CurrentCleanupTask;
}

void UPCGComponent::CancelGeneration()
{
	if (CurrentGenerationTask != InvalidPCGTaskId)
	{
		GetSubsystem()->CancelGeneration(this);
	}
}

void UPCGComponent::NotifyPropertiesChangedFromBlueprint()
{
#if WITH_EDITOR
	DirtyGenerated(EPCGComponentDirtyFlag::Actor);
	Refresh();
#endif
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

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::MoveResourcesToNewActor] Owner is null, child actor not created."));
		return false;
	}

	check(InNewActor);
	AActor* NewActor = InNewActor;

	bool bHasMovedResources = false;

	Modify();

	if (bCreateChild)
	{
		NewActor = UPCGActorHelpers::SpawnDefaultActor(GetWorld(), NewActor->GetClass(), TEXT("PCGStampChild"), Owner->GetTransform());
		NewActor->AttachToActor(InNewActor, FAttachmentTransformRules::KeepWorldTransform);
		check(NewActor);
	}

	// Trying to move all resources for now. Perhaps in the future we won't want that.
	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (TObjectPtr<UPCGManagedResource>& GeneratedResource : GeneratedResources)
		{
			if (GeneratedResource)
			{
				GeneratedResource->MoveResourceToNewActor(NewActor);
				TSet<TSoftObjectPtr<AActor>> Dummy;
				GeneratedResource->ReleaseIfUnused(Dummy);
				bHasMovedResources = true;
			}
			else
			{
				UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::MoveResourcesToNewActor] Null generated resource encountered on actor \"%s\" and will be skipped."), *Owner->GetFName().ToString());
			}
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
	PCGGeneratedResourcesLogging::LogCleanupLocalImmediate(bRemoveComponents, GeneratedResources);

	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;

	if (!bRemoveComponents && UPCGManagedResource::DebugForcePurgeAllResourcesOnGenerate())
	{
		bRemoveComponents = true;
	}

	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
		{
			// Note: resources can be null here in some loading + bp object cases
			UPCGManagedResource* Resource = GeneratedResources[ResourceIndex];

			PCGGeneratedResourcesLogging::LogCleanupLocalImmediateResource(Resource);

			if (!Resource || Resource->Release(bRemoveComponents, ActorsToDelete))
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

	PCGGeneratedResourcesLogging::LogCleanupLocalImmediateFinished(GeneratedResources);
}

FPCGTaskId UPCGComponent::CreateCleanupTask(bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies)
{
	if ((!bGenerated && !IsGenerating()) || IsPartitioned() || IsCleaningUp())
	{
		return InvalidPCGTaskId;
	}

	PCGGeneratedResourcesLogging::LogCreateCleanupTask(bRemoveComponents);

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

				if (!Resource && ThisComponent->GetOwner())
				{
					UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::CreateCleanupTask] Null generated resource encountered on actor \"%s\"."), *ThisComponent->GetOwner()->GetFName().ToString());
				}

				PCGGeneratedResourcesLogging::LogCreateCleanupTaskResource(Resource);

				if (!Resource || Resource->Release(bRemoveComponents, Context->ActorsToDelete))
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

		if (ThisComponentWeakPtr.Get())
		{
			PCGGeneratedResourcesLogging::LogCreateCleanupTaskFinished(ThisComponentWeakPtr->GeneratedResources);
		}

		return true;
	};

	return GetSubsystem()->ScheduleGeneric(CleanupTask, this, *AllDependencies);
}

void UPCGComponent::CleanupUnusedManagedResources()
{
	PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResources(GeneratedResources);

	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;

	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
		{
			UPCGManagedResource* Resource = GeneratedResources[ResourceIndex];

			PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResourcesResource(Resource);

			if (!Resource && GetOwner())
			{
				UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::CleanupUnusedManagedResources] Null generated resource encountered on actor \"%s\"."), *GetOwner()->GetFName().ToString());
			}

			if (!Resource || Resource->ReleaseIfUnused(ActorsToDelete))
			{
				GeneratedResources.RemoveAtSwap(ResourceIndex);
			}
		}
	}

	UPCGActorHelpers::DeleteActors(GetWorld(), ActorsToDelete.Array());

	PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResourcesFinished(GeneratedResources);
}

void UPCGComponent::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (GetOwner() && GetOwner()->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	// Register itself to the PCGSubsystem, to map the component to all its corresponding PartitionActors if it is partition among other things.
	if (GetSubsystem())
	{
		ensure(!bGenerated || LastGeneratedBounds.IsValid);
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
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->CancelGeneration(this);
		Subsystem->UnregisterPCGComponent(this);
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
			Subsystem->CancelGeneration(this);
			Subsystem->UnregisterPCGComponent(this);
		}
	}
#endif

	// Bookkeeping local components that might be deleted by the user.
	// Making sure that the corresponding partition actor doesn't keep a dangling references
	if (APCGPartitionActor* PAOwner = Cast<APCGPartitionActor>(GetOwner()))
	{
		PAOwner->RemoveLocalComponent(this);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPCGComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Force dirty to be false on load. We should never refresh on load.
	bDirtyGenerated = false;

	// If we have both default value (bIsComponentPartitioned = false and bIsPartitioned = true)
	// we will follow the value of bIsPartitioned.
	// bIsPartitioned will be set to false to new objects
	if (!bIsComponentPartitioned && bIsPartitioned)
	{
		bIsComponentPartitioned = bIsPartitioned;
		bIsPartitioned = false;
	}

	/** Deprecation code, should be removed once generated data has been updated */
	if (GetOwner() && bGenerated && GeneratedResources.Num() == 0)
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

	if (Graph_DEPRECATED)
	{
		GraphInstance->SetGraph(Graph_DEPRECATED);
		Graph_DEPRECATED = nullptr;
	}

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

	if (GraphInstance)
	{
		// We might have already connected in PostInitProperties
		// To be sure, remove it and re-add it.
		GraphInstance->OnGraphChangedDelegate.RemoveAll(this);
		GraphInstance->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
	}
}

bool UPCGComponent::CanEditChange(const FProperty* InProperty) const
{
	// Can't change anything if the component is local
	return !bIsComponentLocal;
}
#endif

void UPCGComponent::BeginDestroy()
{
#if WITH_EDITOR
	if (GraphInstance)
	{
		GraphInstance->OnGraphChangedDelegate.RemoveAll(this);
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

void UPCGComponent::PostInitProperties()
{
#if WITH_EDITOR
	GraphInstance->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
#endif // WITH_EDITOR

	// Note that if the component is a default sub object, we find the first outer that is not a sub object.
	UObject* CurrentInspectedObject = this;
	while (CurrentInspectedObject && CurrentInspectedObject->HasAnyFlags(RF_DefaultSubObject))
	{
		CurrentInspectedObject = CurrentInspectedObject->GetOuter();
	}

	// We detect new object if they are not a default object/archetype and/or they do not need load.
	// In some cases, were the component is a default sub object (like APCGVolume), it has no loading flags
	// even if it is loading, so we use the outer found above.
	const bool bIsNewObject = CurrentInspectedObject && !CurrentInspectedObject->HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_NeedPostLoad);

#if WITH_EDITOR
	// Force bIsPartitioned at false for new objects
	if (bIsNewObject)
	{
		bIsPartitioned = false;
	}
#endif // WITH_EDITOR

	Super::PostInitProperties();
}

void UPCGComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (GetOwner() && GetOwner()->bIsEditorPreviewActor)
	{
		return;
	}

	// We can't register to the subsystem in OnRegister if we are at runtime because
	// the landscape can be not loaded yet.
	// It will be done in BeginPlay at runtime.
	if (!PCGHelpers::IsRuntimeOrPIE() && GetSubsystem())
	{
		if (UWorld* World = GetWorld())
		{
			// We won't be able to spawn any actors if we are currently running a construction script.
			if (!World->bIsRunningConstructionScript)
			{
				ensure(!bGenerated || LastGeneratedBounds.IsValid);
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

void UPCGComponent::OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	const bool bIsStructural = ((ChangeType & (EPCGChangeType::Edge | EPCGChangeType::Structural)) != EPCGChangeType::None);
	const bool bDirtyInputs = bIsStructural || ((ChangeType & EPCGChangeType::Input) != EPCGChangeType::None);

	RefreshAfterGraphChanged(InGraph, bIsStructural, bDirtyInputs);
}

void UPCGComponent::RefreshAfterGraphChanged(UPCGGraphInterface* InGraph, bool bIsStructural, bool bDirtyInputs)
{
	if (InGraph != GraphInstance)
	{
		return;
	}

	const bool bHasGraph = (InGraph && InGraph->GetGraph());

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
		if (bHasGraph)
		{
			Refresh();
		}
		else
		{
			// With no graph, we clean up
			CleanupLocal(/*bRemoveComponents=*/true, /*bSave=*/ false);
		}

		InspectionCache.Empty();
		return;
	}
#endif

	// Otherwise, if we are in PIE or runtime, force generate if we have a graph (and were generated). Or cleanup if we have no graph
	if (bHasGraph && bGenerated)
	{
		GenerateLocal(/*bForce=*/true);
	}
	else if (!bHasGraph)
	{
		CleanupLocal(/*bRemoveComponents=*/true, /*bSave=*/ false);
	}
}

#if WITH_EDITOR
/**
* Temporary workaround, while we are waiting for UE-182059
* On the UI side of StructUtils, property widgets are referencing a temporary structure, not the one that is owned by the Graph/GraphInstance.
* After the value is modified and the callback is propagated, the temporary structure is modified, but the one owned by the Graph is not.
* If we keep it like this, PostEditChangeChainProperty will call for a reconstruction of the component if it is on a Blueprint
* and we will go through the process of copying and transfering data between the 2 components (old and new).
* Because of that, we copy the old structure, not modified. And when the structure is actually modified, the callback is called on the old component,
* already marked trash, and we lose the change.
* The workaround is to detect when a value from our parameters is changed, and just discard the call to the super method, avoiding a reconstruction script.
* Note that it should be OK to do so in the current situation, because there is another callback fired just after the copy of the temp 
* structure to the real one, and on this callback we are calling the super method and go through construction script.
* We do this workaround for pre and post edit, since we can run into trouble if a pre edit was processed, but post was not.
*/
bool UPCGComponent::IsChangingGraphInstanceParameterValue(FEditPropertyChain& InEditPropertyChain) const
{
	if (!GraphInstance)
	{
		return false;
	}

	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = InEditPropertyChain.GetActiveNode();

	if (!PropertyNode)
	{
		return false;
	}

	constexpr int32 PropertyPathSize = 4;
	static const FName PropertyPath[PropertyPathSize] = {
		GET_MEMBER_NAME_CHECKED(UPCGComponent, GraphInstance),
		GET_MEMBER_NAME_CHECKED(UPCGGraphInstance, ParametersOverrides),
		GET_MEMBER_NAME_CHECKED(FPCGOverrideInstancedPropertyBag, Parameters),
		FName(TEXT("Value"))
	};

	int32 Index = 0;

	while (Index < PropertyPathSize && PropertyNode)
	{
		FProperty* Property = PropertyNode->GetValue();
		if (Property && Property->GetFName() == PropertyPath[Index])
		{
			PropertyNode = PropertyNode->GetNextNode();
			++Index;
		}
		else
		{
			PropertyNode = nullptr;
		}
	}

	if (Index == PropertyPathSize && PropertyNode)
	{
		FProperty* Property = PropertyNode->GetValue();
		if (Property && Property->GetOwnerStruct() == GraphInstance->ParametersOverrides.Parameters.GetPropertyBagStruct())
		{
			return true;
		}
	}

	return false;
}

void UPCGComponent::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	if (IsChangingGraphInstanceParameterValue(PropertyAboutToChange))
	{
		// Skip pre edit, cf IsChangingGraphInstanceParameterValue comment
		return;
	}

	UObject::PreEditChange(PropertyAboutToChange);
}

void UPCGComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (IsChangingGraphInstanceParameterValue(PropertyChangedEvent.PropertyChain))
	{
		// Skip post edit, cf IsChangingGraphInstanceParameterValue comment
		return;
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
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
	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, bIsComponentPartitioned))
	{
		if (CanPartition())
		{
			// At this point, bIsComponentPartitioned is already set with the new value.
			// But we need to do some cleanup before
			// So keep this new value, and take its negation for the cleanup.
			bool bIsNowPartitioned = bIsComponentPartitioned;
			bIsComponentPartitioned = !bIsComponentPartitioned;

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
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, GraphInstance))
	{
		OnGraphChanged(GraphInstance, EPCGChangeType::Structural);
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
	// Without an owner, it probably means we are in a BP template, so no need to setup callbacks
	if (!GetOwner())
	{
		return;
	}

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
	// Without an owner, it probably means we are in a BP template, so no need to setup callbacks
	if (!GetOwner())
	{
		return;
	}

	CachedTrackedTagsToSettings.Reset();
	CachedTrackedTagsToCulling.Reset();
	if (UPCGGraph* PCGGraph = GetGraph())
	{
		CachedTrackedTagsToSettings = PCGGraph->GetTrackedTagsToSettings();

		// A tag should be culled, if only all the settings that track this tag should cull.
		// Note that is only impact the fact that we track (or not) this tag.
		// If a setting is marked as "should cull", it will only be dirtied (at least by default), if the actor with the
		// given tag intersect with the component.
		for (const TPair<FName, TSet<FPCGSettingsAndCulling>>& It : CachedTrackedTagsToSettings)
		{
			const FName& Tag = It.Key;

			bool bShouldCull = true;
			for (const FPCGSettingsAndCulling& SettingsAndCulling : It.Value)
			{
				if (!SettingsAndCulling.Value)
				{
					bShouldCull = false;
					break;
				}
			}

			CachedTrackedTagsToCulling.Emplace(Tag, bShouldCull);
		}
	}

	if(!CachedTrackedTagsToSettings.IsEmpty())
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UPCGComponent::OnActorAdded);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UPCGComponent::OnActorDeleted);
	}
}

void UPCGComponent::RefreshTrackingData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::RefreshTrackingData);

	// Without an owner, it probably means we are in a BP template, so no need to setup callbacks
	if (!GetOwner())
	{
		return;
	}

	GetActorsFromTags(CachedTrackedTagsToCulling, CachedTrackedActors);
	PopulateTrackedActorToTagsMap(/*bForce=*/true);
}

void UPCGComponent::TeardownTrackingCallbacks()
{
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
}

bool UPCGComponent::ActorIsTracked(AActor* InActor) const
{
	if (!InActor || !GetGraph())
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::OnActorAdded);
	if (!InActor || InActor->bIsEditorPreviewActor)
	{
		return;
	}

	const bool bIsTracked = AddTrackedActor(InActor);
	if (bIsTracked)
	{
		DirtyGenerated(EPCGComponentDirtyFlag::None, /*bDispatchToLocalComponents=*/ false);
		Refresh();
	}
}

void UPCGComponent::OnActorDeleted(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::OnActorDeleted);
	if (!InActor || InActor->bIsEditorPreviewActor)
	{
		return;
	}

	const bool bWasTracked = RemoveTrackedActor(InActor);
	if (bWasTracked)
	{
		DirtyGenerated(EPCGComponentDirtyFlag::None, /*bDispatchToLocalComponents=*/ false);
		Refresh();
	}
}

void UPCGComponent::OnActorMoved(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::OnActorMoved);
	if (!InActor || InActor->bIsEditorPreviewActor)
	{
		return;
	}

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
		if (DirtyTrackedActor(InActor))
		{
			DirtyGenerated(EPCGComponentDirtyFlag::None, /*bDispatchToLocalComponents=*/false);
			Refresh();
		}
	}
}

void UPCGComponent::UpdateTrackedLandscape(bool bBoundsCheck)
{
	TeardownLandscapeTracking();
	TrackedLandscapes.Reset();

	// Without an owner, it probably means we are in a BP template, so no need to setup callbacks
	if (!GetOwner())
	{
		return;
	}

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
					Algo::Transform(PCGHelpers::GetLandscapeProxies(World, ActorSpatialData->GetBounds()), TrackedLandscapes, [](TWeakObjectPtr<ALandscapeProxy> Landscape) { return TSoftObjectPtr<ALandscapeProxy>(Landscape.Get()); });
				}
			}
			else
			{
				Algo::Transform(PCGHelpers::GetAllLandscapeProxies(World), TrackedLandscapes, [](TWeakObjectPtr<ALandscapeProxy> Landscape) { return TSoftObjectPtr<ALandscapeProxy>(Landscape.Get()); });
			}
		}
	}

	SetupLandscapeTracking();
}

void UPCGComponent::SetupLandscapeTracking()
{
	for (TSoftObjectPtr<ALandscapeProxy> LandscapeProxy : TrackedLandscapes)
	{
		if (LandscapeProxy.IsValid())
		{
			LandscapeProxy->OnComponentDataChanged.AddUObject(this, &UPCGComponent::OnLandscapeChanged);
		}
	}
}

void UPCGComponent::TeardownLandscapeTracking()
{
	for (TSoftObjectPtr<ALandscapeProxy> LandscapeProxy : TrackedLandscapes)
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
			DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/ false);
			Refresh();
		}
	}
}

void UPCGComponent::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::OnObjectPropertyChanged);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::OnActorChanged);
	if (Actor == GetOwner())
	{
		// Something has changed on the owner (including properties of this component)
		// In the case of splines, this is where we'd get notified if some component properties (incl. spline vertices) have changed
		// TODO: this should dirty only the actor pcg data.
		DirtyGenerated(EPCGComponentDirtyFlag::Actor);
		Refresh();
	}
	else if(Actor && !Actor->bIsEditorPreviewActor)
	{
		if ((bActorTagChange && Actor == InObject && UpdateTrackedActor(Actor)) || DirtyTrackedActor(Actor))
		{
			DirtyGenerated(EPCGComponentDirtyFlag::None, /*bDispatchToLocalComponents=*/ false);
			Refresh();
		}
	}
}

void UPCGComponent::DirtyGenerated(EPCGComponentDirtyFlag DirtyFlag, const bool bDispatchToLocalComponents)
{
	if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
	{
		UE_LOG(LogPCG, Log, TEXT("[%s] --- DIRTY GENERATED ---"), *GetOwner()->GetName());
	}

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

	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Data))
	{
		CachedPCGData = nullptr;
	}

	// For partitioned graph, we must forward the call to the partition actor, if we need to
	// TODO: Don't forward for None for now, as it could break some stuff
	if (bActivated && IsPartitioned() && bDispatchToLocalComponents)
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
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (CurrentRefreshTask == InvalidPCGTaskId)
		{
			CurrentRefreshTask = Subsystem->ScheduleRefresh(this);
		}
	}
}

bool UPCGComponent::ShouldGenerateBPPCGAddedToWorld() const
{
	if (PCGHelpers::IsRuntimeOrPIE())
	{
		return false;
	}
	else
	{
		// Generate on drop can be disabled by global settings or locally by not having "GenerateOnLoad" as a generation trigger.
		const UPCGEngineSettings* Settings = GetDefault<UPCGEngineSettings>();
		return Settings ? (Settings->bGenerateOnDrop && bForceGenerateOnBPAddedToWorld && (GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad)) : false;
	}
}

void UPCGComponent::OnRefresh()
{
	// Mark the refresh task invalid to allow re-triggering refreshes
	CurrentRefreshTask = InvalidPCGTaskId;

	// Before doing a refresh, update the component to the subsystem if we are partitioned
	// Only redo the mapping if we are generated
	UPCGSubsystem* Subsystem = GetSubsystem();
	bool bWasGenerated = bGenerated;

	if (IsPartitioned())
	{
		// If we are partitioned but we have resources, we need to force a cleanup
		if (!GeneratedResources.IsEmpty())
		{
			CleanupLocalImmediate(true);
		}
	}

	if (Subsystem)
	{
		Subsystem->RegisterOrUpdatePCGComponent(this, /*bDoActorMapping=*/ bWasGenerated);
	}

	// Following a change in some properties or in some spatial information related to this component,
	// We need to regenerate/cleanup the graph, depending of the state in the editor.
	if (!bActivated)
	{
		CleanupLocalImmediate(/*bRemoveComponents=*/true);
		bGenerated = bWasGenerated;
	}
	else
	{
		// If we just cleaned up resources, call back generate
		// Also, for BPs, we ask if we should generate, to support generate on added to world.
		if ((bWasGenerated || ShouldGenerateBPPCGAddedToWorld()) && (!bGenerated || bRegenerateInEditor))
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

		if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
		{
			UE_LOG(LogPCG, Log, TEXT("         [%s] CACHE REFRESH CachedPCGData"), *GetOwner()->GetName());
		}
	}

	return CachedPCGData;
}

UPCGData* UPCGComponent::GetInputPCGData()
{
	if (!CachedInputData)
	{
		CachedInputData = CreateInputPCGData();

		if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
		{
			UE_LOG(LogPCG, Log, TEXT("         [%s] CACHE REFRESH CachedInputData"), *GetOwner()->GetName());
		}
	}

	return CachedInputData;
}

UPCGData* UPCGComponent::GetActorPCGData()
{
	// Actor PCG Data can be a Landscape data too
	if (!CachedActorData || IsLandscapeCachedDataDirty(CachedActorData))
	{
		CachedActorData = CreateActorPCGData();

		if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
		{
			UE_LOG(LogPCG, Log, TEXT("         [%s] CACHE REFRESH CachedActorData"), *GetOwner()->GetName());
		}
	}

	return CachedActorData;
}

UPCGData* UPCGComponent::GetLandscapePCGData()
{
	if (!CachedLandscapeData || IsLandscapeCachedDataDirty(CachedLandscapeData))
	{
		CachedLandscapeData = CreateLandscapePCGData(/*bHeightOnly=*/false);

		if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
		{
			UE_LOG(LogPCG, Log, TEXT("         [%s] CACHE REFRESH CachedLandscapeData"), *GetOwner()->GetName());
		}
	}

	return CachedLandscapeData;
}

UPCGData* UPCGComponent::GetLandscapeHeightPCGData()
{
	if (!CachedLandscapeHeightData || IsLandscapeCachedDataDirty(CachedLandscapeHeightData))
	{
		CachedLandscapeHeightData = CreateLandscapePCGData(/*bHeightOnly=*/true);

		if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
		{
			UE_LOG(LogPCG, Log, TEXT("         [%s] CACHE REFRESH CachedLandscapeHeightData"), *GetOwner()->GetName());
		}
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

UPCGData* UPCGComponent::CreateActorPCGData()
{
	return CreateActorPCGData(GetOwner(), bParseActorComponents);
}

UPCGData* UPCGComponent::CreateActorPCGData(AActor* Actor, bool bParseActor)
{
	return CreateActorPCGData(Actor, this, bParseActor);
}

UPCGData* UPCGComponent::CreateActorPCGData(AActor* Actor, const UPCGComponent* Component, bool bParseActor)
{
	auto AcceptAllData = [](EPCGDataType InDataType) { return true; };
	FPCGDataCollection Collection = CreateActorPCGDataCollection(Actor, Component, AcceptAllData, bParseActor);
	if (Collection.TaggedData.Num() > 1)
	{
		UPCGUnionData* Union = NewObject<UPCGUnionData>();
		for (const FPCGTaggedData& TaggedData : Collection.TaggedData)
		{
			Union->AddData(CastChecked<const UPCGSpatialData>(TaggedData.Data));
		}

		return Union;
	}
	else if(Collection.TaggedData.Num() == 1)
	{
		return Cast<UPCGData>(Collection.TaggedData[0].Data);
	}
	else
	{
		return nullptr;
	}
}

FPCGDataCollection UPCGComponent::CreateActorPCGDataCollection(AActor* Actor, const UPCGComponent* Component, const TFunction<bool(EPCGDataType)>& InDataFilter, bool bParseActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreateActorPCGData);
	FPCGDataCollection Collection;

	if (!Actor)
	{
		return Collection;
	}

	auto NameTagsToStringTags = [](const FName& InName) { return InName.ToString(); };
	TSet<FString> ActorTags;
	Algo::Transform(Actor->Tags, ActorTags, NameTagsToStringTags);

	// Fill in collection based on the data on the given actor.
	// Some actor types we will forego full parsing to build strictly on the actor existence, such as partition actors, volumes and landscape
	// TODO: add factory for extensibility
	// TODO: review the !bParseActor cases - it might make sense to have just a point for a partition actor, even if we preintersect it.
	APCGPartitionActor* PartitionActor = InDataFilter(EPCGDataType::Spatial) ? Cast<APCGPartitionActor>(Actor) : nullptr;
	ALandscapeProxy* LandscapeActor = InDataFilter(EPCGDataType::Landscape) ? Cast<ALandscapeProxy>(Actor) : nullptr;
	AVolume* VolumeActor = InDataFilter(EPCGDataType::Volume) ? Cast<AVolume>(Actor) : nullptr;
	if (PartitionActor)
	{
		check(!Component || Component->GetOwner() == Actor); // Invalid processing otherwise because of the this usage

		UPCGVolumeData* Data = NewObject<UPCGVolumeData>();
		Data->Initialize(PartitionActor->GetFixedBounds(), PartitionActor);

		UPCGComponent* OriginalComponent = Component ? PartitionActor->GetOriginalComponent(Component) : nullptr;
		// Important note: we do NOT call the collection version here, as we want to have a union if that's the case
		const UPCGSpatialData* OriginalComponentSpatialData = OriginalComponent ? Cast<const UPCGSpatialData>(OriginalComponent->GetActorPCGData()) : nullptr;

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = OriginalComponentSpatialData ? Cast<UPCGData>(Data->IntersectWith(OriginalComponentSpatialData)) : Cast<UPCGData>(Data);
		// No need to keep partition actor tags, though we might want to push PCG grid GUID at some point
	}
	else if (LandscapeActor)
	{
		UPCGLandscapeData* Data = NewObject<UPCGLandscapeData>();
		const UPCGGraph* PCGGraph = Component ? Component->GetGraph() : nullptr;
		const bool bUseLandscapeMetadata = (!PCGGraph || PCGGraph->bLandscapeUsesMetadata);

		Data->Initialize({ LandscapeActor }, PCGHelpers::GetGridBounds(Actor, Component), /*bHeightOnly=*/false, bUseLandscapeMetadata);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}
	else if (!bParseActor && InDataFilter(EPCGDataType::Point))
	{
		UPCGPointData* Data = NewObject<UPCGPointData>();
		Data->InitializeFromActor(Actor);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}
	else if (VolumeActor)
	{
		UPCGVolumeData* Data = NewObject<UPCGVolumeData>();
		Data->Initialize(VolumeActor);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}
	else // Prepare data on a component basis
	{
		using PrimitiveComponentArray = TInlineComponentArray<UPrimitiveComponent*, 4>;
		PrimitiveComponentArray Primitives;

		auto RemoveDuplicatesFromPrimitives = [&Primitives](const auto& InComponents)
		{
			Primitives.RemoveAll([&InComponents](UPrimitiveComponent* Component)
			{
				return InComponents.Contains(Component);
			});
		};

		auto RemovePCGGeneratedEntries = [](auto& InComponents)
		{
			for (int32 Index = InComponents.Num() - 1; Index >= 0; --Index)
			{
				if (InComponents[Index]->ComponentTags.Contains(PCGHelpers::DefaultPCGTag))
				{
					InComponents.RemoveAtSwap(Index);
				}
			}
		};

		auto RemoveSplineMeshComponents = [](PrimitiveComponentArray& InComponents)
		{
			for (int32 Index = InComponents.Num() - 1; Index >= 0; --Index)
			{
				if (InComponents[Index]->IsA<USplineMeshComponent>())
				{
					InComponents.RemoveAtSwap(Index);
				}
			}
		};

		Actor->GetComponents(Primitives);
		RemovePCGGeneratedEntries(Primitives);

		TInlineComponentArray<ULandscapeSplinesComponent*, 4> LandscapeSplines;
		Actor->GetComponents(LandscapeSplines);
		RemovePCGGeneratedEntries(LandscapeSplines);
		RemoveDuplicatesFromPrimitives(LandscapeSplines);

		TInlineComponentArray<USplineComponent*, 4> Splines;
		Actor->GetComponents(Splines);
		RemovePCGGeneratedEntries(Splines);
		RemoveDuplicatesFromPrimitives(Splines);

		// If we have a better representation than the spline mesh components, we shouldn't create them
		if (!LandscapeSplines.IsEmpty() || !Splines.IsEmpty())
		{
			RemoveSplineMeshComponents(Primitives);
		}

		TInlineComponentArray<UShapeComponent*, 4> Shapes;
		Actor->GetComponents(Shapes);
		RemovePCGGeneratedEntries(Shapes);
		RemoveDuplicatesFromPrimitives(Shapes);

		if (InDataFilter(EPCGDataType::Spline))
		{
			for (ULandscapeSplinesComponent* SplineComponent : LandscapeSplines)
			{
				UPCGLandscapeSplineData* SplineData = NewObject<UPCGLandscapeSplineData>();
				SplineData->Initialize(SplineComponent);

				FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
				TaggedData.Data = SplineData;
				Algo::Transform(SplineComponent->ComponentTags, TaggedData.Tags, NameTagsToStringTags);
				TaggedData.Tags.Append(ActorTags);
			}

			for (USplineComponent* SplineComponent : Splines)
			{
				UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
				SplineData->Initialize(SplineComponent);

				FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
				TaggedData.Data = SplineData;
				Algo::Transform(SplineComponent->ComponentTags, TaggedData.Tags, NameTagsToStringTags);
				TaggedData.Tags.Append(ActorTags);
			}
		}

		if (InDataFilter(EPCGDataType::Primitive))
		{
			for (UShapeComponent* ShapeComponent : Shapes)
			{
				UPCGSpatialData* Data = nullptr;
				if (UPCGCollisionShapeData::IsSupported(ShapeComponent))
				{
					UPCGCollisionShapeData* ShapeData = NewObject<UPCGCollisionShapeData>();
					ShapeData->Initialize(ShapeComponent);

					Data = ShapeData;
				}
				else
				{
					UPCGPrimitiveData* ShapeData = NewObject<UPCGPrimitiveData>();
					ShapeData->Initialize(ShapeComponent);

					Data = ShapeData;
				}

				FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
				TaggedData.Data = Data;
				Algo::Transform(ShapeComponent->ComponentTags, TaggedData.Tags, NameTagsToStringTags);
				TaggedData.Tags.Append(ActorTags);
			}

			for (UPrimitiveComponent* PrimitiveComponent : Primitives)
			{
				// Exception: skip the billboard component
				if (Cast<UBillboardComponent>(PrimitiveComponent))
				{
					continue;
				}

				UPCGPrimitiveData* PrimitiveData = NewObject<UPCGPrimitiveData>();
				PrimitiveData->Initialize(PrimitiveComponent);

				FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
				TaggedData.Data = PrimitiveData;
				Algo::Transform(PrimitiveComponent->ComponentTags, TaggedData.Tags, NameTagsToStringTags);
				TaggedData.Tags.Append(ActorTags);
			}
		}
	}

	// Finally, if it's not a special actor and there are not parsed components, then return a single point at the actor position
	if (Collection.TaggedData.IsEmpty() && InDataFilter(EPCGDataType::Point))
	{
		UPCGPointData* Data = NewObject<UPCGPointData>();
		Data->InitializeFromActor(Actor);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}

	return Collection;
}

UPCGData* UPCGComponent::CreatePCGData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreatePCGData);
	return GetInputPCGData();
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
	const UPCGGraph* PCGGraph = GetGraph();
	LandscapeData->Initialize(Landscapes, LandscapeBounds, bHeightOnly, /*bUseMetadata=*/PCGGraph && PCGGraph->bLandscapeUsesMetadata);
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
		const UPCGGraph* PCGGraph = GetGraph();
		IsCacheDirty = PCGGraph && (CachedData->IsUsingMetadata() != PCGGraph->bLandscapeUsesMetadata);
	}

	return IsCacheDirty;
}

FBox UPCGComponent::GetGridBounds() const
{
	return PCGHelpers::GetGridBounds(GetOwner(), this);
}

FBox UPCGComponent::GetGridBounds(const AActor* Actor) const
{
	return PCGHelpers::GetGridBounds(Actor, this);
}

UPCGSubsystem* UPCGComponent::GetSubsystem() const
{
	return GetOwner() ? UPCGSubsystem::GetInstance(GetOwner()->GetWorld()) : nullptr;
}

#if WITH_EDITOR
bool UPCGComponent::PopulateTrackedActorToTagsMap(bool bForce)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::PopulateTrackedActorToTagsMap);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::AddTrackedActor);
	if (!bForce)
	{
		PopulateTrackedActorToTagsMap();
	}

	if (!GetOwner())
	{
		// Without an owner, we can't get our bounds, so we won't be able to track.
		// If we don't have an owner, we are probably in a BP, not instanciated, so no need to track anything.
		return false;
	}

	check(InActor);
	bool bAppliedChange = false;

	bool bIntersectionComputed = false;
	bool bIntersect = false;

	for (const FName& Tag : InActor->Tags)
	{
		const bool* ShouldCullPtr = CachedTrackedTagsToCulling.Find(Tag);

		if (!ShouldCullPtr)
		{
			continue;
		}

		if (*ShouldCullPtr)
		{
			if (!bIntersectionComputed)
			{
				bIntersect = GetGridBounds().Intersect(GetGridBounds(InActor));
				bIntersectionComputed = true;
			}

			if (!bIntersect)
			{
				continue;
			}
		}

		bAppliedChange = true;
		CachedTrackedActorToTags.FindOrAdd(InActor).Add(Tag);
		PCGHelpers::GatherDependencies(InActor, CachedTrackedActorToDependencies.FindOrAdd(InActor), 1);

		if (!bForce)
		{
			// Since we arrived here, we already verified culling, so we can ignore it.
			DirtyCacheFromTag(Tag, InActor, /*bIgnoreCull=*/true);
		}
	}

	return bAppliedChange;
}

bool UPCGComponent::RemoveTrackedActor(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::RemoveTrackedActor);
	PopulateTrackedActorToTagsMap();

	check(InActor);
	bool bAppliedChange = false;

	if(CachedTrackedActorToTags.Contains(InActor))
	{
		for (const FName& Tag : CachedTrackedActorToTags[InActor])
		{
			bAppliedChange |= DirtyCacheFromTag(Tag, InActor);
		}

		CachedTrackedActorToTags.Remove(InActor);
		CachedTrackedActorToDependencies.Remove(InActor);
	}

	return bAppliedChange;
}

bool UPCGComponent::UpdateTrackedActor(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::UpdateTrackedActor);
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
				bAppliedChange |= DirtyCacheFromTag(CachedTag, InActor);
				CachedTrackedActorToTags[InActor].Remove(CachedTag);
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
			PCGHelpers::GatherDependencies(InActor, CachedTrackedActorToDependencies.FindOrAdd(InActor), 1);
			bAppliedChange |= DirtyCacheFromTag(Tag, InActor);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::DirtyTrackedActor);
	PopulateTrackedActorToTagsMap();

	check(InActor);
	bool bAppliedChange = false;

	bool bIntersectionComputed = false;
	bool bIntersect = false;

	if (CachedTrackedActorToTags.Contains(InActor))
	{
		TSet<FName> CachedTags = CachedTrackedActorToTags[InActor];
		for (const FName& CachedTag : CachedTags)
		{
			const bool* ShouldCullPtr = CachedTrackedTagsToCulling.Find(CachedTag);

			if (!ShouldCullPtr)
			{
				continue;
			}

			if (*ShouldCullPtr)
			{
				if (!bIntersectionComputed)
				{
					bIntersect = GetGridBounds().Intersect(GetGridBounds(InActor));
					bIntersectionComputed = true;
				}

				if (!bIntersect)
				{
					// We were tracking the tag, but it is now culled. We'll need to dirty the settings
					// even if it is culled (that's why we ignore cull here).
					bAppliedChange |= DirtyCacheFromTag(CachedTag, InActor, /*bIgnoreCull=*/ true);
					CachedTrackedActorToTags[InActor].Remove(CachedTag);
					continue;
				}
			}

			bAppliedChange |= DirtyCacheFromTag(CachedTag, InActor);
		}
	}
	else if (AddTrackedActor(InActor))
	{
		bAppliedChange = true;
	}

	// Since we might have remove some tags, do some cleaning there
	if (CachedTrackedActorToTags.Contains(InActor) && CachedTrackedActorToTags[InActor].IsEmpty())
	{
		CachedTrackedActorToTags.Remove(InActor);
		CachedTrackedActorToDependencies.Remove(InActor);
	}

	return bAppliedChange;
}

bool UPCGComponent::DirtyCacheFromTag(const FName& InTag, const AActor* InActor, bool bIgnoreCull)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::DirtyCacheFromTag);
	bool bWasDirtied = false;

	const TSet<FPCGSettingsAndCulling>* SettingsAndCullingPtr = CachedTrackedTagsToSettings.Find(InTag);

	if (SettingsAndCullingPtr)
	{
		// Don't compute the bounds if we never cull.
		bool bIntersectionComputed = false;
		bool bIntersect = false;

		for (const FPCGSettingsAndCulling& SettingsAndCulling : *SettingsAndCullingPtr)
		{
			const TWeakObjectPtr<const UPCGSettings>& Settings = SettingsAndCulling.Key;
			const bool bShouldCull = SettingsAndCulling.Value && !bIgnoreCull;

			if (Settings.IsValid() && GetSubsystem())
			{
				// If we cull and no intersection, continue
				if (bShouldCull)
				{
					if (!bIntersectionComputed)
					{
						bIntersect = GetGridBounds().Intersect(GetGridBounds(InActor));
						bIntersectionComputed = true;
					}

					if (!bIntersect)
					{
						continue;
					}
				}

				GetSubsystem()->CleanFromCache(Settings->GetElement().Get(), Settings.Get());
				bWasDirtied = true;
			}
		}
	}

	return bWasDirtied;
}

void UPCGComponent::DirtyCacheForAllTrackedTags()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::DirtyCacheForAllTrackedTags);
	for (const auto& TagToSettings : CachedTrackedTagsToSettings)
	{
		for (const FPCGSettingsAndCulling& SettingsAndCulling : TagToSettings.Value)
		{
			const TWeakObjectPtr<const UPCGSettings>& Settings = SettingsAndCulling.Key;
			if (Settings.IsValid() && GetSubsystem())
			{
				GetSubsystem()->CleanFromCache(Settings->GetElement().Get(), Settings.Get());
			}
		}
	}
}

bool UPCGComponent::GraphUsesLandscapePin() const
{
	const UPCGGraph* PCGGraph = GetGraph();
	return PCGGraph &&
		(PCGGraph->GetInputNode()->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeLabel) ||
		PCGGraph->GetInputNode()->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeHeightLabel));
}

#endif // WITH_EDITOR

void UPCGComponent::SetManagedResources(const TArray<TObjectPtr<UPCGManagedResource>>& Resources)
{
	FScopeLock ResourcesLock(&GeneratedResourcesLock);
	check(GeneratedResources.IsEmpty());
	GeneratedResources = Resources;

	// Remove any null entries
	for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
	{
		if (!GeneratedResources[ResourceIndex])
		{
			GeneratedResources.RemoveAtSwap(ResourceIndex);
		}
	}
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

		// IMPORTANT NOTE:
		// Any non-visible (i.e. UPROPERTY() with no specifiers) are NOT copied over when re-running the construction script
		// This means that some properties need to be reapplied here manually unless we make them visible
		if (SourceComponent)
		{
			// Critical: LastGeneratedBounds & GeneratedGraphOutput
			PCGComponent->LastGeneratedBounds = SourceComponent->LastGeneratedBounds;

			PCGComponent->GeneratedGraphOutput = SourceComponent->GeneratedGraphOutput;
			// Re-outer any data moved here
			for (FPCGTaggedData& TaggedData : PCGComponent->GeneratedGraphOutput.TaggedData)
			{
				if (TaggedData.Data)
				{
					const_cast<UPCGData*>(TaggedData.Data.Get())->Rename(nullptr, PCGComponent, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}

#if WITH_EDITOR
			// bDirtyGenerated is transient.
			PCGComponent->bDirtyGenerated = SourceComponent->bDirtyGenerated; 
#endif // WITH_EDITOR

			// Non-critical but should be done: transient data, tracked actors cache, landscape tracking
			// TODO Validate usefulness + move accordingly
		}
		
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

#if WITH_EDITOR
		// Reconnect callbacks
		PCGComponent->GraphInstance->FixCallbacks();
#endif // WITH_EDITOR

		bool bDoActorMapping = PCGComponent->bGenerated || PCGHelpers::IsRuntimeOrPIE();

		// Also remap
		UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem();
		if (Subsystem && SourceComponent)
		{
			Subsystem->RemapPCGComponent(SourceComponent, PCGComponent, bDoActorMapping);
		}

#if WITH_EDITOR
		// Disconnect callbacks on source.
		if (SourceComponent && SourceComponent->GraphInstance)
		{
			SourceComponent->GraphInstance->TeardownCallbacks();
		}

		// Finally, start a delayed refresh task (if there is not one already), in editor only
		// It is important to be delayed, because we cannot spawn Partition Actors within this scope,
		// because we are in a construction script.
		// Note that we only do this if we are not currently loading
		if (!SourceComponent || !SourceComponent->HasAllFlags(RF_WasLoaded))
		{
			PCGComponent->Refresh();
		}
#endif // WITH_EDITOR
	}
}

void FPCGComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SourceComponent);
	Collector.AddReferencedObjects(GeneratedResources);
}

#undef LOCTEXT_NAMESPACE
