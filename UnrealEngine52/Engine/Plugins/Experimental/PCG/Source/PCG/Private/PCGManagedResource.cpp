// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGManagedResource.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "Helpers/PCGHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Utils/PCGGeneratedResourcesLogging.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGManagedResource)

static TAutoConsoleVariable<bool> CVarForceReleaseResourcesOnGenerate(
	TEXT("pcg.ForceReleaseResourcesOnGenerate"),
	false,
	TEXT("Purges all tracked generated resources on generate"));

void UPCGManagedResource::PostApplyToComponent()
{
	PostEditImport();
}

// By default, if it is not a hard release, we mark the resource unused.
bool UPCGManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& /*OutActorsToDelete*/)
{
	if (!bHardRelease)
	{
		bIsMarkedUnused = true;
		return false;
	}

	bIsMarkedUnused = false;
	return true;
}

bool UPCGManagedResource::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	if (bIsMarkedUnused)
	{
		Release(true, OutActorsToDelete);
		return true;
	}

	return false;
}

bool UPCGManagedResource::DebugForcePurgeAllResourcesOnGenerate()
{
	return CVarForceReleaseResourcesOnGenerate.GetValueOnAnyThread();
}

void UPCGManagedActors::PostEditImport()
{
	// In this case, the managed actors won't be copied along the actor/component,
	// So we just have to "forget" the actors.
	Super::PostEditImport();
	GeneratedActors.Reset();
}

void UPCGManagedActors::PostApplyToComponent()
{
	// In this case, we want to preserve the data, so we need to do nothing
}

bool UPCGManagedActors::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGManagedActors::Release);

	if (!Super::Release(bHardRelease, OutActorsToDelete))
	{
		PCGGeneratedResourcesLogging::LogManagedActorsSoftRelease(GeneratedActors);

		// Mark actors as potentially-to-be-cleaned-up
		for (TSoftObjectPtr<AActor> GeneratedActor : GeneratedActors)
		{
			if (GeneratedActor.IsValid())
			{
				GeneratedActor->Tags.Add(PCGHelpers::MarkedForCleanupPCGTag);
			}
		}

		return false;
	}

	OutActorsToDelete.Append(GeneratedActors);

	PCGGeneratedResourcesLogging::LogManagedActorsHardRelease(GeneratedActors);

	// Cleanup recursively
	TInlineComponentArray<UPCGComponent*, 1> ComponentsToCleanup;

	for (TSoftObjectPtr<AActor> GeneratedActor : GeneratedActors)
	{
		if (GeneratedActor.IsValid())
		{
			GeneratedActor.Get()->GetComponents(ComponentsToCleanup);

			for (UPCGComponent* Component : ComponentsToCleanup)
			{
				// It is more complicated to handled a non-immediate cleanup when doing it recursively in the managed actors.
				// Do it all immediate then.
				Component->CleanupLocalImmediate(/*bRemoveComponents=*/bHardRelease);
			}

			ComponentsToCleanup.Reset();
		}
	}

	GeneratedActors.Reset();
	return true;
}

bool UPCGManagedActors::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return Super::ReleaseIfUnused(OutActorsToDelete) || GeneratedActors.IsEmpty();
}

bool UPCGManagedActors::MoveResourceToNewActor(AActor* NewActor)
{
	check(NewActor);

	for (TSoftObjectPtr<AActor>& Actor : GeneratedActors)
	{
		if (!Actor.IsValid())
		{
			continue;
		}

		Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Actor->SetOwner(nullptr);
		Actor->AttachToActor(NewActor, FAttachmentTransformRules::KeepWorldTransform);
	}

	GeneratedActors.Empty();

	return true;
}

void UPCGManagedActors::MarkAsUsed()
{
	Super::MarkAsUsed();
	// Technically we don't ever have to "use" a preexisting managed actor resource, but this is to be consistent with the other implementations
	ensure(0);
}

void UPCGManagedActors::MarkAsReused()
{
	Super::MarkAsReused();

	for (TSoftObjectPtr<AActor> GeneratedActor : GeneratedActors)
	{
		if (GeneratedActor.IsValid())
		{
			GeneratedActor->Tags.Remove(PCGHelpers::MarkedForCleanupPCGTag);
		}
	}
}

void UPCGManagedComponent::PostEditImport()
{
	Super::PostEditImport();

	// Rehook components from the original to the locally duplicated components
	UPCGComponent* OwningComponent = Cast<UPCGComponent>(GetOuter());
	AActor* Actor = OwningComponent ? OwningComponent->GetOwner() : nullptr;

	bool bFoundMatch = false;

	if (Actor && GeneratedComponent.IsValid())
	{
		TInlineComponentArray<UActorComponent*, 16> Components;
		Actor->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component && Component->GetFName() == GeneratedComponent->GetFName())
			{
				GeneratedComponent = Component;
				bFoundMatch = true;
				break;
			}
		}

		if (!bFoundMatch)
		{
			// Not quite clear what to do when we have a component that cannot be remapped.
			// Maybe we should check against guids instead?
			ForgetComponent();
		}
	}
	else
	{
		// Somewhat irrelevant case, if we don't have an actor or a component, there's not a lot we can do.
		ForgetComponent();
	}
}

