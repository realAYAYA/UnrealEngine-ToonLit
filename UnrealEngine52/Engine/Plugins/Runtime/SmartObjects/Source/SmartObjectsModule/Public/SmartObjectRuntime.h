// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityView.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRuntime.generated.h"

class USmartObjectComponent;

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
	UPROPERTY(EditAnywhere, Transient, Category="Default")
	FSmartObjectHandle SmartObjectHandle;

	/** Handle of the claimed slot. */
	UPROPERTY(EditAnywhere, Transient, Category="Default")
	FSmartObjectSlotHandle SlotHandle;

	/** Handle describing the user which claimed the slot. */
	UPROPERTY(EditAnywhere, Transient, Category="Default")
	FSmartObjectUserHandle UserHandle;
};

/**
 * Runtime data holding the final slot transform (i.e. parent transform applied on slot local offset and rotation)
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotTransform : public FSmartObjectSlotStateData
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
	
	explicit FSmartObjectRuntimeSlot(const FSmartObjectHandle InRuntimeObjectHandle, const int32 InSlotIndex)
		: RuntimeObjectHandle(InRuntimeObjectHandle)
		, SlotIndex(IntCastChecked<uint8>(InSlotIndex))
		, bSlotEnabled(true)
		, bObjectEnabled(true)
	{
	}

	/** @return Current claim state of the slot. */
	ESmartObjectSlotState GetState() const { return State; }

	/** @return True if the slot can be claimed. */
	bool CanBeClaimed() const { return IsEnabled() && State == ESmartObjectSlotState::Free; }

	/** @return reference to the slot event delegate. */
	const FOnSmartObjectEvent& GetEventDelegate() const { return OnEvent; }

	/** @return mutable reference to the slot event delegate. */
	FOnSmartObjectEvent& GetMutableEventDelegate() { return OnEvent; }

	/** @return the runtime gameplay tags of the slot. */
	const FGameplayTagContainer& GetTags() const { return Tags; }

	/** @return true if both the slot and its parent smart object are enabled. */
	bool IsEnabled() const { return bSlotEnabled && bObjectEnabled; }

	/** @return Handle of the owner runtime object. */
	FSmartObjectHandle GetOwnerRuntimeObject() const { return RuntimeObjectHandle; }
	
protected:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;
	friend struct FSmartObjectRuntime;

	bool Claim(const FSmartObjectUserHandle& InUser);
	bool Release(const FSmartObjectClaimHandle& ClaimHandle, const bool bAborted);
	
	friend FString LexToString(const FSmartObjectRuntimeSlot& Slot)
	{
		return FString::Printf(TEXT("User:%s State:%s"), *LexToString(Slot.User), *UEnum::GetValueAsString(Slot.State));
	}

	/** Runtime tags associated with this slot. */
	FGameplayTagContainer Tags;

	/** Multicast delegate that is fired when the slot state changes. */
	FOnSmartObjectEvent OnEvent;

	/** Delegate used to notify when a slot gets invalidated. See RegisterSlotInvalidationCallback */
	FOnSlotInvalidated OnSlotInvalidatedDelegate;

	/** Handle of the owner object */
	FSmartObjectHandle RuntimeObjectHandle;
	
	/** Handle to the user that reserves or uses the slot */
	FSmartObjectUserHandle User;

	/** Current availability state of the slot */
	ESmartObjectSlotState State = ESmartObjectSlotState::Free;

	/** World condition runtime state. */
	UPROPERTY(Transient)
	mutable FWorldConditionQueryState PreconditionState;
	
	/** Index of the slot in the definition. */
	uint8 SlotIndex = 0;

	/** True if the slot is enabled */
	uint8 bSlotEnabled : 1;

	/** True if the parent smart object is enabled */
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
	
	/** Indicates that this instance is still part of the simulation (space partition) but should not be considered valid by queries */
	UE_DEPRECATED(5.1, "Use IsEnabled instead.")
	bool IsDisabled() const { return !bEnabled; }

	/** @return True of the Smart Object is enabled. */
	bool IsEnabled() const { return bEnabled; }

	/** @return Pointer to owner actor if present. */
	AActor* GetOwnerActor() const;

	/* Provide default constructor to be able to compile template instantiation 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>' */
	/* Also public to pass void 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>::ConstructForTests(void *)' */
	FSmartObjectRuntime() : bEnabled(true) {}

	/** @return handle of the specified slot. */
	FSmartObjectSlotHandle GetSlotHandle(const int32 Index) const
	{
		if (SlotHandles.IsValidIndex(Index))
		{
			return SlotHandles[Index];
		}
		return FSmartObjectSlotHandle();
	}
	
private:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;

	explicit FSmartObjectRuntime(const USmartObjectDefinition& Definition);

	void SetTransform(const FTransform& Value) { Transform = Value; }

	void SetRegisteredHandle(const FSmartObjectHandle Value) { RegisteredHandle = Value; }

	/** World condition runtime state. */
	UPROPERTY(Transient)
	mutable FWorldConditionQueryState PreconditionState;
	
	/** Runtime SlotHandles associated to each defined slot */
	TArray<FSmartObjectSlotHandle> SlotHandles;

	/** Associated smart object definition */
	UPROPERTY()
	TObjectPtr<const USmartObjectDefinition> Definition = nullptr;

	/** Component that owns the Smart Object. May be empty if the parent Actor is not loaded. */
	UPROPERTY()
	TWeakObjectPtr<USmartObjectComponent> OwnerComponent;
	
	/** Instance specific transform */
	FTransform Transform;

	/** Tags applied to the current instance */
	FGameplayTagContainer Tags;

	/** Delegate fired whenever a new tag is added or an existing one gets removed */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnSmartObjectTagChanged OnTagChangedDelegate;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Delegate that is fired when the Smart Object changes. */
	FOnSmartObjectEvent OnEvent;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered with SmartObjectSubsystem */
	FSmartObjectHandle RegisteredHandle;

	/** Spatial representation data associated to the current instance */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (BaseStruct = "/Script/SmartObjectsModule.SmartObjectSpatialEntryData", ExcludeBaseStruct))
	FInstancedStruct SpatialEntryData;

