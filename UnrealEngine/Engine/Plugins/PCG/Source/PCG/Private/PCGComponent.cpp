// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComponent.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
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
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyBase.h"
#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyDistanceAndDirection.h"
#include "Utils/PCGGeneratedResourcesLogging.h"
#include "Utils/PCGGraphExecutionLogging.h"

#include "CoreGlobals.h"
#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Algo/AllOf.h"
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
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComponent)

#if WITH_EDITOR
#include "Editor.h"
#include "EditorActorFolders.h"
#include "ScopedTransaction.h"
#include "Editor/Transactor.h"
#endif

#define LOCTEXT_NAMESPACE "UPCGComponent"

namespace PCGComponent
{
	static TAutoConsoleVariable<bool> CVarGlobalDisableRefresh(
		TEXT("pcg.GlobalDisableRefresh"),
		false,
		TEXT("Disable refresh for all PCG Components."));

	static TAutoConsoleVariable<bool> CVarConstructionScriptFix(
		TEXT("pcg.ConstructionScriptFix"),
		true,
		TEXT("This CVar will be removed in future releases, it allows disabling this fix if regressions are found."));

	template <typename DelegateType>
	static void BroadcastDynamicDelegate(const DelegateType& Delegate, UPCGComponent* PCGComponent)
	{
#if WITH_EDITOR
		const TGuardValue ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
#endif // WITH_EDITOR
		Delegate.Broadcast(PCGComponent);
	}
}

UPCGComponent::UPCGComponent(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	GraphInstance = InObjectInitializer.CreateDefaultSubobject<UPCGGraphInstance>(this, TEXT("PCGGraphInstance"));
	SchedulingPolicyClass = UPCGSchedulingPolicyDistanceAndDirection::StaticClass();

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
	return Cast<APCGPartitionActor>(GetOwner()) == nullptr;
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
		RefreshAfterGraphChanged(GraphInstance, EPCGChangeType::Structural | EPCGChangeType::GenerationGrid);
	}
}

