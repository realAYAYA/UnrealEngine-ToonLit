// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSubsystem.h"
#include "Math/ColorList.h"
#include "SmartObjectComponent.h"
#include "SmartObjectUserComponent.h"
#include "SmartObjectTypes.h"
#include "EngineUtils.h"
#include "SmartObjectHashGrid.h"
#include "WorldConditionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "Engine/LevelStreaming.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Annotations/SmartObjectSlotEntranceAnnotation.h"
#include "Annotations/SmartObjectAnnotation_SlotUserCollision.h"
#include "Misc/EnumerateRange.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSubsystem)

#if UE_ENABLE_DEBUG_DRAWING
#include "SmartObjectSubsystemRenderingActor.h"
#endif

#if WITH_SMARTOBJECT_DEBUG
#endif

#if WITH_EDITOR
#include "Engine/LevelBounds.h"
#include "WorldPartition/WorldPartition.h"
#endif

#if WITH_EDITORONLY_DATA
#include "SmartObjectCollection.h"
#endif // WITH_EDITORONLY_DATA

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
USmartObjectSubsystem::USmartObjectSubsystem()
	: SmartObjectContainer(this)
{
}

void USmartObjectSubsystem::OnWorldComponentsUpdated(UWorld& World)
{
#if WITH_EDITORONLY_DATA
	bIsPartitionedWorld = World.IsPartitionedWorld();
#endif // WITH_EDITORONLY_DATA

	// Load class required to instantiate the space partition structure
	UE_CVLOG_UELOG(!SpacePartitionClassName.IsValid(), this, LogSmartObject, Error, TEXT("A valid space partition class name is required."));
	if (SpacePartitionClassName.IsValid())
	{
		SpacePartitionClass = LoadClass<USmartObjectSpacePartition>(nullptr, *SpacePartitionClassName.ToString());
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
	if (!World.IsGameWorld() && bAutoInitializeEditorInstances)
	{
		// calculating world bounds first since InitializeRuntime is using that data to create the USmartObjectSpacePartition 
		// instance. Note that we use the World-calculated bounds only for editor worlds, since Runtime SmartObjectContainer's 
		// bounds will rely on existing SmartObjectCollections. In editor we use world's size to not resize the 
		// USmartObjectSpacePartition with SO operations
		SmartObjectContainer.SetBounds(ComputeBounds(World));

		InitializeRuntime();
	}
#endif // WITH_EDITOR
}

USmartObjectSubsystem* USmartObjectSubsystem::GetCurrent(const UWorld* World)
{
	return UWorld::GetSubsystem<USmartObjectSubsystem>(World);
}

FSmartObjectRuntime* USmartObjectSubsystem::AddComponentToSimulation(
	USmartObjectComponent& SmartObjectComponent,
	const FSmartObjectCollectionEntry& NewEntry
	)
{
	checkf(SmartObjectComponent.GetDefinition() != nullptr, TEXT("Shouldn't reach this point with an invalid definition asset"));

	FSmartObjectRuntime* SmartObjectRuntime = AddCollectionEntryToSimulation(NewEntry, *SmartObjectComponent.GetDefinition(), &SmartObjectComponent);
	if (SmartObjectRuntime != nullptr)
	{
		BindComponentToSimulationInternal(SmartObjectComponent, *SmartObjectRuntime);
	}
	return SmartObjectRuntime;
}

void USmartObjectSubsystem::BindComponentToSimulation(USmartObjectComponent& SmartObjectComponent)
{
	ensureMsgf(SmartObjectComponent.GetRegisteredHandle().IsValid(), TEXT("%s expects input SmartObjectComponent to be already registered."), ANSI_TO_TCHAR(__FUNCTION__));

	// Notify the component to bind to its runtime counterpart
	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SmartObjectComponent.GetRegisteredHandle());
	if (ensureMsgf(SmartObjectRuntime != nullptr, TEXT("Binding a component should only be used when an associated runtime instance exists.")))
	{
		BindComponentToSimulationInternal(SmartObjectComponent, *SmartObjectRuntime);
	}
}

void USmartObjectSubsystem::BindComponentToSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime)
{
	// It is possible that the component is already linked to the runtime instance when the collection entry was initially added.
	ensureMsgf(!SmartObjectRuntime.OwnerComponent.IsValid() || SmartObjectRuntime.OwnerComponent == &SmartObjectComponent,
		TEXT("Different OwnerComponent (was %s) when binding SmartObjectComponent %s. This might indicate multiple objects using the same handle."),
			*GetFullNameSafe(SmartObjectRuntime.OwnerComponent.Get()), *GetFullNameSafe(&SmartObjectComponent));

	SmartObjectRuntime.OwnerComponent = &SmartObjectComponent;
	SmartObjectComponent.OnRuntimeInstanceBound(SmartObjectRuntime);
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("SmartObjectComponent %s bound to simulation."), *GetFullNameSafe(&SmartObjectComponent));
}

void USmartObjectSubsystem::UnbindComponentFromSimulation(USmartObjectComponent& SmartObjectComponent)
{
	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SmartObjectComponent.GetRegisteredHandle());

	if (ensureMsgf(SmartObjectRuntime != nullptr,
		TEXT("Unbinding SmartObjectComponent %s but its associated runtime instance doesn't exist. This might indicate multiple objects using the same handle."),
			*GetFullNameSafe(&SmartObjectComponent)))
	{
		UnbindComponentFromSimulationInternal(SmartObjectComponent, *SmartObjectRuntime);
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("SmartObjectComponent %s unbound from simulation."), *GetFullNameSafe(&SmartObjectComponent));		
	}
}

void USmartObjectSubsystem::UnbindComponentFromSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime)
{
	SmartObjectComponent.OnRuntimeInstanceUnbound(SmartObjectRuntime);
	SmartObjectRuntime.OwnerComponent = nullptr;
}

FSmartObjectRuntime* USmartObjectSubsystem::AddCollectionEntryToSimulation(
	const FSmartObjectCollectionEntry& Entry,
	const USmartObjectDefinition& Definition,
	USmartObjectComponent* OwnerComponent
	)
{
	const FSmartObjectHandle Handle = Entry.GetHandle();
	const FTransform& Transform = Entry.GetTransform();
	const FBox& Bounds = Entry.GetBounds();
	const FGameplayTagContainer& Tags = Entry.GetTags();

	if (!ensureMsgf(Handle.IsValid(), TEXT("SmartObject needs a valid Handle to be added to the simulation")))
	{
		return nullptr;
	}

	// @todo temporarily commenting out the ensure while the proper fix is being developed.
	//if (!ensureMsgf(RuntimeSmartObjects.Find(Handle) == nullptr, TEXT("Handle '%s' already registered in runtime simulation"), *LexToString(Handle)))
	if (RuntimeSmartObjects.Find(Handle) != nullptr)
	{
		return nullptr;
	}

	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Adding SmartObject '%s' to runtime simulation."), *LexToString(Handle));

	FSmartObjectRuntime& Runtime = RuntimeSmartObjects.Emplace(Handle, FSmartObjectRuntime(Definition));
	Runtime.SetRegisteredHandle(Handle);
	Runtime.Tags = Tags;
	Runtime.OwnerComponent = OwnerComponent;
	UE_CVLOG_UELOG(OwnerComponent != nullptr, this, LogSmartObject, Verbose, TEXT("SmartObjectComponent %s added to simulation."), *GetFullNameSafe(OwnerComponent));

#if UE_ENABLE_DEBUG_DRAWING
	Runtime.Bounds = Bounds;
#endif

	FWorldConditionContextData ConditionContextData(*Definition.GetWorldConditionSchema());
	SetupConditionContextCommonData(ConditionContextData, Runtime);

	// Always initialize state (handles empty conditions)
	Runtime.PreconditionState.Initialize(*this, Definition.GetPreconditions());

	// Activate Object Preconditions if any
	const FWorldConditionContext ObjectContext(Runtime.PreconditionState, ConditionContextData);
	if (!ObjectContext.Activate())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("Failed to activate Preconditions on SmartObject '%s'."), *LexToString(Handle));
	}
	
	// Create runtime data and entity for each slot
	int32 SlotIndex = 0;
	const USmartObjectWorldConditionSchema* DefaultWorldConditionSchema = GetDefault<USmartObjectWorldConditionSchema>();

	Runtime.Slots.Reserve(Definition.GetSlots().Num());
	
	for (const FSmartObjectSlotDefinition& SlotDefinition : Definition.GetSlots())
	{
		FSmartObjectRuntimeSlot& Slot = Runtime.Slots.AddDefaulted_GetRef();

		// Setup initial state from slot definition and current object state
		Slot.Offset = SlotDefinition.Offset;
		Slot.Rotation = SlotDefinition.Rotation;
		Slot.bSlotEnabled = SlotDefinition.bEnabled;
		Slot.Tags = SlotDefinition.RuntimeTags;
		Slot.bObjectEnabled = Runtime.IsEnabled();

		// Always initialize state (handles empty conditions)
		Slot.PreconditionState.Initialize(*this, SlotDefinition.SelectionPreconditions);

		FSmartObjectSlotHandle SlotHandle(Handle, SlotIndex);
		
		// Activate slot Preconditions if any
		ensureMsgf(ConditionContextData.SetContextData(DefaultWorldConditionSchema->GetSlotHandleRef(), &SlotHandle),
			TEXT("Expecting USmartObjectWorldConditionSchema::SlotHandleRef to be valid."));

		const FWorldConditionContext SlotContext(Slot.PreconditionState, ConditionContextData);
		if (!SlotContext.Activate())
		{
			UE_VLOG_UELOG(this, LogSmartObject, Error,
				TEXT("Failed to activate Preconditions on SmartObject '%s' slot '%s'."), *LexToString(Handle), *LexToString(SlotHandle));
		}
		
		SlotIndex++;
	}
	
	// Transfer spatial information to the runtime instance
	Runtime.SetTransform(Transform);

	// Insert to the spatial representation structure and store associated data
	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	Runtime.SpatialEntryData = SpacePartition->Add(Handle, Bounds);

	return &Runtime;
}

