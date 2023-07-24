// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPartitionActor.h"
#include "Engine/World.h"
#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Components/BoxComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPartitionActor)

constexpr uint32 InvalidPCGGridSizeValue = 0u;

APCGPartitionActor::APCGPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGGridSize = InvalidPCGGridSizeValue;

#if WITH_EDITOR
	// Setup bounds component
	BoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsComponent"));
	BoundsComponent->SetCollisionObjectType(ECC_WorldStatic);
	BoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsComponent->SetGenerateOverlapEvents(false);
	BoundsComponent->SetupAttachment(GetRootComponent());
	BoundsComponent->bDrawOnlyIfSelected = true;
#endif // WITH_EDITOR
}

UPCGSubsystem* APCGPartitionActor::GetSubsystem() const 
{
	return UPCGSubsystem::GetInstance(GetWorld());
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
#endif

	// Safe guard if we ever load a local that was deleted but not removed (like if the user deleted themselves the component)
	// We can have multiple nullptr entries (if multiple components were removed), hence the while.
	while (LocalToOriginal.Contains(nullptr))
	{
		LocalToOriginal.Remove(nullptr);
	}

	// Make sure that we don't track objects that do not exist anymore
	CleanupDeadGraphInstances(/*bRemoveNullOnly=*/true);

#if WITH_EDITOR
	// Mark all our local components as local
	for (UPCGComponent* LocalComponent : GetAllLocalPCGComponents())
	{
		LocalComponent->MarkAsLocalComponent();
		LocalComponent->ConditionalPostLoad();
	}

	bWasPostCreatedLoaded = true;
#endif // WITH_EDITOR
}

void APCGPartitionActor::BeginDestroy()
{
#if WITH_EDITOR
	if (!PCGHelpers::IsRuntimeOrPIE() && GetSubsystem() && PCGGridSize > 0)
	{
		GetSubsystem()->UnregisterPartitionActor(this);
	}
#endif // WITH_EDITOR

	Super::BeginDestroy();
}

void APCGPartitionActor::Destroyed()
{
#if WITH_EDITOR
	if (!PCGHelpers::IsRuntimeOrPIE() && GetSubsystem() && PCGGridSize > 0)
	{
		GetSubsystem()->UnregisterPartitionActor(this);
	}
#endif // WITH_EDITOR

	Super::Destroyed();
}

void APCGPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	if (BoundsComponent)
	{
		BoundsComponent->SetBoxExtent(GetFixedBounds().GetExtent());
	}
#endif // WITH_EDITOR

	// Reset the OriginalToLocal and build it from the local map
	OriginalToLocal.Reset();
	OriginalToLocal.Reserve(LocalToOriginal.Num());
	for (const auto& It : LocalToOriginal)
	{
		OriginalToLocal.Add(It.Value.Get(), It.Key);
	}

	// Make the Partition actor register itself to the PCG Subsystem
	// Always do it at runtime, wait for the post load/creation in editor
	// Only do the mapping if we are at runtime.

	// Make sure the PCGGridSize is not 0, otherwise it will break everything

	const bool bIsRuntimeOrPIE = PCGHelpers::IsRuntimeOrPIE();
	if ((bIsRuntimeOrPIE 
#if WITH_EDITOR
		|| bWasPostCreatedLoaded
#endif // WITH_EDITOR
		) && ensure(PCGGridSize > 0))
	{
		if (UPCGSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RegisterPartitionActor(this, /*bDoComponentMapping*/ bIsRuntimeOrPIE);
		}
	}
}

void APCGPartitionActor::BeginPlay()
{
	// Pass through all the pcg components, to verify if we need to generate them
	for (auto& It : OriginalToLocal)
	{
		if (It.Key && It.Value)
		{
			// If we have an original component that is generated (or generating), this one is automatically generated => GenerateOnLoad
			if (It.Key->bGenerated || It.Key->IsGenerating())
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

	Super::BeginPlay();
}

void APCGPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPCGSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->UnregisterPartitionActor(this);
	}

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
		LocalComponent->SetPropertiesFromOriginal(OriginalComponent);
		LocalComponent->MarkAsLocalComponent();
		return;
	}

	Modify();

	// Create a new local component
	LocalComponent = NewObject<UPCGComponent>(this);
	LocalComponent->SetPropertiesFromOriginal(OriginalComponent);

	LocalComponent->RegisterComponent();
	LocalComponent->MarkAsLocalComponent();
	// TODO: check if we should use a non-instanced component?
	AddInstanceComponent(LocalComponent);

	OriginalToLocal.Emplace(OriginalComponent, LocalComponent);
	LocalToOriginal.Emplace(LocalComponent, OriginalComponent);
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
		Modify();
	}

	OriginalToLocal.Remove(OldOriginalComponent);
	LocalToOriginal.Remove(LocalComponent);

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

	Modify();

	OriginalToLocal.Remove(OriginalComponent);
	LocalToOriginal.Remove(LocalComponent);

	// TODO Add option to not cleanup?
	LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
	LocalComponent->DestroyComponent();

	return OriginalToLocal.IsEmpty();
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
FBox APCGPartitionActor::GetStreamingBounds() const
{
	return Super::GetStreamingBounds() + GetFixedBounds();
}

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

void APCGPartitionActor::PostCreation()
{
	PCGGridSize = GridSize;

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
	// Since we have infinite bounds in 2D, we just disable the bounds
	if (BoundsComponent && !bUse2DGrid)
	{
		BoundsComponent->SetBoxExtent(GetFixedBounds().GetExtent());
	}

	// Make sure PCGGrid size if greater than 0, otherwise it will break everything
	if (!PCGHelpers::IsRuntimeOrPIE() && GetSubsystem() && ensure(PCGGridSize > 0))
	{
		GetSubsystem()->RegisterPartitionActor(this, /*bDoComponentMapping=*/ false);
	}

	bWasPostCreatedLoaded = true;
#endif // WITH_EDITOR
}

bool APCGPartitionActor::IsSafeForDeletion() const
{
	ensure(IsInGameThread());
	for (TObjectPtr<UPCGComponent> PCGComponent : GetAllOriginalPCGComponents())
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

#endif // WITH_EDITOR