void UPCGComponent::AddToManagedResources(UPCGManagedResource* InResource)
{
	PCGGeneratedResourcesLogging::LogAddToManagedResources(this, InResource);

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
	for (const TObjectPtr<UPCGManagedResource>& ManagedResource : GeneratedResources)
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

	if (IsManagedByRuntimeGenSystem())
	{
		// If we're runtime generated, turn down other requests.
		const bool bShouldGenerate = RequestedGenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime;
		if (!bShouldGenerate)
		{
			UE_LOG(LogPCG, Warning, TEXT("Generation request with trigger %d denied as this component is managed by the runtime generation scheduler."), (int)RequestedGenerationTrigger);
		}

		return bShouldGenerate;
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

	if (!ensure(GraphInstance))
	{
		return;
	}

	const bool bGraphInstanceIsDifferent = !GraphInstance->IsEquivalent(Original->GraphInstance);

#if WITH_EDITOR
	const bool bHasDirtyInput = InputType != NewInputType;
	bool bIsDirty = bHasDirtyInput || bGraphInstanceIsDifferent;
#endif // WITH_EDITOR

	InputType = NewInputType;
	Seed = Original->Seed;
	GenerationTrigger = Original->GenerationTrigger;
	bOverrideGenerationRadii = Original->bOverrideGenerationRadii;
	GenerationRadii = Original->GenerationRadii;

	UPCGGraph* OriginalGraph = Original->GraphInstance ? Original->GraphInstance->GetGraph() : nullptr;
	if (OriginalGraph != GraphInstance->GetGraph())
	{
		GraphInstance->SetGraph(OriginalGraph);
	}

	if (bGraphInstanceIsDifferent && OriginalGraph)
	{
		GraphInstance->CopyParameterOverrides(Original->GraphInstance);
	}

	if (!SchedulingPolicy || SchedulingPolicyClass != Original->SchedulingPolicyClass)
	{
		SchedulingPolicyClass = Original->SchedulingPolicyClass;
		RefreshSchedulingPolicy();
	}

	if (SchedulingPolicy && ensure(Original->SchedulingPolicy) && !SchedulingPolicy->IsEquivalent(Original->SchedulingPolicy))
	{
		UEngine::CopyPropertiesForUnrelatedObjects(Original->SchedulingPolicy, SchedulingPolicy);

#if WITH_EDITOR
		bIsDirty = true;
#endif
	}

#if WITH_EDITOR
	// Note that while we dirty here, we won't trigger a refresh since we don't have the required context
	if (bIsDirty)
	{
		Modify(!IsInPreviewMode());
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

	GenerateLocal(/*bForce=*/false);
}

void UPCGComponent::Generate_Implementation(bool bForce)
{
	GenerateLocal(bForce);
}

void UPCGComponent::GenerateLocal(bool bForce)
{
	GenerateLocalGetTaskId(bForce);
}

void UPCGComponent::GenerateLocal(EPCGComponentGenerationTrigger RequestedGenerationTrigger, bool bForce, EPCGHiGenGrid Grid, const TArray<FPCGTaskId>& Dependencies)
{
	GenerateInternal(bForce, Grid, RequestedGenerationTrigger, Dependencies);
}

FPCGTaskId UPCGComponent::GenerateLocalGetTaskId(bool bForce)
{
	return GenerateInternal(bForce, EPCGHiGenGrid::Uninitialized, EPCGComponentGenerationTrigger::GenerateOnDemand, {});
}

FPCGTaskId UPCGComponent::GenerateLocalGetTaskId(EPCGComponentGenerationTrigger RequestedGenerationTrigger, bool bForce, EPCGHiGenGrid Grid)
{
	return GenerateInternal(bForce, Grid, RequestedGenerationTrigger, {});
}

FPCGTaskId UPCGComponent::GenerateInternal(bool bForce, EPCGHiGenGrid Grid, EPCGComponentGenerationTrigger RequestedGenerationTrigger, const TArray<FPCGTaskId>& Dependencies)
{
	if (IsGenerating() || !GetSubsystem() || !ShouldGenerate(bForce, RequestedGenerationTrigger))
	{
		return InvalidPCGTaskId;
	}

	Modify(!IsInPreviewMode());

	CurrentGenerationTask = GetSubsystem()->ScheduleComponent(this, Grid, bForce, Dependencies);

	if (CurrentGenerationTask != InvalidPCGTaskId)
	{
#if WITH_EDITOR
		OnPCGGraphStartGeneratingDelegate.Broadcast(this);
#endif // WITH_EDITOR

		PCGComponent::BroadcastDynamicDelegate(OnPCGGraphStartGeneratingExternal, this);
	}

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

#if WITH_EDITOR
	// No need for lock since it is not executed in parallel.
	CurrentExecutionDynamicTracking.Empty();
	CurrentExecutionDynamicTrackingSettings.Empty();
#endif // WITH_EDITOR

	return GetSubsystem()->ScheduleGraph(this, *AllDependencies);
}

void UPCGComponent::PostProcessGraph(const FBox& InNewBounds, bool bInGenerated, FPCGContext* Context)
{
	PCGGraphExecutionLogging::LogPostProcessGraph(this);

	LastGeneratedBounds = InNewBounds;

	const bool bHadGeneratedOutputBefore = GeneratedGraphOutput.TaggedData.Num() > 0;

	CleanupUnusedManagedResources();

	GeneratedGraphOutput.Reset();

#if WITH_EDITOR
	ResetIgnoredChangeOrigins(/*bLogIfAnyPresent=*/true);
#endif

	if (bInGenerated)
	{
		bGenerated = true;

		CurrentGenerationTask = InvalidPCGTaskId;

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
				if (ensure(TaggedData.Data))
				{
					// TODO: outering the first layer might not be sufficient here - might need to expose
					// some methods in the data to traverse all the data to outer everything for serialization
					if (UPCGData* DuplicatedData = TaggedData.Data->DuplicateData())
					{
						FPCGTaggedData& DuplicatedTaggedData = GeneratedGraphOutput.TaggedData.Add_GetRef(TaggedData);
						DuplicatedTaggedData.Data = DuplicatedData;

						// NOTE: Flatten before outering to this, as doing the flatten afterwards can dirty this package.
						DuplicatedData->Flatten();

						DuplicatedData->Rename(nullptr, this, IsInPreviewMode() ? REN_DoNotDirty : REN_None);
					}
				}
			}

			// If the original component is partitioned, local components have to forward
			// their inputs, so that they can be gathered by the original component.
			// We don't have the info on the original component here, so forward for all
			// components.
			Context->OutputData = Context->InputData;
		}

#if WITH_EDITOR
		// Reset this flag to avoid re-generating on further refreshes.
		bForceGenerateOnBPAddedToWorld = false;

		bDirtyGenerated = false;
		OnPCGGraphGeneratedDelegate.Broadcast(this);

		UpdateDynamicTracking();
#endif // WITH_EDITOR

		if (IsValid(this) && Context)
		{
			CallPostGenerateFunctions(Context);
		}

		// If Generate function made the component invalid for whatever reason, we re-check if this is valid.
		if (IsValid(this))
		{
			PCGComponent::BroadcastDynamicDelegate(OnPCGGraphGeneratedExternal, this);
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

	UPCGSubsystem* Subsystem = GetSubsystem();
	Subsystem->OnComponentGenerationCompleteOrCancelled.Broadcast(Subsystem);
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

	PCGComponent::BroadcastDynamicDelegate(OnPCGGraphCleanedExternal, this);
}

void UPCGComponent::OnProcessGraphAborted(bool bQuiet)
{
	if (!bQuiet)
	{
		UE_LOG(LogPCG, Warning, TEXT("Process Graph was called but aborted, check for errors in log if you expected a result."));
	}

#if WITH_EDITOR
	// On abort, there may be ignores still registered, silently remove these.
	ResetIgnoredChangeOrigins(/*bLogIfAnyPresent=*/false);
#endif

	CleanupUnusedManagedResources();

	CurrentGenerationTask = InvalidPCGTaskId;
	CurrentCleanupTask = InvalidPCGTaskId; // this is needed to support cancellation

#if WITH_EDITOR
	CurrentRefreshTask = InvalidPCGTaskId;
	// Implementation note: while it may seem logical to clear the bDirtyGenerated flag here, 
	// the component is still considered dirty if we aborted processing, hence it should stay this way.

	StopGenerationInProgress();

	OnPCGGraphCancelledDelegate.Broadcast(this);

	UPCGSubsystem* Subsystem = GetSubsystem();
	Subsystem->OnComponentGenerationCompleteOrCancelled.Broadcast(Subsystem);
#endif

	PCGComponent::BroadcastDynamicDelegate(OnPCGGraphCancelledExternal, this);
}

void UPCGComponent::Cleanup()
{
	if ((!bGenerated && !IsGenerating()) || !GetSubsystem() || IsCleaningUp())
	{
		return;
	}

	if (IsManagedByRuntimeGenSystem())
	{
		UE_LOG(LogPCG, Warning, TEXT("Cleanup request denied as this component is managed by the runtime generation scheduler."));
		return;
	}

#if WITH_EDITOR
	FScopedTransaction Transaction(LOCTEXT("PCGCleanup", "Clean up PCG component"));
#endif

	CleanupLocal(/*bRemoveComponents=*/true);
}

void UPCGComponent::Cleanup_Implementation(bool bRemoveComponents, bool bSave)
{
	CleanupLocal(bRemoveComponents);
}

void UPCGComponent::CleanupLocal(bool bRemoveComponents, bool bSave)
{
	CleanupInternal(bRemoveComponents, {});
}

FPCGTaskId UPCGComponent::CleanupInternal(bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies)
{
	if ((!bGenerated && !IsGenerating()) || !GetSubsystem() || IsCleaningUp())
	{
		return InvalidPCGTaskId;
	}

	PCGGeneratedResourcesLogging::LogCleanupInternal(this, bRemoveComponents);

	Modify(!IsInPreviewMode());

#if WITH_EDITOR
	ExtraCapture.ResetCapturedMessages();
#endif

	CurrentCleanupTask = GetSubsystem()->ScheduleCleanup(this, bRemoveComponents, Dependencies);
	return CurrentCleanupTask;
}

void UPCGComponent::CancelGeneration()
{
	if (CurrentGenerationTask != InvalidPCGTaskId && GetSubsystem())
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
	AActor* NewActor = UPCGActorHelpers::SpawnDefaultActor(World, GetOwner()->GetLevel(), TemplateActor ? TemplateActor : AActor::StaticClass(), TEXT("PCGStamp"), GetOwner()->GetTransform());

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
		else if (NewActor)
		{
			NewActor->Destroy();
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
	FWriteScopeLock ScopedWriteLock(PerPinGeneratedOutputLock);
	PerPinGeneratedOutput.FindOrAdd(InResourceKey) = InData;
}

const FPCGDataCollection* UPCGComponent::RetrieveOutputDataForPin(const FString& InResourceKey)
{
	FReadScopeLock ScopedReadLock(PerPinGeneratedOutputLock);
	return PerPinGeneratedOutput.Find(InResourceKey);
}

void UPCGComponent::ClearPerPinGeneratedOutput()
{
	FWriteScopeLock ScopedWriteLock(PerPinGeneratedOutputLock);
	PerPinGeneratedOutput.Reset();
}

void UPCGComponent::SetSchedulingPolicyClass(TSubclassOf<UPCGSchedulingPolicyBase> InSchedulingPolicyClass) 
{
	if (!SchedulingPolicy || InSchedulingPolicyClass != SchedulingPolicyClass)
	{
		if (InSchedulingPolicyClass != SchedulingPolicyClass)
		{
			SchedulingPolicyClass = InSchedulingPolicyClass;
		}
		
		RefreshSchedulingPolicy();
	}
}

double UPCGComponent::GetGenerationRadiusFromGrid(EPCGHiGenGrid Grid) const
{
	if (bOverrideGenerationRadii)
	{
		return GenerationRadii.GetGenerationRadiusFromGrid(Grid);
	}

	const UPCGGraph* Graph = GetGraph();
	if (ensure(Graph))
	{
		return Graph->GenerationRadii.GetGenerationRadiusFromGrid(Grid);
	}

	return 0;
}

double UPCGComponent::GetCleanupRadiusFromGrid(EPCGHiGenGrid Grid) const
{
	if (bOverrideGenerationRadii)
	{
		return GenerationRadii.GetCleanupRadiusFromGrid(Grid);
	}

	const UPCGGraph* Graph = GetGraph();
	if (ensure(Graph))
	{
		return Graph->GenerationRadii.GetCleanupRadiusFromGrid(Grid);
	}

	return 0;
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

	Modify(!IsInPreviewMode());

	if (bCreateChild)
	{
		NewActor = UPCGActorHelpers::SpawnDefaultActor(GetWorld(), GetOwner()->GetLevel(), NewActor->GetClass(), TEXT("PCGStampChild"), Owner->GetTransform());
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

void UPCGComponent::CleanupLocalImmediate(bool bRemoveComponents, bool bCleanupLocalComponents)
{
	PCGGeneratedResourcesLogging::LogCleanupLocalImmediate(this, bRemoveComponents, GeneratedResources);

	UPCGSubsystem* Subsystem = GetSubsystem();

	// Cleanup Local should work even if we don't have any subsytem. In cook (or in other places), if Cleanup is necessary, we need to make sure to 
	// cleanup the managed resources on the component even if we don't have a subsystem. If we don't have a subsystem, assume we are unbounded.
	bool bHasUnbounded = true;

	if (Subsystem)
	{
		PCGHiGenGrid::FSizeArray GridSizes;
		ensure(PCGHelpers::GetGenerationGridSizes(GetGraph(), Subsystem->GetPCGWorldActor(), GridSizes, bHasUnbounded));
	}

	// Cancels generation of this component if there is an ongoing generation in progress.
	CancelGeneration();

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

			PCGGeneratedResourcesLogging::LogCleanupLocalImmediateResource(this, Resource);

			if (!Resource || Resource->Release(bRemoveComponents, ActorsToDelete))
			{
				if (Resource)
				{
#if WITH_EDITOR
					if (Resource->IsMarkedTransientOnLoad())
					{
						LoadedPreviewResources.Add(Resource);
					}
					else
					{
						Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					}
#endif
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

	// If the component is partitioned, we will forward the calls to its local components.
	if (Subsystem && bCleanupLocalComponents && IsPartitioned())
	{
		Subsystem->CleanupLocalComponentsImmediate(this, bRemoveComponents);
	}

	PCGGeneratedResourcesLogging::LogCleanupLocalImmediateFinished(this, GeneratedResources);
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

	PCGGeneratedResourcesLogging::LogCreateCleanupTask(this, bRemoveComponents);

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

				PCGGeneratedResourcesLogging::LogCreateCleanupTaskResource(ThisComponentWeakPtr.Get(), Resource);

				if (!Resource || Resource->Release(bRemoveComponents, Context->ActorsToDelete))
				{
					if (Resource)
					{
#if WITH_EDITOR
						if (Resource->IsMarkedTransientOnLoad())
						{
							ThisComponent->LoadedPreviewResources.Add(Resource);
						}
						else
						{
							Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
						}
#endif
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
			if (UWorld* ThisWorld = ThisComponent->GetWorld(); ThisWorld && GEditor) // FActorFolders require the editor
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

						for (const FFolder& FolderToDelete : SubfoldersToDelete) //-V1078
						{
							FActorFolders::Get().DeleteFolder(*ThisWorld, FolderToDelete);
						}

						// Finally, delete folder
						FActorFolders::Get().DeleteFolder(*ThisWorld, GeneratedFolder);
					}
				}
			}
#endif

			PCGGeneratedResourcesLogging::LogCreateCleanupTaskFinished(ThisComponentWeakPtr.Get(), ThisComponentWeakPtr.IsValid() ? &ThisComponentWeakPtr->GeneratedResources : nullptr);
		}

		return true;
	};

	return GetSubsystem()->ScheduleGeneric(CleanupTask, this, *AllDependencies);
}

void UPCGComponent::CleanupUnusedManagedResources()
{
	PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResources(this, GeneratedResources);

	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;

	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);
		for (int32 ResourceIndex = GeneratedResources.Num() - 1; ResourceIndex >= 0; --ResourceIndex)
		{
			UPCGManagedResource* Resource = GetValid(GeneratedResources[ResourceIndex]);

			PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResourcesResource(this, Resource);

			if (!Resource && GetOwner())
			{
				UE_LOG(LogPCG, Error, TEXT("[UPCGComponent::CleanupUnusedManagedResources] Null generated resource encountered on actor \"%s\"."), *GetOwner()->GetFName().ToString());
			}

			if (!Resource || Resource->ReleaseIfUnused(ActorsToDelete))
			{
				if (Resource)
				{
#if WITH_EDITOR
					if (Resource->IsMarkedTransientOnLoad())
					{
						LoadedPreviewResources.Add(Resource);
					}
					else
					{
						Resource->Rename(nullptr, nullptr, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					}
#endif
				}

				GeneratedResources.RemoveAtSwap(ResourceIndex);
			}
		}
	}

	UPCGActorHelpers::DeleteActors(GetWorld(), ActorsToDelete.Array());

	PCGGeneratedResourcesLogging::LogCleanupUnusedManagedResourcesFinished(this, GeneratedResources);
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
			GenerateInternal(/*bForce=*/false, EPCGHiGenGrid::Uninitialized, EPCGComponentGenerationTrigger::GenerateOnLoad, {});
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

#if WITH_EDITOR
	// Don't do the unregister in OnUnregister, because we have flows where the component gets Unregistered/Registered without getting destroyed.
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (!PCGHelpers::IsRuntimeOrPIE())
		{
			Subsystem->UnregisterPCGComponent(this);
		}

		if (IsCreatedByConstructionScript() && PCGComponent::CVarConstructionScriptFix.GetValueOnAnyThread())
		{
			Subsystem->SetConstructionScriptSourceComponent(this);
		}
	}
#endif // WITH_EDITOR

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPCGComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	TArray<TObjectPtr<UPCGManagedResource>> GeneratedResourcesCopy;

	if (Ar.IsSaving() && CurrentEditingMode == EPCGEditorDirtyMode::Preview)
	{
		GeneratedResourcesCopy = GeneratedResources;
		GeneratedResources = LoadedPreviewResources;
	}
#endif // WITH_EDITOR

	Ar.UsingCustomVersion(FPCGCustomVersion::GUID);

	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsCooking())
	{
		int32 DataVersion = FPCGCustomVersion::LatestVersion;
		if (Ar.IsLoading())
		{
			DataVersion = Ar.CustomVer(FPCGCustomVersion::GUID);

			if (DataVersion < FPCGCustomVersion::SupportPartitionedComponentsInNonPartitionedLevels)
			{
				bDisableIsComponentPartitionedOnLoad = true;
			}
		}

		if (DataVersion >= FPCGCustomVersion::DynamicTrackingKeysSerializedInComponent)
		{
			Ar << DynamicallyTrackedKeysToSettings;
		}
	}
#endif // WITH_EDITORONLY_DATA


#if WITH_EDITOR
	if (Ar.IsSaving() && CurrentEditingMode == EPCGEditorDirtyMode::Preview)
	{
		GeneratedResources = GeneratedResourcesCopy;
	}
#endif // WITH_EDITOR
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

	// Components marked as partitioned in non-WP worlds from BEFORE partitioning was supported in non-WP worlds can leak resources. To fix this, we can just unset 'bIsComponentPartitioned'.
	if (bDisableIsComponentPartitionedOnLoad)
	{
		const UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;

		if (IsPartitioned() && !IsManagedByRuntimeGenSystem() && World && World->GetWorldPartition() == nullptr)
		{
			bIsComponentPartitioned = false;
		}

		bDisableIsComponentPartitionedOnLoad = false;
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

	// Always set the editing mode to Preview when we're in GenerateAtRuntime mode
	CurrentEditingMode = IsManagedByRuntimeGenSystem() ? EPCGEditorDirtyMode::Preview : SerializedEditingMode;

	if (CurrentEditingMode == EPCGEditorDirtyMode::Preview)
	{
		// Make sure we update the transient state if we have been forced into Preview mode by runtime generation.
		if (CurrentEditingMode != SerializedEditingMode)
		{
			PreviousEditingMode = SerializedEditingMode;
			ChangeTransientState(CurrentEditingMode);
		}

		bGenerated = false;
	}
	else if (CurrentEditingMode == EPCGEditorDirtyMode::LoadAsPreview && !PCGHelpers::IsRuntimeOrPIE())
	{
		CurrentEditingMode = EPCGEditorDirtyMode::Preview;
		MarkResourcesAsTransientOnLoad();
		bDirtyGenerated = true;
	}
#endif

	if (!SchedulingPolicy || IsManagedByRuntimeGenSystem())
	{
		RefreshSchedulingPolicy();
	}
	else
	{
		const EObjectFlags Flags = GetMaskedFlags(RF_PropagateToSubObjects) | RF_Transactional;
		SchedulingPolicy->SetFlags(Flags);
#if WITH_EDITOR
		SchedulingPolicy->SetShouldDisplayProperties(IsManagedByRuntimeGenSystem());
#endif
	}
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

	// For the special case where a component is part of a reconstruction script (from a BP),
	// but gets destroyed immediately, we need to force the unregistering. 
	if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
	{
		PCGSubsystem->UnregisterPCGComponent(this, /*bForce=*/true);
	}
#endif // WITH_EDITOR

	Super::BeginDestroy();
}

bool UPCGComponent::IsEditorOnly() const
{
	return Super::IsEditorOnly() || (GraphInstance && GraphInstance->IsEditorOnly());
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
	RefreshAfterGraphChanged(InGraph, ChangeType);
}

void UPCGComponent::RefreshAfterGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph != GraphInstance)
	{
		return;
	}

	if (ChangeType == EPCGChangeType::Cosmetic || ChangeType == EPCGChangeType::None)
	{
		// If it is a cosmetic change (or no change), nothing to do
		return;
	}

	const bool bHasGraph = (InGraph && InGraph->GetGraph());

	const bool bIsStructural = ((ChangeType & (EPCGChangeType::Edge | EPCGChangeType::Structural)) != EPCGChangeType::None);
	const bool bDirtyInputs = bIsStructural || ((ChangeType & EPCGChangeType::Input) != EPCGChangeType::None);

#if WITH_EDITOR
	// In editor, since we've changed the graph, we might have changed the tracked actor tags as well
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			// Don't update the tracking if nothing changed for the tracking.
			TArray<FPCGSelectionKey> ChangedKeys;
			if (UpdateTrackingCache(&ChangedKeys))
			{
				Subsystem->UpdateComponentTracking(this, /*bInShouldDirtyActors=*/ true, &ChangedKeys);
			}
		}

		DirtyGenerated(bDirtyInputs ? (EPCGComponentDirtyFlag::Actor | EPCGComponentDirtyFlag::Landscape) : EPCGComponentDirtyFlag::None);

		// If there is no graph, we should still refresh if we are runtime-managed, since the RuntimeGenScheduler will need to flush its resources.
		if (bHasGraph || IsManagedByRuntimeGenSystem())
		{
			Refresh(ChangeType);
		}
		else
		{
			// With no graph, we clean up
			CleanupLocal(/*bRemoveComponents=*/true);
		}

		ClearInspectionData();

		return;
	}