bool USmartObjectSubsystem::RemoveRuntimeInstanceFromSimulation(const FSmartObjectHandle Handle, USmartObjectComponent* SmartObjectComponent)
{
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Removing SmartObject '%s' from runtime simulation."), *LexToString(Handle));

	FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(Handle);
#if WITH_SMARTOBJECT_DEBUG
	ensureMsgf(SmartObjectRuntime != nullptr, TEXT("RemoveFromSimulation is an internal call and should only be used for objects still part of the simulation"));
#endif // WITH_SMARTOBJECT_DEBUG

	if (SmartObjectRuntime == nullptr)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%s called with %s SO Handle and no corresponding SmartObjectRuntime")
			, ANSI_TO_TCHAR(__FUNCTION__)
			, Handle.IsValid() ? TEXT("a VALID") : TEXT("an INVALID"));

		return false;
	}

	if (SmartObjectComponent != nullptr)
	{
		UnbindComponentFromSimulationInternal(*SmartObjectComponent, *SmartObjectRuntime);
	}

	DestroyRuntimeInstanceInternal(Handle, *SmartObjectRuntime);

	// Remove object runtime data
	RuntimeSmartObjects.Remove(Handle);

	return true;
}

void USmartObjectSubsystem::DestroyRuntimeInstanceInternal(
	const FSmartObjectHandle Handle,
	FSmartObjectRuntime& SmartObjectRuntime
	)
{
	// Abort everything before removing since abort flow may require access to runtime data
	AbortAll(Handle, SmartObjectRuntime);

	// Remove from space partition
	checkfSlow(SpacePartition != nullptr, TEXT("Space partition is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition->Remove(Handle, SmartObjectRuntime.SpatialEntryData);

	FWorldConditionContextData ConditionContextData(*SmartObjectRuntime.GetDefinition().GetWorldConditionSchema());
	SetupConditionContextCommonData(ConditionContextData, SmartObjectRuntime);

	// Deactivate object and slot Preconditions
	const FWorldConditionContext ObjectContext(SmartObjectRuntime.PreconditionState, ConditionContextData);
	ObjectContext.Deactivate();

	const USmartObjectWorldConditionSchema* DefaultWorldConditionSchema = GetDefault<USmartObjectWorldConditionSchema>();
	for (TConstEnumerateRef<FSmartObjectRuntimeSlot> RuntimeSlot : EnumerateRange(SmartObjectRuntime.Slots))
	{
		const FSmartObjectSlotHandle SlotHandle(Handle, RuntimeSlot.GetIndex());
		ensureMsgf(ConditionContextData.SetContextData(DefaultWorldConditionSchema->GetSlotHandleRef(), &SlotHandle),
			TEXT("Expecting USmartObjectWorldConditionSchema::SlotHandleRef to be valid."));

		// Deactivate slot Preconditions (if successfully initialized)
		const FWorldConditionContext SlotContext(RuntimeSlot->PreconditionState, ConditionContextData);
		SlotContext.Deactivate();
	}
}

bool USmartObjectSubsystem::RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry)
{
	return RemoveRuntimeInstanceFromSimulation(Entry.GetHandle(), /*SmartObjectComponent*/nullptr);
}

void USmartObjectSubsystem::RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent)
{
	if (RemoveRuntimeInstanceFromSimulation(SmartObjectComponent.GetRegisteredHandle(), &SmartObjectComponent))
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%s call succeeded for %s")
			, ANSI_TO_TCHAR(__FUNCTION__)
			, *GetFullNameSafe(&SmartObjectComponent));
	}
	else
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("%s call failed for %s")
			, ANSI_TO_TCHAR(__FUNCTION__)
			, *GetFullNameSafe(&SmartObjectComponent));
	}
}

void USmartObjectSubsystem::AbortAll(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime) const
{
	for (TEnumerateRef<FSmartObjectRuntimeSlot> RuntimeSlot : EnumerateRange(SmartObjectRuntime.Slots))
	{
		const FSmartObjectSlotHandle SlotHandle(Handle, RuntimeSlot.GetIndex());

		switch (RuntimeSlot->State)
		{
		case ESmartObjectSlotState::Claimed:
		case ESmartObjectSlotState::Occupied:
			{
				const FSmartObjectClaimHandle ClaimHandle(SmartObjectRuntime.GetRegisteredHandle(), SlotHandle, RuntimeSlot->User);

				// Keep user data to be used as payload in the notification event
				// since it will be released by the following call to Slot.Release 
				const FInstancedStruct Payload(MoveTemp(RuntimeSlot->UserData));
				if (RuntimeSlot->Release(ClaimHandle, /* bAborted */true))
				{
					OnSlotChanged(SmartObjectRuntime, *RuntimeSlot, SlotHandle, ESmartObjectChangeReason::OnReleased, Payload);

					UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Slot %s released by an abort"), *LexToString(ClaimHandle.SlotHandle));
					UE_VLOG_LOCATION(this, LogSmartObject, Display, GetSlotLocation(ClaimHandle.SlotHandle).GetValue(), /*Radius*/50.f, FColor::Red, TEXT("Released by abort"));
				}
				break;
			}
		case ESmartObjectSlotState::Free: // falling through on purpose
		default:
			UE_CVLOG_UELOG(RuntimeSlot->User.IsValid(), this, LogSmartObject, Warning,
				TEXT("Smart object %s used by %s while the slot it's assigned to is not marked Claimed nor Occupied"),
				*LexToString(SmartObjectRuntime.GetDefinition()),
				*LexToString(RuntimeSlot->User));
			break;
		}
		RuntimeSlot->State = ESmartObjectSlotState::Free;
	}
}

bool USmartObjectSubsystem::RegisterSmartObject(USmartObjectComponent& SmartObjectComponent)
{
	const USmartObjectDefinition* Definition = SmartObjectComponent.GetDefinition();
	if (Definition == nullptr)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Attempting to register %s while its DefinitionAsset is not set. Bailing out."),
			*GetFullNameSafe(&SmartObjectComponent));
		return false;
	}

	TOptional<bool> bIsValid = Definition->IsValid();
	if (bIsValid.IsSet() == false)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Attempting to register %s while its DefinitionAsset has not been Validated. Validating now."),
			*GetFullNameSafe(&SmartObjectComponent));
		bIsValid = Definition->Validate();
	}
	
	if (bIsValid.GetValue() == false)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Attempting to register %s while its DefinitionAsset fails validation test. Bailing out."
													" Resave asset %s to see the errors and fix the problem."),
			*GetFullNameSafe(&SmartObjectComponent),
			*GetFullNameSafe(Definition));
		return false;
	}

	if (Definition->GetSlots().IsEmpty())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Attempting to register %s while its DefinitionAsset doesn't contain any slots. Bailing out."),
			*GetFullNameSafe(&SmartObjectComponent),
			*GetFullNameSafe(Definition));
		return false;
	}

	if (!RegisteredSOComponents.Contains(&SmartObjectComponent))
	{
		return RegisterSmartObjectInternal(SmartObjectComponent);
	}
	
	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Failed to register %s. Already registered"),	*GetFullNameSafe(SmartObjectComponent.GetOwner()));

	return false;
}

bool USmartObjectSubsystem::RegisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent)
{
	UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("Registering %s using definition %s."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	// until the runtime is initialized we're not ready to register SmartObject. We collect them in PendingSmartObjectRegistration
	// and process them in InitializeRuntime call.
	if (bRuntimeInitialized)
	{
		if (SmartObjectComponent.GetRegisteredHandle().IsValid())
		{
			// Simply bind the newly available component to its active runtime instance
			BindComponentToSimulation(SmartObjectComponent);
		}
		else
		{
			bool bAlreadyInCollection = false;
			if (const FSmartObjectCollectionEntry* Entry = SmartObjectContainer.AddSmartObject(SmartObjectComponent, bAlreadyInCollection))
			{
				if (bAlreadyInCollection)
				{
					SmartObjectComponent.SetRegisteredHandle(Entry->GetHandle(), ESmartObjectRegistrationType::WithCollection);
					BindComponentToSimulation(SmartObjectComponent);
				}
				else
				{
					SmartObjectComponent.SetRegisteredHandle(Entry->GetHandle(), ESmartObjectRegistrationType::Dynamic);
					AddComponentToSimulation(SmartObjectComponent, *Entry);
#if WITH_EDITOR
					OnMainCollectionDirtied.Broadcast();
#endif
				}
			}
		}

		ensureMsgf(RegisteredSOComponents.Find(&SmartObjectComponent) == INDEX_NONE
			, TEXT("Adding %s to RegisteredSOColleciton, but it has already been added. Missing unregister call?"), *SmartObjectComponent.GetFullName());
		RegisteredSOComponents.Add(&SmartObjectComponent);

#if UE_ENABLE_DEBUG_DRAWING
		// Refresh debug draw
		if (RenderingActor != nullptr)
		{
			RenderingActor->MarkComponentsRenderStateDirty();
		}
#endif // UE_ENABLE_DEBUG_DRAWING
	}
	else
	{
		UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose,
			TEXT("%s not added to collection since InitializeRuntime has not been called yet. Storing SOComponent instance for registration during InitializeRuntime call.")
			, *GetNameSafe(SmartObjectComponent.GetOwner()));
		PendingSmartObjectRegistration.Add(&SmartObjectComponent);
	}

	return true;
}

bool USmartObjectSubsystem::RemoveSmartObject(USmartObjectComponent& SmartObjectComponent)
{
	if (RegisteredSOComponents.Contains(&SmartObjectComponent))
	{
		return UnregisterSmartObjectInternal(SmartObjectComponent, /*bDestroyRuntimeState=*/true);
	}

	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Failed to remove %s since it doesn't seem registered or has already been unregistered."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	return false;
}

bool USmartObjectSubsystem::UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent)
{
	if (RegisteredSOComponents.Contains(&SmartObjectComponent))
	{
		return UnregisterSmartObjectInternal(SmartObjectComponent,
			/*bDestroyRuntimeState=*/SmartObjectComponent.GetRegistrationType() == ESmartObjectRegistrationType::Dynamic);
	}

	UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Failed to unregister %s since it doesn't seem registered or has already been unregistered."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	return false;
}

bool USmartObjectSubsystem::UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const bool bDestroyRuntimeState)
{
	UE_VLOG_UELOG(this, LogSmartObject, VeryVerbose, TEXT("Unregistering %s using definition %s."),
		*GetFullNameSafe(SmartObjectComponent.GetOwner()),
		*GetFullNameSafe(SmartObjectComponent.GetDefinition()));

	if (bRuntimeInitialized)
	{
		ensure(SmartObjectComponent.GetRegisteredHandle().IsValid());

		if (SmartObjectComponent.IsBoundToSimulation())
		{
			if (bDestroyRuntimeState)
			{
				RemoveComponentFromSimulation(SmartObjectComponent);
				SmartObjectContainer.RemoveSmartObject(SmartObjectComponent);
			}
			// otherwise we keep all the runtime entries in place - those will be removed along with the collection that has added them 
			else
			{
				// Unbind the component from its associated runtime instance
				UnbindComponentFromSimulation(SmartObjectComponent);
			}
		}

		RegisteredSOComponents.Remove(&SmartObjectComponent);
	}
	else
	{
		PendingSmartObjectRegistration.RemoveSingleSwap(&SmartObjectComponent);
	}

	return true;
}

