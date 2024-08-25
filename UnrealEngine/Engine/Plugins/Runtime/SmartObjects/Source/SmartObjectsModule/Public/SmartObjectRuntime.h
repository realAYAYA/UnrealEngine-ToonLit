// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRuntime.generated.h"

class USmartObjectComponent;

namespace UE::SmartObject
{
uint16 GetMaskForEnabledReasonTag(const FGameplayTag Tag);
}

/** Delegate fired when a given tag is added or removed. Tags on smart object are not using reference counting so count will be 0 or 1 */
UE_DEPRECATED(5.2, "Tag changes are now broadcasted using FOnSmartObjectEvent.")
DECLARE_DELEGATE_TwoParams(FOnSmartObjectTagChanged, const FGameplayTag, int32);

/**
 * Enumeration to represent the runtime state of a slot
 */
UENUM()
enum class ESmartObjectSlotState : uint8
{
	Invalid,
	/** Slot is available */
	Free,
	/** Slot is claimed but interaction is not active yet */
	Claimed,
	/** Slot is claimed and interaction is active */
	Occupied,
	/** Slot can no longer be claimed or used since the parent object and its slot are disabled (e.g. instance tags) */
	Disabled UE_DEPRECATED(5.2, "Use IsEnabled() instead."),
};

/**
 * Indicates if the subsystem should try to spawn the actor associated to the smartobject
 * if it is currently owned by an instanced actor.
 */
UENUM()
enum class ETrySpawnActorIfDehydrated : uint8
{
	No,
	Yes
};

/**
 * Struct describing a reservation between a user and a smart object slot.
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectClaimHandle
{
	GENERATED_BODY()

	FSmartObjectClaimHandle(const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectSlotHandle InSlotHandle, const FSmartObjectUserHandle& InUser)
		: SmartObjectHandle(InSmartObjectHandle), SlotHandle(InSlotHandle), UserHandle(InUser)
	{}

	FSmartObjectClaimHandle()
	{}

	bool operator==(const FSmartObjectClaimHandle& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle
			&& SlotHandle == Other.SlotHandle
			&& UserHandle == Other.UserHandle;
	}

	bool operator!=(const FSmartObjectClaimHandle& Other) const
	{
		return !(*this == Other);
	}

	friend FString LexToString(const FSmartObjectClaimHandle& Handle)
	{
		return FString::Printf(TEXT("Object:%s Slot:%s User:%s"), *LexToString(Handle.SmartObjectHandle), *LexToString(Handle.SlotHandle), *LexToString(Handle.UserHandle));
	}

	void Invalidate() { *this = InvalidHandle; }

	/**
	 * Indicates that the handle was properly assigned by a call to 'Claim' but doesn't guarantee that the associated
	 * object and slot are still registered in the simulation.
	 * This information requires a call to `USmartObjectSubsystem::IsClaimedObjectValid` using the handle.
	 */
	bool IsValid() const
	{
		return SmartObjectHandle.IsValid()
			&& SlotHandle.IsValid()
			&& UserHandle.IsValid();
	}

	static const FSmartObjectClaimHandle InvalidHandle;

	/** Handle to the Smart Object where the claimed slot belongs to.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category="Default")
	FSmartObjectHandle SmartObjectHandle;

	/** Handle of the claimed slot. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category="Default")
	FSmartObjectSlotHandle SlotHandle;

	/** Handle describing the user which claimed the slot. */
	UPROPERTY(EditAnywhere, Transient, Category="Default")
	FSmartObjectUserHandle UserHandle;
};

/**
 * Runtime data holding the final slot transform (i.e. parent transform applied on slot local offset and rotation)
 */
USTRUCT()
struct UE_DEPRECATED(5.3, "Transform is moved to FSmartObjectRuntimeSlot.") SMARTOBJECTSMODULE_API FSmartObjectSlotTransform : public FSmartObjectSlotStateData
{
	GENERATED_BODY()

	const FTransform& GetTransform() const { return Transform; }
	FTransform& GetMutableTransform() { return Transform; }

	void SetTransform(const FTransform& InTransform) { Transform = InTransform; }

protected:

	UPROPERTY(Transient)
	FTransform Transform;
};

