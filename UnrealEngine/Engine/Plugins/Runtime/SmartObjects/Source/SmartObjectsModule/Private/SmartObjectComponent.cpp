// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectComponent)

#if WITH_EDITOR
#include "Engine/World.h"
#endif

USmartObjectComponent::USmartObjectComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USmartObjectComponent::OnRegister()
{
	Super::OnRegister();

	RegisterToSubsystem();
}

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
#endif

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	/** Do not register components that don't have a valid definition */
	if (!IsValid(DefinitionAsset))
	{
		return;
	}

	// Note: we don't report error or ensure on missing subsystem since it might happen
	// in various scenarios (e.g. inactive world)
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		Subsystem->RegisterSmartObjectInternal(*this);
	}
}

void USmartObjectComponent::OnUnregister()
{
	Super::OnUnregister();

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
#endif

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	/** Do not register components that don't have a valid definition */
	if (!IsValid(DefinitionAsset))
	{
		return;
	}

	// Note: we don't report error or ensure on missing subsystem since it might happen
	// in various scenarios (e.g. inactive world, AI system is cleaned up before the components gets unregistered, etc.)
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		// The default behavior is to maintain the runtime instance alive when the component is unregistered (streamed out).
		Subsystem->UnregisterSmartObjectInternal(*this, ESmartObjectUnregistrationMode::KeepRuntimeInstanceActive);
	}

	ensureMsgf(!OnComponentTagsModifiedHandle.IsValid(), TEXT("AbilitySystemComponent delegate is expected to be unbound after unregistration"));
	ensureMsgf(!bInstanceTagsDelegateBound, TEXT("SmartObject runtime instance delegate is expected to be unbound after unregistration"));
}

void USmartObjectComponent::BeginPlay()
{
	Super::BeginPlay();

	/*
	 * Ability system components can be sometimes added after the call to OnRuntimeInstanceCreated() or OnRuntimeInstanceBound().
	 * This code attempts handle the case that the ability component was not read at the right time.
	 * @todo: validate that this logic is correct.
	*/
	if (!bInstanceTagsDelegateBound)
	{
		if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			if (FSmartObjectRuntime* RuntimeInstance = Subsystem->GetRuntimeInstance(RegisteredHandle))
			{
				OnRuntimeInstanceBound(*RuntimeInstance);
			}
		}
	}
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

void USmartObjectComponent::OnRuntimeInstanceCreated(FSmartObjectRuntime& RuntimeInstance)
{
	// A new runtime instance is always created from a collection entry which was initialized with
	// the list of tags provided by the IGameplayTagAssetInterface of the SmartObjectComponent owning actor.
	// This means that at this point both are already in sync and we simply need to register our delegates
	// to synchronize changes coming from either the AbilitySystemComponent or the SmartObjectRuntime.
	if (UAbilitySystemComponent* AbilityComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		BindTagsDelegates(RuntimeInstance, *AbilityComponent);
	}
}

void USmartObjectComponent::OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance)
{
	if (UAbilitySystemComponent* AbilityComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		BindTagsDelegates(RuntimeInstance, *AbilityComponent);

		// Update component using the tags from the instance since the component might get reloaded while the instance
		// was still part of the simulation (i.e. persistent). In this case we need to apply the most up to date
		// tag counts to the component. Unfortunately there is no way at the moment to replace all tags in one go
		// so update each tag count individually.
		const FGameplayTagContainer& InstanceTags = RuntimeInstance.GetTags();
		FGameplayTagContainer AbilityComponentTags;
		AbilityComponent->GetOwnedGameplayTags(AbilityComponentTags);

		// Adjust count of any existing and add the missing ones
		for (auto It(InstanceTags.CreateConstIterator()); It; ++It)
		{
			AbilityComponentTags.RemoveTag(*It);
			AbilityComponent->SetTagMapCount(*It, 1);
		}

		// Remove all remaining tags that are no longer valid
		for (auto It(AbilityComponentTags.CreateConstIterator()); It; ++It)
		{
			AbilityComponent->SetTagMapCount(*It, 0);
		}
	}
}

void USmartObjectComponent::OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance)
{
	UnbindComponentTagsDelegate();
	UnbindRuntimeInstanceTagsDelegate(RuntimeInstance);
}

void USmartObjectComponent::OnRuntimeInstanceDestroyed()
{
	UnbindComponentTagsDelegate();

	// No need to try to unbind the Runtime instance delegate since it was destroyed.
	// Simply invalidate our handle.
	bInstanceTagsDelegateBound = false;
}

void USmartObjectComponent::BindTagsDelegates(FSmartObjectRuntime& RuntimeInstance, UAbilitySystemComponent& AbilitySystemComponent)
{
	USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());

	if (Subsystem != nullptr)
	{
		// Register callback when Tags in the component are modified to mirror the change in the Runtime instance
		OnComponentTagsModifiedHandle = AbilitySystemComponent.RegisterGenericGameplayTagEvent().AddLambda
		([Subsystem, Handle = RuntimeInstance.GetRegisteredHandle()](const FGameplayTag Tag, const int32 NewCount)
		{
			// This specific delegate is only invoked whenever a tag is added or removed (but not if just count is increased)
			// so we can add or remove the tag on the instance (no reference counting)
			if (NewCount)
			{
				Subsystem->AddTagToInstance(Handle, Tag);
			}
			else
			{
				Subsystem->RemoveTagFromInstance(Handle, Tag);
			}
		});

		// Register callback when Tags in the Runtime instance are modified to mirror the change in the component
		// The lambda capture assumes that the AbilitySystemComponent has the same lifetime as the current SmartObjectComponent
		RuntimeInstance.GetTagChangedDelegate().BindLambda
		([&AbilitySystemComponent](const FGameplayTag Tag, const int32 NewCount)
		{
			AbilitySystemComponent.SetTagMapCount(Tag, NewCount);
		});
		bInstanceTagsDelegateBound = true;
	}
}

void USmartObjectComponent::UnbindComponentTagsDelegate()
{
	if (OnComponentTagsModifiedHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilityComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
		{
			AbilityComponent->RegisterGenericGameplayTagEvent().Remove(OnComponentTagsModifiedHandle);
		}
		OnComponentTagsModifiedHandle.Reset();
	}
}

void USmartObjectComponent::UnbindRuntimeInstanceTagsDelegate(FSmartObjectRuntime& RuntimeInstance)
{
	if (bInstanceTagsDelegateBound)
	{
		RuntimeInstance.GetTagChangedDelegate().Unbind();
		bInstanceTagsDelegateBound = false;
	}
}

TStructOnScope<FActorComponentInstanceData> USmartObjectComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FSmartObjectComponentInstanceData>(this, DefinitionAsset);
}

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
			// Registering to the subsystem should only be attempted on registered component
			// otherwise the OnRegister callback will take care of it.
			if (SmartObjectComponent->IsRegistered())
			{
				SmartObjectComponent->RegisterToSubsystem();
			}
		}
	}

	Super::ApplyToComponent(Component, CacheApplyPhase);
}