#if UE_ENABLE_DEBUG_DRAWING
	FBox Bounds = FBox(EForceInit::ForceInit);
#endif

	/** Each slot has its own disable state but keeping it also in the parent instance allow faster validation in some cases. */
	uint8 bEnabled : 1;
};

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotView
{
	GENERATED_BODY()

public:
	FSmartObjectSlotView() = default;

	bool IsValid() const { return EntityView.IsSet(); }

	FSmartObjectSlotHandle GetSlotHandle() const { return EntityView.GetEntity(); }

	/**
	 * Returns a reference to the slot state data of the specified type.
	 * Method will fail a check if the slot doesn't have the given type.
	 */
	template<typename T>
	T& GetStateData() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectSlotStateData>::IsDerived,
			"Given struct doesn't represent a valid runtime data type. Make sure to inherit from FSmartObjectSlotStateData or one of its child-types.");

		return EntityView.GetFragmentData<T>();
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

		return EntityView.GetFragmentDataPtr<T>();
	}

	/**
	 * Returns a reference to the definition of the slot's parent object.
	 * Method will fail a check if called on an invalid SlotView.
	 * @note The definition fragment is always created and assigned when creating an entity associated to a slot
	 * so a valid SlotView is guaranteed to be able to provide it.
	 */
	const USmartObjectDefinition& GetSmartObjectDefinition() const
	{
		checkf(EntityView.IsSet(), TEXT("Definition can only be accessed through a valid SlotView"));
		return *(EntityView.GetConstSharedFragmentData<FSmartObjectSlotDefinitionFragment>().SmartObjectDefinition);
	}

	/**
	 * Returns a reference to the main definition of the slot.
	 * Method will fail a check if called on an invalid SlotView.
	 */
	const FSmartObjectSlotDefinition& GetDefinition() const
	{
		checkf(EntityView.IsSet(), TEXT("Definition can only be accessed through a valid SlotView"));
		return *(EntityView.GetConstSharedFragmentData<FSmartObjectSlotDefinitionFragment>().SlotDefinition);
	}

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag filtering policy.
	 * Method will fail a check if called on an invalid SlotView.
	 */
	void GetActivityTags(FGameplayTagContainer& OutActivityTags) const
	{
		checkf(EntityView.IsSet(), TEXT("Definition can only be accessed through a valid SlotView"));
		const FSmartObjectSlotDefinitionFragment& DefinitionFragment = EntityView.GetConstSharedFragmentData<FSmartObjectSlotDefinitionFragment>();
		checkf(DefinitionFragment.SmartObjectDefinition != nullptr, TEXT("SmartObjectDefinition should always be valid in a valid SlotView"));
		checkf(DefinitionFragment.SlotDefinition != nullptr, TEXT("SlotDefinition should always be valid in a valid SlotView"));

		DefinitionFragment.SmartObjectDefinition->GetSlotActivityTags(*DefinitionFragment.SlotDefinition, OutActivityTags);
	}

	/**
	 * Returns a reference to the definition data of the specified type from the main slot definition.
	 * Method will fail a check if the slot definition doesn't contain the given type.
	 */
	template<typename T>
	const T& GetDefinitionData() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectSlotDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectSlotDefinitionData or one of its child-types.");

		const FSmartObjectSlotDefinition& SlotDefinition = GetDefinition();
		for (const FInstancedStruct& Data : SlotDefinition.Data)
		{
			if (Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return Data.Get<T>();
			}
		}

		return nullptr;
	}

	/**
	 * Returns a pointer to the definition data of the specified type from the main slot definition.
	 * Method will return null if the slot doesn't contain the given type.
	 */
	template<typename T>
	const T* GetDefinitionDataPtr() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectSlotDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectSlotDefinitionData or one of its child-types.");

		const FSmartObjectSlotDefinition& SlotDefinition = GetDefinition();
		for (const FInstancedStruct& Data : SlotDefinition.Data)
		{
			if (Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return Data.GetPtr<T>();
			}
		}

		return nullptr;
	}

	/** @return the claim state of the slot. */
	ESmartObjectSlotState GetState() const
	{
		checkf(Slot, TEXT("State can only be accessed through a valid SlotView"));
		return Slot->GetState();
	}

	/** @return true of the slot can be claimed. */
	bool CanBeClaimed() const
	{
		checkf(Slot, TEXT("Claim can only be accessed through a valid SlotView"));
		return Slot->CanBeClaimed();
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
		checkf(Slot, TEXT("Tags can only be accessed through a valid SlotView"));
		return Slot->GetOwnerRuntimeObject();
	}

private:
	friend class USmartObjectSubsystem;

	FSmartObjectSlotView(const FMassEntityManager& EntityManager, const FSmartObjectSlotHandle SlotHandle, const FSmartObjectRuntimeSlot* InSlot) 
		: EntityView(EntityManager, SlotHandle.EntityHandle)
		, Slot(InSlot)
	{}

	FMassEntityView EntityView;
	const FSmartObjectRuntimeSlot* Slot = nullptr;
};