/** Delegate to notify when a given slot gets invalidated and the interaction must be aborted */
DECLARE_DELEGATE_TwoParams(FOnSlotInvalidated, const FSmartObjectClaimHandle&, ESmartObjectSlotState /* Current State */);

/**
 * Struct to store and manage state of a runtime instance associated to a given slot definition
 */

USTRUCT()
struct FSmartObjectRuntimeSlot
{
	GENERATED_BODY()

public:
	/* Provide default constructor to be able to compile template instantiation 'UScriptStruct::TCppStructOps<FSmartObjectSlotState>' */
	/* Also public to pass void 'UScriptStruct::TCppStructOps<FSmartObjectSlotState>::ConstructForTests(void *)' */
	FSmartObjectRuntimeSlot() : bSlotEnabled(true), bObjectEnabled(true) {}

	FVector3f GetSlotOffset() const { return Offset; }

	FRotator3f GetSlotRotation() const { return Rotation; }

	FTransform GetSlotLocalTransform() const
	{
		return FTransform(FRotator(Rotation), FVector(Offset));
	}

	FTransform GetSlotWorldTransform(const FTransform& OwnerTransform) const
	{
		return FTransform(FRotator(Rotation), FVector(Offset)) * OwnerTransform;
	}

	/** @return Current claim state of the slot. */
	ESmartObjectSlotState GetState() const { return State; }

	UE_DEPRECATED(5.4, "Use CanBeClaimed() with priority instead.")
	bool CanBeClaimed() const
	{
		return CanBeClaimed(ESmartObjectClaimPriority::Normal);
	}

	/**
	 * Sets the slot claimed.
	 * @param ClaimPriority Claim priority, a slot claimed at lower priority can be claimed by higher priority (unless already in use).
	 * @return True if the slot can be claimed. */
	bool CanBeClaimed(ESmartObjectClaimPriority ClaimPriority) const
	{
		return IsEnabled()
			&& (State == ESmartObjectSlotState::Free
				|| (State == ESmartObjectSlotState::Claimed
					&& ClaimedPriority < ClaimPriority));
	}

	/** @return the runtime gameplay tags of the slot. */
	const FGameplayTagContainer& GetTags() const { return Tags; }

	/** @return true if both the slot and its parent smart object are enabled. */
	bool IsEnabled() const { return bSlotEnabled && bObjectEnabled; }

	/** @return User data struct that can be associated to the slot when claimed or used. */
	FConstStructView GetUserData() const { return UserData; }

	FInstancedStructContainer& GetMutableStateData() { return StateData; }
	const FInstancedStructContainer& GetStateData() const { return StateData; }

	/** Indicates if preconditions were successfully initialized. */
	bool ArePreconditionsInitialized() const
	{
		return PreconditionState.IsInitialized();
	}

protected:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;
	friend struct FSmartObjectRuntime;

	UE_DEPRECATED(5.4, "Use Claim() with priority instead.")
	bool Claim(const FSmartObjectUserHandle& InUser)
	{
		return Claim(InUser, ESmartObjectClaimPriority::Normal);
	}

	bool Claim(const FSmartObjectUserHandle& InUser, ESmartObjectClaimPriority ClaimPriority);
	bool Release(const FSmartObjectClaimHandle& ClaimHandle, const bool bAborted);

	friend FString LexToString(const FSmartObjectRuntimeSlot& Slot)
	{
		return FString::Printf(TEXT("User:%s State:%s"), *LexToString(Slot.User), *UEnum::GetValueAsString(Slot.State));
	}

	/** Offset of the slot relative to the Smart Object. */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FVector3f Offset = FVector3f::ZeroVector;

	/** Rotation of the slot relative to the Smart Object. */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FRotator3f Rotation = FRotator3f::ZeroRotator;
	
	/** Runtime tags associated with this slot. */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FGameplayTagContainer Tags;

	/** Struct used to store contextual data of the user when claiming or using a slot. */
	FInstancedStruct UserData;

	/** Slot state data that can be added at runtime. */
	FInstancedStructContainer StateData;
	
	/** Delegate used to notify when a slot gets invalidated. See RegisterSlotInvalidationCallback */
	FOnSlotInvalidated OnSlotInvalidatedDelegate;

	/** Handle to the user that reserves or uses the slot */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FSmartObjectUserHandle User;

	/** World condition runtime state. */
	UPROPERTY(Transient)
	mutable FWorldConditionQueryState PreconditionState;

