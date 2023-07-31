// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSubsystem.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectComponent.h"
#include "SmartObjectCollection.h"
#include "EngineUtils.h"
#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectHashGrid.h"
#include "VisualLogger/VisualLogger.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSubsystem)

#if UE_ENABLE_DEBUG_DRAWING
#include "SmartObjectSubsystemRenderingActor.h"
#endif

#if WITH_SMARTOBJECT_DEBUG
#include "MassExecutor.h"
#endif

#if WITH_EDITOR
#include "Engine/LevelBounds.h"
#include "WorldPartition/WorldPartition.h"
#endif

namespace UE::SmartObject
{

// Indicates that runtime shouldn't be initialized.
// This flag must be set BEFORE launching the game and not toggled after.
bool bDisableRuntime = false;
FAutoConsoleVariableRef CVarDisableRuntime(
	TEXT("ai.smartobject.DisableRuntime"),
	bDisableRuntime,
	TEXT("If enabled, runtime instances won't be created for baked collection entries or runtime added ones from component registration."),
	ECVF_Default);

#if WITH_SMARTOBJECT_DEBUG
namespace Debug
{
	static FAutoConsoleCommandWithWorld RegisterAllSmartObjectsCmd
	(
		TEXT("ai.debug.so.RegisterAllSmartObjects"),
		TEXT("Force register all objects registered in the subsystem to simulate & debug runtime flows (will ignore already registered components)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](const UWorld* InWorld)
		{
			if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(InWorld))
			{
				Subsystem->DebugRegisterAllSmartObjects();
			}
		})
	);

	static FAutoConsoleCommandWithWorld UnregisterAllSmartObjectsCmd
	(
		TEXT("ai.debug.so.UnregisterAllSmartObjects"),
		TEXT("Force unregister all objects registered in the subsystem to simulate & debug runtime flows (will ignore already unregistered components)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](const UWorld* InWorld)
		{
			if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(InWorld))
			{
				Subsystem->DebugUnregisterAllSmartObjects();
			}
		})
	);
} // UE::SmartObject::Debug
#endif // WITH_SMARTOBJECT_DEBUG
} // UE::SmartObject

//----------------------------------------------------------------------//
// USmartObjectSubsystem
//----------------------------------------------------------------------//

void USmartObjectSubsystem::OnWorldComponentsUpdated(UWorld& World)
{
	// Load class required to instantiate the space partition structure
	UE_CVLOG_UELOG(!SpacePartitionClassName.IsValid(), this, LogSmartObject, Error, TEXT("A valid space partition class name is required."));
	if (SpacePartitionClassName.IsValid())
	{
		SpacePartitionClass = LoadClass<USmartObjectSpacePartition>(this, *SpacePartitionClassName.ToString());
		UE_CVLOG_UELOG(*SpacePartitionClass == nullptr, this, LogSmartObject, Error, TEXT("Unable to load class %s"), *SpacePartitionClassName.ToString());
	}

	// Class not specified or invalid, use some default
	if (SpacePartitionClass.Get() == nullptr)
	{
		SpacePartitionClassName = FSoftClassPath(USmartObjectHashGrid::StaticClass());
		SpacePartitionClass = USmartObjectHashGrid::StaticClass();
		UE_VLOG_UELOG(this, LogSmartObject, Warning, TEXT("Using default class %s"), *SpacePartitionClassName.ToString());
	}

#if UE_ENABLE_DEBUG_DRAWING
	// Spawn the rendering actor
	if (RenderingActor == nullptr)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RenderingActor = World.SpawnActor<ASmartObjectSubsystemRenderingActor>(SpawnInfo);
	}
#endif // UE_ENABLE_DEBUG_DRAWING

	// Register collections that were unable to register since they got loaded before the subsystem got created/initialized.
	RegisterCollectionInstances();

#if WITH_EDITOR
	SpawnMissingCollection();

	if (!World.IsGameWorld())
	{
		ComputeBounds(World, *MainCollection);
	}
#endif // WITH_EDITOR
}

USmartObjectSubsystem* USmartObjectSubsystem::GetCurrent(const UWorld* World)
{
	return UWorld::GetSubsystem<USmartObjectSubsystem>(World);
}

FSmartObjectRuntime* USmartObjectSubsystem::AddComponentToSimulation(USmartObjectComponent& SmartObjectComponent, const FSmartObjectCollectionEntry& NewEntry)
{
	checkf(SmartObjectComponent.GetDefinition() != nullptr, TEXT("Shouldn't reach this point with an invalid definition asset"));
	FSmartObjectRuntime* SmartObjectRuntime = AddCollectionEntryToSimulation(NewEntry, *SmartObjectComponent.GetDefinition());
	if (SmartObjectRuntime != nullptr)
	{
		SmartObjectComponent.OnRuntimeInstanceCreated(*SmartObjectRuntime);
	}
	return SmartObjectRuntime;
}

void USmartObjectSubsystem::BindComponentToSimulation(USmartObjectComponent& SmartObjectComponent)
{
	// Notify the component to bind to its runtime counterpart
	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SmartObjectComponent.GetRegisteredHandle());
	if (ensureMsgf(SmartObjectRuntime != nullptr, TEXT("Binding a component should only be used when an associated runtime instance exists.")))
	{
		SmartObjectComponent.OnRuntimeInstanceBound(*SmartObjectRuntime);
	}
}

void USmartObjectSubsystem::UnbindComponentFromSimulation(USmartObjectComponent& SmartObjectComponent)
{
	// Notify the component to unbind from its runtime counterpart
	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SmartObjectComponent.GetRegisteredHandle());
	if (ensureMsgf(SmartObjectRuntime != nullptr, TEXT("Unbinding a component should only be used when an associated runtime instance exists.")))
	{
		SmartObjectComponent.OnRuntimeInstanceUnbound(*SmartObjectRuntime);
	}
}