#endif

	if (IsManagedByRuntimeGenSystem())
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RefreshRuntimeGenComponent(this, ChangeType);
		}
	}
	else
	{
		// Otherwise, if we are in PIE or runtime, force generate if we have a graph (and were generated). Or cleanup if we have no graph
		if (bHasGraph && bGenerated)
		{
			GenerateLocal(/*bForce=*/true);
		}
		else if (!bHasGraph)
		{
			CleanupLocal(/*bRemoveComponents=*/true);
		}
	}
}

#if WITH_EDITOR
void UPCGComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (!PropertyAboutToChange)
	{
		return;
	}

	const FName PropName = PropertyAboutToChange->GetFName();

	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, GenerationTrigger))
	{
		if (IsManagedByRuntimeGenSystem())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				// When toggling off of GenerateAtRuntime, we should flush the RuntimeGenScheduler state for this component.
				Subsystem->RefreshRuntimeGenComponent(this, EPCGChangeType::GenerationGrid);
			}

			// Reset to the the editing mode we were in before entering GenerateAtRuntime mode.
			SetEditingMode(PreviousEditingMode, SerializedEditingMode);
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, bIsComponentPartitioned))
	{
		if (IsManagedByRuntimeGenSystem())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				// When toggling IsPartitioned, we should proactively flush the RuntimeGenScheduler.
				Subsystem->RefreshRuntimeGenComponent(this, EPCGChangeType::GenerationGrid);
			}
		}
	}
}

void UPCGComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!PropertyChangedEvent.Property || !IsValid(this))
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	const FName PropName = PropertyChangedEvent.Property->GetFName();

	bool bTransientPropertyChangedThatDoesNotRequireARefresh = false;

	// Implementation note:
	// Since the current editing mode is a transient variable, if we do not do this transition here before going in the Super call,
	//  we can end up in a situation where BP actors are reconstructed (... this component included ...) which makes this fall into the !IsValid case just after
	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, CurrentEditingMode))
	{
		// When affecting the editing mode from the user's point of view, we need to change both the current & serialized values
		SetEditingMode(CurrentEditingMode, CurrentEditingMode);
		ChangeTransientState(CurrentEditingMode);
		bTransientPropertyChangedThatDoesNotRequireARefresh = true;
	}

	bool bWasDirtyGenerated = bDirtyGenerated;
	bDirtyGenerated = true;

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// BP actors will early out here as construction script will have created a new component.
	if (!IsValid(this))
	{
		return;
	}

	// Restore dirty flag for non BP cases. BP components will always regenerate for now.
	bDirtyGenerated = bWasDirtyGenerated;

	const FName MemberName = PropertyChangedEvent.MemberProperty->GetFName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPCGComponent, GenerationRadii))
	{
		// RuntimeGen will automatically pick up any changes to generation radii, we don't need to do any work here.
		return;
	}

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

			// SetIsPartitioned cleans up before, so keep track if we were generated or not.
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
		// If anything happens on the graph instance, it will be handled there.
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, InputType))
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			TArray<FPCGSelectionKey> ChangedKeys;
			if (UpdateTrackingCache(&ChangedKeys))
			{
				Subsystem->UpdateComponentTracking(this, /*bInShouldDirtyActors=*/true, &ChangedKeys);
			}
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
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, SchedulingPolicyClass))
	{
		// We don't need to refresh the component here because this does not effect generation behavior, only scheduling behavior.
		RefreshSchedulingPolicy();
	} 
	else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGComponent, GenerationTrigger))
	{
		if (!SchedulingPolicy)
		{
			RefreshSchedulingPolicy();
		}

		if (SchedulingPolicy)
		{
			// We should only display scheduling policy parameters when in runtime generation mode.
			SchedulingPolicy->SetShouldDisplayProperties(IsManagedByRuntimeGenSystem());
		}

		if (IsManagedByRuntimeGenSystem())
		{
			// If we have been set to GenerateAtRuntime, we should cleanup any artifacts.
			CleanupLocalImmediate(/*bRemoveComponents=*/true, /*bCleanupLocalComponents=*/true);

			PreviousEditingMode = CurrentEditingMode;

			SetEditingMode(/*InEditingMode=*/EPCGEditorDirtyMode::Preview, SerializedEditingMode);
			ChangeTransientState(CurrentEditingMode);
		}
		else
		{
			Refresh();
		}
	}
	// General properties that don't affect behavior
	else if(!bTransientPropertyChangedThatDoesNotRequireARefresh)
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
	Super::PreEditUndo();

	// Here we will keep a copy of flags that we require to keep through the undo
	// so we can have a consistent state
	LastGeneratedBoundsPriorToUndo = LastGeneratedBounds;

	if (IsManagedByRuntimeGenSystem())
	{
		// Always flush the runtime gen state before undo/redo to avoid leaking resources or locking grid cells from future generation.
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RefreshRuntimeGenComponent(this, EPCGChangeType::GenerationGrid);
		}
	}

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
		// Cancel existing means a refresh will always be scheduled even if another refresh was pending. If an undo
		// operation removes the component, a valid refresh task ID is set but the refresh task itself will fail
		// and leave the valid task ID hanging on the component. Forcing here means if we later retrieve this state
		// from the undo/redo buffer, the refresh will be forced which will reset the state.
		Refresh(EPCGChangeType::Structural | EPCGChangeType::GenerationGrid, /*bCancelExistingRefresh=*/true);
	}

	Super::PostEditUndo();
}

