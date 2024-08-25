// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGPartitionActor.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPartitionActor)

constexpr uint32 InvalidPCGGridSizeValue = 0u;

APCGPartitionActor::APCGPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGGridSize = InvalidPCGGridSizeValue;

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Setup bounds component
	BoundsComponent = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBoxComponent>(this, TEXT("BoundsComponent"));
	if (BoundsComponent)
	{
		BoundsComponent->SetCollisionObjectType(ECC_WorldStatic);
		BoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoundsComponent->SetGenerateOverlapEvents(false);
		BoundsComponent->SetupAttachment(GetRootComponent());
		BoundsComponent->bDrawOnlyIfSelected = true;
	}
#endif // WITH_EDITOR
}

void APCGPartitionActor::PostLoad()
{
	Super::PostLoad();

	// If the grid size is not set, set it to the default value.
	if (PCGGridSize == InvalidPCGGridSizeValue)
	{
		PCGGridSize = APCGWorldActor::DefaultPartitionGridSize;
	}

#if WITH_EDITOR
	if (LocalToOriginalMap_DEPRECATED.Num() > 0)
	{
		for (const TPair<TObjectPtr<UPCGComponent>, TWeakObjectPtr<UPCGComponent>>& LocalToOriginalPair : LocalToOriginalMap_DEPRECATED)
		{
			LocalToOriginal.Emplace(LocalToOriginalPair.Key, LocalToOriginalPair.Value.Get());
		}

		LocalToOriginalMap_DEPRECATED.Reset();
	}

	if (GetGridSize() != PCGGridSize)
	{
		SetGridSize(PCGGridSize);
	}
#endif

	// Safe guard if we ever load a local that was deleted but not removed (like if the user deleted themselves the component)
	// We can have multiple nullptr entries (if multiple components were removed), hence the while.
	while (LocalToOriginal.Contains(nullptr))
	{
		LocalToOriginal.Remove(nullptr);
	}

	// Make sure that we don't track objects that do not exist anymore
	CleanupDeadGraphInstances(/*bRemoveNullOnly=*/true);

	// Mark all our local components as local
	for (UPCGComponent* LocalComponent : GetAllLocalPCGComponents())
	{
		LocalComponent->MarkAsLocalComponent();
		LocalComponent->ConditionalPostLoad();
	}

	bWasPostCreatedLoaded = true;
}

void APCGPartitionActor::BeginDestroy()
{
	UnregisterPCG();

	Super::BeginDestroy();
}

void APCGPartitionActor::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	TMap<TObjectPtr<UPCGComponent>, TSoftObjectPtr<UPCGComponent>> LocalToOriginalCopy;
	TMap<TObjectPtr<UPCGComponent>, TSoftObjectPtr<UPCGComponent>> Transients;
	TSet<TObjectPtr<UPCGComponent>> AddedLoadedPreviewKeys;

	if (Ar.IsSaving())
	{
		// Split out the map into persistent and transient components. Keep the persistent ones in LocalToOriginal
		// for serialization purposes (saving, linker fixup of SOPs).
		LocalToOriginalCopy = MoveTemp(LocalToOriginal);

		LocalToOriginal.Reset();
		for (const auto& It : LocalToOriginalCopy)
		{
			if (!ensure(It.Key))
			{
				continue;
			}

			if (!It.Key->HasAnyFlags(RF_Transient))
			{
				LocalToOriginal.Add(It);
			}
			else
			{
				Transients.Add(It);
			}
		}

		// For components that were cleared but marked as load-as-preview
		// we need to keep those around only if the original is still in its original state,
		// i.e. the serialization mode is load on preview and the current mode is preview.
		for (const auto& It : LoadedPreviewComponents)
		{
			if (UPCGComponent* OriginalComponent = It.Value.Get())
			{
				if (OriginalComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview && OriginalComponent->GetEditingMode() == EPCGEditorDirtyMode::Preview)
				{
					if (ensure(!LocalToOriginal.Contains(It.Key)))
					{
						LocalToOriginal.Add(It);
						AddedLoadedPreviewKeys.Add(It.Key);
					}
				}
			}
		}
	}