FSmartObjectRuntime* USmartObjectSubsystem::AddCollectionEntryToSimulation(const FSmartObjectCollectionEntry& Entry, const USmartObjectDefinition& Definition)
{
	const FSmartObjectHandle Handle = Entry.GetHandle();
	const FTransform& Transform = Entry.GetTransform();
	const FBox& Bounds = Entry.GetBounds();
	const FGameplayTagContainer& Tags = Entry.GetTags();

	if (!ensureMsgf(Handle.IsValid(), TEXT("SmartObject needs a valid Handle to be added to the simulation")))
	{
		return nullptr;
	}

	if (!ensureMsgf(RuntimeSmartObjects.Find(Handle) == nullptr, TEXT("Handle '%s' already registered in runtime simulation"), *LexToString(Handle)))
	{
		return nullptr;
	}

	if (!ensureMsgf(EntityManager, TEXT("Entity subsystem required to add a smartobject to the simulation")))
	{
		return nullptr;
	}

	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Adding SmartObject '%s' to runtime simulation."), *LexToString(Handle));

	FSmartObjectRuntime& Runtime = RuntimeSmartObjects.Emplace(Handle, FSmartObjectRuntime(Definition));
	Runtime.SetRegisteredHandle(Handle);
	Runtime.Tags = Tags;

#if UE_ENABLE_DEBUG_DRAWING
	Runtime.Bounds = Bounds;
#endif

	// Create runtime data and entity for each slot
	int32 SlotIndex = 0;
	for (const FSmartObjectSlotDefinition& SlotDefinition : Definition.GetSlots())
	{
		// Build our shared fragment
		FMassArchetypeSharedFragmentValues SharedFragmentValues;
		FConstSharedStruct& SharedFragment = EntityManager->GetOrCreateConstSharedFragment(FSmartObjectSlotDefinitionFragment(Definition, SlotDefinition));
		SharedFragmentValues.AddConstSharedFragment(SharedFragment);

		FSmartObjectSlotTransform TransformFragment;
		TOptional<FTransform> OptionalTransform = Definition.GetSlotTransform(Transform, FSmartObjectSlotIndex(SlotIndex));
		TransformFragment.SetTransform(OptionalTransform.Get(Transform));

		const FMassEntityHandle EntityHandle = EntityManager->ReserveEntity();
		EntityManager->Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments>(EntityHandle, MoveTemp(SharedFragmentValues), TransformFragment);

		FSmartObjectSlotHandle SlotHandle(EntityHandle);
		RuntimeSlotStates.Add(SlotHandle, FSmartObjectSlotClaimState());
		Runtime.SlotHandles[SlotIndex] = SlotHandle;
		SlotIndex++;
	}

	// For objects added to simulation after initial collection, we need to flush the command buffer
	if (bInitialCollectionAddedToSimulation)
	{
		// This is the temporary way to force our commands to be processed until MassEntitySubsystem
		// offers a threadsafe solution to push and flush commands in our own execution context.
		EntityManager->FlushCommands();
	}

	// Transfer spatial information to the runtime instance
	Runtime.SetTransform(Transform);

	// Insert to the spatial representation structure and store associated data
	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	Runtime.SpatialEntryData = SpacePartition->Add(Handle, Bounds);

	return &Runtime;
}

void USmartObjectSubsystem::RemoveRuntimeInstanceFromSimulation(const FSmartObjectHandle Handle)
{
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Removing SmartObject '%s' from runtime simulation."), *LexToString(Handle));

	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(Handle);
	if (!ensureMsgf(SmartObjectRuntime != nullptr, TEXT("RemoveFromSimulation is an internal call and should only be used for objects still part of the simulation")))
	{
		return;
	}

	if (!ensureMsgf(EntityManager, TEXT("Entity subsystem required to remove a smartobject from the simulation")))
	{
		return;
	}

	// Abort everything before removing since abort flow may require access to runtime data
	AbortAll(*SmartObjectRuntime, ESmartObjectSlotState::Free);

	// Remove from space partition
	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition->Remove(Handle, SmartObjectRuntime->SpatialEntryData);

	// Destroy entities associated to slots
	TArray<FMassEntityHandle> EntitiesToDestroy;
	EntitiesToDestroy.Reserve(SmartObjectRuntime->SlotHandles.Num());
	for (const FSmartObjectSlotHandle SlotHandle : SmartObjectRuntime->SlotHandles)
	{
		RuntimeSlotStates.Remove(SlotHandle);
		EntitiesToDestroy.Add(SlotHandle);
	}

	EntityManager->Defer().DestroyEntities(EntitiesToDestroy);

	// Remove object runtime data
	RuntimeSmartObjects.Remove(Handle);
}

void USmartObjectSubsystem::RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry)
{
	RemoveRuntimeInstanceFromSimulation(Entry.GetHandle());
}

void USmartObjectSubsystem::RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent)
{
	RemoveRuntimeInstanceFromSimulation(SmartObjectComponent.GetRegisteredHandle());
	SmartObjectComponent.OnRuntimeInstanceDestroyed();
}

void USmartObjectSubsystem::AbortAll(FSmartObjectRuntime& SmartObjectRuntime, const ESmartObjectSlotState NewState)
{
	for (const FSmartObjectSlotHandle SlotHandle : SmartObjectRuntime.SlotHandles)
	{
		FSmartObjectSlotClaimState& SlotState = RuntimeSlotStates.FindChecked(SlotHandle);
		switch (SlotState.State)
		{
		case ESmartObjectSlotState::Claimed:
		case ESmartObjectSlotState::Occupied:
			{
				const FSmartObjectClaimHandle ClaimHandle(SmartObjectRuntime.GetRegisteredHandle(), SlotHandle, SlotState.User);
				SlotState.Release(ClaimHandle, NewState, /* bAborted */ true);
				break;
			}
		case ESmartObjectSlotState::Disabled:
		case ESmartObjectSlotState::Free: // falling through on purpose
		default:
			SlotState.State = NewState;
			UE_CVLOG_UELOG(SlotState.User.IsValid(), this, LogSmartObject, Warning,
				TEXT("Smart object %s used by %s while the slot it's assigned to is not marked Claimed nor Occupied"),
				*LexToString(SmartObjectRuntime.GetDefinition()),
				*LexToString(SlotState.User));
			break;
		}
	}
}

bool USmartObjectSubsystem::RegisterSmartObject(USmartObjectComponent& SmartObjectComponent)
{
	if (SmartObjectComponent.GetDefinition() == nullptr)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Warning, TEXT("Attempting to register %s while its DefinitionAsset is not set. Bailing out."),
			*GetFullNameSafe(&SmartObjectComponent));
		return false;
	}

	if (!RegisteredSOComponents.Contains(&SmartObjectComponent))
	{
		return RegisterSmartObjectInternal(SmartObjectComponent);
	}
	
	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Failed to register %s. Already registered"),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	return false;
}