bool UPCGComponent::UpdateTrackingCache(TArray<FPCGSelectionKey>* OptionalChangedKeys)
{
	// Without an owner, it probably means we are in a BP template, so no need to update the tracking cache.
	// Same for local components, as we will use the cache of the original component.
	if (!GetOwner() || IsLocalComponent())
	{
		return false;
	}

	int32 FoundKeys = 0;

	// Store in a temporary map to detect key changes.
	FPCGSelectionKeyToSettingsMap NewTrackedKeysToSettings;

	if (UPCGGraph* PCGGraph = GetGraph())
	{
		NewTrackedKeysToSettings = PCGGraph->GetTrackedActorKeysToSettings();

		// Also add a key for the landscape, with settings null and always culled, if we should track the landscape
		if (ShouldTrackLandscape())
		{
			FPCGSelectionKey LandscapeKey = FPCGSelectionKey(ALandscapeProxy::StaticClass());
			NewTrackedKeysToSettings.FindOrAdd(LandscapeKey).Emplace(/*Settings*/nullptr, /*bIsCulled*/true);
		}

		// A tag should be culled, if only all the settings that track this tag should cull.
		// Note that is only impact the fact that we track (or not) this tag.
		// If a setting is marked as "should cull", it will only be dirtied (at least by default), if the actor with the
		// given tag intersect with the component.
		for (const TPair<FPCGSelectionKey, TArray<FPCGSettingsAndCulling>>& It : NewTrackedKeysToSettings)
		{
			const FPCGSelectionKey& Key = It.Key;

			// Should cull only if all the settings requires a cull.
			const bool bShouldCull = PCGSettings::IsKeyCulled(It.Value);
			const TArray<FPCGSettingsAndCulling>* OldSettingsAndCulling = StaticallyTrackedKeysToSettings.Find(It.Key);
			const bool OldCulling = OldSettingsAndCulling && PCGSettings::IsKeyCulled(*OldSettingsAndCulling);
			const bool bNewKeyOrCullChanged = !OldSettingsAndCulling || (OldCulling != bShouldCull);

			StaticallyTrackedKeysToSettings.Remove(Key);

			if (!bNewKeyOrCullChanged)
			{
				++FoundKeys;
			}
			else if (OptionalChangedKeys)
			{
				OptionalChangedKeys->Add(Key);
			}
		}

		// At the end, we also have keys that were tracked but no more, so add them at the list of tracked keys
		if (OptionalChangedKeys)
		{
			OptionalChangedKeys->Reserve(OptionalChangedKeys->Num() + StaticallyTrackedKeysToSettings.Num());

			for (const TPair<FPCGSelectionKey, TArray<FPCGSettingsAndCulling>>& It : StaticallyTrackedKeysToSettings)
			{
				OptionalChangedKeys->Add(It.Key);
			}
		}
	}

	bool bHasChanged = NewTrackedKeysToSettings.Num() != FoundKeys;

	StaticallyTrackedKeysToSettings = MoveTemp(NewTrackedKeysToSettings);

	return bHasChanged;
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
		ClearInspectionData(/*bClearPerNodeExecutionData=*/false);
	}
};

void UPCGComponent::NotifyNodeExecuted(const UPCGNode* InNode, const FPCGStack* InStack, bool bNodeUsedCache)
{
	if (!ensure(InStack && InNode))
	{
		return;
	}

	FPCGStack Stack = *InStack;

	// Reset timer information if taken from cache to provide good info in the profiling window
	if (bNodeUsedCache)
	{
		Stack.Timer = PCGUtils::FCallTime();
	}

	FWriteScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	NodeToStacksInWhichNodeExecuted.FindOrAdd(InNode).Add(MoveTemp(Stack));
}

TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> UPCGComponent::GetExecutedNodeStacks() const
{
	FReadScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	return NodeToStacksInWhichNodeExecuted;
}

uint64 UPCGComponent::GetNodeInactivePinMask(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStackToInactivePinMaskLock);

	if (const TMap<const FPCGStack, uint64>* StackToMask = NodeToStackToInactivePinMask.Find(InNode))
	{
		if (const uint64* Mask = StackToMask->Find(Stack))
		{
			return *Mask;
		}
	}

	return 0;
}

void UPCGComponent::NotifyNodeDynamicInactivePins(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactivePinBitmask) const
{
	if (!ensure(InStack && InNode))
	{
		return;
	}

	FWriteScopeLock Lock(NodeToStackToInactivePinMaskLock);
	TMap<const FPCGStack, uint64>& StackToInactivePinMask = NodeToStackToInactivePinMask.FindOrAdd(InNode);
	StackToInactivePinMask.FindOrAdd(*InStack) = InactivePinBitmask;
}

bool UPCGComponent::WasNodeExecuted(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	const TSet<FPCGStack>* FoundStacks = NodeToStacksInWhichNodeExecuted.Find(InNode);

	return FoundStacks && FoundStacks->Contains(Stack);
}

void UPCGComponent::StoreInspectionData(const FPCGStack* InStack, const UPCGNode* InNode, const FPCGDataCollection& InInputData, const FPCGDataCollection& InOutputData, bool bUsedCache)
{
	if (!InNode || !ensure(InStack))
	{
		return;
	}

	// Notify component that this task executed. Useful for editor visualization.
	NotifyNodeExecuted(InNode, InStack, bUsedCache);

	if (!InOutputData.TaggedData.IsEmpty())
	{
		FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);

		NodeToStacksThatProducedData.FindOrAdd(InNode).Add(*InStack);
	}
	else
	{
		FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);

		if (TSet<FPCGStack>* Stacks = NodeToStacksThatProducedData.Find(InNode))
		{
			Stacks->Remove(*InStack);
		}
	}

	if (IsInspecting())
	{
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
				InData.GetInputsAndCrcsByPin(Pin->Properties.Label, PinDataCollection.TaggedData, PinDataCollection.DataCrcs);

				// Implementation note: since static subgraphs actually are visited twice and the second time the input doesn't match the input pins, we don't clear the data.
				if (!PinDataCollection.TaggedData.IsEmpty())
				{
					InOutInspectionCache.Add(Stack, PinDataCollection);
				}
			}
		};

		FWriteScopeLock Lock(InspectionCacheLock);

		StorePinInspectionData(InNode->GetInputPins(), InInputData, InspectionCache);
		StorePinInspectionData(InNode->GetOutputPins(), InOutputData, InspectionCache);
	}
}

const FPCGDataCollection* UPCGComponent::GetInspectionData(const FPCGStack& InStack) const
{
	FReadScopeLock Lock(InspectionCacheLock);
	return InspectionCache.Find(InStack);
}

void UPCGComponent::ClearInspectionData(bool bClearPerNodeExecutionData)
{
	{
		FWriteScopeLock Lock(InspectionCacheLock);
		InspectionCache.Reset();
	}

	if (bClearPerNodeExecutionData)
	{
		{
			FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);
			NodeToStacksThatProducedData.Reset();
		}

		{
			FWriteScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
			NodeToStacksInWhichNodeExecuted.Reset();
		}

		{
			FWriteScopeLock Lock(NodeToStackToInactivePinMaskLock);
			NodeToStackToInactivePinMask.Reset();
		}
	}
}

bool UPCGComponent::HasNodeProducedData(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksThatProducedDataLock);

	const TSet<FPCGStack>* StacksThatProducedData = NodeToStacksThatProducedData.Find(InNode);

	return StacksThatProducedData && StacksThatProducedData->Contains(Stack);
}

void UPCGComponent::Refresh(EPCGChangeType ChangeType, bool bCancelExistingRefresh)
{
	// Disable auto-refreshing on preview actors until we have something more robust on the execution side.
	if (GetOwner() && GetOwner()->bIsEditorPreviewActor)
	{
		return;
	}

	// Runtime component refreshes should go through the runtime scheduler.
	if (IsManagedByRuntimeGenSystem())
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RefreshRuntimeGenComponent(this, ChangeType);
		}

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

	// If refresh is disabled, just exit
	if (PCGComponent::CVarGlobalDisableRefresh.GetValueOnAnyThread() || IsRunningCommandlet())
	{
		return;
	}

	// Discard any refresh if have already one scheduled.
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		// Cancel an already existing generation if either the change is structural in nature (which requires a recompilation, so a full-rescheduling)
		// or if the generation is already started
		const bool bGenerationWasInProgress = IsGenerationInProgress();
		const bool bStructural = !!(ChangeType & EPCGChangeType::Structural);
		bool bNeedToCancelCurrentTasks = (CurrentGenerationTask != InvalidPCGTaskId && (bStructural || bGenerationWasInProgress));

		// Cancel an already existing refresh if caller allows this
		if (bCancelExistingRefresh && CurrentRefreshTask != InvalidPCGTaskId)
		{
			bNeedToCancelCurrentTasks = true;

			CurrentRefreshTask = InvalidPCGTaskId;
		}

		if (bNeedToCancelCurrentTasks)
		{
			Subsystem->CancelGeneration(this);
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
		// Generate on drop can be disabled by global settings or locally by not having "GenerateOnLoad" as a generation trigger (and Generate on Drop option to false).
		if (const UPCGEngineSettings* Settings = GetDefault<UPCGEngineSettings>())
		{
			return Settings->bGenerateOnDrop && bForceGenerateOnBPAddedToWorld &&
				(GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad || (GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand && bGenerateOnDropWhenTriggerOnDemand));
		}
		else
		{
			return false;
		}
	}
}