	/** Current availability state of the slot */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	ESmartObjectSlotState State = ESmartObjectSlotState::Free;

	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	ESmartObjectClaimPriority ClaimedPriority = ESmartObjectClaimPriority::None;
	
	/** True if the slot is enabled */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	uint8 bSlotEnabled : 1;

	/** True if the parent smart object is enabled */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	uint8 bObjectEnabled : 1;
};

using FSmartObjectSlotClaimState UE_DEPRECATED(5.2, "Deprecated struct. Please use FSmartObjectRuntimeSlot instead.") = FSmartObjectRuntimeSlot;

/**
 * Struct to store and manage state of a runtime instance associated to a given smart object definition
 */
USTRUCT()
struct FSmartObjectRuntime
{
	GENERATED_BODY()

public:
	/* Provide default constructor to be able to compile template instantiation 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>' */
	/* Also public to pass void 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>::ConstructForTests(void *)' */
	FSmartObjectRuntime() {}

	FSmartObjectHandle GetRegisteredHandle() const { return RegisteredHandle; }
	const FTransform& GetTransform() const { return Transform; }
	const USmartObjectDefinition& GetDefinition() const { checkf(Definition != nullptr, TEXT("Initialized from a valid reference from the constructor")); return *Definition; }
	
	/** Returns all tags assigned to the smart object instance */
	const FGameplayTagContainer& GetTags() const { return Tags; }

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Returns delegate that is invoked whenever a tag is added or removed */
	UE_DEPRECATED(5.2, "Tag changes are now broadcasted using FOnSmartObjectEvent. Please use GetEventDelegate().")
	FOnSmartObjectTagChanged& GetTagChangedDelegate() { return OnTagChangedDelegate; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** @return reference to the Smart Object event delegate. */
	const FOnSmartObjectEvent& GetEventDelegate() const { return OnEvent; }

	/** @return mutable reference to the Smart Object event delegate. */
	FOnSmartObjectEvent& GetMutableEventDelegate() { return OnEvent; }

	/**
	 * Indicates if the Smart Object is enabled regardless of the reason.
	 * @return True of the Smart Object is enabled.
	 */
	bool IsEnabled() const
	{
		return DisableFlags == 0;
	}

	/**
	 * Indicates if the Smart Object is enabled based on a specific reason.
	 * @param ReasonTag Valid Tag to specify the reason for changing the enabled state of the object. Method will ensure if not valid (i.e. None).
	 * @return True of the Smart Object is enabled.
	 */
	bool IsEnabledForReason(FGameplayTag ReasonTag) const;

	/**
	 * Enables or disables the entire smart object.
	 * @param ReasonTag Valid Tag to specify the reason for changing the enabled state of the object. Method will ensure if not valid (i.e. None).
	 * @param bEnabled Flag indicating if the object should be enable or not.
	 */
	void SetEnabled(FGameplayTag ReasonTag, bool bEnabled);

	/**
	 * Returns the actor associated to the smart object instance.
	 * @param TrySpawnActorIfDehydrated Indicates if the instance should try to spawn the actor/component
	 *        associated to the smartobject if it is currently owned by an instanced actor.
	 * @return Pointer to owner actor if present.
	 */
	AActor* GetOwnerActor(ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated = ETrySpawnActorIfDehydrated::No) const;

	/**
	 * Returns the actor associated to the smart object instance.
	 * @param TrySpawnActorIfDehydrated Indicates if the instance should try to spawn the actor/component
	 *        associated to the smartobject if it is currently owned by an instanced actor.
	 * @return Pointer to owning component if present.
	 */
	USmartObjectComponent* GetOwnerComponent(ETrySpawnActorIfDehydrated TrySpawnActorIfDehydrated = ETrySpawnActorIfDehydrated::No) const;

	/** @return handle of the specified slot. */
	const FSmartObjectRuntimeSlot& GetSlot(const int32 Index) const
	{
		return Slots[Index];
	}

	FSmartObjectRuntimeSlot& GetMutableSlot(const int32 Index)
	{
		return Slots[Index];
	}

	TConstArrayView<FSmartObjectRuntimeSlot> GetSlots() const
	{
		return Slots;
	}

	UE_DEPRECATED(5.3, "You can access the slots directly via GetSlots().")
	FSmartObjectSlotHandle GetSlotHandle(const int32 Index) const
	{
		return {};
	}

	/** Indicates if preconditions were successfully initialized. */
	bool ArePreconditionsInitialized() const
	{
		return PreconditionState.IsInitialized();
	}

#if WITH_SMARTOBJECT_DEBUG
	FString DebugGetDisableFlagsString() const;
#endif // WITH_SMARTOBJECT_DEBUG

private:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;

	explicit FSmartObjectRuntime(const USmartObjectDefinition& Definition);

	void SetTransform(const FTransform& Value) { Transform = Value; }

	void SetRegisteredHandle(const FSmartObjectHandle Value) { RegisteredHandle = Value; }

	/**
	 * Enables or disables the entire smart object using the bit mask from a reason tag.
	 * @param bEnabled Flag indicating if the object should be enable or not. 
	 * @param ReasonMask Bit mask associated to the reason for disabling the object.
	 */
	void SetEnabled(bool bEnabled, uint16 ReasonMask);

	/**
	 * Creates full actor from instanced actor owner, if any.
	 * That actor will register its SmartObjectComponents that will then update OwnerComponent.
	 */
	bool ResolveOwnerActor() const;

	/** World condition runtime state. */
	UPROPERTY(Transient)
	mutable FWorldConditionQueryState PreconditionState;
	
	/** Runtime slots */
	UPROPERTY(Transient)
	TArray<FSmartObjectRuntimeSlot> Slots;

	/** Associated smart object definition */
	UPROPERTY()
	TObjectPtr<const USmartObjectDefinition> Definition = nullptr;

	/** Component that owns the Smart Object. May be empty if the parent Actor is not loaded. */
	UPROPERTY()
	TWeakObjectPtr<USmartObjectComponent> OwnerComponent;

	/** Struct used to store contextual data of the owner of that SmartObject. */
	FInstancedStruct OwnerData;
	
	/** Instance specific transform */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FTransform Transform;

	/** Tags applied to the current instance */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FGameplayTagContainer Tags;

	/** Delegate fired whenever a new tag is added or an existing one gets removed */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnSmartObjectTagChanged OnTagChangedDelegate;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Delegate that is fired when the Smart Object changes. */
	FOnSmartObjectEvent OnEvent;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered with SmartObjectSubsystem */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	FSmartObjectHandle RegisteredHandle;

	/** Spatial representation data associated to the current instance */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (BaseStruct = "/Script/SmartObjectsModule.SmartObjectSpatialEntryData", ExcludeBaseStruct))
	FInstancedStruct SpatialEntryData;

#if UE_ENABLE_DEBUG_DRAWING
	FBox Bounds = FBox(EForceInit::ForceInit);
#endif

