// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGManagedResource.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "Helpers/PCGHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Level.h"
#include "UObject/Package.h"
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
	bIsMarkedUnused = true;
	return bHardRelease;
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

bool UPCGManagedResource::CanBeUsed() const
{
#if WITH_EDITOR
	return !bMarkedTransientOnLoad;
#else
	return true;
#endif
}

bool UPCGManagedResource::DebugForcePurgeAllResourcesOnGenerate()
{
	return CVarForceReleaseResourcesOnGenerate.GetValueOnAnyThread();
}

#if WITH_EDITOR
void UPCGManagedResource::ChangeTransientState(EPCGEditorDirtyMode /*NewEditingMode*/)
{
	// Any change in the transient state resets the transient state that was set on load, regardless of the bNowTransient flag
	bMarkedTransientOnLoad = false;
}
#endif // WITH_EDITOR

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
		PCGGeneratedResourcesLogging::LogManagedActorsRelease(this, GeneratedActors, bHardRelease, /*bOnlyMarkedForCleanup=*/true);

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

#if WITH_EDITOR
	if (bMarkedTransientOnLoad)
	{
		// Here, instead of adding the actors to be deleted (which has the side effect of potentially emptying the package, which leads to its deletion, we will hide the actors instead
		for (TSoftObjectPtr<AActor> GeneratedActor : GeneratedActors)
		{
			// In rare cases where the actor wouldn't be currently loaded (in WP) we must make sure it is done here
			if (AActor* Actor = GeneratedActor.LoadSynchronous())
			{
				Actor->SetIsTemporarilyHiddenInEditor(true);
				Actor->SetHidden(true);
				Actor->SetActorEnableCollision(false);
			}
		}
	}
	else
#endif // WITH_EDITOR
	{
		OutActorsToDelete.Append(GeneratedActors);
	}

	PCGGeneratedResourcesLogging::LogManagedActorsRelease(this, GeneratedActors, bHardRelease, /*bOnlyMarkedForCleanup=*/false);

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

#if WITH_EDITOR
	if (!bMarkedTransientOnLoad)
#endif
	{
		GeneratedActors.Reset();
	}
	
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

		const bool bWasAttached = (Actor->GetAttachParentActor() != nullptr);

		if (bWasAttached)
		{
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->SetOwner(nullptr);
			Actor->AttachToActor(NewActor, FAttachmentTransformRules::KeepWorldTransform);
		}
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

#if WITH_EDITOR
void UPCGManagedActors::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	const bool bNowTransient = (NewEditingMode == EPCGEditorDirtyMode::Preview);

	for (TSoftObjectPtr<AActor> GeneratedActor : GeneratedActors)
	{
		// Make sure to load if needed because we need to affect the actors regardless of the current WP state
		if (GeneratedActor.LoadSynchronous() != nullptr)
		{
			const bool bWasTransient = GeneratedActor->HasAnyFlags(RF_Transient);

			if (bNowTransient != bWasTransient)
			{
				if (bNowTransient)
				{
					GeneratedActor->SetFlags(RF_Transient);
				}
				else
				{
					GeneratedActor->ClearFlags(RF_Transient);
				}
			}

			// If the actor had PCG components, propagate this downward
			{
				TInlineComponentArray<UPCGComponent*, 4> PCGComponents;
				GeneratedActor->GetComponents(PCGComponents);

				for (UPCGComponent* PCGComponent : PCGComponents)
				{
					PCGComponent->SetEditingMode(/*CurrentEditingMode=*/NewEditingMode, /*SerializedEditingMode=*/NewEditingMode);
					PCGComponent->ChangeTransientState(NewEditingMode);
				}
			}

			if (bNowTransient != bWasTransient)
			{
				if (GeneratedActor->GetLevel() && GeneratedActor->GetLevel()->IsUsingExternalActors())
				{
					UPackage* PreviousExternalPackage = bNowTransient ? GeneratedActor->GetExternalPackage() : nullptr;

					GeneratedActor->SetPackageExternal(/*bExternal=*/!bNowTransient, /*bShouldDirty=*/false);

					if (PreviousExternalPackage)
					{
						PreviousExternalPackage->MarkPackageDirty();
					}
				
					if (!bNowTransient && GeneratedActor->GetExternalPackage())
					{
						MarkPackageDirty();
					}
				}

				if (!bNowTransient || GeneratedActor->IsPackageExternal())
				{
					ForEachObjectWithOuter(GeneratedActor.Get(), [bNowTransient](UObject* Object)
					{
						if (bNowTransient)
						{
							Object->SetFlags(RF_Transient);
						}
						else
						{
							Object->ClearFlags(RF_Transient);
						}
					});
				}
			}
		}
	}

	Super::ChangeTransientState(NewEditingMode);
}
#endif // WITH_EDITOR

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

#if WITH_EDITOR
void UPCGManagedComponent::HideComponent()
{
	if (GeneratedComponent.IsValid())
	{
		GeneratedComponent->UnregisterComponent();
	}
}
#endif // WITH_EDITOR