bool UPCGComponent::IsObjectTracked(const UObject* InObject, bool& bOutIsCulled) const
{
	check(InObject);

	if (!GetOwner())
	{
		return false;
	}


	// We should always track the owner of the component, without culling
	if (GetOwner() == InObject)
	{
		bOutIsCulled = false;
		return true;
	}

	// If we track the landscape using legacy methods and it is a landscape, it should be tracked as culled
	if (InObject && InObject->IsA<ALandscapeProxy>() && ShouldTrackLandscape())
	{
		bOutIsCulled = true;
		return true;
	}

	auto CheckMap = [this, &InObject, &bOutIsCulled](const FPCGSelectionKeyToSettingsMap& InMap) -> bool
	{
		for (const auto& It : InMap)
		{
			bool bFound = It.Key.IsMatching(InObject, this);

			if (bFound)
			{
				bOutIsCulled = PCGSettings::IsKeyCulled(It.Value);
				return true;
			}
		}

		return false;
	};

	return CheckMap(StaticallyTrackedKeysToSettings) || CheckMap(DynamicallyTrackedKeysToSettings);
}

void UPCGComponent::OnRefresh(bool bForceRefresh)
{
	check(!IsManagedByRuntimeGenSystem());

	// Mark the refresh task invalid to allow re-triggering refreshes
	CurrentRefreshTask = InvalidPCGTaskId;

	// Before doing a refresh, update the component to the subsystem if we are partitioned
	// Only redo the mapping if we are generated
	UPCGSubsystem* Subsystem = GetSubsystem();
	const bool bWasGenerated = bGenerated;
	const bool bWasGeneratedOrGenerating = bWasGenerated || bForceRefresh;

	// If we are partitioned but we have resources, we need to force a cleanup
	if (IsPartitioned() && !GeneratedResources.IsEmpty())
	{
		CleanupLocalImmediate(/*bRemoveComponents=*/true);
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

		// Retain our generated state when going inactive, and mark bDirtyGenerated so that the component will re-generate upon re-activation (if necessary).
		bGenerated = bWasGenerated;
		bDirtyGenerated = bWasGenerated;
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
		return const_cast<UPCGData*>(Collection.TaggedData[0].Data.Get());
	}
	else
	{
		return nullptr;
	}
}

FPCGDataCollection UPCGComponent::CreateActorPCGDataCollection(AActor* Actor, const UPCGComponent* Component, EPCGDataType InDataFilter, bool bParseActor, bool* bOutOptionalSanitizedTagAttributeName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::CreateActorPCGData);

	if (bOutOptionalSanitizedTagAttributeName)
	{
		*bOutOptionalSanitizedTagAttributeName = false;
	}

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
		Data->InitializeFromActor(Actor, bOutOptionalSanitizedTagAttributeName);

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

		FPCGLandscapeDataProps LandscapeDataProps;
		LandscapeDataProps.bGetHeightOnly = false;
		LandscapeDataProps.bGetLayerWeights = (!PCGGraph || PCGGraph->bLandscapeUsesMetadata);

		Data->Initialize({ LandscapeActor }, PCGHelpers::GetGridBounds(Actor, Component), LandscapeDataProps);

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
		Data->InitializeFromActor(Actor, bOutOptionalSanitizedTagAttributeName);

		FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
		TaggedData.Data = Data;
		TaggedData.Tags = ActorTags;
	}

	return Collection;
}

void UPCGComponent::RefreshSchedulingPolicy()
{
	// Only delete it if we are the owner, it's for deprecation where local components had hard ref on original policy.
	if (IsValid(SchedulingPolicy) && SchedulingPolicy->GetOuter() == this)
	{
#if WITH_EDITOR
		SchedulingPolicy->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
#endif
		SchedulingPolicy->MarkAsGarbage();
	}

	SchedulingPolicy = nullptr;

	// We should never create the scheduling policy when not in runtime generation mode.
	if (SchedulingPolicyClass && IsManagedByRuntimeGenSystem())
	{
		const EObjectFlags Flags = GetMaskedFlags(RF_PropagateToSubObjects);
		SchedulingPolicy = NewObject<UPCGSchedulingPolicyBase>(this, SchedulingPolicyClass, NAME_None, Flags);

#if WITH_EDITOR
		SchedulingPolicy->SetShouldDisplayProperties(IsManagedByRuntimeGenSystem());
#endif
	}
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

	FPCGLandscapeDataProps LandscapeDataProps;
	LandscapeDataProps.bGetHeightOnly = bHeightOnly;
	LandscapeDataProps.bGetLayerWeights = (PCGGraph && PCGGraph->bLandscapeUsesMetadata);

	LandscapeData->Initialize(Landscapes, LandscapeBounds, LandscapeDataProps);

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
void UPCGComponent::ApplyToEachSettings(const FPCGSelectionKey& InKey, const TFunctionRef<void(const FPCGSelectionKey&, const FPCGSettingsAndCulling&)> InCallback) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::ApplyToEachSettings);

	auto FindAndApplyInMap = [&InKey, &InCallback](const FPCGSelectionKeyToSettingsMap& InMap)
	{
		if (const TArray<FPCGSettingsAndCulling>* StaticallyTrackedSettings = InMap.Find(InKey))
		{
			for (const FPCGSettingsAndCulling& SettingsAndCulling : *StaticallyTrackedSettings)
			{
				InCallback(InKey, SettingsAndCulling);
			}
		}
	};

	FindAndApplyInMap(StaticallyTrackedKeysToSettings);
	FindAndApplyInMap(DynamicallyTrackedKeysToSettings);
}

TArray<FPCGSelectionKey> UPCGComponent::GatherTrackingKeys() const
{
	TArray<FPCGSelectionKey> Keys;
	Keys.Reserve(StaticallyTrackedKeysToSettings.Num() + DynamicallyTrackedKeysToSettings.Num());
	for (const auto& It : StaticallyTrackedKeysToSettings)
	{
		Keys.Add(It.Key);
	}

	for (const auto& It : DynamicallyTrackedKeysToSettings)
	{
		Keys.Add(It.Key);
	}

	return Keys;
}