#endif

	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsSaving())
	{
		// Remove added loaded preview components.
		for (const TObjectPtr<UPCGComponent>& Key : AddedLoadedPreviewKeys)
		{
			LocalToOriginal.Remove(Key);
		}

		// Restore removed transient components.
		for (const auto& It : Transients)
		{
			LocalToOriginal.Add(It);
		}
	}
#endif
}

void APCGPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	UpdateBoundsComponentExtents();
#endif // WITH_EDITOR

	// Reset the OriginalToLocal and build it from the local map
	OriginalToLocal.Reset();
	OriginalToLocal.Reserve(LocalToOriginal.Num());
	for (const auto& It : LocalToOriginal)
	{
		OriginalToLocal.Add(It.Value.Get(), It.Key);
	}

	// Make the Partition actor register itself to the PCG Subsystem. RuntimeGen components should only register in the PostCreation path.
	if (PCGGridSize != InvalidPCGGridSizeValue && !IsRuntimeGenerated())
	{
		RegisterPCG();
	}
}

void APCGPartitionActor::PostUnregisterAllComponents()
{
	UnregisterPCG();

	Super::PostUnregisterAllComponents();
}

void APCGPartitionActor::BeginPlay()
{
	// bIsRuntimeGenerated is not set yet, so we also need to check if the PA is transient.
	if (!IsRuntimeGenerated() && !HasAnyFlags(RF_Transient))
	{
		// Pass through all the pcg components, to verify if we need to generate them.
		for (auto& It : OriginalToLocal)
		{
			if (It.Key && It.Value)
			{
				// If we have an original component that is generated (or generating), this one is automatically generated => GenerateOnLoad.
				// But if its runtime generated then it's handled by the runtime generation scheduler.
				if ((It.Key->bGenerated || It.Key->IsGenerating()) && ensure(!It.Key->IsManagedByRuntimeGenSystem()))
				{
					It.Value->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;
				}
				// Otherwise, make them match
				else
				{
					It.Value->GenerationTrigger = It.Key->GenerationTrigger;
				}
			}
		}
	}

	Super::BeginPlay();
}

void APCGPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterPCG();

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
uint32 APCGPartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	if (APCGWorldActor* PCGActor = PCGHelpers::GetPCGWorldActor(InWorld))
	{
		return PCGActor->PartitionGridSize;
	}

	UE_LOG(LogPCG, Error, TEXT("[APCGPartitionActor::InternalGetDefaultGridSize] PCG World Actor was null. Returning default value"));
	return APCGWorldActor::DefaultPartitionGridSize;
}
#endif

FBox APCGPartitionActor::GetFixedBounds() const
{
	const FVector Center = GetActorLocation();
	const FVector::FReal HalfGridSize = PCGGridSize / 2.0;

	FVector Extent(HalfGridSize, HalfGridSize, HalfGridSize);

	// In case of 2D grid, it's like the actor has infinite bounds on the Z axis
	if (bUse2DGrid)
	{
		Extent.Z = HALF_WORLD_MAX1;
	}

	return FBox(Center - Extent, Center + Extent);
}

FIntVector APCGPartitionActor::GetGridCoord() const
{
	const FVector Center = GetActorLocation();
	return UPCGActorHelpers::GetCellCoord(Center, PCGGridSize, bUse2DGrid);
}

bool APCGPartitionActor::Teleport(const FVector& NewLocation)
{
	// We should not be teleporting a PA that is in use. We only teleport empty RuntimeGen PAs to their grid cell before initialization.
	check(bIsRuntimeGenerated && OriginalToLocal.IsEmpty() && LocalToOriginal.IsEmpty());

	RootComponent->SetMobility(EComponentMobility::Movable);
	bool bResult = Super::SetActorLocation(NewLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	RootComponent->SetMobility(EComponentMobility::Static);

	return bResult;
}

void APCGPartitionActor::RegisterPCG()
{
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (!bIsRegistered)
		{
			Subsystem->RegisterPartitionActor(this, /*bDoComponentMapping=*/false);
			bIsRegistered = true;
		}
	}
}

void APCGPartitionActor::UnregisterPCG()
{
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		if (bIsRegistered)
		{
			Subsystem->UnregisterPartitionActor(this);
			bIsRegistered = false;
		}
	}
}