bool USmartObjectSubsystem::RegisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent)
{
	UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("Registering %s using definition %s."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	// Main collection may not assigned until world components are updated (active level set and actors registered)
	// In this case objects will be part of the loaded collection or collection will be rebuilt from registered components
	if (IsValid(MainCollection))
	{
		const UWorld& World = GetWorldRef();
		bool bAddToCollection = true;

#if WITH_EDITOR
		if (!World.IsGameWorld())
		{
			// For collections not built automatically we wait an explicit build request to clear and repopulate
			if (MainCollection->ShouldBuildCollectionAutomatically() == false)
			{
				bAddToCollection = false;
				UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("%s not added to collection that is built on demand only."), *GetNameSafe(SmartObjectComponent.GetOwner()));
			}
			// For partition world we don't alter the collection unless we are explicitly building the collection
			else if(World.IsPartitionedWorld() && !MainCollection->IsBuildingForWorldPartition())
			{
				bAddToCollection = false;
				UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("%s not added to collection that is owned by partitioned world."), *GetNameSafe(SmartObjectComponent.GetOwner()));
			}
		}
#endif // WITH_EDITOR

		if (bAddToCollection)
		{
			bool bAlreadyInCollection = false;
			const FSmartObjectCollectionEntry* Entry = MainCollection->AddSmartObject(SmartObjectComponent, bAlreadyInCollection);

#if WITH_EDITOR
			if (Entry != nullptr && !bAlreadyInCollection)
			{
				OnMainCollectionDirtied.Broadcast();
			}
#endif

			// At runtime we only consider registrations after collection was pushed to the simulation. All existing entries were added on WorldBeginPlay
			if (World.IsGameWorld() && bInitialCollectionAddedToSimulation && Entry != nullptr)
			{
				if (bAlreadyInCollection)
				{
					// Simply bind the newly available component to its active runtime instance
					BindComponentToSimulation(SmartObjectComponent);
				}
				else
				{
					// This is a new entry added after runtime initialization, mark it as a runtime entry (lifetime is tied to the component)
					AddComponentToSimulation(SmartObjectComponent, *Entry);
					RuntimeCreatedEntries.Add(SmartObjectComponent.GetRegisteredHandle());
				}
			}
		}

		ensureMsgf(RegisteredSOComponents.Find(&SmartObjectComponent) == INDEX_NONE
			, TEXT("Adding %s to RegisteredSOColleciton, but it has already been added. Missing unregister call?"), *SmartObjectComponent.GetName());
		RegisteredSOComponents.Add(&SmartObjectComponent);
	}
	else
	{
		if (bInitialCollectionAddedToSimulation)
		{
			// Report error regarding missing Collection only when SmartObjects are registered (i.e. Collection is required)
			UE_VLOG_UELOG(this, LogSmartObject, Error,
				TEXT("%s not added to collection since Main Collection doesn't exist in world '%s'. You need to open and save that world to create the missing collection."),
				*GetNameSafe(SmartObjectComponent.GetOwner()),
				*GetWorldRef().GetFullName());
		}
		else
		{
			UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("%s not added to collection since Main Collection is not set yet. Storing SOComponent instance for registration once a collection is set."), *GetNameSafe(SmartObjectComponent.GetOwner()));	
			PendingSmartObjectRegistration.Add(&SmartObjectComponent);
		}
	}

	return true;
}

bool USmartObjectSubsystem::UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent)
{
	if (RegisteredSOComponents.Contains(&SmartObjectComponent))
	{
		return UnregisterSmartObjectInternal(SmartObjectComponent, ESmartObjectUnregistrationMode::DestroyRuntimeInstance);
	}

	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Failed to unregister %s. Already unregistered"),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	return false;
}

bool USmartObjectSubsystem::UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const ESmartObjectUnregistrationMode UnregistrationMode)
{
	UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("Unregistering %s using definition %s."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	if (IsValid(MainCollection))
	{
		const UWorld& World = GetWorldRef();
		bool bRemoveFromCollection = true;

#if WITH_EDITOR
		if (!World.IsGameWorld())
		{
			// For collections not built automatically we wait an explicit build request to clear and repopulate
			if (MainCollection->ShouldBuildCollectionAutomatically() == false)
			{
				bRemoveFromCollection = false;
				UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("%s not removed from collection that is built on demand only."), *GetNameSafe(SmartObjectComponent.GetOwner()));
			}
			// For partition world we never remove from the collection since it is built incrementally
			else if (World.IsPartitionedWorld())
			{
				bRemoveFromCollection = false;
				UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("%s not removed from collection that is owned by partitioned world."), *GetNameSafe(SmartObjectComponent.GetOwner()));
			}
		}
#endif // WITH_EDITOR

		// At runtime, only entries created outside the initial collection are removed from simulation and collection
		if (World.IsGameWorld()
			&& bInitialCollectionAddedToSimulation
			&& SmartObjectComponent.GetRegisteredHandle().IsValid()) // Make sure component was registered to simulation (e.g. Valid associated definition)
		{
			bRemoveFromCollection = RuntimeCreatedEntries.Remove(SmartObjectComponent.GetRegisteredHandle()) != 0
				|| UnregistrationMode == ESmartObjectUnregistrationMode::DestroyRuntimeInstance;
			if (bRemoveFromCollection)
			{
				RemoveComponentFromSimulation(SmartObjectComponent);
			}
			else
			{
				// Unbind the component from its associated runtime instance
				UnbindComponentFromSimulation(SmartObjectComponent);
			}
		}

		if (bRemoveFromCollection)
		{
			MainCollection->RemoveSmartObject(SmartObjectComponent);
		}
	}

	RegisteredSOComponents.Remove(&SmartObjectComponent);

	return true;
}

bool USmartObjectSubsystem::RegisterSmartObjectActor(const AActor& SmartObjectActor)
{
	TArray<USmartObjectComponent*> Components;
	SmartObjectActor.GetComponents(Components);
	UE_CVLOG_UELOG(Components.Num() == 0, &SmartObjectActor, LogSmartObject, Log,
		TEXT("Failed to register SmartObject components for %s. No components found."), *SmartObjectActor.GetName());

	int32 NumSuccess = 0;
	for (USmartObjectComponent* SOComponent : Components)
	{
		if (RegisterSmartObject(*SOComponent))
		{
			NumSuccess++;
		}
	}
	return NumSuccess > 0 && NumSuccess == Components.Num();
}

bool USmartObjectSubsystem::UnregisterSmartObjectActor(const AActor& SmartObjectActor)
{
	TArray<USmartObjectComponent*> Components;
	SmartObjectActor.GetComponents(Components);
	UE_CVLOG_UELOG(Components.Num() == 0, &SmartObjectActor, LogSmartObject, Log,
		TEXT("Failed to unregister SmartObject components for %s. No components found."), *SmartObjectActor.GetName());

	int32 NumSuccess = 0;
	for (USmartObjectComponent* SOComponent : Components)
	{
		if (UnregisterSmartObject(*SOComponent))
		{
			NumSuccess++;
		}
	}
	return NumSuccess > 0 && NumSuccess == Components.Num();
}

FSmartObjectClaimHandle USmartObjectSubsystem::Claim(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__));
	if (SmartObjectRuntime == nullptr)
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	TArray<FSmartObjectSlotHandle> SlotHandles;
	FindSlots(*SmartObjectRuntime, Filter, SlotHandles);
	if (SlotHandles.IsEmpty())
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	return Claim(Handle, SlotHandles.Top());
}