bool UPCGComponent::IsKeyTrackedAndCulled(const FPCGSelectionKey& Key, bool& bOutIsCulled) const
{
	bool bIsTracked = false;

	bool bStaticallyCulled = true;
	bool bDynamicallyCulled = true;

	if (auto* It = StaticallyTrackedKeysToSettings.Find(Key))
	{
		bIsTracked = true;
		bStaticallyCulled = PCGSettings::IsKeyCulled(*It);
	}

	if (auto* It = DynamicallyTrackedKeysToSettings.Find(Key))
	{
		bIsTracked = true;
		bDynamicallyCulled = PCGSettings::IsKeyCulled(*It);
	}

	// If it is tracked statically and dynamically, we will cull only and only if both are culling.
	// Otherwise, it means that at least one key requires to always track, so bOutIsCulled needs to be False.
	bOutIsCulled = bIsTracked && bStaticallyCulled && bDynamicallyCulled;

	return bIsTracked;
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

void UPCGComponent::RegisterDynamicTracking(const UPCGSettings* InSettings, const TArrayView<TPair<FPCGSelectionKey, bool>>& InDynamicKeysAndCulling)
{
	if (!InSettings)
	{
		return;
	}

	FScopeLock Lock(&CurrentExecutionDynamicTrackingLock);
	CurrentExecutionDynamicTrackingSettings.Add(InSettings);

	for (const TPair<FPCGSelectionKey, bool>& It : InDynamicKeysAndCulling)
	{
		// Make sure to not register null assets
		if (It.Key.Selection == EPCGActorSelection::ByPath && It.Key.ObjectPath.IsNull())
		{
			continue;
		}

		CurrentExecutionDynamicTracking.FindOrAdd(It.Key).Emplace(InSettings, It.Value);
	}
}

void UPCGComponent::UpdateDynamicTracking()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::UpdateDynamicTracking);

	UPCGSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	// If the component is local, we defer the tracking to the original component.
	// So move everything to the original (while making sure we are not duplicating keys/settings).
	// Since it can happen in parallel, we need to lock.
	if (IsLocalComponent())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComponent::UpdateDynamicTracking::LocalComponent);

		UPCGComponent* OriginalComponent = GetOriginalComponent();
		if (ensure(OriginalComponent))
		{
			FScopeLock Lock(&OriginalComponent->CurrentExecutionDynamicTrackingLock);
			for (auto& It : CurrentExecutionDynamicTracking)
			{
				TArray<FPCGSettingsAndCulling>& OriginalSettingsAndCulling = OriginalComponent->CurrentExecutionDynamicTracking.FindOrAdd(It.Key);
				for (FPCGSettingsAndCulling& SettingsAndCulling : It.Value)
				{
					OriginalSettingsAndCulling.AddUnique(std::move(SettingsAndCulling));
				}
			}

			OriginalComponent->CurrentExecutionDynamicTrackingSettings.Append(CurrentExecutionDynamicTrackingSettings);
		}

		CurrentExecutionDynamicTracking.Empty();
		CurrentExecutionDynamicTrackingSettings.Empty();
		return;
	}

	TArray<FPCGSelectionKey> ChangedKeys;

	// Locking to make sure we never hit this multiple times.
	{
		FScopeLock Lock(&CurrentExecutionDynamicTrackingLock);

		// Go over all dynamic keys gathered during this execution.
		// If they are not already tracked, we need to register this key.
		// Otherwise, we need to gather all the settings that tracked this key that were not executed (because of caching).
		for (auto& It : CurrentExecutionDynamicTracking)
		{
			if (TArray<FPCGSettingsAndCulling>* AllSettingsAndCulling = DynamicallyTrackedKeysToSettings.Find(It.Key))
			{
				for (FPCGSettingsAndCulling& SettingsAndCulling : *AllSettingsAndCulling)
				{
					if (SettingsAndCulling.Key.Get() && !CurrentExecutionDynamicTrackingSettings.Contains(SettingsAndCulling.Key.Get()))
					{
						It.Value.AddUnique(std::move(SettingsAndCulling));
					}
				}
			}
			else
			{
				ChangedKeys.Add(It.Key);
			}
		}

		// Go over all already registered dynamic keys
		// If they are not in the current execution gathered keys, we check if they are associated with settings that 
		// were executed. If so, we re-add them to the current execution gathered keys.
		// If not, it means that the key is no longer tracked and should be unregistered.
		for (auto& It : DynamicallyTrackedKeysToSettings)
		{
			if (!CurrentExecutionDynamicTracking.Contains(It.Key))
			{
				TArray<FPCGSettingsAndCulling>* AllSettingsAndCulling = nullptr;

				for (FPCGSettingsAndCulling& SettingsAndCulling : It.Value)
				{
					if (SettingsAndCulling.Key.Get() && !CurrentExecutionDynamicTrackingSettings.Contains(SettingsAndCulling.Key.Get()))
					{
						if (!AllSettingsAndCulling)
						{
							AllSettingsAndCulling = &CurrentExecutionDynamicTracking.Add(It.Key);
						}

						check(AllSettingsAndCulling);
						// No need for Add Unique since they are already unique in the original map.
						AllSettingsAndCulling->Add(std::move(SettingsAndCulling));
					}
				}

				if (!AllSettingsAndCulling)
				{
					ChangedKeys.Add(It.Key);
				}
			}
		}

		DynamicallyTrackedKeysToSettings = std::move(CurrentExecutionDynamicTracking);
		CurrentExecutionDynamicTracking.Empty();
		CurrentExecutionDynamicTrackingSettings.Empty();
	}

	if (!ChangedKeys.IsEmpty())
	{
		Subsystem->UpdateComponentTracking(this, /*bShouldDirtyActors=*/false, &ChangedKeys);
	}
}

void UPCGComponent::StartIgnoringChangeOriginDuringGeneration(UObject* InChangeOriginToIgnore)
{
	FWriteScopeLock Lock(IgnoredChangeOriginsLock);

	if (int32* FoundCounter = IgnoredChangeOriginsToCounters.Find(InChangeOriginToIgnore))
	{
		int32& Counter = *FoundCounter; // Put in local variable to evade SA warnings.
		ensure(Counter >= 0);

		Counter = FMath::Max(0, Counter) + 1;
	}
	else
	{

		IgnoredChangeOriginsToCounters.Add(InChangeOriginToIgnore, 1);
	}
}

void UPCGComponent::StopIgnoringChangeOriginDuringGeneration(UObject* InChangeOriginToIgnore)
{
	FWriteScopeLock Lock(IgnoredChangeOriginsLock);

	int32* FoundCounter = IgnoredChangeOriginsToCounters.Find(InChangeOriginToIgnore);
	if (ensure(FoundCounter))
	{
		int32& Counter = *FoundCounter; // Put in local variable to evade SA warnings.
		ensure(Counter > 0);

		if (--Counter <= 0)
		{
			IgnoredChangeOriginsToCounters.Remove(InChangeOriginToIgnore);
		}
	}
}

bool UPCGComponent::IsIgnoringChangeOrigin(UObject* InChangeOrigin)
{
	FReadScopeLock Lock(IgnoredChangeOriginsLock);
	const int32* Counter = IgnoredChangeOriginsToCounters.Find(InChangeOrigin);
	return Counter && ensure(*Counter > 0);
}

void UPCGComponent::ResetIgnoredChangeOrigins(bool bLogIfAnyPresent)
{
	FWriteScopeLock Lock(IgnoredChangeOriginsLock);

	if (bLogIfAnyPresent && !IgnoredChangeOriginsToCounters.IsEmpty())
	{
		UE_LOG(LogPCG, Warning, TEXT("[%s/%s] ResetIgnoredChangeOrigins: IgnoredChangeOrigins should be empty but %d found, purged."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("MISSINGACTOR"),
			*GetName(),
			IgnoredChangeOriginsToCounters.Num());
	}

	IgnoredChangeOriginsToCounters.Reset();
}

#endif // WITH_EDITOR

void UPCGComponent::SetManagedResources(const TArray<TObjectPtr<UPCGManagedResource>>& Resources)
{
	FScopeLock ResourcesLock(&GeneratedResourcesLock);

	// We expect the GeneratedResources to be empty here, as otherwise they might not be taken care of properly - they
	// will be lost down below, but this should not happen. However, if the GeneratedResources are marked as Visible,
	// then they will be copied over during BP duplication, hence why this will happen, hence the ensure here.
	ensure(GeneratedResources.IsEmpty());

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

void UPCGComponent::SetEditingMode(EPCGEditorDirtyMode InEditingMode, EPCGEditorDirtyMode InSerializedEditingMode)
{
	CurrentEditingMode = InEditingMode;
	SerializedEditingMode = InSerializedEditingMode;
}

#if WITH_EDITOR
bool UPCGComponent::DeletePreviewResources()
{
	bool bResourceWasReleased = false;

	TSet<TSoftObjectPtr<AActor>> ActorsToDelete;
	// Make sure to release fully the resources that were loaded
	for (TObjectPtr<UPCGManagedResource> ResourceToRelease : LoadedPreviewResources)
	{
		// Changing the transient state will clear the "marked transient on load" flag
		ResourceToRelease->ChangeTransientState(EPCGEditorDirtyMode::Normal);
		ResourceToRelease->Release(/*bHardRelease=*/true, ActorsToDelete);
		bResourceWasReleased = true;
	}

	LoadedPreviewResources.Empty();

	if (!ActorsToDelete.IsEmpty())
	{
		UPCGActorHelpers::DeleteActors(GetWorld(), ActorsToDelete.Array());
	}

	return bResourceWasReleased;
}

void UPCGComponent::MarkResourcesAsTransientOnLoad()
{
	for (TObjectPtr<UPCGManagedResource>& GeneratedResource : GeneratedResources)
	{
		if (GeneratedResource)
		{
			GeneratedResource->MarkTransientOnLoad();
		}
	}
}

void UPCGComponent::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	bool bShouldMarkDirty = false;

	// Affect all resources
	{
		FScopeLock ResourcesLock(&GeneratedResourcesLock);
		check(!GeneratedResourcesInaccessible);

		for (TObjectPtr<UPCGManagedResource>& GeneratedResource : GeneratedResources)
		{
			if (GeneratedResource)
			{
				GeneratedResource->ChangeTransientState(NewEditingMode);
				bShouldMarkDirty = true;
			}
		}

		// If switching from preview mode to normal or preview-on-load,
		// we must materialize any kind of change we've done on the packages that had a different behavior on load (e.g. actor packages)
		if (NewEditingMode != EPCGEditorDirtyMode::Preview)
		{
			bShouldMarkDirty |= DeletePreviewResources();
		}
	}

	if (IsLocalComponent())
	{
		if (NewEditingMode == EPCGEditorDirtyMode::Preview)
		{
			bShouldMarkDirty = true;
			MarkPackageDirty();
		}

		if (NewEditingMode == EPCGEditorDirtyMode::Preview)
		{
			SetFlags(RF_Transient);
		}
		else
		{
			ClearFlags(RF_Transient);
		}

		ForEachObjectWithOuter(this, [NewEditingMode](UObject* Object)
		{
			if (NewEditingMode == EPCGEditorDirtyMode::Preview)
			{
				Object->SetFlags(RF_Transient);
			}
			else
			{
				Object->ClearFlags(RF_Transient);
			}
		});

		if (NewEditingMode != EPCGEditorDirtyMode::Preview)
		{
			bShouldMarkDirty = true;
			MarkPackageDirty();
		}
	}
	else if (bShouldMarkDirty)
	{
		MarkPackageDirty();
	}

	// Un-transient PAs if needed and propagate the call
	if (IsPartitioned())
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->PropagateEditingModeToLocalComponents(this, NewEditingMode);
		}
	}

	// Changing the transient state can and will play with packages and is not meant to be undoable.
	// However, don't kill the transaction buffer unless it's actually needed, and FIXME: especially not during a construction script
	if (bShouldMarkDirty && GEditor && GEditor->Trans)
	{
		UWorld* World = GetWorld();

		if (!World || !World->bIsRunningConstructionScript)
		{
			GEditor->Trans->Reset(LOCTEXT("ChangeEditingMode", "Changing Editing Mode"));
		}
	}
}

