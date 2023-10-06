// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponent.h"

#include "Engine/World.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectComponent)

#if WITH_EDITORONLY_DATA
USmartObjectComponent::FOnSmartObjectChanged USmartObjectComponent::OnSmartObjectChanged;
#endif // WITH_EDITORONLY_DATA

USmartObjectComponent::USmartObjectComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USmartObjectComponent::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AActor* Actor = GetOwner();
		if (Actor != nullptr && Actor->HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			// tagging owner actors since the tags get included in FWorldPartitionActorDesc 
			// and that's the only way we can tell a given actor has a SmartObjectComponent 
			// until it's fully loaded
			if (Actor->Tags.Contains(UE::SmartObjects::WithSmartObjectTag) == false)
			{
				Actor->Tags.AddUnique(UE::SmartObjects::WithSmartObjectTag);
				Actor->MarkPackageDirty();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void USmartObjectComponent::OnRegister()
{
	Super::OnRegister();

	// Component gets registered on BeginPlay for game worlds
	const UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		RegisterToSubsystem();
	}
}

void USmartObjectComponent::OnUnregister()
{
	// Component gets unregistered on EndPlay for game worlds
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld() == false)
	{
		UnregisterFromSubsystem(ESmartObjectUnregistrationType::RegularProcess);
	}

	Super::OnUnregister();
}
#endif // WITH_EDITOR

void USmartObjectComponent::RegisterToSubsystem()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

#if WITH_EDITOR
	// Do not process any component registered to preview world
	if (World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}
#endif // WITH_EDITOR

	// Note: we don't report error or ensure on missing subsystem since it might happen
	// in various scenarios (e.g. inactive world)
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		Subsystem->RegisterSmartObject(*this);
	}
}

void USmartObjectComponent::UnregisterFromSubsystem(const ESmartObjectUnregistrationType UnregistrationType)
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

#if WITH_EDITOR
	// Do not process any component registered to preview world
	if (World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}
#endif // WITH_EDITOR

	if (GetRegisteredHandle().IsValid())
	{
		if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
		{
			if (UnregistrationType == ESmartObjectUnregistrationType::ForceRemove
				|| IsBeingDestroyed()
				|| (GetOwner() && GetOwner()->IsActorBeingDestroyed()))
			{
				// note that this case is really only expected in the editor when the component is being unregistered 
				// as part of DestroyComponent (or from its owner destruction).
				// In default game flow EndPlay will get called first and once we make it here the RegisteredHandle should already be Invalid
				Subsystem->RemoveSmartObject(*this);
			}
			else
			{
				Subsystem->UnregisterSmartObject(*this);
			}
		}
	}
}

void USmartObjectComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register only for game worlds only since component is registered by OnRegister for the other scenarios.
	// Can't enforce a check here in case BeginPlay is manually dispatched on worlds of other type (e.g. Editor, Preview).
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		RegisterToSubsystem();
	}
}

void USmartObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister only for game worlds (see details in BeginPlay)
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		// When the object gets streamed out we unregister the component according to its registration type to preserve runtime data for persistent objects.
		// In all other scenarios (e.g. Destroyed, EndPIE, Quit, etc.) we always remove the runtime data
		const ESmartObjectUnregistrationType UnregistrationType =
			(EndPlayReason == EEndPlayReason::RemovedFromWorld) ? ESmartObjectUnregistrationType::RegularProcess : ESmartObjectUnregistrationType::ForceRemove;

		UnregisterFromSubsystem(UnregistrationType);
	}

	Super::EndPlay(EndPlayReason);
}

FBox USmartObjectComponent::GetSmartObjectBounds() const
{
	FBox BoundingBox(ForceInitToZero);

	const AActor* Owner = GetOwner();
	if (Owner != nullptr && DefinitionAsset != nullptr)
	{
		BoundingBox = DefinitionAsset->GetBounds().TransformBy(Owner->GetTransform());
	}

	return BoundingBox;
}