bool USmartObjectSubsystem::RegisterSmartObjectActor(const AActor& SmartObjectActor)
{
	TArray<USmartObjectComponent*> Components;
	SmartObjectActor.GetComponents(Components);
	UE_CVLOG_UELOG(Components.Num() == 0, &SmartObjectActor, LogSmartObject, Log,
		TEXT("Failed to register SmartObject components for %s. No components found."), *SmartObjectActor.GetFullName());

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
		TEXT("Failed to unregister SmartObject components for %s. No components found."), *SmartObjectActor.GetFullName());

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

bool USmartObjectSubsystem::RemoveSmartObjectActor(const AActor& SmartObjectActor)
{
	TArray<USmartObjectComponent*> Components;
	SmartObjectActor.GetComponents(Components);
	UE_CVLOG_UELOG(Components.Num() == 0, &SmartObjectActor, LogSmartObject, Log,
		TEXT("Failed to remove SmartObject components runtime data for %s. No components found."), *SmartObjectActor.GetFullName());

	int32 NumSuccess = 0;
	for (USmartObjectComponent* SOComponent : Components)
	{
		if (RemoveSmartObject(*SOComponent))
		{
			NumSuccess++;
		}
	}
	return NumSuccess > 0 && NumSuccess == Components.Num();
}

bool USmartObjectSubsystem::SetSmartObjectActorEnabled(const AActor& SmartObjectActor, const bool bEnabled)
{
	TArray<USmartObjectComponent*> Components;
	SmartObjectActor.GetComponents(Components);
	UE_CVLOG_UELOG(Components.Num() == 0, this, LogSmartObject, Log,
		TEXT("Failed to change SmartObject components enable state for %s. No components found."), *SmartObjectActor.GetFullName());

	int32 NumSuccess = 0;
	for (const USmartObjectComponent* SOComponent : Components)
	{
		if (SetEnabled(SOComponent->GetRegisteredHandle(), bEnabled))
		{
			NumSuccess++;
		}
	}

	return NumSuccess > 0 && NumSuccess == Components.Num();
}

bool USmartObjectSubsystem::SetEnabled(const FSmartObjectHandle Handle, const bool bEnabled)
{
	FSmartObjectRuntime* SmartObjectRuntime = GetRuntimeInstance(Handle);
	if (SmartObjectRuntime == nullptr)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log,
			TEXT("Failed to change SmartObject enable state for %s. No associated runtime instance found."), *LexToString(Handle));

		return false;
	}
	
	if (SmartObjectRuntime->bEnabled == bEnabled)
	{
		// Already in the proper state, nothing to notify
		return true;
	}

	SmartObjectRuntime->bEnabled = bEnabled;

	// Notify if needed
	if (SmartObjectRuntime->OnEvent.IsBound())
	{
		FSmartObjectEventData Data;
		Data.SmartObjectHandle = SmartObjectRuntime->GetRegisteredHandle();
		Data.Reason = bEnabled ? ESmartObjectChangeReason::OnObjectEnabled : ESmartObjectChangeReason::OnObjectDisabled;
		SmartObjectRuntime->OnEvent.Broadcast(Data);
	}
	
	// Propagate object enable state to slots and notify if needed.
	for (TEnumerateRef<FSmartObjectRuntimeSlot> RuntimeSlot : EnumerateRange(SmartObjectRuntime->Slots))
	{
		const FSmartObjectSlotHandle SlotHandle(Handle, RuntimeSlot.GetIndex());
		
		// Using 'IsEnabled' to combine slot enable and smart object enable
		const bool bSlotPreviousValue = RuntimeSlot->IsEnabled();

		// Always set object enabled state even if combined result might not be affected
		RuntimeSlot->bObjectEnabled = bEnabled;

		// Using new combined value to detect changes
		if (RuntimeSlot->IsEnabled() != bSlotPreviousValue)
		{
			OnSlotChanged(*SmartObjectRuntime, *RuntimeSlot, SlotHandle, RuntimeSlot->IsEnabled() ? ESmartObjectChangeReason::OnSlotEnabled : ESmartObjectChangeReason::OnSlotDisabled, RuntimeSlot->UserData);
		}
	}

	return true;
}

bool USmartObjectSubsystem::IsEnabled(const FSmartObjectHandle Handle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetRuntimeInstance(Handle);
	UE_CVLOG_UELOG(SmartObjectRuntime == nullptr, this, LogSmartObject, Log,
		TEXT("Failed to get the SmartObject enable state for %s. No associated runtime instance found."), *LexToString(Handle));
	
	return SmartObjectRuntime && SmartObjectRuntime->bEnabled;
}

void USmartObjectSubsystem::SetupConditionContextCommonData(FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const
{
	const USmartObjectWorldConditionSchema* DefaultSchema = GetDefault<USmartObjectWorldConditionSchema>();		
	ensureMsgf(ContextData.SetContextData(DefaultSchema->GetSmartObjectActorRef(), SmartObjectRuntime.GetOwnerActor()),
		TEXT("Expecting USmartObjectWorldConditionSchema::GetSmartObjectActorRef to be valid."));
	ensureMsgf(ContextData.SetContextData(DefaultSchema->GetSmartObjectHandleRef(), &SmartObjectRuntime.RegisteredHandle),
		TEXT("Expecting USmartObjectWorldConditionSchema::SmartObjectHandleRef to be valid."));
	ensureMsgf(ContextData.SetContextData(DefaultSchema->GetSubsystemRef(), this),
		TEXT("Expecting USmartObjectWorldConditionSchema::SubsystemRef to be valid."));
}

void USmartObjectSubsystem::BindPropertiesFromStruct(FWorldConditionContextData& ContextData, const FConstStructView& UserData) const
{
	const UWorldConditionSchema* Schema = ContextData.GetSchema();
	check(Schema);

	// @todo SO: could create a cache of layouts since user data types shouldn't vary much
	// @todo SO: consider moving this into FWorldConditionContextData

	for (TFieldIterator<FProperty> It(UserData.GetScriptStruct(), EFieldIterationFlags::None); It; ++It)
	{
		const FProperty* Property = *It;
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const FWorldConditionContextDataRef Ref = Schema->GetContextDataRefByName(Property->GetFName(), StructProperty->Struct);
			if (Ref.IsValid())
			{
				const FConstStructView StructView(StructProperty->Struct, UserData.GetMemory() + Property->GetOffset_ForInternal());
				ContextData.SetContextData(Ref, StructView);
			}
		}
		else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			const FWorldConditionContextDataRef Ref = Schema->GetContextDataRefByName(Property->GetFName(), ObjectProperty->PropertyClass);
			if (Ref.IsValid())
			{
				const UObject* Object = ObjectProperty->GetObjectPropertyValue(UserData.GetMemory() + Property->GetOffset_ForInternal());
				ContextData.SetContextData(Ref, Object);
			}
		}
	}
}

bool USmartObjectSubsystem::EvaluateObjectConditions(const FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const
{
	// Evaluate object conditions. Note that unsuccessfully initialized conditions is supported (i.e. error during activation)
	const FWorldConditionContext Context(SmartObjectRuntime.PreconditionState, ConditionContextData);
	if (!Context.IsTrue())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Preconditions for owning smart object %s failed."), *LexToString(SmartObjectRuntime.GetRegisteredHandle()));
		return false;
	}

	return true;
}

bool USmartObjectSubsystem::EvaluateSlotConditions(
	FWorldConditionContextData& ConditionContextData,
	const FSmartObjectRuntime& SmartObjectRuntime,
	const FSmartObjectSlotHandle SlotHandle
	) const
{
	// Add slot data to the context
	const USmartObjectWorldConditionSchema* DefaultSchema = GetDefault<USmartObjectWorldConditionSchema>();
	ensureMsgf(ConditionContextData.SetContextData(DefaultSchema->GetSlotHandleRef(), &SlotHandle),
		TEXT("Expecting USmartObjectWorldConditionSchema::SlotHandleRef to be valid."));

	// Evaluate slot conditions. Note that unsuccessfully initialized conditions is supported (i.e. error during activation)
	const FWorldConditionContext Context(SmartObjectRuntime.Slots[SlotHandle.GetSlotIndex()].PreconditionState, ConditionContextData);
	if (!Context.IsTrue())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Preconditions for slot %s failed."), *LexToString(SlotHandle));
		return false;
	}

	return true;
}

FSmartObjectClaimHandle USmartObjectSubsystem::Claim(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__));
	if (SmartObjectRuntime == nullptr)
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	TArray<FSmartObjectSlotHandle> SlotHandles;
	FindSlots(Handle, *SmartObjectRuntime, Filter, SlotHandles, {});
	if (SlotHandles.IsEmpty())
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	return MarkSlotAsClaimed(SlotHandles.Top(), {});
}

FSmartObjectClaimHandle USmartObjectSubsystem::MarkSlotAsClaimed(const FSmartObjectSlotHandle SlotHandle, const FConstStructView UserData)
{
	if (!SlotHandle.IsValid())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("Claiming using an unset smart object slot handle. Returning invalid FSmartObjectClaimHandle."));
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (!GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	// Fast test to see if slot can be claimed (Parent smart object is enabled AND slot is free and enabled) 
	if (!Slot->CanBeClaimed())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log,
			TEXT("Can't claim slot handle %s since it is, or its owning smart object %s, disabled or not free."), *LexToString(SlotHandle), *LexToString(SlotHandle.GetSmartObjectHandle()));
		return FSmartObjectClaimHandle::InvalidHandle;
	}

	const FSmartObjectUserHandle User(NextFreeUserID++);
	const bool bClaimed = Slot->Claim(User);

	const FSmartObjectClaimHandle ClaimHandle(SlotHandle.GetSmartObjectHandle(), SlotHandle, User);
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Claim %s for handle %s. Slot State is '%s'"),
		bClaimed ? TEXT("SUCCEEDED") : TEXT("FAILED"),
		*LexToString(ClaimHandle),
		*UEnum::GetValueAsString(Slot->GetState()));
	UE_CVLOG_LOCATION(bClaimed, this, LogSmartObject, Display, GetSlotLocation(ClaimHandle).GetValue(), 50.f, FColor::Yellow, TEXT("Claim"));

	if (bClaimed)
	{
		Slot->UserData = UserData;
		OnSlotChanged(*SmartObjectRuntime, *Slot, SlotHandle, ESmartObjectChangeReason::OnClaimed, Slot->UserData);
		return ClaimHandle;
	}

	return FSmartObjectClaimHandle::InvalidHandle;
}

