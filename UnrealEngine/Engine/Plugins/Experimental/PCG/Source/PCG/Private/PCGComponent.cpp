// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComponent.h"

#include "PCGContext.h"
#include "PCGEngineSettings.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGManagedResource.h"
#include "PCGParamData.h"
#include "PCGPin.h"
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
#include "Graph/PCGStackContext.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGGeneratedResourcesLogging.h"

#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Algo/AnyOf.h"
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
#include "EditorActorFolders.h"
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

UPCGComponent::~UPCGComponent()
{
#if WITH_EDITOR
	// For the special case where a component is part of a reconstruction script (from a BP),
	// but gets destroyed immediately, we need to force the unregistering. 
	if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
	{
		PCGSubsystem->UnregisterPCGComponent(this, /*bForce=*/true);
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

	TSet<FPCGActorSelectionKey> TrackedKeys;
	TSet<FPCGActorSelectionKey> OriginalTrackedKeys;
	CachedTrackedKeysToSettings.GetKeys(TrackedKeys);
	Original->CachedTrackedKeysToSettings.GetKeys(OriginalTrackedKeys);

	const bool bHasDirtyTracking = !(TrackedKeys.Num() == OriginalTrackedKeys.Num() && TrackedKeys.Includes(OriginalTrackedKeys));

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
		UpdateTrackingCache();
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

FPCGTaskId UPCGComponent::GenerateLocalGetTaskId(bool bForce)
{
	return GenerateInternal(bForce, EPCGComponentGenerationTrigger::GenerateOnDemand, {});
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

	StopGenerationInProgress();
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
	// Implementation note: while it may seem logical to clear the bDirtyGenerated flag here, 
	// the component is still considered dirty if we aborted processing, hence it should stay this way.

	StopGenerationInProgress();
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

#if WITH_EDITOR
	// If there is an associated generated folder from this actor, rename it according to the stamp name
	if (World && NewActor)
	{
		FString GeneratedFolderPath;
		PCGHelpers::GetGeneratedActorsFolderPath(GetOwner(), GeneratedFolderPath);
		
		FString GeneratedStampFolder;
		PCGHelpers::GetGeneratedActorsFolderPath(NewActor, GeneratedStampFolder);

		if (!GeneratedFolderPath.IsEmpty() && !GeneratedStampFolder.IsEmpty())
		{
			FFolder GeneratedFolder(FFolder::GetWorldRootFolder(World).GetRootObject(), *GeneratedFolderPath);
			FFolder StampFolder(FFolder::GetWorldRootFolder(World).GetRootObject(), *GeneratedStampFolder);

			const bool bGeneratedFolderExists = GeneratedFolder.IsValid() && FActorFolders::Get().ContainsFolder(*World, GeneratedFolder);
			const bool bStampFolderExists = FActorFolders::Get().ContainsFolder(*World, StampFolder);

			// TODO: improve behavior when target stamp folder would exist
			if (bGeneratedFolderExists && !bStampFolderExists)
			{
				FActorFolders::Get().RenameFolderInWorld(*World, GeneratedFolder, StampFolder);
			}
		}
	}
#endif

	return NewActor;
}

EPCGHiGenGrid UPCGComponent::GetGenerationGrid() const
{
	const uint32 GridSize = GetGenerationGridSize();
	if (PCGHiGenGrid::IsValidGridSize(GridSize))
	{
		return PCGHiGenGrid::GridSizeToGrid(GridSize);
	}
	else if (GridSize == PCGHiGenGrid::UnboundedGridSize())
	{
		return EPCGHiGenGrid::Unbounded;
	}
	else
	{
		return PCGHiGenGrid::GridSizeToGrid(GetGraph()->GetDefaultGridSize());
	}
}

void UPCGComponent::StoreOutputDataForPin(const FString& InResourceKey, const FPCGDataCollection& InData)
{
	FReadScopeLock ScopedWriteLock(PerPinGeneratedOutputLock);
	PerPinGeneratedOutput.FindOrAdd(InResourceKey) = InData;
}

const FPCGDataCollection* UPCGComponent::RetrieveOutputDataForPin(const FString& InResourceKey)
{
	FReadScopeLock ScopedReadLock(PerPinGeneratedOutputLock);
	return PerPinGeneratedOutput.Find(InResourceKey);
}

void UPCGComponent::ClearPerPinGeneratedOutput()
{
	FReadScopeLock ScopedWriteLock(PerPinGeneratedOutputLock);
	PerPinGeneratedOutput.Reset();
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
				GeneratedResource->MoveResourceToNewActor(NewActor, Owner);
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
				if (Resource)
				{
					Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				}

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
	if (GetSubsystem() && GetSubsystem()->IsGraphCacheDebuggingEnabled())
	{
		UE_LOG(LogPCG, Log, TEXT("[%s] --- CLEANUP COMPONENT ---"), GetOwner() ? *GetOwner()->GetName() : TEXT("MissingComponent"));
	}

	if ((!bGenerated && GeneratedResources.IsEmpty() && !IsGenerating()) || IsCleaningUp())
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
					if (Resource)
					{
						Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					}

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

		if (UPCGComponent* ThisComponent = ThisComponentWeakPtr.Get())
		{
#if WITH_EDITOR
			if (UWorld* ThisWorld = ThisComponent->GetWorld())
			{
				// Look for a nested generated results subfolder and remove it if it exists
				FString FolderPath;
				PCGHelpers::GetGeneratedActorsFolderPath(ThisComponent->GetOwner(), FolderPath);

				if (!FolderPath.IsEmpty())
				{
					FFolder GeneratedFolder(FFolder::GetWorldRootFolder(ThisWorld).GetRootObject(), *FolderPath);

					const bool bFolderExists = GeneratedFolder.IsValid() && FActorFolders::Get().ContainsFolder(*ThisWorld, GeneratedFolder);
					bool bFoundActors = false;

					if (bFolderExists)
					{
						TArray<FName> Paths = { *FolderPath };
						FActorFolders::ForEachActorInFolders(*ThisWorld, Paths, [&bFoundActors](AActor* InActor)
						{
							if (InActor)
							{
								bFoundActors = true;
								return false;
							}
							else
							{
								return true;
							}
						});
					}

					if (bFolderExists && !bFoundActors)
					{
						// Delete all subfolders
						TArray<FFolder> SubfoldersToDelete;
						FActorFolders::Get().ForEachFolder(*ThisWorld, [&GeneratedFolder, &SubfoldersToDelete](const FFolder& InFolder)
						{
							if (InFolder.IsChildOf(GeneratedFolder))
							{
								SubfoldersToDelete.Add(InFolder);
							}

							return true;
						});

						for (const FFolder& FolderToDelete : SubfoldersToDelete)
						{
							FActorFolders::Get().DeleteFolder(*ThisWorld, FolderToDelete);
						}

						// Finally, delete folder
						FActorFolders::Get().DeleteFolder(*ThisWorld, GeneratedFolder);
					}
				}
			}
#endif

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
			UPCGManagedResource* Resource = GetValid(GeneratedResources[ResourceIndex]);

			PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResourcesResource(Resource);

			if (!Resource && GetOwner())
			{
				UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::CleanupUnusedManagedResources] Null generated resource encountered on actor \"%s\"."), *GetOwner()->GetFName().ToString());
			}

			if (!Resource || Resource->ReleaseIfUnused(ActorsToDelete))
			{
				if (Resource)
				{
					Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				}

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

void UPCGComponent::OnUnregister()
{
#if WITH_EDITOR
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (!PCGHelpers::IsRuntimeOrPIE())
		{
			Subsystem->CancelGeneration(this);
			Subsystem->UnregisterPCGComponent(this);
		}
	}
#endif // WITH_EDITOR

	Super::OnUnregister();
}

void UPCGComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	// BeginDestroy is not called immediately when a component is destroyed. Therefore callbacks are not cleaned
	// until GC is ran, and can stack up with BP reconstruction scripts. Force the removal of callbacks here. If the component
	// is dead, we don't want to react to callbacks anyway.
	if (GraphInstance)
	{
		GraphInstance->OnGraphChangedDelegate.RemoveAll(this);
		GraphInstance->TeardownCallbacks();
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
	UpdateTrackingCache();

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
#endif

	Super::BeginDestroy();
}

void UPCGComponent::PostInitProperties()
{
#if WITH_EDITOR
	GraphInstance->OnGraphChangedDelegate.AddUObject(this, &UPCGComponent::OnGraphChanged);
#endif // WITH_EDITOR

#if WITH_EDITOR
	// Force bIsPartitioned at false for new objects
	if (PCGHelpers::IsNewObjectAndNotDefault(this, /*bCheckHierarchy=*/true))
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
		UpdateTrackingCache();
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->UpdateComponentTracking(this, /*bInShouldDirtyActors=*/ true);
		}

		DirtyGenerated(bDirtyInputs ? (EPCGComponentDirtyFlag::Actor | EPCGComponentDirtyFlag::Landscape) : EPCGComponentDirtyFlag::None);
		if (bHasGraph)
		{
			Refresh(bIsStructural);
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
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->UpdateComponentTracking(this);
		}

		DirtyGenerated(EPCGComponentDirtyFlag::Input);
		Refresh();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, bParseActorComponents))
	{
		DirtyGenerated(EPCGComponentDirtyFlag::Input);
		Refresh();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, Seed))
	{
		DirtyGenerated();
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
}