void USmartObjectComponent::SetRegisteredHandle(const FSmartObjectHandle Value, const ESmartObjectRegistrationType InRegistrationType)
{
	ensure(Value.IsValid());
	ensure(RegisteredHandle.IsValid() == false || RegisteredHandle == Value);
	RegisteredHandle = Value;
	ensure(RegistrationType == ESmartObjectRegistrationType::None && InRegistrationType != ESmartObjectRegistrationType::None);
	RegistrationType = InRegistrationType;
}

void USmartObjectComponent::InvalidateRegisteredHandle()
{
	RegisteredHandle = FSmartObjectHandle::Invalid;
	RegistrationType = ESmartObjectRegistrationType::None;
}

void USmartObjectComponent::OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance)
{
	EventDelegateHandle = RuntimeInstance.GetMutableEventDelegate().AddUObject(this, &USmartObjectComponent::OnRuntimeEventReceived);
}

void USmartObjectComponent::OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance)
{
	if (EventDelegateHandle.IsValid())
	{
		RuntimeInstance.GetMutableEventDelegate().Remove(EventDelegateHandle);
		EventDelegateHandle.Reset();
	}
}

TStructOnScope<FActorComponentInstanceData> USmartObjectComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FSmartObjectComponentInstanceData>(this, DefinitionAsset);
}

#if WITH_EDITOR
void USmartObjectComponent::PostEditUndo()
{
	Super::PostEditUndo();

	OnSmartObjectChanged.Broadcast(*this);
}

void USmartObjectComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnSmartObjectChanged.Broadcast(*this);
}
#endif // WITH_EDITOR

//-----------------------------------------------------------------------------
// FSmartObjectComponentInstanceData
//-----------------------------------------------------------------------------
bool FSmartObjectComponentInstanceData::ContainsData() const
{
	return true;
}

void FSmartObjectComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	// Apply data first since we might need to register to the subsystem
	// before the component gets re-registered by the base class.
	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		USmartObjectComponent* SmartObjectComponent = CastChecked<USmartObjectComponent>(Component);
		// We only need to force a register if DefinitionAsset is currently null and a valid one was backed up.
		// Reason is that our registration to the Subsystem depends on a valid definition so it can be skipped.
		if (SmartObjectComponent->DefinitionAsset != DefinitionAsset && SmartObjectComponent->DefinitionAsset == nullptr)
		{
			SmartObjectComponent->DefinitionAsset = DefinitionAsset;

			const UWorld* World = SmartObjectComponent->GetWorld();
			if (World != nullptr && !World->IsGameWorld())
			{
				// Registering to the subsystem should only be attempted on registered component
				// otherwise the OnRegister callback will take care of it.
				if (SmartObjectComponent->IsRegistered())
				{
					SmartObjectComponent->RegisterToSubsystem();
				}
			}
		}
	}

	Super::ApplyToComponent(Component, CacheApplyPhase);
}

void USmartObjectComponent::OnRuntimeEventReceived(const FSmartObjectEventData& Event)
{
	const AActor* Interactor = nullptr;
	if (const FSmartObjectActorUserData* ActorUser = Event.EventPayload.GetPtr<const FSmartObjectActorUserData>())
	{
		Interactor = ActorUser->UserActor.Get();
	}
					
	UE_CVLOG_LOCATION(Interactor != nullptr, USmartObjectSubsystem::GetCurrent(GetWorld()), LogSmartObject, Display,
		Interactor->GetActorLocation(), /*Radius*/25.f, FColor::Green, TEXT("%s: %s. Interactor: %s"),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Event.Reason), *GetNameSafe(Interactor));

	ReceiveOnEvent(Event, Interactor);
	OnSmartObjectEvent.Broadcast(Event, Interactor);
	OnSmartObjectEventNative.Broadcast(Event, Interactor);
}