bool USmartObjectSubsystem::CanBeClaimed(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Slot->CanBeClaimed();
	}
	return false;
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
	return IsSmartObjectSlotValid(SlotHandle);
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinition(
	const FSmartObjectClaimHandle& ClaimHandle,
	TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
	)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(ClaimHandle.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? GetBehaviorDefinition(*SmartObjectRuntime, ClaimHandle.SlotHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinitionByRequestResult(
	const FSmartObjectRequestResult& RequestResult,
	TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
	)
{
	const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(RequestResult.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? GetBehaviorDefinition(*SmartObjectRuntime, RequestResult.SlotHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::GetBehaviorDefinition(
	const FSmartObjectRuntime& SmartObjectRuntime,
	const FSmartObjectSlotHandle SlotHandle,
	TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
	)
{
	const USmartObjectDefinition& Definition = SmartObjectRuntime.GetDefinition();
	return Definition.GetBehaviorDefinition(SlotHandle.GetSlotIndex(), DefinitionClass);
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::MarkSlotAsOccupied(
	const FSmartObjectClaimHandle& ClaimHandle,
	TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
	)
{
	FSmartObjectRuntime* SmartObjectRuntime = GetValidatedMutableRuntime(ClaimHandle.SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__));
	return SmartObjectRuntime != nullptr ? MarkSlotAsOccupied(*SmartObjectRuntime, ClaimHandle, DefinitionClass) : nullptr;
}

const USmartObjectBehaviorDefinition* USmartObjectSubsystem::MarkSlotAsOccupied(
	FSmartObjectRuntime& SmartObjectRuntime,
	const FSmartObjectClaimHandle& ClaimHandle,
	const TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
	)
{
	checkf(ClaimHandle.IsValid(), TEXT("This is an internal method that should only be called with an assigned claim handle"));

	if (!SmartObjectRuntime.IsEnabled())
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
	UE_VLOG_LOCATION(this, LogSmartObject, Display, GetSlotLocation(ClaimHandle).GetValue(), 50.f, FColor::Green, TEXT("Use"));

	FSmartObjectRuntimeSlot& Slot = SmartObjectRuntime.Slots[ClaimHandle.SlotHandle.GetSlotIndex()];

	if (ensureMsgf(Slot.GetState() == ESmartObjectSlotState::Claimed, TEXT("Should have been claimed first: %s"), *LexToString(ClaimHandle)) &&
		ensureMsgf(Slot.User == ClaimHandle.UserHandle, TEXT("Attempt to use slot %s from handle %s but already assigned to %s"),
			*LexToString(Slot), *LexToString(ClaimHandle), *LexToString(Slot.User)))
	{
		Slot.State = ESmartObjectSlotState::Occupied;
		OnSlotChanged(SmartObjectRuntime, Slot, ClaimHandle.SlotHandle, ESmartObjectChangeReason::OnOccupied, Slot.UserData);
		return BehaviorDefinition;
	}

	return nullptr;
}

bool USmartObjectSubsystem::MarkSlotAsFree(const FSmartObjectClaimHandle& ClaimHandle)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (!GetValidatedMutableRuntimeAndSlot(ClaimHandle.SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return false;
	}

	// Keep user data to be used as payload in the notification event
	// since it will be released by the following call to Slot.Release 
	const FInstancedStruct Payload(MoveTemp(Slot->UserData));
	const bool bSuccess = Slot->Release(ClaimHandle, /*bAborted*/ false);
	if (bSuccess)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Released using handle %s"), *LexToString(ClaimHandle));
		UE_VLOG_LOCATION(this, LogSmartObject, Display, GetSlotLocation(ClaimHandle).GetValue(), 50.f, FColor::White, TEXT("Released"));
		OnSlotChanged(*SmartObjectRuntime, *Slot, ClaimHandle.SlotHandle, ESmartObjectChangeReason::OnReleased, Payload);
	}

	return bSuccess;
}

ESmartObjectSlotState USmartObjectSubsystem::GetSlotState(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Slot->GetState();
	}
	return ESmartObjectSlotState::Invalid;
}

bool USmartObjectSubsystem::GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle, FVector& OutSlotLocation) const
{
	const TOptional<FVector> OptionalLocation = GetSlotLocation(ClaimHandle);
	OutSlotLocation = OptionalLocation.Get(FVector::ZeroVector);
	return OptionalLocation.IsSet();
}

TOptional<FVector> USmartObjectSubsystem::GetSlotLocation(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return SmartObjectRuntime->Transform.TransformPosition(FVector(Slot->Offset));
	}
	return {};
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
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Slot->GetSlotWorldTransform(SmartObjectRuntime->Transform);
	}
	return {};
}

FTransform USmartObjectSubsystem::GetSlotTransformChecked(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	check(GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)));
	return Slot->GetSlotLocalTransform() * SmartObjectRuntime->Transform;
}

bool USmartObjectSubsystem::GetValidatedMutableRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, FSmartObjectRuntime*& OutSmartObjectRuntime, FSmartObjectRuntimeSlot*& OutSlot, const TCHAR* Context)
{
	const FSmartObjectRuntime* ConstSmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* ConstSlot = nullptr;

	if (GetValidatedRuntimeAndSlot(SlotHandle, ConstSmartObjectRuntime, ConstSlot, Context))
	{
		OutSmartObjectRuntime = const_cast<FSmartObjectRuntime*>(ConstSmartObjectRuntime);
		OutSlot = const_cast<FSmartObjectRuntimeSlot*>(ConstSlot);
		return true;
	}

	OutSmartObjectRuntime = nullptr;
	OutSlot = nullptr;
	return false;
}

bool USmartObjectSubsystem::GetValidatedRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, const FSmartObjectRuntime*& OutSmartObjectRuntime, const FSmartObjectRuntimeSlot*& OutSlot, const TCHAR* Context) const
{
	if (SlotHandle.IsValid())
	{
		if (const FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SlotHandle.GetSmartObjectHandle()))
		{
			if (SmartObjectRuntime->Slots.IsValidIndex(SlotHandle.GetSlotIndex()))
			{
				OutSmartObjectRuntime = SmartObjectRuntime;
				OutSlot = SmartObjectRuntime != nullptr ? &SmartObjectRuntime->Slots[SlotHandle.GetSlotIndex()] : nullptr;	
				return true;
			}

			UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("%s Invalid slot index %d (%d slots)."), Context, SlotHandle.GetSlotIndex(), SmartObjectRuntime->Slots.Num());
		}
		else
		{
			UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("%s failed using handle '%s'. SmartObject is no longer part of the simulation."), Context, *LexToString(SlotHandle));
		}
	}
	else
	{
		UE_VLOG_UELOG(this, LogSmartObject, Log, TEXT("%s failed. Handle is not set."), Context);
	}

	OutSmartObjectRuntime = nullptr;
	OutSlot = nullptr;
	return false;
}

FSmartObjectRuntime* USmartObjectSubsystem::GetValidatedMutableRuntime(const FSmartObjectHandle Handle, const TCHAR* Context) const
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

FOnSmartObjectEvent* USmartObjectSubsystem::GetEventDelegate(const FSmartObjectHandle SmartObjectHandle)
{
	if (FSmartObjectRuntime* SmartObjectRuntime = GetValidatedMutableRuntime(SmartObjectHandle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return &SmartObjectRuntime->GetMutableEventDelegate();
	}

	return nullptr;
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

const FGameplayTagContainer& USmartObjectSubsystem::GetSlotTags(const FSmartObjectSlotHandle SlotHandle) const
{
	static const FGameplayTagContainer EmptyTags;

	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Slot->Tags;
	}

	return EmptyTags;
}


void USmartObjectSubsystem::AddTagToSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		if (!Slot->Tags.HasTag(Tag))
		{
			Slot->Tags.AddTagFast(Tag);
			OnSlotChanged(*SmartObjectRuntime, *Slot, SlotHandle, ESmartObjectChangeReason::OnTagAdded, Slot->GetUserData(), Tag);
		}
	}
}

bool USmartObjectSubsystem::RemoveTagFromSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return false;
	}
	
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		if (Slot->Tags.RemoveTag(Tag))
		{
			OnSlotChanged(*SmartObjectRuntime, *Slot, SlotHandle, ESmartObjectChangeReason::OnTagRemoved, Slot->GetUserData(), Tag);
			return true;
		}
	}
	return false;
}

bool USmartObjectSubsystem::SetSlotEnabled(const FSmartObjectSlotHandle SlotHandle, const bool bEnabled)
{
	bool bPreviousValue = false;

	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		// Using 'IsEnabled' that combines both slot and smart object enabled state
		bPreviousValue = Slot->IsEnabled();

		// Always set slot enabled state even if combined result might not be affected
		Slot->bSlotEnabled = bEnabled;

		// Using new combined value to detect changes
		if (Slot->IsEnabled() != bPreviousValue)
		{
			OnSlotChanged(*SmartObjectRuntime, *Slot, SlotHandle,
				Slot->IsEnabled() ? ESmartObjectChangeReason::OnSlotEnabled : ESmartObjectChangeReason::OnSlotDisabled, Slot->UserData);
		}
	}

	return bPreviousValue;
}

bool USmartObjectSubsystem::SendSlotEvent(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag EventTag, const FConstStructView Payload)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		// Runtime slot lifetime is bound to the runtime smart object so it should always be available.
		if (SmartObjectRuntime->GetEventDelegate().IsBound())
		{
			FSmartObjectEventData Data;
			Data.SmartObjectHandle = SlotHandle.GetSmartObjectHandle();
			Data.SlotHandle = SlotHandle;
			Data.Reason = ESmartObjectChangeReason::OnEvent;
			Data.Tag = EventTag;
			Data.EventPayload = Payload;
			SmartObjectRuntime->GetEventDelegate().Broadcast(Data);
			return true;
		}
	}
	return false;
}

void USmartObjectSubsystem::AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
{
	if (!SmartObjectRuntime.Tags.HasTag(Tag))
	{
		SmartObjectRuntime.Tags.AddTagFast(Tag);

		FSmartObjectEventData Data;
		Data.SmartObjectHandle = SmartObjectRuntime.GetRegisteredHandle();
		Data.Reason = ESmartObjectChangeReason::OnTagAdded;
		Data.Tag = Tag;
		SmartObjectRuntime.OnEvent.Broadcast(Data);
	}
}