bool UPCGManagedComponent::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& /*OutActorsToDelete*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGManagedComponent::Release);

	const bool bSupportsComponentReset = SupportsComponentReset();
	bool bDeleteComponent = bHardRelease || !bSupportsComponentReset;

	if (GeneratedComponent.IsValid())
	{
#if WITH_EDITOR
		if (bMarkedTransientOnLoad)
		{
			PCGGeneratedResourcesLogging::LogManagedComponentHidden(this);

			HideComponent();
			bIsMarkedUnused = true;
		}
		else
#endif // WITH_EDITOR
		{
			if (bDeleteComponent)
			{
				PCGGeneratedResourcesLogging::LogManagedResourceHardRelease(this);

				GeneratedComponent->DestroyComponent();
				ForgetComponent();
			}
			else
			{
				PCGGeneratedResourcesLogging::LogManagedResourceSoftRelease(this);

				// We can only mark it unused if we can reset the component.
				bIsMarkedUnused = true;
				GeneratedComponent->ComponentTags.Add(PCGHelpers::MarkedForCleanupPCGTag);
			}
		}
	}
	else
	{
		PCGGeneratedResourcesLogging::LogManagedComponentDeleteNull(this);

		// Dead component reference - clear it out.
		bDeleteComponent = true;
	}

	return bDeleteComponent;
}

bool UPCGManagedComponent::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	return Super::ReleaseIfUnused(OutActorsToDelete) || !GeneratedComponent.IsValid();
}

bool UPCGManagedComponent::MoveResourceToNewActor(AActor* NewActor, const AActor* ExpectedPreviousOwner)
{
	check(NewActor);

	if (!GeneratedComponent.IsValid())
	{
		return false;
	}

	TObjectPtr<AActor> OldOwner = GeneratedComponent->GetOwner();
	check(OldOwner);

	// Prevent moving of components on external (or spawned) actors
	if (ExpectedPreviousOwner && OldOwner != ExpectedPreviousOwner)
	{
		return false;
	}

	bool bDetached = false;
	bool bAttached = false;

	GeneratedComponent->UnregisterComponent();

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

	GeneratedComponent->RegisterComponent();

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

#if WITH_EDITOR
void UPCGManagedComponent::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	const bool bNowTransient = (NewEditingMode == EPCGEditorDirtyMode::Preview);
	if (GeneratedComponent.Get())
	{
		const bool bWasTransient = GeneratedComponent->HasAnyFlags(RF_Transient);

		if (bWasTransient != bNowTransient)
		{
			if (bNowTransient)
			{
				GeneratedComponent->SetFlags(RF_Transient);
			}
			else
			{
				GeneratedComponent->ClearFlags(RF_Transient);
			}

			ForEachObjectWithOuter(GeneratedComponent.Get(), [bNowTransient](UObject* Object)
			{
				if (bNowTransient)
				{
					Object->SetFlags(RF_Transient);
				}
				else
				{
					Object->ClearFlags(RF_Transient);
				}
			});

			GeneratedComponent->MarkPackageDirty(); // should dirty actor this component is attached to
		}
	}

	Super::ChangeTransientState(NewEditingMode);
}
#endif // WITH_EDITOR

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
	if (UInstancedStaticMeshComponent* ISMC = GetComponent())
	{
		ISMC->ClearInstances();
		ISMC->UpdateBounds();
	}
}

void UPCGManagedISMComponent::MarkAsUsed()
{
	const bool bWasMarkedUnused = bIsMarkedUnused;
	Super::MarkAsUsed();

	if (!bWasMarkedUnused)
	{
		return;
	}

	if (UInstancedStaticMeshComponent* ISMC = GetComponent())
	{
		// Keep track of the current root location so if we reuse this later we are able to update this appropriately
		if (USceneComponent* RootComponent = ISMC->GetAttachmentRoot())
		{
			bHasRootLocation = true;
			RootLocation = RootComponent->GetComponentLocation();
		}
		else
		{
			bHasRootLocation = false;
			RootLocation = FVector::ZeroVector;
		}

		// Reset the rotation/scale to be identity otherwise if the root component transform has changed, the final transform will be wrong
		// Since this is technically 'moving' the ISM, we need to unregister it before moving otherwise we could get a warning that we're moving a component with static mobility
		ISMC->UnregisterComponent();
		ISMC->SetWorldTransform(FTransform(FQuat::Identity, RootLocation, FVector::OneVector));
		ISMC->RegisterComponent();
	}
}

void UPCGManagedISMComponent::MarkAsReused()
{
	Super::MarkAsReused();

	if (UInstancedStaticMeshComponent* ISMC = GetComponent())
	{
		// Reset the rotation/scale to be identity otherwise if the root component transform has changed, the final transform will be wrong
		FVector TentativeRootLocation = RootLocation;

		if (!bHasRootLocation)
		{
			if (USceneComponent* RootComponent = ISMC->GetAttachmentRoot())
			{
				TentativeRootLocation = RootComponent->GetComponentLocation();
			}
		}

		// Since this is technically 'moving' the ISM, we need to unregister it before moving otherwise we could get a warning that we're moving a component with static mobility
		ISMC->UnregisterComponent();
		ISMC->SetWorldTransform(FTransform(FQuat::Identity, TentativeRootLocation, FVector::OneVector));
		ISMC->RegisterComponent();
	}
}

void UPCGManagedISMComponent::SetRootLocation(const FVector& InRootLocation)
{
	bHasRootLocation = true;
	RootLocation = InRootLocation;
}

UInstancedStaticMeshComponent* UPCGManagedISMComponent::GetComponent() const
{
	if (!CachedRawComponentPtr)
	{
		UInstancedStaticMeshComponent* GeneratedComponentPtr = Cast<UInstancedStaticMeshComponent>(GeneratedComponent.Get());

		// Implementation note:
		// There is no surefire way to make sure that we can use the raw pointer UNLESS it is from the same owner
		if (GeneratedComponentPtr && Cast<UPCGComponent>(GetOuter()) && GeneratedComponentPtr->GetOwner() == Cast<UPCGComponent>(GetOuter())->GetOwner())
		{
			CachedRawComponentPtr = GeneratedComponentPtr;
		}

		return GeneratedComponentPtr;
	}

	return CachedRawComponentPtr;
}

void UPCGManagedISMComponent::SetComponent(UInstancedStaticMeshComponent* InComponent)
{
	GeneratedComponent = InComponent;
	CachedRawComponentPtr = InComponent;
}