void APCGPartitionActor::GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors) const
{
	Super::GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);

	// To keep consistency with the other GetBounds functions, transform our result into an origin / extent formatting
	FBox Bounds(Origin - BoxExtent, Origin + BoxExtent);
	Bounds += GetFixedBounds();
	Bounds.GetCenterAndExtents(Origin, BoxExtent);
}

UPCGComponent* APCGPartitionActor::GetLocalComponent(const UPCGComponent* OriginalComponent) const
{
	const TObjectPtr<UPCGComponent>* LocalComponent = OriginalToLocal.Find(OriginalComponent);
	return LocalComponent ? *LocalComponent : nullptr;
}

UPCGComponent* APCGPartitionActor::GetOriginalComponent(const UPCGComponent* LocalComponent) const
{
	const TSoftObjectPtr<UPCGComponent>* OriginalComponent = LocalToOriginal.Find(LocalComponent);
	return OriginalComponent ? (*OriginalComponent).Get() : nullptr;
}

void APCGPartitionActor::CleanupDeadGraphInstances(bool bRemoveNonNullOnly)
{
	// First find if we have any local dead instance (= nullptr) hooked to an original component.
	TSet<TObjectPtr<UPCGComponent>> DeadOriginalInstances;
	for (const auto& OriginalToLocalItem : OriginalToLocal)
	{
		if (!OriginalToLocalItem.Value)
		{
			DeadOriginalInstances.Add(OriginalToLocalItem.Key);
		}
	}

	if (!DeadOriginalInstances.IsEmpty())
	{
		Modify();

		for (const TObjectPtr<UPCGComponent>& DeadInstance : DeadOriginalInstances)
		{
			OriginalToLocal.Remove(DeadInstance);
		}

		LocalToOriginal.Remove(nullptr);
	}

	// And do the same with dead original ones.
	TSet<TObjectPtr<UPCGComponent>> DeadLocalInstances;
	for (const auto& LocalToOriginalItem : LocalToOriginal)
	{
		if((bRemoveNonNullOnly && LocalToOriginalItem.Value.IsNull()) ||
			(!bRemoveNonNullOnly && !LocalToOriginalItem.Value.IsValid()))
		{
			DeadLocalInstances.Add(LocalToOriginalItem.Key);
		}
	}

	if (!DeadLocalInstances.IsEmpty())
	{
		Modify();

		for (const TObjectPtr<UPCGComponent>& DeadInstance : DeadLocalInstances)
		{
			LocalToOriginal.Remove(DeadInstance);

			if (DeadInstance)
			{
				DeadInstance->CleanupLocalImmediate(/*bRemoveComponents=*/true);
				DeadInstance->DestroyComponent();
			}
		}

		// Remove all dead entries
		OriginalToLocal.Remove(nullptr);
	}
}