void USmartObjectSubsystem::RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag)
{
	if (SmartObjectRuntime.Tags.RemoveTag(Tag))
	{
		FSmartObjectEventData Data;
		Data.SmartObjectHandle = SmartObjectRuntime.GetRegisteredHandle();
		Data.Reason = ESmartObjectChangeReason::OnTagRemoved;
		Data.Tag = Tag;
		SmartObjectRuntime.OnEvent.Broadcast(Data);
	}
}

void USmartObjectSubsystem::OnSlotChanged(
	const FSmartObjectRuntime& SmartObjectRuntime,
	const FSmartObjectRuntimeSlot& Slot,
	const FSmartObjectSlotHandle SlotHandle,
	const ESmartObjectChangeReason Reason,
	const FConstStructView Payload,		
	const FGameplayTag ChangedTag
	)
{
	if (SmartObjectRuntime.GetEventDelegate().IsBound())
	{
		FSmartObjectEventData Data;
		Data.SmartObjectHandle = SlotHandle.GetSmartObjectHandle();
		Data.SlotHandle = SlotHandle;
		Data.Reason = Reason;
		Data.Tag = ChangedTag;
		Data.EventPayload = Payload;
		SmartObjectRuntime.GetEventDelegate().Broadcast(Data);
	}
}

void USmartObjectSubsystem::RegisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle, const FOnSlotInvalidated& Callback)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(ClaimHandle.SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		Slot->OnSlotInvalidatedDelegate = Callback;
	}
}

void USmartObjectSubsystem::UnregisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(ClaimHandle.SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		Slot->OnSlotInvalidatedDelegate.Unbind();
	}
}

FOnSmartObjectEvent* USmartObjectSubsystem::GetSlotEventDelegate(const FSmartObjectSlotHandle SlotHandle)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedMutableRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return &SmartObjectRuntime->GetMutableEventDelegate();
	}
	return nullptr;
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectSubsystem::DebugDraw(FDebugRenderSceneProxy* DebugProxy) const
{
	if (!bRuntimeInitialized)
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

void USmartObjectSubsystem::AddSlotData(const FSmartObjectClaimHandle& ClaimHandle, const FConstStructView InData)
{
	FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	FSmartObjectRuntimeSlot* Slot = nullptr;
	if (!GetValidatedMutableRuntimeAndSlot(ClaimHandle.SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return;
	}

	// If we have a data of same type, override, else add.
	bool bFound = false;
	for (FStructView Data : Slot->StateData)
	{
		if (Data.GetScriptStruct() == InData.GetScriptStruct())
		{
			Data.GetScriptStruct()->CopyScriptStruct(Data.GetMemory(), InData.GetMemory());
			bFound = true;
			break;;
		}
	}

	if (!bFound)
	{
		Slot->StateData.Append({ InData });
	}
}

FSmartObjectSlotView USmartObjectSubsystem::GetSlotView(const FSmartObjectSlotHandle SlotHandle) const
{
	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return FSmartObjectSlotView(SlotHandle, *const_cast<FSmartObjectRuntime*>(SmartObjectRuntime), *const_cast<FSmartObjectRuntimeSlot*>(Slot));
	}
	return {};
}

void USmartObjectSubsystem::FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutSlots, const FConstStructView UserData) const
{
	if (const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		FindSlots(Handle, *SmartObjectRuntime, Filter, OutSlots, UserData);
	}
}

void USmartObjectSubsystem::GetAllSlots(const FSmartObjectHandle Handle, TArray<FSmartObjectSlotHandle>& OutSlots) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SmartObject_FilterSlots");

	OutSlots.Reset();
	if (const FSmartObjectRuntime* SmartObjectRuntime = GetValidatedRuntime(Handle, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		OutSlots.Reserve(SmartObjectRuntime->Slots.Num());
		for (int32 Index = 0; Index < SmartObjectRuntime->Slots.Num(); Index++)
		{
			OutSlots.Add(FSmartObjectSlotHandle(Handle, Index));
		}
	}
}

bool USmartObjectSubsystem::EvaluateConditionsForFiltering(
	const FSmartObjectRuntime& SmartObjectRuntime,
	const FSmartObjectSlotHandle SlotHandle,
	FWorldConditionContextData& ContextData,
	const FConstStructView UserData,
	TPair<const FSmartObjectRuntime*, bool>& LastEvaluatedRuntime
	) const
{
	// Evaluate preconditions on the parent object only once if all slots have the same one (usual case)  
	if (&SmartObjectRuntime != LastEvaluatedRuntime.Key)
	{
		LastEvaluatedRuntime.Key = &SmartObjectRuntime;
			
		// Set context schema and bind user data only if not set or changed 
		const UWorldConditionSchema* PrevSchema = ContextData.GetSchema();
		if (PrevSchema == nullptr || PrevSchema != SmartObjectRuntime.GetDefinition().GetWorldConditionSchema())
		{
			ContextData.SetSchema(*SmartObjectRuntime.GetDefinition().GetWorldConditionSchema());

			// Setup some context data using user data
			BindPropertiesFromStruct(ContextData, UserData);	
		}

		// Setup system related data (object runtime, slot, subsystem, etc.)
		SetupConditionContextCommonData(ContextData, SmartObjectRuntime);

		// Evaluate object conditions.
		LastEvaluatedRuntime.Value = EvaluateObjectConditions(ContextData, SmartObjectRuntime);
	}

	// Evaluate slot conditions only if parent runtime passed its own selection conditions
	return LastEvaluatedRuntime.Value ? EvaluateSlotConditions(ContextData, SmartObjectRuntime, SlotHandle) : false;
}

TArray<FSmartObjectSlotHandle> USmartObjectSubsystem::FilterSlotsBySelectionConditions(
	const TConstArrayView<FSmartObjectSlotHandle>& SlotsToFilter,
	const FConstStructView UserData
	) const
{
	TArray<FSmartObjectSlotHandle> Result;
	Result.Reserve(SlotsToFilter.Num());

	FWorldConditionContextData ContextData;	
	TPair<const FSmartObjectRuntime*, bool> LastEvaluatedSmartObjectRuntime = {nullptr, false};

	const FSmartObjectRuntime* CurrentRuntime = nullptr;
	FSmartObjectHandle CurrentRuntimeHandle = {};
	
	bool bSlotIsValid = false;
	for (const FSmartObjectSlotHandle SlotHandle : SlotsToFilter)
	{
		if (!CurrentRuntime || CurrentRuntimeHandle != SlotHandle.GetSmartObjectHandle())
		{
			CurrentRuntime = GetValidatedRuntime(SlotHandle.GetSmartObjectHandle(), ANSI_TO_TCHAR(__FUNCTION__));
			CurrentRuntimeHandle = SlotHandle.GetSmartObjectHandle();
		}
		if (!CurrentRuntime || !CurrentRuntime->Slots.IsValidIndex(SlotHandle.GetSlotIndex()))
		{
			bSlotIsValid = false;
		}
		UE_CVLOG_UELOG(bSlotIsValid == false, this, LogSmartObject, Log,
			TEXT("%hs failed using handle '%s'. Slot is no longer part of the simulation. Consider calling IsSmartObjectSlotValid to avoid this message."),
			__FUNCTION__, *LexToString(SlotHandle));

		if (bSlotIsValid && EvaluateConditionsForFiltering(*CurrentRuntime, SlotHandle, ContextData, UserData, LastEvaluatedSmartObjectRuntime))
		{
			Result.Add(SlotHandle);
		}
	}

	Result.Shrink();
	return MoveTemp(Result);
}

TArray<FSmartObjectRequestResult> USmartObjectSubsystem::FilterResultsBySelectionConditions(
	const TConstArrayView<FSmartObjectRequestResult>& ResultsToFilter,
	const FConstStructView UserData
	) const
{
	TArray<FSmartObjectRequestResult> Result;
	Result.Reserve(ResultsToFilter.Num());

	FWorldConditionContextData ContextData;	
	TPair<const FSmartObjectRuntime*, bool> LastEvaluatedSmartObjectRuntime = {nullptr, false};

	const FSmartObjectRuntime* CurrentRuntime = nullptr;
	FSmartObjectHandle CurrentRuntimeHandle = {};

	bool bSlotIsValid = false;
	for (const FSmartObjectRequestResult RequestResult : ResultsToFilter)
	{
		if (!CurrentRuntime || CurrentRuntimeHandle != RequestResult.SlotHandle.GetSmartObjectHandle())
		{
			CurrentRuntime = GetValidatedRuntime(RequestResult.SlotHandle.GetSmartObjectHandle(), ANSI_TO_TCHAR(__FUNCTION__));
			CurrentRuntimeHandle = RequestResult.SlotHandle.GetSmartObjectHandle();
		}
		if (!CurrentRuntime || !CurrentRuntime->Slots.IsValidIndex(RequestResult.SlotHandle.GetSlotIndex()))
		{
			bSlotIsValid = false;
		}

		UE_CVLOG_UELOG(bSlotIsValid == false, this, LogSmartObject, Log,
			TEXT("%hs failed using handle '%s'. Slot is no longer part of the simulation. Consider calling IsSmartObjectSlotValid to avoid this message."),
			__FUNCTION__, *LexToString(RequestResult.SlotHandle));

		if (bSlotIsValid && EvaluateConditionsForFiltering(*CurrentRuntime, RequestResult.SlotHandle, ContextData, UserData, LastEvaluatedSmartObjectRuntime))
		{
			Result.Add(RequestResult);
		}
	}

	Result.Shrink();
	return MoveTemp(Result);
}

bool USmartObjectSubsystem::EvaluateSelectionConditions(const FSmartObjectSlotHandle SlotHandle, const FConstStructView UserData) const
{
	FWorldConditionContextData ContextData;
	TPair<const FSmartObjectRuntime*, bool> LastEvaluatedSmartObjectRuntime = {nullptr, false};

	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return EvaluateConditionsForFiltering(*SmartObjectRuntime, SlotHandle, ContextData, UserData, LastEvaluatedSmartObjectRuntime);
	}

	return false;
}