void UPCGComponent::PostEditUndo()
{
	LastGeneratedBounds = LastGeneratedBoundsPriorToUndo;

	UpdateTrackingCache();
	DirtyGenerated(EPCGComponentDirtyFlag::All);

	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->UpdateComponentTracking(this, /*bInShouldDirtyActors=*/ true);
	}

	if (bGenerated)
	{
		Refresh(/*bIsStructural=*/true);
	}
}

void UPCGComponent::UpdateTrackingCache()
{
	// Without an owner, it probably means we are in a BP template, so no need to setup callbacks
	if (!GetOwner())
	{
		return;
	}

	CachedTrackedKeysToSettings.Reset();
	CachedTrackedKeysToCulling.Reset();

	if (UPCGGraph* PCGGraph = GetGraph())
	{
		CachedTrackedKeysToSettings = PCGGraph->GetTrackedActorKeysToSettings();

		// A tag should be culled, if only all the settings that track this tag should cull.
		// Note that is only impact the fact that we track (or not) this tag.
		// If a setting is marked as "should cull", it will only be dirtied (at least by default), if the actor with the
		// given tag intersect with the component.
		for (const TPair<FPCGActorSelectionKey, TArray<FPCGSettingsAndCulling>>& It : CachedTrackedKeysToSettings)
		{
			const FPCGActorSelectionKey& Key = It.Key;

			bool bShouldCull = true;
			for (const FPCGSettingsAndCulling& SettingsAndCulling : It.Value)
			{
				if (!SettingsAndCulling.Value)
				{
					bShouldCull = false;
					break;
				}
			}

			CachedTrackedKeysToCulling.Emplace(Key, bShouldCull);
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

	ClearPerPinGeneratedOutput();

	// Dirty data as a waterfall from basic values
	if (!!(DirtyFlag & EPCGComponentDirtyFlag::Actor))
	{
		CachedActorData = nullptr;
		// Since landscape data is related on the bounds of the current actor, when we dirty the actor data, we need to dirty the landscape data as well
		CachedLandscapeData = nullptr;
		CachedLandscapeHeightData = nullptr;
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

bool UPCGComponent::IsInspecting() const
{
	return InspectionCounter > 0;
}

void UPCGComponent::EnableInspection()
{
	if (!ensure(InspectionCounter >= 0))
	{
		InspectionCounter = 0;
	}
	
	InspectionCounter++;
}

void UPCGComponent::DisableInspection()
{
	if (ensure(InspectionCounter > 0))
	{
		InspectionCounter--;
	}
	
	if (InspectionCounter == 0)
	{
		InspectionCache.Empty();
	}
};

void UPCGComponent::StoreInspectionData(const FPCGStack* InStack, const UPCGNode* InNode, const FPCGDataCollection& InInputData, const FPCGDataCollection& InOutputData)
{
	if (!IsInspecting() || !InNode || !ensure(InStack))
	{
		return;
	}

	auto StorePinInspectionData = [InStack, InNode](const TArray<TObjectPtr<UPCGPin>>& InPins, const FPCGDataCollection& InData, TMap<FPCGStack, FPCGDataCollection>& InOutInspectionCache)
	{
		for (const UPCGPin* Pin : InPins)
		{
			FPCGStack Stack = *InStack;

			// Append the Node and Pin to the current Stack to uniquely identify each DataCollection
			TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFramesMutable();
			StackFrames.Reserve(StackFrames.Num() + 2);
			StackFrames.Emplace(InNode);
			StackFrames.Emplace(Pin);

			FPCGDataCollection PinDataCollection;
			PinDataCollection.TaggedData = InData.GetInputsByPin(Pin->Properties.Label);
			// The data collection for each pin is given the Crc from the data collection. This is to enable inspecting the normal node output Crc
			// when cache debugging is enabled.
			PinDataCollection.Crc = InData.Crc;

			if (!PinDataCollection.TaggedData.IsEmpty())
			{
				InOutInspectionCache.Add(Stack, PinDataCollection);
			}
			else
			{
				InOutInspectionCache.Remove(Stack);
			}
		}
	};

	StorePinInspectionData(InNode->GetInputPins(), InInputData, InspectionCache);
	StorePinInspectionData(InNode->GetOutputPins(), InOutputData, InspectionCache);
}

const FPCGDataCollection* UPCGComponent::GetInspectionData(const FPCGStack& InStack) const
{
	return InspectionCache.Find(InStack);
}

void UPCGComponent::Refresh(bool bStructural)
{
	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (GetOwner() && GetOwner()->bIsEditorPreviewActor)
	{
		return;
	}

	// If the component is tagged as not to regenerate in the editor, only exceptional cases should trigger a refresh
	// namely: the component is deactivated.
	// Note that the component changing its IsPartitioned state is already covered in the PostEditChangeProperty
	// Note that even if this is force refresh/structural change, we will NOT refresh
	if (!bRegenerateInEditor && bActivated)
	{
		// We still need to trigger component registration event otherwise further generations will fail if this is moved.
		// Note that we pass in false here to remove everything when moving a partitioned graph because we would otherwise need to do a reversible stamp to support this
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RegisterOrUpdatePCGComponent(this, bGenerated);
		}

		return;
	}

	// Discard any refresh if have already one scheduled.
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		// Cancel an already existing generation if either the change is structural in nature (which requires a recompilation, so a full-rescheduling)
		// or if the generation is already started
		const bool bGenerationWasInProgress = IsGenerationInProgress();
		if (CurrentGenerationTask != InvalidPCGTaskId && (bStructural || bGenerationWasInProgress))
		{
			CancelGeneration();
		}

		// Calling a new refresh here might not be sufficient; if the current component was generating but was not previously generated,
		// then the bGenerated flag will be false, which will prevent a subsequent update here
		if (CurrentRefreshTask == InvalidPCGTaskId && CurrentCleanupTask == InvalidPCGTaskId)
		{
			CurrentRefreshTask = Subsystem->ScheduleRefresh(this, bGenerationWasInProgress);
		}
	}
}

void UPCGComponent::StartGenerationInProgress()
{
	// Implementation detail:
	// Since the original component is not guaranteed to run the FetchInput element, local components are "allowed" to mark generation in progress on their original component.
	// However, the PostProcessGraph on the original component will be guaranteed to be called at the end of the execution so we do not need this mechanism in that case.
	bGenerationInProgress = true;

	if (IsLocalComponent())
	{
		if (UPCGComponent* OriginalComponent = CastChecked<APCGPartitionActor>(GetOwner())->GetOriginalComponent(this))
		{
			OriginalComponent->bGenerationInProgress = true;
		}
	}
}

void UPCGComponent::StopGenerationInProgress()
{
	bGenerationInProgress = false;
}

bool UPCGComponent::IsGenerationInProgress()
{
	return bGenerationInProgress;
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

bool UPCGComponent::IsActorTracked(AActor* InActor, bool& bOutIsCulled) const
{
	check(InActor);

	if (!GetOwner())
	{
		return false;
	}

	// We should always track the owner of the component, without culling
	if (GetOwner() == InActor)
	{
		bOutIsCulled = false;
		return true;
	}

	// If we track the landscape using legacy methods and it is a landscape, it should be tracked as culled
	if (InActor->IsA<ALandscapeProxy>() && ShouldTrackLandscape())
	{
		bOutIsCulled = true;
		return true;
	}

	bool bFound = false;

	for (const TPair<FPCGActorSelectionKey, bool>& It : CachedTrackedKeysToCulling)
	{
		if (It.Key.IsMatching(InActor, this))
		{
			bOutIsCulled = It.Value;
			bFound = true;

			// Check for other tags that might not be culled
			if (bOutIsCulled)
			{
				continue;
			}

			return true;
		}
	}

	return bFound;
}

void UPCGComponent::OnRefresh(bool bForceRefresh)
{
	// Mark the refresh task invalid to allow re-triggering refreshes
	CurrentRefreshTask = InvalidPCGTaskId;

	// Before doing a refresh, update the component to the subsystem if we are partitioned
	// Only redo the mapping if we are generated
	UPCGSubsystem* Subsystem = GetSubsystem();
	const bool bWasGenerated = bGenerated;
	const bool bWasGeneratedOrGenerating = bWasGenerated || bForceRefresh;

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
		Subsystem->RegisterOrUpdatePCGComponent(this, /*bDoActorMapping=*/ bWasGeneratedOrGenerating);
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
		// If we just cleaned up resources, call back generate. Only do this for original component, which will then trigger
		// generation of local components. Also, for BPs, we ask if we should generate, to support generate on added to world.
		if ((bWasGeneratedOrGenerating || ShouldGenerateBPPCGAddedToWorld()) && !IsLocalComponent() && (!bGenerated || bRegenerateInEditor))
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

UPCGComponent* UPCGComponent::GetOriginalComponent()
{
	if (!IsLocalComponent())
	{
		return this;
	}

	APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(GetOwner());
	return ensure(PartitionActor) ? PartitionActor->GetOriginalComponent(this) : this;
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
	FPCGDataCollection Collection = CreateActorPCGDataCollection(Actor, Component, EPCGDataType::Any, bParseActor);
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

FPCGDataCollection UPCGComponent::CreateActorPCGDataCollection(AActor* Actor, const UPCGComponent* Component, EPCGDataType InDataFilter, bool bParseActor)
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
	APCGPartitionActor* PartitionActor = (!!(InDataFilter & EPCGDataType::Spatial) || !!(InDataFilter & EPCGDataType::Volume)) ? Cast<APCGPartitionActor>(Actor) : nullptr;
	ALandscapeProxy* LandscapeActor = !!(InDataFilter & EPCGDataType::Landscape) ? Cast<ALandscapeProxy>(Actor) : nullptr;
	AVolume* VolumeActor = !!(InDataFilter & EPCGDataType::Volume) ? Cast<AVolume>(Actor) : nullptr;
	if (!bParseActor && !!(InDataFilter & EPCGDataType::Point))
	{
		UPCGPointData* Data = NewObject<UPCGPointData>();
		Data->InitializeFromActor(Actor);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}
	else if (PartitionActor)
	{
		check(!Component || Component->GetOwner() == Actor); // Invalid processing otherwise because of the this usage

		UPCGVolumeData* VolumeData = NewObject<UPCGVolumeData>();
		UPCGSpatialData* Result = VolumeData;
		if (InDataFilter == EPCGDataType::Volume)
		{
			VolumeData->Initialize(PCGHelpers::GetGridBounds(Actor, Component));
		}
		else
		{
			VolumeData->Initialize(PartitionActor->GetFixedBounds());

			UPCGComponent* OriginalComponent = Component ? PartitionActor->GetOriginalComponent(Component) : nullptr;
			// Important note: we do NOT call the collection version here, as we want to have a union if that's the case
			const UPCGSpatialData* OriginalComponentSpatialData = OriginalComponent ? Cast<const UPCGSpatialData>(OriginalComponent->GetActorPCGData()) : nullptr;

			if (OriginalComponentSpatialData)
			{
				Result = Result->IntersectWith(OriginalComponentSpatialData);
			}
		}

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Result;
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

		if (!!(InDataFilter & EPCGDataType::Spline))
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

		if (!!(InDataFilter & EPCGDataType::Primitive))
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
	if (Collection.TaggedData.IsEmpty() && !!(InDataFilter & EPCGDataType::Point))
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
bool UPCGComponent::DirtyTrackedActor(AActor* InActor, bool bIntersect, const TSet<FName>& InRemovedTags, const UObject* InOriginatingChangeObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::DirtyTrackedActor);

	if (!InActor)
	{
		return false;
	}

	bool bWasDirtied = false;

	for (const auto& It : CachedTrackedKeysToSettings)
	{
		const FPCGActorSelectionKey& Key = It.Key;

		const bool bRemovedTagIsTracked = (Key.Selection == EPCGActorSelection::ByTag) && InRemovedTags.Contains(Key.Tag);

		if (It.Key.IsMatching(InActor, this) || bRemovedTagIsTracked)
		{
			// Extra care if the change originates from a PCGComponent. Only dirty if we are tracking a PCG component.
			if (InOriginatingChangeObject && InOriginatingChangeObject->IsA<UPCGComponent>() 
				&& (!It.Key.OptionalExtraDependency || !It.Key.OptionalExtraDependency->IsChildOf(UPCGComponent::StaticClass())))
			{
				continue;
			}

			for (const FPCGSettingsAndCulling& SettingsAndCulling : It.Value)
			{
				if (SettingsAndCulling.Value && !bIntersect)
				{
					continue;
				}

				const TWeakObjectPtr<const UPCGSettings>& Settings = SettingsAndCulling.Key;
				if (ensure(Settings.IsValid()))
				{
					GetSubsystem()->CleanFromCache(Settings->GetElement().Get(), Settings.Get());
				}

				bWasDirtied = true;
			}
		}
	}

	// Special case for landscape, we should dirty.
	if (ShouldTrackLandscape() && InActor->IsA<ALandscapeProxy>())
	{
		bWasDirtied = true;
	}

	return bWasDirtied;
}

bool UPCGComponent::ShouldTrackLandscape() const
{
	const UPCGGraph* PCGGraph = GetGraph();
	
	// We should track the landscape if the landscape pins are connected, or if the input type is Landscape and we are using the Input pin.
	const bool bUseLandscapePin = PCGGraph &&
		(PCGGraph->GetInputNode()->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeLabel) ||
		PCGGraph->GetInputNode()->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeHeightLabel));


	const bool bHasLandscapeHasInput = PCGGraph && InputType == EPCGComponentInput::Landscape 
		&& Algo::AnyOf(PCGGraph->GetInputNode()->GetOutputPins(), [](const UPCGPin* InPin) { return InPin && InPin->IsConnected(); });

	return bUseLandscapePin || bHasLandscapeHasInput;
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
		if (PCGComponent->GraphInstance)
		{
			PCGComponent->GraphInstance->SetupCallbacks();
			PCGComponent->GraphInstance->OnGraphChangedDelegate.RemoveAll(PCGComponent);
			PCGComponent->GraphInstance->OnGraphChangedDelegate.AddUObject(PCGComponent, &UPCGComponent::OnGraphChanged);
		}
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