	/** 
	 * Each slot has its own enabled state but the parent instance also have a more high level state that could be split into different reasons.
	 * Note: The enabled state is stored as disable bits to make it easier to check for "is the object disabled for a given or any reason".
	 */
	UPROPERTY(Transient, VisibleAnywhere, Category=SmartObjects)
	uint16 DisableFlags = 0;

public:
	static constexpr int32 MaxNumDisableFlags = sizeof(DisableFlags) * 8;
};

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotView
{
	GENERATED_BODY()

public:
	FSmartObjectSlotView() = default;

	bool IsValid() const { return SlotHandle.IsValid() && Runtime && Slot; }

	FSmartObjectSlotHandle GetSlotHandle() const { return SlotHandle; }

	/**
	 * Returns a reference to the slot state data of the specified type.
	 * Method will fail a check if the slot doesn't have the given type.
	 */
	template<typename T>
	T& GetStateData() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectSlotStateData>::IsDerived,
			"Given struct doesn't represent a valid runtime data type. Make sure to inherit from FSmartObjectSlotStateData or one of its child-types.");

		checkf(Slot, TEXT("StateData can only be accessed through a valid SlotView"));

		T* Item = nullptr;
		for (FStructView Data : Slot->GetMutableStateData())
		{
			Item = Data.template GetPtr<T>();
			if (Item != nullptr)
			{
				break;
			}
		}
		check(Item);
		return *Item;
	}

	/**
	 * Returns a pointer to the slot state data of the specified type.
	 * Method will return null if the slot doesn't have the given type.
	 */
	template<typename T>
	T* GetStateDataPtr() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectSlotStateData>::IsDerived,
					"Given struct doesn't represent a valid runtime data type. Make sure to inherit from FSmartObjectSlotStateData or one of its child-types.");

		checkf(Slot, TEXT("StateData can only be accessed through a valid SlotView"));

		for (FStructView Data : Slot->GetMutableStateData())
		{
			if (T* Item = Data.template GetPtr<T>())
			{
				return Item;
			}
		}
		
		return nullptr;
	}

	/**
	 * Returns a reference to the definition of the slot's parent object.
	 * Method will fail a check if called on an invalid SlotView.
	 * @note The definition fragment is always created and assigned when creating an entity associated to a slot
	 * so a valid SlotView is guaranteed to be able to provide it.
	 */
	const USmartObjectDefinition& GetSmartObjectDefinition() const
	{
		checkf(Runtime, TEXT("Definition can only be accessed through a valid SlotView"));
		return Runtime->GetDefinition();
	}

	/**
	 * Returns a reference to the main definition of the slot.
	 * Method will fail a check if called on an invalid SlotView.
	 */
	const FSmartObjectSlotDefinition& GetDefinition() const
	{
		checkf(Runtime, TEXT("Definition can only be accessed through a valid SlotView"));
		return Runtime->GetDefinition().GetSlot(SlotHandle.GetSlotIndex());
	}

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag filtering policy.
	 * Method will fail a check if called on an invalid SlotView.
	 */
	void GetActivityTags(FGameplayTagContainer& OutActivityTags) const
	{
		GetSmartObjectDefinition().GetSlotActivityTags(GetDefinition(), OutActivityTags);
	}

	/**
	 * Returns a reference to the definition data of the specified type from the main slot definition.
	 * Method will fail a check if the slot definition doesn't contain the given type.
	 */
	template<typename T>
	const T& GetDefinitionData() const
	{
		const FSmartObjectSlotDefinition& SlotDefinition = GetDefinition();
		return SlotDefinition.template GetDefinitionData<T>();
	}

	/**
	 * Returns a pointer to the definition data of the specified type from the main slot definition.
	 * Method will return null if the slot doesn't contain the given type.
	 */
	template<typename T>
	const T* GetDefinitionDataPtr() const
	{
		const FSmartObjectSlotDefinition& SlotDefinition = GetDefinition();
		return SlotDefinition.template GetDefinitionDataPtr<T>();
	}

	/** @return the claim state of the slot. */
	ESmartObjectSlotState GetState() const
	{
		checkf(Slot, TEXT("State can only be accessed through a valid SlotView"));
		return Slot->GetState();
	}

	UE_DEPRECATED(5.4, "Use CanBeClaimed() with priority instead.")
	bool CanBeClaimed() const
	{
		return CanBeClaimed(ESmartObjectClaimPriority::Normal);
	}

	/** @return true of the slot can be claimed. */
	bool CanBeClaimed(ESmartObjectClaimPriority ClaimPriority) const
	{
		checkf(Slot, TEXT("Claim can only be accessed through a valid SlotView"));
		return Slot->CanBeClaimed(ClaimPriority);
	}

	/** @return true if the slot and the object is enabled. */
	bool IsEnabled() const
	{
		checkf(Slot, TEXT("Enabled can only be accessed through a valid SlotView"));
		return Slot->IsEnabled();
	}
	
	/** @return runtime gameplay tags of the slot. */
	const FGameplayTagContainer& GetTags() const
	{
		checkf(Slot, TEXT("Tags can only be accessed through a valid SlotView"));
		return Slot->GetTags();
	}

	/** @return handle to the owning Smart Object. */
	FSmartObjectHandle GetOwnerRuntimeObject() const
	{
		return SlotHandle.GetSmartObjectHandle();
	}

private:
	friend class USmartObjectSubsystem;

	FSmartObjectSlotView(const FSmartObjectSlotHandle InSlotHandle, FSmartObjectRuntime& InRuntime, FSmartObjectRuntimeSlot& InSlot) 
		: SlotHandle(InSlotHandle)
		, Runtime(&InRuntime)
		, Slot(&InSlot)
	{}

	FSmartObjectSlotHandle SlotHandle;
	FSmartObjectRuntime* Runtime;
	FSmartObjectRuntimeSlot* Slot;
};