bool USmartObjectSubsystem::FindEntranceLocationForSlot(const FSmartObjectSlotHandle SlotHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const
{
	return FindEntranceLocationInternal(SlotHandle, FSmartObjectSlotEntranceHandle(), Request, Result);
}

bool USmartObjectSubsystem::UpdateEntranceLocation(const FSmartObjectSlotEntranceHandle EntranceHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const
{
	return FindEntranceLocationInternal(EntranceHandle.GetSlotHandle(), EntranceHandle, Request, Result);
}

bool USmartObjectSubsystem::FindEntranceLocationInternal(
	const FSmartObjectSlotHandle SlotHandle,
	const FSmartObjectSlotEntranceHandle SlotEntranceHandle,
	const FSmartObjectSlotEntranceLocationRequest& Request,
	FSmartObjectSlotEntranceLocationResult& Result
	) const
{
	Result = {};

	const FSmartObjectRuntime* SmartObjectRuntime = nullptr;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
	if (!GetValidatedRuntimeAndSlot(SlotHandle, SmartObjectRuntime, Slot, ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return false;
	}

	UWorld* World = GetWorld();

	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilterClass = Request.ValidationFilter;
	const ANavigationData* NavigationData = Request.NavigationData;
	
	if (Request.UserActor)
	{
		// If user actor is present, try to query some data automatically from interfaces and components. 
		if (!ValidationFilterClass.Get())
		{
			if (const USmartObjectUserComponent* UserComponent = Request.UserActor->GetComponentByClass<USmartObjectUserComponent>())
			{
				ValidationFilterClass = UserComponent->GetValidationFilter();
			}
		}

		if (!NavigationData)
		{
			NavigationData = UE::SmartObject::Annotations::GetNavDataForActor(*World, Request.UserActor);
		}
	}

	if (!ValidationFilterClass.Get())
	{
		// @todo: restore this as an error.
		UE_VLOG_UELOG(this, LogSmartObject, Warning,
			TEXT("%hs: Slot %s, Invalid validation filter for user actor %s."),
			__FUNCTION__, *LexToString(SlotHandle), *GetNameSafe(Request.UserActor));
		// return false;
	}
	
	const USmartObjectSlotValidationFilter* ValidationFilter = ValidationFilterClass.Get() ? ValidationFilterClass.GetDefaultObject() : GetDefault<USmartObjectSlotValidationFilter>();
	check(ValidationFilter);
	const FSmartObjectSlotValidationParams& ValidationParams = ValidationFilter->GetValidationParams(Request.LocationType);

	FSmartObjectUserCapsuleParams UserCapsule;
	const bool bRequireValidUserCapsule = Request.bCheckSlotLocationOverlap || Request.bCheckEntranceLocationOverlap;
	if (bRequireValidUserCapsule)
	{
		if (Request.UserCapsule.IsSet())
		{
			UserCapsule = ValidationParams.GetUserCapsule(Request.UserCapsule.GetValue());
		}
		else
		{
			if (Request.UserActor)
			{
				if (!ValidationParams.GetUserCapsuleForActor(*Request.UserActor, UserCapsule))
				{
					UE_VLOG_UELOG(this, LogSmartObject, Error,
						TEXT("%hs: Slot %s, Could not resolve user capsule size. Failed to access navigation parameters for user actor %s."),
						__FUNCTION__, *LexToString(SlotHandle), *GetNameSafe(Request.UserActor));
					return false;
				}
			}
			else
			{
				UE_VLOG_UELOG(this, LogSmartObject, Error,
					TEXT("%hs: Slot %s, Could not resolve user capsule size. Request's UserCapsule is expected to be set when user actor is not specified."),
					__FUNCTION__, *LexToString(SlotHandle));
				return false;
			}
		}
	}

	// Navdata must be valid when checking testing for navigable. 
	if (Request.bProjectNavigationLocation && !NavigationData)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error,
			TEXT("%hs: Slot %s, ProjectNavigationLocation is requested, expecting valid navigation data, NavigationData is not set."),
			__FUNCTION__, *LexToString(SlotHandle));
		return false;
	}

	FSharedConstNavQueryFilter NavigationFilter;
	if (NavigationData)
	{
		if (ValidationParams.GetNavigationFilter().Get())
		{
			NavigationFilter = UNavigationQueryFilter::GetQueryFilter(*NavigationData, Request.UserActor, ValidationParams.GetNavigationFilter());
			if (!NavigationFilter.IsValid())
			{
				UE_VLOG_UELOG(this, LogSmartObject, Error,
					TEXT("%hs: Slot %s, Navigation filter was specified was failed to resolve it."),
					__FUNCTION__, *LexToString(SlotHandle));
				return false;
			}
		}
	}

	struct FSlotEntryCandidate
	{
		FVector Location;
		FRotator Rotation;
		NavNodeRef NodeRef;
		FVector::FReal DistanceSqr = 0.0;
		const FSmartObjectSlotEntranceAnnotation* EntranceAnnotation = nullptr;
		ESmartObjectEntrancePriority SelectionPriority = ESmartObjectEntrancePriority::Normal;
		bool bTraceGroundLocation = false;
		bool bCheckTransitionTrajectory = false;
		FSmartObjectSlotEntranceHandle Handle;
	};

	const FSmartObjectAnnotation_SlotUserCollision* SlotUserCollisionAnnotation = nullptr; // @todo: currently assumes only one.

	TArray<FSlotEntryCandidate, TInlineAllocator<8>> Candidates;
	const FTransform& SlotTransform = Slot->GetSlotWorldTransform(SmartObjectRuntime->Transform);

	const bool bIncludeEntries = Request.LocationType == ESmartObjectSlotNavigationLocationType::Entry;
	const bool bIncludeExits = Request.LocationType == ESmartObjectSlotNavigationLocationType::Exit;

	const FSmartObjectSlotDefinition& SlotDefinition = SmartObjectRuntime->GetDefinition().GetSlot(SlotHandle.GetSlotIndex());
	
	for (TConstEnumerateRef<const FInstancedStruct> Data : EnumerateRange(SlotDefinition.Data))
	{
		if (const FSmartObjectSlotEntranceAnnotation* EntranceAnnotation = Data->GetPtr<FSmartObjectSlotEntranceAnnotation>())
		{
			// If specific entry location was requested and this is not the one, skip it.
			if (SlotEntranceHandle.Type == FSmartObjectSlotEntranceHandle::EType::Entrance
				&& SlotEntranceHandle.Index != Data.GetIndex())
			{
				continue;
			}
			
			if ((EntranceAnnotation->bIsEntry == bIncludeEntries
					|| EntranceAnnotation->bIsExit == bIncludeExits)
				&& EntranceAnnotation->HasTransform())
			{
				const FTransform EntryTransform = EntranceAnnotation->GetAnnotationWorldTransform(SlotTransform);
				FSlotEntryCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.Location = EntryTransform.GetLocation();
				Candidate.Rotation = EntryTransform.GetRotation().Rotator();
				Candidate.EntranceAnnotation = EntranceAnnotation;
				Candidate.bTraceGroundLocation = EntranceAnnotation->bTraceGroundLocation;
				Candidate.bCheckTransitionTrajectory = EntranceAnnotation->bCheckTransitionTrajectory;
				Candidate.SelectionPriority = EntranceAnnotation->SelectionPriority;
				Candidate.Handle = FSmartObjectSlotEntranceHandle(SlotHandle, FSmartObjectSlotEntranceHandle::EType::Entrance, Data.GetIndex());
			}
		}
		else if (const FSmartObjectAnnotation_SlotUserCollision* UserCollisionAnnotation = Data->GetPtr<FSmartObjectAnnotation_SlotUserCollision>())
		{
			SlotUserCollisionAnnotation = UserCollisionAnnotation;
		}
	}

	if ((Candidates.IsEmpty() && Request.bUseSlotLocationAsFallback)
		|| SlotEntranceHandle.Type == FSmartObjectSlotEntranceHandle::EType::Slot)
	{
		FSlotEntryCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Location = SlotTransform.GetLocation();
		Candidate.Rotation = SlotTransform.GetRotation().Rotator();
		Candidate.Handle = FSmartObjectSlotEntranceHandle(SlotHandle, FSmartObjectSlotEntranceHandle::EType::Slot);
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	// Sort candidates so that the best candidate is first.
	if (Candidates.Num() > 1)
	{
		if (Request.SelectMethod == FSmartObjectSlotEntrySelectionMethod::NearestToSearchLocation)
		{
			for (FSlotEntryCandidate& Candidate : Candidates)
			{
				Candidate.DistanceSqr = FVector::DistSquared(Request.SearchLocation, Candidate.Location);
			}
			
			Candidates.Sort([](const FSlotEntryCandidate& A, const FSlotEntryCandidate& B)
			{
				if (A.SelectionPriority == B.SelectionPriority)
				{
					return A.DistanceSqr < B.DistanceSqr;
				}
				return A.SelectionPriority > B.SelectionPriority;
			});
		}
		else
		{
			// Use stable sort to keep initial order.
			Candidates.StableSort([](const FSlotEntryCandidate& A, const FSlotEntryCandidate& B)
			{
				return A.SelectionPriority > B.SelectionPriority;
			});
		}
	}

	check(Candidates.Num() > 0);

	const FSmartObjectTraceParams& GroundTraceParameters = ValidationParams.GetGroundTraceParameters();
	const FSmartObjectTraceParams& TransitionTraceParameters = ValidationParams.GetTransitionTraceParameters();
	const FVector SearchExtents = FVector(ValidationParams.GetSearchExtents());

	FCollisionQueryParams GroundTraceQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), GroundTraceParameters.bTraceComplex);
	FCollisionQueryParams TransitionTraceQueryParams(SCENE_QUERY_STAT(SmartObjectTrace), TransitionTraceParameters.bTraceComplex);

	GroundTraceQueryParams.bIgnoreTouches = true;
	TransitionTraceQueryParams.bIgnoreTouches = true;
	
	if (const AActor* SmartObjectActor = SmartObjectRuntime->GetOwnerActor())
	{
		GroundTraceQueryParams.AddIgnoredActor(SmartObjectActor);
		TransitionTraceQueryParams.AddIgnoredActor(SmartObjectActor);
	}
	if (Request.UserActor)
	{
		GroundTraceQueryParams.AddIgnoredActor(Request.UserActor);
		TransitionTraceQueryParams.AddIgnoredActor(Request.UserActor);
	}

	// If the slot location should be free of collisions, check it now since it's shared for all entries.
	if (Request.bCheckSlotLocationOverlap
		&& SlotUserCollisionAnnotation)
	{
		TArray<FSmartObjectAnnotationCollider> Colliders;
		SlotUserCollisionAnnotation->GetColliders(UserCapsule, SlotTransform, Colliders);

		if (UE::SmartObject::Annotations::TestCollidersOverlap(*World, Colliders, TransitionTraceParameters, TransitionTraceQueryParams))
		{
			return false;
		}
	}
	
	bool bHasResult = false;

	// Candidates are now in order of preference. Test each for hard requirements and pick the first that passes all checks. 
	for (FSlotEntryCandidate& Candidate : Candidates)
	{
		const FBox SearchBounds(Candidate.Location - SearchExtents, Candidate.Location + SearchExtents);

		// Check and adjust the location on navigable space.
		if (Request.bProjectNavigationLocation)
		{
			FNavLocation NavLocation;
			if (!UE::SmartObject::Annotations::ProjectNavigationLocation(*NavigationData, Candidate.Location, SearchBounds, NavigationFilter, Request.UserActor, NavLocation))
			{
				// If no navigable area found, skip the candidate.
				continue;
			}
			Candidate.Location = NavLocation.Location;
			Candidate.NodeRef = NavLocation.NodeRef;
		}

		// Check that the entry location is free of collisions if requested.
		if (Request.bCheckEntranceLocationOverlap)
		{
			const FSmartObjectAnnotationCollider Collider = UserCapsule.GetAsCollider(Candidate.Location, Candidate.Rotation.Quaternion());
			if (UE::SmartObject::Annotations::TestCollidersOverlap(*World, { Collider }, TransitionTraceParameters, TransitionTraceQueryParams))
			{
				// If the colliders overlap, skip the candidate.
				continue;
			}
		}

		// Check and adjust the location on ground.
		if (Request.bTraceGroundLocation
			&& Candidate.bTraceGroundLocation)
		{
			FVector GroundLocation;
			if (!UE::SmartObject::Annotations::TraceGroundLocation(*World, Candidate.Location, SearchBounds, GroundTraceParameters, GroundTraceQueryParams, GroundLocation))
			{
				// If not ground location found, skip the candidate.
				continue;
			}

			Candidate.Location = GroundLocation;
		}

		// Check that there's no collision during transition to slot location.
 		if (Request.bCheckTransitionTrajectory
 			&& Candidate.bCheckTransitionTrajectory
			&& Candidate.EntranceAnnotation)
		{
 			// @todo: we're currently _not_ using the adjusted location (Candidate.Location), consider if we should.
			TArray<FSmartObjectAnnotationCollider> Colliders;
			Candidate.EntranceAnnotation->GetTrajectoryColliders(SlotTransform, Colliders);

 			if (UE::SmartObject::Annotations::TestCollidersOverlap(*World, Colliders, TransitionTraceParameters, TransitionTraceQueryParams))
 			{
	 			// If the colliders overlap, skip the candidate.
 				continue;
 			}
		}

		// Passed all validation, use it as the result.

		Result.Location = Candidate.Location;
		Result.Rotation = Candidate.Rotation;
		Result.NodeRef = INVALID_NAVNODEREF;
		if (Candidate.EntranceAnnotation)
		{
			Result.Tags = Candidate.EntranceAnnotation->Tags;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Result.Tag = Result.Tags.First();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (Request.LocationType == ESmartObjectSlotNavigationLocationType::Exit)
		{
			// Reverse direction for exits.
			Result.Rotation = Result.Rotation.Add(0.0, 180.0, 0.0).Clamp();
		}

		Result.EntranceHandle = Candidate.Handle;
		
		bHasResult = true;
		
		break;
	}

	return bHasResult;
}

void USmartObjectSubsystem::FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutResults, const FConstStructView UserData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SmartObject_FilterSlots");

	// Use the high level flag, no need to dig into each slot state since they are also all disabled.
	if (!SmartObjectRuntime.IsEnabled())
	{
		return;
	}

	const USmartObjectDefinition& Definition = SmartObjectRuntime.GetDefinition();
	const int32 NumSlots = Definition.GetSlots().Num();
	checkf(NumSlots > 0, TEXT("Definition should contain slot definitions at this point"));
	checkf(SmartObjectRuntime.Slots.Num() == NumSlots, TEXT("Number of runtime slot handles should match number of slot definitions"));

	// Applying caller's predicate
	if (Filter.Predicate && !Filter.Predicate(SmartObjectRuntime.GetRegisteredHandle()))
	{
		return;
	}

	// Apply definition level filtering (Tags and BehaviorDefinition)
	// This could be improved to cache results between a single query against multiple instances of the same definition
	TArray<int32> ValidSlotIndices;
	FindMatchingSlotDefinitionIndices(Definition, Filter, ValidSlotIndices);

	FWorldConditionContextData ConditionContextData;
	ConditionContextData.SetSchema(*Definition.GetWorldConditionSchema());

	// Setup default data
	SetupConditionContextCommonData(ConditionContextData, SmartObjectRuntime);

	// Setup additional data related to requester
	BindPropertiesFromStruct(ConditionContextData, UserData);

	// Check object conditions.
	if (Filter.bShouldEvaluateConditions && !EvaluateObjectConditions(ConditionContextData, SmartObjectRuntime))
	{
		return;
	}
	
	// Build list of available slot indices (filter out occupied or reserved slots or disabled slots)
	for (const int32 SlotIndex : ValidSlotIndices)
	{
		const FSmartObjectRuntimeSlot& RuntimeSlot = SmartObjectRuntime.GetSlot(SlotIndex);
		if (!RuntimeSlot.CanBeClaimed())
		{
			continue;
		}
		const FSmartObjectSlotHandle SlotHandle(Handle, SlotIndex);
		
		// Check slot conditions.
		if (Filter.bShouldEvaluateConditions && !EvaluateSlotConditions(ConditionContextData, SmartObjectRuntime, SlotHandle))
		{
			continue;
		}

		OutResults.Add(SlotHandle);
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
			&& Definition.GetBehaviorDefinition(i, Filter.BehaviorDefinitionClass) == nullptr)
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
				if (Definition.GetBehaviorDefinition(i, BehaviorDefinitionClass) != nullptr)
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

FSmartObjectRequestResult USmartObjectSubsystem::FindSmartObject(const FSmartObjectRequest& Request, const FConstStructView UserData) const
{
	TArray<FSmartObjectRequestResult> Results;
	FindSmartObjects(Request, Results, UserData);

	return Results.Num() ? Results.Top() : FSmartObjectRequestResult();
}

bool USmartObjectSubsystem::FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const FConstStructView UserData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SmartObject_FindAllResults");

	if (!bRuntimeInitialized)
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
		FindSlots(SmartObjectHandle, *SmartObjectRuntime, Filter, SlotHandles, UserData);
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
	for (TActorIterator<ASmartObjectPersistentCollection> It(GetWorld()); It; ++It)
	{
		ASmartObjectPersistentCollection* Collection = (*It);
		if (IsValid(Collection) && Collection->IsRegistered() == false)
		{
			const ESmartObjectCollectionRegistrationResult Result = RegisterCollection(*Collection);
			UE_VLOG_UELOG(Collection, LogSmartObject, Log,
				TEXT("Collection '%s' registration from USmartObjectSubsystem initialization - %s"), *Collection->GetFullName(), *UEnum::GetValueAsString(Result));
		}
	}
}