FSmartObjectClaimHandle USmartObjectSubsystem::Claim(const FSmartObjectHandle Handle, const FSmartObjectSlotHandle SlotHandle)
{
	if (!Handle.IsValid())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Claiming using an unset smart object handle. Returning invalid FSmartObjectClaimHandle."));
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	if (!IsSlotValidVerbose(SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	// Call to IsSlotValid should guarantee availability of the slot state.
	FSmartObjectSlotClaimState& SlotState = RuntimeSlotStates.FindChecked(SlotHandle);

	const FSmartObjectUserHandle User(NextFreeUserID++);
	const bool bClaimed = SlotState.Claim(User);

	const FSmartObjectClaimHandle ClaimHandle(Handle, SlotHandle, User);
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Claim %s for handle %s. Slot State is '%s'"),
		bClaimed ? TEXT("SUCCEEDED") : TEXT("FAILED"),
		*LexToString(ClaimHandle),
	*UEnum::GetValueAsString(SlotState.GetState()));
	UE_CVLOG_LOCATION(bClaimed, this, LogSmartObject, Display, GetSlotLocation(ClaimHandle).GetValue(), 50.f, FColor::Yellow, TEXT("Claim"));

	if (bClaimed)
	{
		return ClaimHandle;
	}

	return FSmartObjectClaimHandle::InvalidHandle;
}

bool USmartObjectSubsystem::CanBeClaimed(FSmartObjectSlotHandle SlotHandle) const
{
	if (!IsSlotValidVerbose(SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return false;
	}

	// Call to IsSlotValid should guarantee availability of the slot state.
	const FSmartObjectSlotClaimState& SlotState = RuntimeSlotStates.FindChecked(SlotHandle);
	return SlotState.CanBeClaimed();
}

bool USmartObjectSubsystem::IsSmartObjectValid(const FSmartObjectHandle SmartObjectHandle) const
{
	return SmartObjectHandle.IsValid() && RuntimeSmartObjects.Find(SmartObjectHandle) != nullptr;
}

bool USmartObjectSubsystem::IsClaimedSmartObjectValid(const FSmartObjectClaimHandle& ClaimHandle) const
{
	return ClaimHandle.IsValid() && RuntimeSmartObjects.Find(ClaimHandle.SmartObjectHandle) != nullptr;
}

bool USmartObjectSubsystem::IsSlotValidVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext) const
{
	UE_CVLOG_UELOG(!SlotHandle.IsValid(), this, LogSmartObject, Log,
		TEXT("%s failed. SlotHandle is not set."), LogContext);
	UE_CVLOG_UELOG(SlotHandle.IsValid() && RuntimeSlotStates.Find(SlotHandle) == nullptr, this, LogSmartObject, Log,
		TEXT("%s failed using handle '%s'. Slot is no longer part of the simulation."), LogContext, *LexToString(SlotHandle));

	return IsSmartObjectSlotValid(SlotHandle);
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinition(const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(ClaimHandle.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? GetBehaviorDefinition(*SmartObjectRuntime, ClaimHandle.SlotHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinitionByRequestResult(const FSmartObjectRequestResult& RequestResult, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(RequestResult.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? GetBehaviorDefinition(*SmartObjectRuntime, RequestResult.SlotHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinition(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectSlotHandle SlotHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	const USmartObjectDefinition& Definition = SmartObjectRuntime.GetDefinition();

	const FSmartObjectSlotIndex SlotIndex(SmartObjectRuntime.SlotHandles.IndexOfByKey(SlotHandle));
	return Definition.GetBehaviorDefinition(SlotIndex, DefinitionClass);
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::Use(const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(ClaimHandle.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? Use(*SmartObjectRuntime, ClaimHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::Use(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	checkf(ClaimHandle.IsValid(), TEXT("This is an internal method that should only be called with an assigned claim handle"));

	if (SmartObjectRuntime.IsDisabled())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Can't Use handle %s since associated object is disabled."), *LexToString(ClaimHandle));
		return nullptr;
	}

	const USmartObjectBehaviorDefinition* BehaviorDefinition = GetBehaviorDefinition(SmartObjectRuntime, ClaimHandle.SlotHandle, DefinitionClass);
	if (BehaviorDefinition == nullptr)
	{
		const UClass* ClassPtr = DefinitionClass.Get();
		UE_VLOG_UELOG(this, LogSmartObject, Warning, TEXT("Unable to find a behavior definition of type %s in %s"),
			ClassPtr != nullptr ? *ClassPtr->GetName(): TEXT("Null"), *LexToString(SmartObjectRuntime.GetDefinition()));
		return nullptr;
	}

	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Start using handle %s"), *LexToString(ClaimHandle));
	UE_VLOG_LOCATION(this, LogSmartObject, Display, SmartObjectRuntime.GetTransform().GetLocation(), 50.f, FColor::Green, TEXT("Use"));

	FSmartObjectSlotClaimState& SlotState = RuntimeSlotStates.FindChecked(ClaimHandle.SlotHandle);

	if (ensureMsgf(SlotState.GetState() == ESmartObjectSlotState::Claimed, TEXT("Should have been claimed first: %s"), *LexToString(ClaimHandle)) &&
		ensureMsgf(SlotState.User == ClaimHandle.UserHandle, TEXT("Attempt to use slot %s from handle %s but already assigned to %s"),
			*LexToString(SlotState), *LexToString(ClaimHandle), *LexToString(SlotState.User)))
	{
		SlotState.State = ESmartObjectSlotState::Occupied;
		return BehaviorDefinition;
	}

	return nullptr;
}

bool USmartObjectSubsystem::Release(const FSmartObjectClaimHandle& ClaimHandle)
{
	if (!IsSlotValidVerbose(ClaimHandle.SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return false;
	}

	// Call to IsSlotValid should guarantee availability of the slot state.
	FSmartObjectSlotClaimState& SlotState = RuntimeSlotStates.FindChecked(ClaimHandle.SlotHandle);
	if (SlotState.GetState() == ESmartObjectSlotState::Disabled)
	{
		// We don't consider this case as an error since the user is simply attempting to release a slot
		// that was disabled internally.
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Skipped Release using claim handle '%s'. "
			"Slot was disabled. Consider using RegisterSlotInvalidationCallback to get notification when a slot gets invalidated."), *LexToString(ClaimHandle));
		return false;
	}

	const bool bSuccess = SlotState.Release(ClaimHandle, ESmartObjectSlotState::Free, /*bAborted*/ false);
	UE_CVLOG_UELOG(bSuccess, this, LogSmartObject, Verbose, TEXT("Released using handle %s"), *LexToString(ClaimHandle));
	UE_CVLOG_LOCATION(bSuccess, this, LogSmartObject, Display, GetSlotLocation(ClaimHandle).GetValue(), 50.f, FColor::Red, TEXT("Release"));
	return bSuccess;
}

ESmartObjectSlotState USmartObjectSubsystem::GetSlotState(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectSlotClaimState* SlotState = RuntimeSlotStates.Find(SlotHandle);
	return SlotState != nullptr ? SlotState->GetState() : ESmartObjectSlotState::Invalid;
}

bool USmartObjectSubsystem::GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle, FVector& OutSlotLocation) const
{
	const TOptional<FVector> OptionalLocation = GetSlotLocation(ClaimHandle);
	OutSlotLocation = OptionalLocation.Get(FVector::ZeroVector);
	return OptionalLocation.IsSet();
}

TOptional<FVector> USmartObjectSubsystem::GetSlotLocation(const FSmartObjectSlotHandle SlotHandle) const
{
	TOptional<FTransform> Transform = GetSlotTransform(SlotHandle);
	return (Transform.IsSet() ? Transform.GetValue().GetLocation() : TOptional<FVector>());
}

bool USmartObjectSubsystem::GetSlotTransform(const FSmartObjectClaimHandle& ClaimHandle, FTransform& OutSlotTransform) const
{
	const TOptional<FTransform> OptionalTransform = GetSlotTransform(ClaimHandle);
	OutSlotTransform = OptionalTransform.Get(FTransform::Identity);
	return OptionalTransform.IsSet();
}

bool USmartObjectSubsystem::GetSlotTransformFromRequestResult(const FSmartObjectRequestResult& RequestResult, FTransform& OutSlotTransform) const
{
	const TOptional<FTransform> OptionalTransform = GetSlotTransform(RequestResult);
	OutSlotTransform = OptionalTransform.Get(FTransform::Identity);
	return OptionalTransform.IsSet();
}

TOptional<FTransform> USmartObjectSubsystem::GetSlotTransform(const FSmartObjectSlotHandle SlotHandle) const
{
	TOptional<FTransform> Transform;

	if (IsSlotValidVerbose(SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		if (ensureMsgf(EntityManager, TEXT("Entity subsystem required to retrieve slot transform")))
		{
			const FSmartObjectSlotView View(*EntityManager.Get(), SlotHandle);
			const FSmartObjectSlotTransform& SlotTransform = View.GetStateData<FSmartObjectSlotTransform>();
			Transform = SlotTransform.GetTransform();
		}
	}

	return Transform;
}

const FTransform& USmartObjectSubsystem::GetSlotTransformChecked(const FSmartObjectSlotHandle SlotHandle) const
{
	check(EntityManager);
	const FSmartObjectSlotView View(*EntityManager.Get(), SlotHandle);
	const FSmartObjectSlotTransform& SlotTransform = View.GetStateData<FSmartObjectSlotTransform>();
	return SlotTransform.GetTransform();
}

FSmartObjectRuntime* USmartObjectSubsystem::GetValidatedMutableRuntime(const FSmartObjectHandle Handle, const TCHAR* Context)
{
	return const_cast<FSmartObjectRuntime*>(GetValidatedRuntime(Handle, Context));
}

const FSmartObjectRuntime* USmartObjectSubsystem::GetValidatedRuntime(const FSmartObjectHandle Handle, const TCHAR* Context) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(Handle);
	UE_CVLOG_UELOG(!Handle.IsValid(), this, LogSmartObject, Log, TEXT("%s failed. Handle is not set."), Context);
	UE_CVLOG_UELOG(Handle.IsValid() && SmartObjectRuntime == nullptr, this, LogSmartObject, Log,
		TEXT("%s failed using handle '%s'. SmartObject is no longer part of the simulation."), Context, *LexToString(Handle));

	return SmartObjectRuntime;
}

const FGameplayTagContainer& USmartObjectSubsystem::GetInstanceTags(const FSmartObjectHandle Handle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? SmartObjectRuntime->GetTags() : FGameplayTagContainer::EmptyContainer;
}

void USmartObjectSubsystem::AddTagToInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag)
{
	if (FSmartObjectRuntime* SmartObjectRuntime = GetValidatedMutableRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		AddTagToInstance(*SmartObjectRuntime, Tag);
	}
}

void USmartObjectSubsystem::RemoveTagFromInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag)
{
	if (FSmartObjectRuntime* SmartObjectRuntime = GetValidatedMutableRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		RemoveTagFromInstance(*SmartObjectRuntime, Tag);
	}
}

void USmartObjectSubsystem::AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
{
	if (!SmartObjectRuntime.Tags.HasTag(Tag))
	{
		SmartObjectRuntime.Tags.AddTagFast(Tag);
		SmartObjectRuntime.OnTagChangedDelegate.ExecuteIfBound(Tag, 1);
		UpdateRuntimeInstanceStatus(SmartObjectRuntime);
	}
}

void USmartObjectSubsystem::RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
{
	if (SmartObjectRuntime.Tags.RemoveTag(Tag))
	{
		SmartObjectRuntime.OnTagChangedDelegate.ExecuteIfBound(Tag, 0);
		UpdateRuntimeInstanceStatus(SmartObjectRuntime);
	}
}

void USmartObjectSubsystem::UpdateRuntimeInstanceStatus(FSmartObjectRuntime& SmartObjectRuntime)
{
	const FGameplayTagQuery& Requirements = SmartObjectRuntime.GetDefinition().GetObjectTagFilter();
	const bool bDisabledByTags = !(Requirements.IsEmpty() || Requirements.Matches(SmartObjectRuntime.Tags));

	if (SmartObjectRuntime.bDisabledByTags != bDisabledByTags)
	{
		if (bDisabledByTags)
		{
			// Newly disabled so abort any active interaction (keep registered in space partition but filter results)
			AbortAll(SmartObjectRuntime, ESmartObjectSlotState::Disabled);
		}
		else
		{
			// Restore slots availability
			for (const FSmartObjectSlotHandle SlotHandle : SmartObjectRuntime.SlotHandles)
			{
				RuntimeSlotStates.FindChecked(SlotHandle).State = ESmartObjectSlotState::Free;
			}
		}

		SmartObjectRuntime.bDisabledByTags = bDisabledByTags;
	}
}

FSmartObjectSlotClaimState* USmartObjectSubsystem::GetMutableSlotState(const FSmartObjectClaimHandle& ClaimHandle)
{
	return RuntimeSlotStates.Find(ClaimHandle.SlotHandle);
}

void USmartObjectSubsystem::RegisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle, const FOnSlotInvalidated& Callback)
{
	FSmartObjectSlotClaimState* Slot = GetMutableSlotState(ClaimHandle);
	if (Slot != nullptr)
	{
		Slot->OnSlotInvalidatedDelegate = Callback;
	}
}

void USmartObjectSubsystem::UnregisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle)
{
	FSmartObjectSlotClaimState* Slot = GetMutableSlotState(ClaimHandle);
	if (Slot != nullptr)
	{
		Slot->OnSlotInvalidatedDelegate.Unbind();
	}
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectSubsystem::DebugDraw(FDebugRenderSceneProxy* DebugProxy) const
{
	if (!bInitialCollectionAddedToSimulation)
	{
		return;
	}

	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition->Draw(DebugProxy);

	for (auto It(RuntimeSmartObjects.CreateConstIterator()); It; ++It)
	{
		const FSmartObjectRuntime& Runtime = It.Value();
		DebugProxy->Boxes.Emplace(Runtime.Bounds, GColorList.Blue);
	}
}
#endif // UE_ENABLE_DEBUG_DRAWING

void USmartObjectSubsystem::AddSlotDataDeferred(const FSmartObjectClaimHandle& ClaimHandle, const FConstStructView InData) const
{
	if (IsSlotValidVerbose(ClaimHandle.SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		if (ensureMsgf(EntityManager, TEXT("Entity subsystem required to add slot data")) &&
			ensureMsgf(InData.GetScriptStruct()->IsChildOf(FSmartObjectSlotStateData::StaticStruct()),
				TEXT("Given struct doesn't represent a valid runtime data type. Make sure to inherit from FSmartObjectSlotState or one of its child-types.")))
		{
			EntityManager->Defer().PushCommand<FMassDeferredAddCommand>(
			[EntityHandle = FMassEntityHandle(ClaimHandle.SlotHandle), DataView = InData](FMassEntityManager& System)
			{
				FInstancedStruct Struct = DataView;
				System.AddFragmentInstanceListToEntity(EntityHandle, MakeArrayView(&Struct, 1));
			});
		}
	}
}

FSmartObjectSlotView USmartObjectSubsystem::GetSlotView(const FSmartObjectSlotHandle SlotHandle) const
{
	if (IsSlotValidVerbose(SlotHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		if (ensureMsgf(EntityManager, TEXT("Entity subsystem required to create slot view")))
		{
			return FSmartObjectSlotView(*EntityManager.Get(), SlotHandle);
		}
	}

	return FSmartObjectSlotView();
}

void USmartObjectSubsystem::FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutSlots) const
{
	if (const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		FindSlots(*SmartObjectRuntime, Filter, OutSlots);
	}
}

void USmartObjectSubsystem::FindSlots(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutResults) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SmartObject_FilterSlots");

	// Use the high level flag, no need to dig into each slot state since they are also all disabled.
	if (SmartObjectRuntime.IsDisabled())
	{
		return;
	}

	const USmartObjectDefinition& Definition = SmartObjectRuntime.GetDefinition();
	const int32 NumSlots = Definition.GetSlots().Num();
	checkf(NumSlots > 0, TEXT("Definition should contain slot definitions at this point"));
	checkf(SmartObjectRuntime.SlotHandles.Num() == NumSlots, TEXT("Number of runtime slot handles should match number of slot definitions"));

	// Applying caller's predicate
	if (Filter.Predicate && !Filter.Predicate(SmartObjectRuntime.GetRegisteredHandle()))
	{
		return;
	}

	// Apply predicate on runtime instance tags (affecting all slots and not affected by the filtering policy)
	if (!Definition.GetObjectTagFilter().IsEmpty()
		&& !Definition.GetObjectTagFilter().Matches(SmartObjectRuntime.GetTags()))
	{
		return;
	}

	// Apply definition level filtering (Tags and BehaviorDefinition)
	// This could be improved to cache results between a single query against multiple instances of the same definition
	TArray<int32> ValidSlotIndices;
	FindMatchingSlotDefinitionIndices(Definition, Filter, ValidSlotIndices);

	// Build list of available slot indices (filter out occupied or reserved slots)
	for (const int32 SlotIndex : ValidSlotIndices)
	{
		if (RuntimeSlotStates.FindChecked(SmartObjectRuntime.SlotHandles[SlotIndex]).State == ESmartObjectSlotState::Free)
		{
			OutResults.Add(SmartObjectRuntime.SlotHandles[SlotIndex]);
		}
	}
}

void USmartObjectSubsystem::FindMatchingSlotDefinitionIndices(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices)
{
	const ESmartObjectTagFilteringPolicy UserTagsFilteringPolicy = Definition.GetUserTagsFilteringPolicy();

	// Define our Tags filtering predicate
	auto MatchesTagQueryFunc = [](const FGameplayTagQuery& Query, const FGameplayTagContainer& Tags){ return Query.IsEmpty() || Query.Matches(Tags); };

	// When filter policy is to use combined we can validate the user tag query of the parent object first
	// since they can't be merge so we need to apply them one after the other.
	// For activity requirements we have to merge parent and slot tags together before testing.
	if (UserTagsFilteringPolicy == ESmartObjectTagFilteringPolicy::Combine
		&& !MatchesTagQueryFunc(Definition.GetUserTagFilter(), Filter.UserTags))
	{
		return;
	}

	// Apply filter to individual slots
	const TConstArrayView<FSmartObjectSlotDefinition> SlotDefinitions = Definition.GetSlots();
	OutValidIndices.Reserve(SlotDefinitions.Num());
	for (int i = 0; i < SlotDefinitions.Num(); ++i)
	{
		const FSmartObjectSlotDefinition& Slot = SlotDefinitions[i];

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// (Deprecated property handling) Filter out mismatching behavior type (if specified)
		if (Filter.BehaviorDefinitionClass != nullptr
			&& Definition.GetBehaviorDefinition(FSmartObjectSlotIndex(i), Filter.BehaviorDefinitionClass) == nullptr)
		{
			continue;
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Filter out mismatching behavior type (if specified)
		if (!Filter.BehaviorDefinitionClasses.IsEmpty())
		{
			bool bMatchesAny = false;
			for (const TSubclassOf<USmartObjectBehaviorDefinition>& BehaviorDefinitionClass : Filter.BehaviorDefinitionClasses)
			{
				if (Definition.GetBehaviorDefinition(FSmartObjectSlotIndex(i), BehaviorDefinitionClass) != nullptr)
				{
					bMatchesAny = true;
					break;
				}
			}
			
			if (!bMatchesAny)
			{
				continue;
			}
		}

		// Filter out slots based on their activity tags
		FGameplayTagContainer ActivityTags;
		Definition.GetSlotActivityTags(Slot, ActivityTags);
		if (!MatchesTagQueryFunc(Filter.ActivityRequirements, ActivityTags))
		{
			continue;
		}

		// Filter out slots based on their TagQuery applied on provided User Tags
		//  - override: we only run query from the slot if provided otherwise we run the one from the parent object
		//  - combine: we run slot query (parent query was applied before processing individual slots)
		if (UserTagsFilteringPolicy == ESmartObjectTagFilteringPolicy::Combine
			&& !MatchesTagQueryFunc(Slot.UserTagFilter, Filter.UserTags))
		{
			continue;
		}

		if (UserTagsFilteringPolicy == ESmartObjectTagFilteringPolicy::Override
			&& !MatchesTagQueryFunc((Slot.UserTagFilter.IsEmpty() ? Definition.GetUserTagFilter() : Slot.UserTagFilter), Filter.UserTags))
		{
			continue;
		}

		OutValidIndices.Add(i);
	}
}

FSmartObjectRequestResult USmartObjectSubsystem::FindSmartObject(const FSmartObjectRequest& Request) const
{
	TArray<FSmartObjectRequestResult> Results;
	FindSmartObjects(Request, Results);

	return Results.Num() ? Results.Top() : FSmartObjectRequestResult();
}

bool USmartObjectSubsystem::FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SmartObject_FindAllResults");

	if (!bInitialCollectionAddedToSimulation)
	{
		// Do not report warning if runtime was explicitly disabled by CVar
		UE_CVLOG_UELOG(!UE::SmartObject::bDisableRuntime, this, LogSmartObject, Warning,
			TEXT("Can't find smart objet before runtime gets initialized (i.e. InitializeRuntime gets called)."));
		return false;
	}

	const FSmartObjectRequestFilter& Filter = Request.Filter;
	TArray<FSmartObjectHandle> QueryResults;

	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition->Find(Request.QueryBox, QueryResults);

	for (const FSmartObjectHandle SmartObjectHandle : QueryResults)
	{
		const FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SmartObjectHandle);
		checkf(SmartObjectRuntime != nullptr, TEXT("Results returned by the space partition are expected to be valid."));

		if (!Request.QueryBox.IsInside(SmartObjectRuntime->GetTransform().GetLocation()))
		{
			continue;
		}

		TArray<FSmartObjectSlotHandle> SlotHandles;
		FindSlots(*SmartObjectRuntime, Filter, SlotHandles);
		OutResults.Reserve(OutResults.Num() + SlotHandles.Num());
		for (FSmartObjectSlotHandle SlotHandle: SlotHandles)
		{
			OutResults.Emplace(SmartObjectHandle, SlotHandle);
		}
	}

	return (OutResults.Num() > 0);
}

void USmartObjectSubsystem::RegisterCollectionInstances()
{
	for (TActorIterator<ASmartObjectCollection> It(GetWorld()); It; ++It)
	{
		ASmartObjectCollection* Collection = (*It);
		if (IsValid(Collection) && Collection->IsRegistered() == false)
		{
			const ESmartObjectCollectionRegistrationResult Result = RegisterCollection(*Collection);
			UE_VLOG_UELOG(Collection, LogSmartObject, Log, TEXT("Collection '%s' registration from USmartObjectSubsystem initialization - %s"), *Collection->GetName(), *UEnum::GetValueAsString(Result));
		}
	}
}

ESmartObjectCollectionRegistrationResult USmartObjectSubsystem::RegisterCollection(ASmartObjectCollection& InCollection)
{
	if (!IsValid(&InCollection))
	{
		return ESmartObjectCollectionRegistrationResult::Failed_InvalidCollection;
	}

	if (InCollection.IsRegistered())
	{
		UE_VLOG_UELOG(&InCollection, LogSmartObject, Error, TEXT("Trying to register collection '%s' more than once"), *InCollection.GetName());
		return ESmartObjectCollectionRegistrationResult::Failed_AlreadyRegistered;
	}

	ESmartObjectCollectionRegistrationResult Result = ESmartObjectCollectionRegistrationResult::Succeeded;
	if (InCollection.GetLevel()->IsPersistentLevel())
	{
		ensureMsgf(!IsValid(MainCollection), TEXT("Not expecting to set the main collection more than once"));
		UE_VLOG_UELOG(&InCollection, LogSmartObject, Log, TEXT("Main collection '%s' registered with %d entries"), *InCollection.GetName(), InCollection.GetEntries().Num());
		MainCollection = &InCollection;

		for (TObjectPtr<USmartObjectComponent>& SOComponent : PendingSmartObjectRegistration)
		{
			// ensure the SOComponent is still valid - things could have happened to it between adding to PendingSmartObjectRegistration and it beind processed here
			if (SOComponent && IsValid(SOComponent))
			{
				RegisterSmartObject(*SOComponent);
			}
		}
		PendingSmartObjectRegistration.Empty();

#if WITH_EDITOR
		// For a collection that is automatically updated, it gets rebuilt on registration in the Edition world.
		const UWorld& World = GetWorldRef();
		if (!World.IsGameWorld() &&
			!World.IsPartitionedWorld() &&
			MainCollection->ShouldBuildCollectionAutomatically())
		{
			RebuildCollection(InCollection);
		}

		// Broadcast after rebuilding so listeners will be able to access up-to-date data
		OnMainCollectionChanged.Broadcast();
#endif // WITH_EDITOR

		InCollection.OnRegistered();
		Result = ESmartObjectCollectionRegistrationResult::Succeeded;
	}
	else
	{
		InCollection.Destroy();
		Result = ESmartObjectCollectionRegistrationResult::Failed_NotFromPersistentLevel;
	}

	return Result;
}

void USmartObjectSubsystem::UnregisterCollection(ASmartObjectCollection& InCollection)
{
	if (MainCollection != &InCollection)
	{
		UE_VLOG_UELOG(&InCollection, LogSmartObject, Verbose, TEXT("Ignoring unregistration of collection '%s' since this is not the main collection."), *InCollection.GetName());
		return;
	}

	if (bInitialCollectionAddedToSimulation)
	{
		CleanupRuntime();
	}

	MainCollection = nullptr;
	InCollection.OnUnregistered();
}

USmartObjectComponent* USmartObjectSubsystem::GetSmartObjectComponent(const FSmartObjectClaimHandle& ClaimHandle) const
{
	return (IsValid(MainCollection) ? MainCollection->GetSmartObjectComponent(ClaimHandle.SmartObjectHandle) : nullptr);
}

USmartObjectComponent* USmartObjectSubsystem::GetSmartObjectComponentByRequestResult(const FSmartObjectRequestResult& Result) const
{
	return (IsValid(MainCollection) ? MainCollection->GetSmartObjectComponent(Result.SmartObjectHandle) : nullptr);
}

void USmartObjectSubsystem::InitializeRuntime()
{
	const UWorld& World = GetWorldRef();
	UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();

	if (!ensureMsgf(EntitySubsystem != nullptr, TEXT("Entity subsystem required to use SmartObjects")))
	{
		return;
	}
	EntityManager = EntitySubsystem->GetMutableEntityManager().AsShared();

	if (UE::SmartObject::bDisableRuntime)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Runtime explicitly disabled by CVar. Initialization skipped in %s."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (!IsValid(MainCollection))
	{
		if (MainCollection != nullptr && !MainCollection->bNetLoadOnClient && World.IsNetMode(NM_Client))
		{
			UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Collection not loaded on client. Initialization skipped in %s."), ANSI_TO_TCHAR(__FUNCTION__));
		}
		else
		{
			// Report error regarding missing Collection only when SmartObjects are present in the world (i.e. Collection is required)
			UE_CVLOG_UELOG(RegisteredSOComponents.Num(), this, LogSmartObject, Error,
				TEXT("Missing collection in world '%s' during %s. You need to open and save that world to create the missing collection."),
				*GetWorldRef().GetFullName(),
				ANSI_TO_TCHAR(__FUNCTION__));
		}

		return;
	}

	// Initialize spatial representation structure
	checkfSlow(*SpacePartitionClass != nullptr, TEXT("Partition class is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition = NewObject<USmartObjectSpacePartition>(this, SpacePartitionClass);
	SpacePartition->SetBounds(MainCollection->GetBounds());

	// Build all runtime from collection
	// Perform all validations at once since multiple entries can share the same definition
	MainCollection->ValidateDefinitions();

	for (const FSmartObjectCollectionEntry& Entry : MainCollection->GetEntries())
	{
		const USmartObjectDefinition* Definition = MainCollection->GetDefinitionForEntry(Entry);
		USmartObjectComponent* Component = Entry.GetComponent();

		if (Definition == nullptr || Definition->IsValid() == false)
		{
			UE_CVLOG_UELOG(Component != nullptr, Component->GetOwner(), LogSmartObject, Error,
				TEXT("Skipped runtime data creation for SmartObject %s: Invalid definition"), *GetNameSafe(Component->GetOwner()));
			continue;
		}

		if (Component != nullptr)
		{
			// When component is available we add it to the simulation along with its collection entry to create the runtime instance and bound them together.
			Component->SetRegisteredHandle(Entry.GetHandle());
			AddComponentToSimulation(*Component, Entry);
		}
		else
		{
			// Otherwise we create the runtime instance based on the information from the collection and component will be bound later (e.g. on load)
			AddCollectionEntryToSimulation(Entry, *Definition);
		}
	}

	// Until this point all runtime entries were created from the collection, start tracking newly created
	RuntimeCreatedEntries.Reset();

	// Note that we use our own flag instead of relying on World.HasBegunPlay() since world might not be marked
	// as BegunPlay immediately after subsystem OnWorldBeingPlay gets called (e.g. waiting game mode to be ready on clients)
	bInitialCollectionAddedToSimulation = true;

	// Flush all entity subsystem commands pushed while adding collection entries to the simulation
	// This is the temporary way to force our commands to be processed until MassEntitySubsystem
	// offers a threadsafe solution to push and flush commands in our own execution context.
	EntityManager->FlushCommands();

#if UE_ENABLE_DEBUG_DRAWING
	// Refresh debug draw
	if (RenderingActor != nullptr)
	{
		RenderingActor->MarkComponentsRenderStateDirty();
	}
#endif // UE_ENABLE_DEBUG_DRAWING
}

void USmartObjectSubsystem::CleanupRuntime()
{
	EntityManager = UE::Mass::Utils::GetEntityManagerChecked(GetWorldRef()).AsShared();

	if (!ensureMsgf(EntityManager, TEXT("Entity manager required to use SmartObjects")))
	{
		return;
	}

	// Process component list first so they can be notified before we destroy their associated runtime instance
	for (USmartObjectComponent* Component : RegisteredSOComponents)
	{
		// Make sure component was registered to simulation (e.g. Valid associated definition)
		if (Component != nullptr && Component->GetRegisteredHandle().IsValid())
		{
			RemoveComponentFromSimulation(*Component);
		}
	}

	// Cleanup all remaining entries (e.g. associated to unloaded entries)
	TArray<FSmartObjectHandle> RemainingHandles;
	if (RuntimeSmartObjects.GetKeys(RemainingHandles))
	{
		for (const FSmartObjectHandle& SOHandle : RemainingHandles)
		{
			if (SOHandle.IsValid())
			{
				RemoveRuntimeInstanceFromSimulation(SOHandle);
			}
		}
	}

	RuntimeCreatedEntries.Reset();
	bInitialCollectionAddedToSimulation = false;

	// Flush all entity subsystem commands pushed while stopping the simulation
	// This is the temporary way to force our commands to be processed until MassEntitySubsystem
	// offers a threadsafe solution to push and flush commands in our own execution context.
	EntityManager->FlushCommands();

#if UE_ENABLE_DEBUG_DRAWING
	// Refresh debug draw
	if (RenderingActor != nullptr)
	{
		RenderingActor->MarkComponentsRenderStateDirty();
	}
#endif // UE_ENABLE_DEBUG_DRAWING
}

void USmartObjectSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);

	InitializeRuntime();
}

void USmartObjectSubsystem::Deinitialize()
{
	EntityManager.Reset();

	Super::Deinitialize();
}

#if WITH_EDITOR
void USmartObjectSubsystem::ComputeBounds(const UWorld& World, ASmartObjectCollection& Collection) const
{
	FBox Bounds(ForceInitToZero);

	if (const UWorldPartition* WorldPartition = World.GetWorldPartition())
	{
		Bounds = WorldPartition->GetRuntimeWorldBounds();
	}
	else if (const ULevel* PersistentLevel = World.PersistentLevel.Get())
	{
		if (PersistentLevel->LevelBoundsActor.IsValid())
		{
			Bounds = PersistentLevel->LevelBoundsActor.Get()->GetComponentsBoundingBox();
		}
		else
		{
			Bounds = ALevelBounds::CalculateLevelBounds(PersistentLevel);
		}
	}
	else
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("Unable to determine world bounds: no world partition or persistent level."));
	}

	Collection.SetBounds(Bounds);
}

void USmartObjectSubsystem::RebuildCollection(ASmartObjectCollection& InCollection)
{
	InCollection.RebuildCollection(RegisteredSOComponents);
}

void USmartObjectSubsystem::SpawnMissingCollection() const
{
	if (IsValid(MainCollection))
	{
		return;
	}

	UWorld& World = GetWorldRef();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = World.PersistentLevel;
	SpawnInfo.bAllowDuringConstructionScript = true;

	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Spawning missing collection for world '%s'."), *World.GetName());
	World.SpawnActor<ASmartObjectCollection>(ASmartObjectCollection::StaticClass(), SpawnInfo);

	checkf(IsValid(MainCollection), TEXT("MainCollection must be assigned after spawning"));
}
#endif // WITH_EDITOR

#if WITH_SMARTOBJECT_DEBUG
void USmartObjectSubsystem::DebugUnregisterAllSmartObjects()
{
	for (USmartObjectComponent* Cmp : RegisteredSOComponents)
	{
		if (Cmp != nullptr && RuntimeSmartObjects.Find(Cmp->GetRegisteredHandle()) != nullptr)
		{
			RemoveComponentFromSimulation(*Cmp);
		}
	}
}

void USmartObjectSubsystem::DebugRegisterAllSmartObjects()
{
	if (MainCollection == nullptr)
	{
		return;
	}
	for (USmartObjectComponent* Cmp : RegisteredSOComponents)
	{
		if (Cmp != nullptr)
		{
			const FSmartObjectCollectionEntry* Entry = MainCollection->GetEntries().FindByPredicate(
				[Handle=Cmp->GetRegisteredHandle()](const FSmartObjectCollectionEntry& CollectionEntry)
				{
					return CollectionEntry.GetHandle() == Handle;
				});

			// In this debug command we register back components that were already part of the simulation but
			// removed using debug command 'ai.debug.so.UnregisterAllSmartObjects'.
			// We need to find associated collection entry and pass it back so the callbacks can be bound properly
			if (Entry && RuntimeSmartObjects.Find(Entry->GetHandle()) == nullptr)
			{
				AddComponentToSimulation(*Cmp, *Entry);
			}
		}
	}
}

void USmartObjectSubsystem::DebugInitializeRuntime()
{
	// do not initialize more than once or on a GameWorld
	if (bInitialCollectionAddedToSimulation || GetWorldRef().IsGameWorld())
	{
		return;
	}
	InitializeRuntime();
}

#if WITH_EDITOR
void USmartObjectSubsystem::DebugRebuildCollection()
{
	if (MainCollection != nullptr)
	{
		RebuildCollection(*MainCollection);
	}
}
#endif // WITH_EDITOR

void USmartObjectSubsystem::DebugCleanupRuntime()
{
	// do not cleanup more than once or on a GameWorld
	if (!bInitialCollectionAddedToSimulation || GetWorldRef().IsGameWorld())
	{
		return;
	}
	CleanupRuntime();
}

#endif // WITH_SMARTOBJECT_DEBUG