void APCGPartitionActor::AddGraphInstance(UPCGComponent* OriginalComponent)
{
	if (!OriginalComponent)
	{
		return;
	}

	// Make sure we don't have that graph twice;
	// Here we'll check if there has been some changes worth propagating or not
	UPCGComponent* LocalComponent = GetLocalComponent(OriginalComponent);

	if (LocalComponent)
	{
		// Update properties as needed and early out
		LocalComponent->SetEditingMode(OriginalComponent->GetEditingMode(), OriginalComponent->GetSerializedEditingMode());
		LocalComponent->SetPropertiesFromOriginal(OriginalComponent);
		LocalComponent->MarkAsLocalComponent();
		LocalComponent->SetGenerationGridSize(PCGGridSize);
		return;
	}

	Modify(!OriginalComponent->IsInPreviewMode());

	// Create a new local component
	LocalComponent = NewObject<UPCGComponent>(this, NAME_None, OriginalComponent->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
	LocalComponent->MarkAsLocalComponent();
	LocalComponent->SetGenerationGridSize(PCGGridSize);

	// Note: we'll place the local component prior to the SetPropertiesFromOriginal so that any code that relies on the parent-child relationship works here
	OriginalToLocal.Emplace(OriginalComponent, LocalComponent);
	LocalToOriginal.Emplace(LocalComponent, OriginalComponent);

	// Implementation note: since this is a new component, we need to use the current editing mode only for both the current & serialized editing modes
	LocalComponent->SetEditingMode(/*InEditingMode=*/OriginalComponent->GetEditingMode(), /*InSerializedEditingMode=*/OriginalComponent->GetEditingMode());
	LocalComponent->SetPropertiesFromOriginal(OriginalComponent);

	LocalComponent->RegisterComponent();

	// TODO: check if we should use a non-instanced component?
	AddInstanceComponent(LocalComponent);
}

void APCGPartitionActor::RemapGraphInstance(const UPCGComponent* OldOriginalComponent, UPCGComponent* NewOriginalComponent)
{
	check(OldOriginalComponent && NewOriginalComponent);

	UPCGComponent* LocalComponent = GetLocalComponent(OldOriginalComponent);

	if (!LocalComponent)
	{
		return;
	}

	// If the old original component was loaded, we can assume we are in a loading phase.
	// In this case, we don't want to dirty or register a transaction
	const bool bIsLoading = OldOriginalComponent->HasAnyFlags(RF_WasLoaded);

	if (!bIsLoading)
	{
		Modify(!LocalComponent->IsInPreviewMode());
	}

	OriginalToLocal.Remove(OldOriginalComponent);
	LocalToOriginal.Remove(LocalComponent);

	LocalComponent->SetEditingMode(NewOriginalComponent->GetEditingMode(), NewOriginalComponent->GetSerializedEditingMode());
	LocalComponent->SetPropertiesFromOriginal(NewOriginalComponent);
	OriginalToLocal.Emplace(NewOriginalComponent, LocalComponent);
	LocalToOriginal.Emplace(LocalComponent, NewOriginalComponent);

#if WITH_EDITOR
	// When changing original data, it means that the data we have might point to newly stale data, hence we need to force dirty here
	if (!bIsLoading)
	{
		LocalComponent->DirtyGenerated(EPCGComponentDirtyFlag::Actor);
	}
#endif
}

bool APCGPartitionActor::RemoveGraphInstance(UPCGComponent* OriginalComponent)
{
	UPCGComponent* LocalComponent = GetLocalComponent(OriginalComponent);

	if (!LocalComponent)
	{
		// If we don't have a local component, perhaps the original component is already dead,
		// so do some clean up
		CleanupDeadGraphInstances();
		return false;
	}

	Modify(!LocalComponent->IsInPreviewMode());

	OriginalToLocal.Remove(OriginalComponent);
	LocalToOriginal.Remove(LocalComponent);

	// TODO Add option to not cleanup?
	LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);

	// If the component is tagged as "preview-on-load" we shouldn't actually remove it, otherwise we'll cause a change on the actor.
	if (LocalComponent->GetEditingMode() == EPCGEditorDirtyMode::Preview && LocalComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview)
	{
		LocalComponent->UnregisterComponent();
		LoadedPreviewComponents.Emplace(LocalComponent, OriginalComponent);
	}
	else
	{
		LocalComponent->DestroyComponent();
	}

	return OriginalToLocal.IsEmpty() && LoadedPreviewComponents.IsEmpty();
}

void APCGPartitionActor::RemoveLocalComponent(UPCGComponent* LocalComponent)
{
	if (!LocalComponent)
	{
		return;
	}

	UPCGComponent* OriginalComponent = GetOriginalComponent(LocalComponent);

	LocalToOriginal.Remove(LocalComponent);
	OriginalToLocal.Remove(OriginalComponent);
}

#if WITH_EDITOR
AActor* APCGPartitionActor::GetSceneOutlinerParent() const
{
	if (APCGWorldActor* PCGActor = PCGHelpers::GetPCGWorldActor(GetWorld()))
	{
		return PCGActor;
	}
	else
	{
		return Super::GetSceneOutlinerParent();
	}	
}
#endif // WITH_EDITOR

void APCGPartitionActor::PostCreation(const FGuid& InGridGUID, uint32 InGridSize)
{
	PCGGuid = InGridGUID;
	PCGGridSize = InGridSize;

	// Put in cache if we use the 2D grid or not.
	if (APCGWorldActor* PCGActor = PCGHelpers::GetPCGWorldActor(GetWorld()))
	{
		bUse2DGrid = PCGActor->bUse2DGrid;
	}
	else
	{
		bUse2DGrid = true;
	}

#if WITH_EDITOR
	UpdateBoundsComponentExtents();
#endif // WITH_EDITOR

	RegisterPCG();

	bWasPostCreatedLoaded = true;
}