ESmartObjectCollectionRegistrationResult USmartObjectSubsystem::RegisterCollection(ASmartObjectPersistentCollection& InCollection)
{
	if (!IsValid(&InCollection))
	{
		return ESmartObjectCollectionRegistrationResult::Failed_InvalidCollection;
	}

	if (InCollection.IsRegistered())
	{
		UE_VLOG_UELOG(&InCollection, LogSmartObject, Error, TEXT("Trying to register collection '%s' more than once"), *InCollection.GetFullName());
		return ESmartObjectCollectionRegistrationResult::Failed_AlreadyRegistered;
	}

	ESmartObjectCollectionRegistrationResult Result = ESmartObjectCollectionRegistrationResult::Succeeded;

	UE_VLOG_UELOG(&InCollection, LogSmartObject, Log, TEXT("Adding collection '%s' registered with %d entries"), *InCollection.GetName(), InCollection.GetEntries().Num());

	InCollection.GetMutableSmartObjectContainer().ValidateDefinitions();

	SmartObjectContainer.Append(InCollection.GetSmartObjectContainer());
	RegisteredCollections.Add(&InCollection);

	// We want to add the new collection to the "simulation" only if the Runtime part of the subsystem has been initialized.
	// SmartObjectContainer is added to simulation in one go in InitializeRuntime.
	if (bRuntimeInitialized)
	{
		AddContainerToSimulation(InCollection.GetSmartObjectContainer());
	}

#if WITH_EDITOR
	// Broadcast after rebuilding so listeners will be able to access up-to-date data
	OnMainCollectionChanged.Broadcast();
#endif // WITH_EDITOR

	InCollection.OnRegistered();
	Result = ESmartObjectCollectionRegistrationResult::Succeeded;

	return Result;
}

void USmartObjectSubsystem::UnregisterCollection(ASmartObjectPersistentCollection& InCollection)
{
	if (RegisteredCollections.Remove(&InCollection))
	{
		SmartObjectContainer.Remove(InCollection.GetSmartObjectContainer());

		for (const FSmartObjectCollectionEntry& Entry : InCollection.GetSmartObjectContainer().GetEntries())
		{
			FSmartObjectRuntime SORuntime;
			// even though we did add this entry to RuntimeSmartObjects at some point it could have been removed 
			// when the smart object in question got disabled or removed
			if (RuntimeSmartObjects.RemoveAndCopyValue(Entry.GetHandle(), SORuntime))
			{
				if (USmartObjectComponent* SOComponent = Entry.GetComponent())
				{
					UnbindComponentFromSimulationInternal(*SOComponent, SORuntime);
				}
				DestroyRuntimeInstanceInternal(Entry.GetHandle(), SORuntime);
			}
		}
		
		InCollection.OnUnregistered();
	}
	else
	{
		UE_VLOG_UELOG(&InCollection, LogSmartObject, Verbose,
			TEXT("Ignoring unregistration of collection '%s' since this is not one of the previously registered collections."), *InCollection.GetFullName());
		return;
	}
}

void USmartObjectSubsystem::AddContainerToSimulation(const FSmartObjectContainer& InSmartObjectContainer)
{
	if (!ensureMsgf(bRuntimeInitialized, TEXT("%s called before InitializeRuntime, this is not expected to happen."), ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return;
	}

	for (const FSmartObjectCollectionEntry& Entry : InSmartObjectContainer.GetEntries())
	{
		const USmartObjectDefinition* Definition = InSmartObjectContainer.GetDefinitionForEntry(Entry);
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
			Component->SetRegisteredHandle(Entry.GetHandle(), ESmartObjectRegistrationType::WithCollection);
			AddComponentToSimulation(*Component, Entry);
		}
		else
		{
			// Otherwise we create the runtime instance based on the information from the collection and component will be bound later (e.g. on load)
			AddCollectionEntryToSimulation(Entry, *Definition, nullptr);
		}
	}
}