bool UPCGComponent::GetStackContext(FPCGStackContext& OutStackContext) const
{
	UPCGSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && Subsystem->GetStackContext(this, OutStackContext))
	{
		FPCGStack ComponentStack;
		ComponentStack.PushFrame(this);
		OutStackContext.PrependParentStack(&ComponentStack);
		return true;
	}

	return false;
}
#endif // WITH_EDITOR

FPCGComponentInstanceData::FPCGComponentInstanceData(const UPCGComponent* InSourceComponent)
	: FActorComponentInstanceData(InSourceComponent)
	, SourceComponent(InSourceComponent)
{
}

bool FPCGComponentInstanceData::ContainsData() const
{
	return true;
}

void FPCGComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	Super::ApplyToComponent(Component, CacheApplyPhase);

	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		UPCGComponent* PCGComponent = CastChecked<UPCGComponent>(Component);

		// IMPORTANT NOTE:
		// ConstructionSourceComponent: The previous instance of the PCGComponent (not related to annotation, will be the OldComponent in the OnObjectsReplaced call)
		// We will transfer/copy properties/resources from this component that flow forward only (aren't undo/redoable)
		//
		// SourceComponent: The previous instance (same as ConstructionSourceComponent on a regular edit but an annotated Component when in a Undo/Redo operation)
		// We will transfer/copy properties from this component that should reflect undo redo (annotation)
		const UPCGComponent* ConstructionSourceComponent = SourceComponent;
#if WITH_EDITOR
		if (UPCGSubsystem* Subsystem = PCGComponent->GetSubsystem())
		{
			UPCGComponent* FoundConstructionSourceComponent = nullptr;
			if(Subsystem->RemoveAndCopyConstructionScriptSourceComponent(Component->GetOwner(), Component->GetFName(), FoundConstructionSourceComponent))
			{
				ConstructionSourceComponent = FoundConstructionSourceComponent;
			}
		}
#endif
		// IMPORTANT NOTE:
		// Any non-visible (i.e. UPROPERTY() with no specifiers) are NOT copied over when re-running the construction script
		// This means that some properties need to be reapplied here manually unless we make them visible
		if (SourceComponent)
		{
			// Critical: GeneratedGraphOutput
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
			// While this is serialized, it is not properly copied because it is not visible. This is needed otherwise a refresh can retoggle from not generated to generated
			PCGComponent->bForceGenerateOnBPAddedToWorld = SourceComponent->bForceGenerateOnBPAddedToWorld;

			PCGComponent->CurrentEditingMode = SourceComponent->CurrentEditingMode;
			PCGComponent->PreviousEditingMode = SourceComponent->PreviousEditingMode;
			PCGComponent->DynamicallyTrackedKeysToSettings = SourceComponent->DynamicallyTrackedKeysToSettings;
#endif // WITH_EDITOR

			// Non-critical but should be done: transient data, tracked actors cache, landscape tracking
			// TODO Validate usefulness + move accordingly
		}
		
		if (ConstructionSourceComponent)
		{
			// Critical: LastGeneratedBounds
			PCGComponent->LastGeneratedBounds = ConstructionSourceComponent->LastGeneratedBounds;

			// Duplicate generated resources + retarget them
			TArray<TObjectPtr<UPCGManagedResource>> DuplicatedResources;
			for (const TObjectPtr<UPCGManagedResource>& Resource : ConstructionSourceComponent->GeneratedResources)
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
			// bDirtyGenerated is transient.
			PCGComponent->bDirtyGenerated = ConstructionSourceComponent->bDirtyGenerated;

			TArray<TObjectPtr<UPCGManagedResource>> DuplicateLoadedPreviewResources;
			for (const TObjectPtr<UPCGManagedResource>& Resource : ConstructionSourceComponent->LoadedPreviewResources)
			{
				if (Resource)
				{
					UPCGManagedResource* DuplicatedResource = CastChecked<UPCGManagedResource>(StaticDuplicateObject(Resource, PCGComponent, FName()));
					DuplicatedResource->PostApplyToComponent();
					DuplicateLoadedPreviewResources.Add(DuplicatedResource);
				}
			}

			if (DuplicateLoadedPreviewResources.Num() > 0)
			{
				PCGComponent->LoadedPreviewResources = DuplicateLoadedPreviewResources;
			}

			// Move over invocation lists for dynamic delegates
			PCGComponent->OnPCGGraphStartGeneratingExternal = ConstructionSourceComponent->OnPCGGraphStartGeneratingExternal;
			PCGComponent->OnPCGGraphCancelledExternal = ConstructionSourceComponent->OnPCGGraphCancelledExternal;
			PCGComponent->OnPCGGraphGeneratedExternal = ConstructionSourceComponent->OnPCGGraphGeneratedExternal;
			PCGComponent->OnPCGGraphCleanedExternal = ConstructionSourceComponent->OnPCGGraphCleanedExternal;
#endif
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
		if (Subsystem && ConstructionSourceComponent)
		{
			Subsystem->RemapPCGComponent(ConstructionSourceComponent, PCGComponent, bDoActorMapping);
		}

#if WITH_EDITOR
		// Disconnect callbacks on source.
		if (ConstructionSourceComponent && ConstructionSourceComponent->GraphInstance)
		{
			ConstructionSourceComponent->GraphInstance->TeardownCallbacks();
		}

		// Finally, start a delayed refresh task (if there is not one already), in editor only
		// It is important to be delayed, because we cannot spawn Partition Actors within this scope,
		// because we are in a construction script.
		// Note that we only do this if we are not currently loading
		if (!ConstructionSourceComponent || !ConstructionSourceComponent->HasAllFlags(RF_WasLoaded))
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
}

#undef LOCTEXT_NAMESPACE