bool APCGPartitionActor::IsSafeForDeletion() const
{
	ensure(IsInGameThread());
	for (const TObjectPtr<UPCGComponent>& PCGComponent : GetAllOriginalPCGComponents())
	{
		if (PCGComponent && (PCGComponent->IsGenerating() || PCGComponent->IsCleaningUp()))
		{
			return false;
		}
	}

	return true;
}

TSet<TObjectPtr<UPCGComponent>> APCGPartitionActor::GetAllLocalPCGComponents() const
{
	TSet<TObjectPtr<UPCGComponent>> ResultComponents;
	LocalToOriginal.GetKeys(ResultComponents);

	return ResultComponents;
}

TSet<TObjectPtr<UPCGComponent>> APCGPartitionActor::GetAllOriginalPCGComponents() const
{
	TSet<TObjectPtr<UPCGComponent>> ResultComponents;
	for(const TPair<TObjectPtr<UPCGComponent>, TObjectPtr<UPCGComponent>>& OriginalPair : OriginalToLocal)
	{
		ResultComponents.Add(OriginalPair.Key.Get());
	}

	return ResultComponents;
}

UPCGSubsystem* APCGPartitionActor::GetSubsystem() const 
{
	return UPCGSubsystem::GetInstance(GetWorld());
}

#if WITH_EDITOR
bool APCGPartitionActor::ChangeTransientState(UPCGComponent* OriginalComponent, EPCGEditorDirtyMode EditingMode)
{
	check(OriginalComponent);

	// First, propagate the transient state to the matching local component if any
	if (UPCGComponent* LocalComponent = GetLocalComponent(OriginalComponent))
	{
		LocalComponent->SetEditingMode(/*CurrentEditingMode=*/EditingMode, /*SerializedEditingMode=*/EditingMode);
		LocalComponent->ChangeTransientState(EditingMode);
	}

	// Then, when switching to anything but preview, we must get rid of any still-loaded components that would have been removed otherwise
	if (EditingMode != EPCGEditorDirtyMode::Preview)
	{
		TArray<UPCGComponent*> LocalComponentsToDelete;

		for (const auto& LoadedPreviewLocalToOriginal : LoadedPreviewComponents)
		{
			if (LoadedPreviewLocalToOriginal.Value == OriginalComponent)
			{
				LocalComponentsToDelete.Add(LoadedPreviewLocalToOriginal.Key);
			}
		}

		// Streamlined version of RemoveGraphInstance
		Modify(!LocalComponentsToDelete.IsEmpty());

		for (UPCGComponent* LocalComponent : LocalComponentsToDelete)
		{
			LoadedPreviewComponents.Remove(LocalComponent);
			LocalComponent->ChangeTransientState(EditingMode);
			LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
			LocalComponent->DestroyComponent();
		}
	}

	return OriginalToLocal.IsEmpty() && LoadedPreviewComponents.IsEmpty();
}

void APCGPartitionActor::UpdateBoundsComponentExtents()
{
	// Disable BoundsComponent if in 2D
	if (BoundsComponent && !bUse2DGrid)
	{
		BoundsComponent->SetBoxExtent(GetFixedBounds().GetExtent());
	}
}
#endif // WITH_EDITOR

FString APCGPartitionActor::GetPCGPartitionActorName(uint32 GridSize, const FIntVector& GridCoords, bool bRuntimeGenerated)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ActorNameBuilder;

	if (bRuntimeGenerated)
	{
		ActorNameBuilder += TEXT("PCGRuntimeGenPartitionActor_");
	}
	else
	{
		ActorNameBuilder += TEXT("PCGPartitionActor_");
	}

	ActorNameBuilder += FString::Printf(TEXT("%d_"), GridSize);
	ActorNameBuilder += FString::Printf(TEXT("%d_%d_%d"), GridCoords.X, GridCoords.Y, GridCoords.Z);

	return ActorNameBuilder.ToString();
}