bool UPCGManagedComponent::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& /*OutActorsToDelete*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGManagedComponent::Release);

	const bool bSupportsComponentReset = SupportsComponentReset();
	const bool bDeleteComponent = bHardRelease || !bSupportsComponentReset;

	if (GeneratedComponent.IsValid())
	{
		if (bDeleteComponent)
		{
			GeneratedComponent->DestroyComponent();
		}
		else
		{
			// We can only mark it unused if we can reset the component.
			bIsMarkedUnused = true;
			GeneratedComponent->ComponentTags.Add(PCGHelpers::MarkedForCleanupPCGTag);
		}
	}

	return bDeleteComponent;
}

bool UPCGManagedComponent::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return Super::ReleaseIfUnused(OutActorsToDelete) || !GeneratedComponent.IsValid();
}

bool UPCGManagedComponent::MoveResourceToNewActor(AActor* NewActor)
{
	check(NewActor);

	if (!GeneratedComponent.IsValid())
	{
		return false;
	}

	TObjectPtr<AActor> OldOwner = GeneratedComponent->GetOwner();
	check(OldOwner);

	bool bDetached = false;
	bool bAttached = false;

	// Need to change owner first to avoid that the PCG Component will react to this component changes.
	GeneratedComponent->Rename(nullptr, NewActor);

	// Check if it is a scene component, and if so, use its method to attach/detach to root component
	if (TObjectPtr<USceneComponent> GeneratedSceneComponent = Cast<USceneComponent>(GeneratedComponent.Get()))
	{
		GeneratedSceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		bDetached = true;
		bAttached = GeneratedSceneComponent->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	// Otherwise use the default one.
	if (!bAttached)
	{
		if (!bDetached)
		{
			OldOwner->RemoveInstanceComponent(GeneratedComponent.Get());
		}

		NewActor->AddInstanceComponent(GeneratedComponent.Get());
	}

	ForgetComponent();

	return true;
}

void UPCGManagedComponent::MarkAsUsed()
{
	if (!bIsMarkedUnused)
	{
		return;
	}

	Super::MarkAsUsed();

	// Can't reuse a resource if we can't reset it. Make sure we never take this path in this case.
	check(SupportsComponentReset());

	ResetComponent();

	if (GeneratedComponent.Get())
	{
		GeneratedComponent->ComponentTags.Remove(PCGHelpers::MarkedForCleanupPCGTag);
	}
}

void UPCGManagedComponent::MarkAsReused()
{
	Super::MarkAsReused();

	if (GeneratedComponent.Get())
	{
		GeneratedComponent->ComponentTags.Remove(PCGHelpers::MarkedForCleanupPCGTag);
	}
}

void UPCGManagedISMComponent::PostLoad()
{
	Super::PostLoad();

	if (!bHasDescriptor)
	{
		if (UInstancedStaticMeshComponent* ISMC = GetComponent())
		{
			FISMComponentDescriptor NewDescriptor;
			NewDescriptor.InitFrom(ISMC);

			SetDescriptor(NewDescriptor);
		}
	}

	// Cache raw ptr
	GetComponent();
}

void UPCGManagedISMComponent::ForgetComponent()
{
	Super::ForgetComponent();
	CachedRawComponentPtr = nullptr;
}

void UPCGManagedISMComponent::SetDescriptor(const FISMComponentDescriptor& InDescriptor)
{
	bHasDescriptor = true;
	Descriptor = InDescriptor;
}

bool UPCGManagedISMComponent::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	if (Super::ReleaseIfUnused(OutActorsToDelete) || !GetComponent())
	{
		return true;
	}
	else if (GetComponent()->GetInstanceCount() == 0)
	{
		GeneratedComponent->DestroyComponent();
		ForgetComponent();
		return true;
	}
	else
	{
		return false;
	}
}

void UPCGManagedISMComponent::ResetComponent()
{
	if (UInstancedStaticMeshComponent * ISMC = GetComponent())
	{
		ISMC->ClearInstances();
		ISMC->UpdateBounds();
	}
}

UInstancedStaticMeshComponent* UPCGManagedISMComponent::GetComponent() const
{
	if (!CachedRawComponentPtr)
	{
		CachedRawComponentPtr = Cast<UInstancedStaticMeshComponent>(GeneratedComponent.Get());
	}

	return CachedRawComponentPtr;
}

void UPCGManagedISMComponent::SetComponent(UInstancedStaticMeshComponent* InComponent)
{
	GeneratedComponent = InComponent;
	CachedRawComponentPtr = InComponent;
}