USmartObjectComponent* USmartObjectSubsystem::GetSmartObjectComponent(const FSmartObjectClaimHandle& ClaimHandle) const
{
	return SmartObjectContainer.GetSmartObjectComponent(ClaimHandle.SmartObjectHandle);
}

USmartObjectComponent* USmartObjectSubsystem::GetSmartObjectComponentByRequestResult(const FSmartObjectRequestResult& Result) const
{
	return SmartObjectContainer.GetSmartObjectComponent(Result.SmartObjectHandle);
}

void USmartObjectSubsystem::InitializeRuntime()
{
	if (UE::SmartObject::bDisableRuntime)
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("Runtime explicitly disabled by CVar. Initialization skipped in %s."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	// Initialize spatial representation structure
	checkfSlow(*SpacePartitionClass != nullptr, TEXT("Partition class is expected to be valid since we use the plugins default in OnWorldComponentsUpdated."));
	SpacePartition = NewObject<USmartObjectSpacePartition>(this, SpacePartitionClass);
	SpacePartition->SetBounds(SmartObjectContainer.GetBounds());

	// Note that we use our own flag instead of relying on World.HasBegunPlay() since world might not be marked
	// as BegunPlay immediately after subsystem OnWorldBeingPlay gets called (e.g. waiting game mode to be ready on clients)
	// Setting bRuntimeInitialized at this point since the following code assumes the SpatialPartition has been created 
	// and EntityManager cached. 
	bRuntimeInitialized = true; 
	
	AddContainerToSimulation(SmartObjectContainer);

	UE_CVLOG_UELOG(PendingSmartObjectRegistration.Num() > 0, this, LogSmartObject, VeryVerbose,
		TEXT("SmartObjectSubsystem: Handling %d pending registrations during runtime initialization."), PendingSmartObjectRegistration.Num());	

	for (TObjectPtr<USmartObjectComponent>& SOComponent : PendingSmartObjectRegistration)
	{
		// ensure the SOComponent is still valid - things could have happened to it between adding to PendingSmartObjectRegistration and it being processed here
		if (SOComponent && IsValid(SOComponent))
		{
			RegisterSmartObject(*SOComponent);
		}
	}
	PendingSmartObjectRegistration.Empty();

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
	// Process component list first so they can be notified before we destroy their associated runtime instance
	for (USmartObjectComponent* Component : RegisteredSOComponents)
	{
		// Make sure component was registered to simulation (e.g. Valid associated definition)
		if (Component != nullptr && Component->IsBoundToSimulation())
		{
			RemoveComponentFromSimulation(*Component);
		}
	}

	// Cleanup all remaining entries (e.g. associated to unloaded SmartObjectComponents)
	for (auto It(RuntimeSmartObjects.CreateIterator()); It; ++It)
	{
		DestroyRuntimeInstanceInternal(It.Key(), It.Value());
	}
	
	RuntimeSmartObjects.Reset();

	bRuntimeInitialized = false;

	RegisteredCollections.Reset();

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
	CleanupRuntime();

	Super::Deinitialize();
}

bool USmartObjectSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (Super::ShouldCreateSubsystem(Outer))
	{
		if (const UWorld* OuterWorld = Cast<UWorld>(Outer))
		{
			return OuterWorld->IsNetMode(NM_Client) == false;
		}
	}

	return false;
}

#if WITH_EDITOR
FBox USmartObjectSubsystem::ComputeBounds(const UWorld& World) const
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

	return Bounds;
}

void USmartObjectSubsystem::PopulateCollection(ASmartObjectPersistentCollection& InCollection) const
{
	TArray<USmartObjectComponent*> RelevantComponents;
	if (GetRegisteredSmartObjectsCompatibleWithCollection(InCollection, RelevantComponents) > 0)
	{
		InCollection.AppendToCollection(RelevantComponents);
	}
}

int32 USmartObjectSubsystem::GetRegisteredSmartObjectsCompatibleWithCollection(
	const ASmartObjectPersistentCollection& InCollection,
	TArray<USmartObjectComponent*>& OutRelevantComponents
	) const
{
	const int32 InitialCount = OutRelevantComponents.Num();

	if (bIsPartitionedWorld == false)
	{
		const ULevel* MyLevel = InCollection.GetLevel();
		const ULevelStreaming* MyLevelStreaming = ULevelStreaming::FindStreamingLevel(MyLevel);
		const bool bCollectionShouldAlwaysBeLoaded = (MyLevelStreaming == nullptr) || MyLevelStreaming->ShouldBeAlwaysLoaded();

		const ULevel* PreviousLevel = nullptr;
		bool bPreviousLevelValid = false;
		for (const TObjectPtr<USmartObjectComponent>& Component : RegisteredSOComponents)
		{
			check(Component);
			if (Component->GetCanBePartOfCollection() == false)
			{
				continue;
			}

			const ULevel* OwnerLevel = Component->GetComponentLevel();
			bool bValid = bPreviousLevelValid;

			if (OwnerLevel != PreviousLevel)
			{
				const ULevelStreaming* LevelStreaming = ULevelStreaming::FindStreamingLevel(OwnerLevel);
				bValid = (MyLevelStreaming == LevelStreaming)
					|| (bCollectionShouldAlwaysBeLoaded && LevelStreaming && LevelStreaming->ShouldBeAlwaysLoaded());
			}

			if (bValid)
			{
				OutRelevantComponents.Add(Component);
			}
			bPreviousLevelValid = bValid;
			PreviousLevel = OwnerLevel;
		}
	}
	else
	{
		TArray<const UDataLayerInstance*> DataLayers = InCollection.GetDataLayerInstances();
		const bool bPersistentLevelCollection = (DataLayers.Num() == 0);

		for (const TObjectPtr<USmartObjectComponent>& Component : RegisteredSOComponents)
		{
			check(Component);
			if (Component->GetCanBePartOfCollection() == false)
			{
				continue;
			}

			if (const AActor* Owner = Component->GetOwner())
			{
				const bool bInPersistentLayer = (Owner->HasDataLayers() == false);
				if (bPersistentLevelCollection == bInPersistentLayer)
				{
					if (bPersistentLevelCollection)
					{
						OutRelevantComponents.Add(Component);
					}
					else
					{
						for (const UDataLayerInstance* DataLayerInstance : DataLayers)
						{
							if (Owner->ContainsDataLayer(DataLayerInstance))
							{
								OutRelevantComponents.Add(Component);
								// breaking here since at the moment we only support registering smart objects only 
								// with a single collection
								break;
							}
						}
					}
				}
			}
		}
	}

	return (OutRelevantComponents.Num() - InitialCount);
}

void USmartObjectSubsystem::IterativelyBuildCollections()
{
	ensureMsgf(bIsPartitionedWorld, TEXT("%s expected to be called in World Paritioned worlds"), ANSI_TO_TCHAR(__FUNCTION__));

	if (RegisteredSOComponents.Num() == 0)
	{
		return;
	}

	TArray<USmartObjectComponent*> ComponentsToRestore = RegisteredSOComponents;
	TArray<USmartObjectComponent*> RelevantComponents;
	for (TWeakObjectPtr<ASmartObjectPersistentCollection>& WeakCollection : RegisteredCollections)
	{
		if (ASmartObjectPersistentCollection* Collection = WeakCollection.Get())
		{
			RelevantComponents.Reset();

			if (GetRegisteredSmartObjectsCompatibleWithCollection(*Collection, RelevantComponents) > 0)
			{
				Collection->AppendToCollection(RelevantComponents);

				// A component can belong to only a single collection.
				// We remove objects added to the collection so that they do not get added to another collection.
				// Also, the subsequent GetRegisteredSmartObjectsCompatibleWithCollection calls get less data to consider.
				for (USmartObjectComponent* SOComponent : RelevantComponents)
				{
					RegisteredSOComponents.RemoveSingleSwap(SOComponent);
				}
			}
		}
	}

	// Restore registered components so they can be unregistered properly by the normal streaming flow (i.e. not reporting any warnings/errors)
	RegisteredSOComponents = MoveTemp(ComponentsToRestore);
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void USmartObjectSubsystem::CreatePersistentCollectionFromDeprecatedData(UWorld& World, const ADEPRECATED_SmartObjectCollection& DeprecatedCollection)
{
	if (DeprecatedCollection.CollectionEntries.Num() == 0)
	{
		// we ignore the empty deprecated collections - we used to always create these even if no smart objects were being used
		// and an empty collection is an indication of such a case. No point in creating a replacement for such a collection.
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = DeprecatedCollection.GetLevel();

	if (ASmartObjectPersistentCollection* NewCollection = World.SpawnActor<ASmartObjectPersistentCollection>(SpawnParams))
	{
		NewCollection->SmartObjectContainer.Bounds = DeprecatedCollection.Bounds;
		NewCollection->SmartObjectContainer.CollectionEntries = DeprecatedCollection.CollectionEntries;
		NewCollection->SmartObjectContainer.RegisteredIdToObjectMap = DeprecatedCollection.RegisteredIdToObjectMap;
		NewCollection->SmartObjectContainer.Definitions = DeprecatedCollection.Definitions;
		NewCollection->bUpdateCollectionOnSmartObjectsChange = DeprecatedCollection.bBuildCollectionAutomatically;
	}
}
#endif // WITH_EDITORONLY_DATA

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
	for (USmartObjectComponent* Cmp : RegisteredSOComponents)
	{
		if (Cmp != nullptr)
		{
			const FSmartObjectCollectionEntry* Entry = SmartObjectContainer.GetEntries().FindByPredicate(
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
	if (bRuntimeInitialized || GetWorldRef().IsGameWorld())
	{
		return;
	}
	InitializeRuntime();
}

void USmartObjectSubsystem::DebugCleanupRuntime()
{
	// do not cleanup more than once or on a GameWorld
	if (!bRuntimeInitialized || GetWorldRef().IsGameWorld())
	{
		return;
	}
	CleanupRuntime();
}

#endif // WITH_SMARTOBJECT_DEBUG

//----------------------------------------------------------------------//
// deprecated functions implementations
//----------------------------------------------------------------------//
PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool USmartObjectSubsystem::UnregisterSmartObjectInternal(USmartObjectComponent & SmartObjectComponent, const ESmartObjectUnregistrationMode UnregistrationMode)
{
	const bool bShouldDestroyRuntimeData = (UnregistrationMode == ESmartObjectUnregistrationMode::DestroyRuntimeInstance)
		|| (SmartObjectComponent.GetRegistrationType() == ESmartObjectRegistrationType::Dynamic);
	return UnregisterSmartObjectInternal(SmartObjectComponent, bShouldDestroyRuntimeData);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
