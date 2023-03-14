// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassEntityView.h"
#include "Delegates/DelegateCombinations.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRuntime.generated.h"

/** Delegate fired when a given tag is added or removed. Tags on smart object are not using reference counting so count will be 0 or 1 */
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
	Disabled
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

	UPROPERTY(Transient)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(Transient)
	FSmartObjectSlotHandle SlotHandle;

	UPROPERTY(Transient)
	FSmartObjectUserHandle UserHandle;
};

/**
 * Runtime data holding the final slot transform (i.e. parent transform applied on slot local offset and rotation)
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API  FSmartObjectSlotTransform : public FSmartObjectSlotStateData
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
struct FSmartObjectSlotClaimState
{
	GENERATED_BODY()

public:
	/* Provide default constructor to be able to compile template instantiation 'UScriptStruct::TCppStructOps<FSmartObjectSlotState>' */
	/* Also public to pass void 'UScriptStruct::TCppStructOps<FSmartObjectSlotState>::ConstructForTests(void *)' */
	FSmartObjectSlotClaimState() {}

	ESmartObjectSlotState GetState() const { return State; }

	bool CanBeClaimed() const { return State == ESmartObjectSlotState::Free; }

protected:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;
	friend struct FSmartObjectRuntime;

	bool Claim(const FSmartObjectUserHandle& InUser);
	bool Release(const FSmartObjectClaimHandle& ClaimHandle, const ESmartObjectSlotState NewState, const bool bAborted);
	
	friend FString LexToString(const FSmartObjectSlotClaimState& ClaimState)
	{
		return FString::Printf(TEXT("User:%s State:%s"), *LexToString(ClaimState.User), *UEnum::GetValueAsString(ClaimState.State));
	}

	/** Handle to the user that reserves or uses the slot */
	FSmartObjectUserHandle User;

	/** Delegate used to notify when a slot gets invalidated. See RegisterSlotInvalidationCallback */
	FOnSlotInvalidated OnSlotInvalidatedDelegate;

	/** Current availability state of the slot */
	ESmartObjectSlotState State = ESmartObjectSlotState::Free;
};

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

	/** Returns delegate that is invoked whenever a tag is added or removed */
	FOnSmartObjectTagChanged& GetTagChangedDelegate() { return OnTagChangedDelegate; }

	/** Indicates that this instance is still part of the simulation (space partition) but should not be considered valid by queries */
	uint32 IsDisabled() const { return bDisabledByTags; }

	/* Provide default constructor to be able to compile template instantiation 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>' */
	/* Also public to pass void 'UScriptStruct::TCppStructOps<FSmartObjectRuntime>::ConstructForTests(void *)' */
	FSmartObjectRuntime() {}

private:
	/** Struct could have been nested inside the subsystem but not possible with USTRUCT */
	friend class USmartObjectSubsystem;

	explicit FSmartObjectRuntime(const USmartObjectDefinition& Definition);

	void SetTransform(const FTransform& Value) { Transform = Value; }

	void SetRegisteredHandle(const FSmartObjectHandle Value) { RegisteredHandle = Value; }

	/** Runtime SlotHandles associated to each defined slot */
	TArray<FSmartObjectSlotHandle> SlotHandles;

	/** Associated smart object definition */
	UPROPERTY()
	TObjectPtr<const USmartObjectDefinition> Definition = nullptr;

	/** Instance specific transform */
	FTransform Transform;

	/** Tags applied to the current instance */
	FGameplayTagContainer Tags;

	/** Delegate fired whenever a new tag is added or an existing one gets removed */
	FOnSmartObjectTagChanged OnTagChangedDelegate;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered with SmartObjectSubsystem */
	FSmartObjectHandle RegisteredHandle;

	/** Spatial representation data associated to the current instance */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (BaseStruct = "/Script/SmartObjectsModule.SmartObjectSpatialEntryData", ExcludeBaseStruct))
	FInstancedStruct SpatialEntryData;

#if UE_ENABLE_DEBUG_DRAWING
	FBox Bounds = FBox(EForceInit::ForceInit);
#endif

	/** Each slot has its own disable state but keeping it also in the parent instance allow faster validation in some cases. */
	bool bDisabledByTags = false;
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

private:
	friend class USmartObjectSubsystem;

	FSmartObjectSlotView(const FMassEntityManager& EntityManager, const FSmartObjectSlotHandle SlotHandle) 
		: EntityView(EntityManager, SlotHandle.EntityHandle)
	{}

	FMassEntityView EntityView;
};
