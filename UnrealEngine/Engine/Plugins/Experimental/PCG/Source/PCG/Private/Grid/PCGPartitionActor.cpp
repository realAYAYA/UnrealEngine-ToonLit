// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPartitionActor.h"
#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "Landscape.h"
#include "Components/BoxComponent.h"

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
	return GetWorld() ? GetWorld()->GetSubsystem<UPCGSubsystem>() : nullptr;
}

void APCGPartitionActor::PostLoad()
{
	Super::PostLoad();

	// If the grid size is not set, set it to the default value.
	if (PCGGridSize == InvalidPCGGridSizeValue)
	{
		PCGGridSize = APCGWorldActor::DefaultPartitionGridSize;
	}

	// Make sure that we don't track objects that do not exist anymore
	CleanupDeadGraphInstances();

#if WITH_EDITOR
	// Mark all our local components as local
	for (UPCGComponent* LocalComponent : GetAllLocalPCGComponents())
	{
		LocalComponent->MarkAsLocalComponent();
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
	for (auto& It : OriginalToLocalMap)
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
	const TObjectPtr<UPCGComponent>* LocalComponent = OriginalToLocalMap.Find(OriginalComponent);
	return LocalComponent ? *LocalComponent : nullptr;
}

UPCGComponent* APCGPartitionActor::GetOriginalComponent(const UPCGComponent* LocalComponent) const
{
	const TObjectPtr<UPCGComponent>* OriginalComponent = LocalToOriginalMap.Find(LocalComponent);
	return OriginalComponent ? (*OriginalComponent).Get() : nullptr;
}

void APCGPartitionActor::CleanupDeadGraphInstances()
{
	// First find if we have any local dead instance (= nullptr) hooked to an original component.
	TSet<TObjectPtr<UPCGComponent>> DeadOriginalInstances;
	for (const auto& OriginalToLocalItem : OriginalToLocalMap)
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
			OriginalToLocalMap.Remove(DeadInstance);
		}

		LocalToOriginalMap.Remove(nullptr);
	}

	// And do the same with dead original ones.
	TSet<TObjectPtr<UPCGComponent>> DeadLocalInstances;
	for (const auto& LocalToOriginalItem : LocalToOriginalMap)
	{
		if (!LocalToOriginalItem.Value)
		{
			DeadLocalInstances.Add(LocalToOriginalItem.Key);
		}
	}


	if (!DeadLocalInstances.IsEmpty())
	{
		Modify();

		for (const TObjectPtr<UPCGComponent>& DeadInstance : DeadLocalInstances)
		{
			LocalToOriginalMap.Remove(DeadInstance);

			if (DeadInstance)
			{
				DeadInstance->CleanupLocalImmediate(/*bRemoveComponents=*/true);
				DeadInstance->DestroyComponent();
			}
		}

		// Remove all dead entries
		OriginalToLocalMap.Remove(nullptr);
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

	OriginalToLocalMap.Add(OriginalComponent, LocalComponent);
	LocalToOriginalMap.Add(LocalComponent, OriginalComponent);
}

void APCGPartitionActor::RemapGraphInstance(const UPCGComponent* OldOriginalComponent, UPCGComponent* NewOriginalComponent)
{
	UPCGComponent* LocalComponent = GetLocalComponent(OldOriginalComponent);

	if (!LocalComponent)
	{
		return;
	}

	Modify();

	OriginalToLocalMap.Remove(OldOriginalComponent);
	LocalToOriginalMap.Remove(LocalComponent);

	LocalComponent->SetPropertiesFromOriginal(NewOriginalComponent);
	OriginalToLocalMap.Add(NewOriginalComponent, LocalComponent);
	LocalToOriginalMap.Add(LocalComponent, NewOriginalComponent);
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

	OriginalToLocalMap.Remove(OriginalComponent);
	LocalToOriginalMap.Remove(LocalComponent);

	// TODO Add option to not cleanup?
	LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
	LocalComponent->DestroyComponent();

	return OriginalToLocalMap.IsEmpty();
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
		bUse2DGrid = false;
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
	LocalToOriginalMap.GetKeys(ResultComponents);

	return ResultComponents;
}

TSet<TObjectPtr<UPCGComponent>> APCGPartitionActor::GetAllOriginalPCGComponents() const
{
	TSet<TObjectPtr<UPCGComponent>> ResultComponents;
	OriginalToLocalMap.GetKeys(ResultComponents);

	return ResultComponents;
}

#endif // WITH_EDITOR