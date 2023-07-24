// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "EngineDefines.h"
#include "GameplayTagContainer.h"
#include "Math/Box.h"
#include "SmartObjectTypes.generated.h"

class FDebugRenderSceneProxy;

#define WITH_SMARTOBJECT_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && 1)

SMARTOBJECTSMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartObject, Warning, All);

namespace UE::SmartObjects
{
#if WITH_EDITORONLY_DATA
	inline const FName WithSmartObjectTag = FName("WithSmartObject");
#endif // WITH_EDITORONLY_DATA
}

/** Indicates how Tags from slots and parent object are combined to be evaluated by a TagQuery from a find request. */
UENUM()
enum class ESmartObjectTagMergingPolicy : uint8
{
	/** Tags are combined (parent object and slot) and TagQuery from the request will be run against the combined list. */
	Combine,
	/** Tags in slot (if any) will be used instead of the parent object Tags when running the TagQuery from a request. Empty Tags on a slot indicates no override. */
	Override
};


/** Indicates how TagQueries from slots and parent object will be processed against Tags from a find request. */
UENUM()
enum class ESmartObjectTagFilteringPolicy : uint8
{
	/** TagQueries in the object and slot definitions are not used by the framework to filter results. Users can access them and perform its own filtering. */
	NoFilter,
	/** Both TagQueries (parent object and slot) will be applied to the Tags provided by a request. */
	Combine,
	/** TagQuery in slot (if any) will be used instead of the parent object TagQuery to run against the Tags provided by a request. EmptyTagQuery on a slot indicates no override. */
	Override
};


/**
 * Handle to a smartobject user.
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectUserHandle
{
	GENERATED_BODY()

public:
	FSmartObjectUserHandle() = default;

	bool IsValid() const { return *this != Invalid; }
	void Invalidate() { *this = Invalid; }

	bool operator==(const FSmartObjectUserHandle& Other) const { return ID == Other.ID; }
	bool operator!=(const FSmartObjectUserHandle& Other) const { return !(*this == Other); }

	friend FString LexToString(const FSmartObjectUserHandle& UserHandle)
	{
		return LexToString(UserHandle.ID);
	}

private:
	/** Valid Id must be created by the subsystem */
	friend class USmartObjectSubsystem;

	explicit FSmartObjectUserHandle(const uint32 InID) : ID(InID) {}

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	uint32 ID = INDEX_NONE;

public:
	static const FSmartObjectUserHandle Invalid;
};


/**
 * Handle to a registered smartobject.
 * Internal IDs are assigned in editor by the collection and then serialized for runtime.
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectHandle
{
	GENERATED_BODY()

public:
	FSmartObjectHandle() {}

	/**
	 * Indicates that the handle was properly assigned but doesn't guarantee that the associated object is still accessible.
	 * This information requires a call to `USmartObjectSubsystem::IsObjectValid` using the handle.
	 */
	bool IsValid() const { return *this != Invalid; }
	void Invalidate() { *this = Invalid; }

	friend FString LexToString(const FSmartObjectHandle Handle)
	{
		return LexToString(Handle.ID);
	}

	bool operator==(const FSmartObjectHandle Other) const { return ID == Other.ID; }
	bool operator!=(const FSmartObjectHandle Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FSmartObjectHandle Handle)
	{
		return Handle.ID;
	}

private:
	/** Valid Id must be created by the collection */
	friend struct FSmartObjectHandleFactory;

	explicit FSmartObjectHandle(const uint32 InID) : ID(InID) {}

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	uint32 ID = INDEX_NONE;

 public:
 	static const FSmartObjectHandle Invalid;
};


/**
 * Struct used to identify a runtime slot instance
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectSlotHandle
{
	GENERATED_BODY()

public:
	FSmartObjectSlotHandle() = default;

	/**
	 * Indicates that the handle was properly assigned but doesn't guarantee that the associated slot is still accessible.
	 * This information requires a call to `USmartObjectSubsystem::IsSlotValid` using the handle.
	 */
	bool IsValid() const { return EntityHandle.IsValid(); }
	void Invalidate() { EntityHandle.Reset(); }

	bool operator==(const FSmartObjectSlotHandle Other) const { return EntityHandle == Other.EntityHandle; }
	bool operator!=(const FSmartObjectSlotHandle Other) const { return !(*this == Other); }
	/** Has meaning only for sorting purposes */
	bool operator<(const FSmartObjectSlotHandle Other) const { return EntityHandle < Other.EntityHandle; }

	friend uint32 GetTypeHash(const FSmartObjectSlotHandle SlotHandle)
	{
		return GetTypeHash(SlotHandle.EntityHandle);
	}

	friend FString LexToString(const FSmartObjectSlotHandle SlotHandle)
	{
		return LexToString(SlotHandle.EntityHandle.Index);
	}

protected:
	/** Do not expose the EntityHandle anywhere else than SlotView or the Subsystem. */
	friend class USmartObjectSubsystem;
	friend struct FSmartObjectSlotView;

	FSmartObjectSlotHandle(const FMassEntityHandle InEntityHandle) : EntityHandle(InEntityHandle)
	{
	}

	operator FMassEntityHandle() const
	{
		return EntityHandle;
	}

	/** The MassEntity associated to the slot */
	FMassEntityHandle EntityHandle;
};


/**
 * This is the base struct to inherit from to store custom definition data within the main slot definition
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotDefinitionData
{
	GENERATED_BODY()
	virtual ~FSmartObjectSlotDefinitionData() {}
};

/**
 * This is the base struct to inherit from to store custom state data associated to a slot
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotStateData : public FMassFragment
{
	GENERATED_BODY()
};

/**
 * This is the base struct to inherit from to store some data associated to a specific entry in the spatial representation structure
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSpatialEntryData
{
	GENERATED_BODY()
};

/**
 * Base class for space partitioning structure that can be used to store smart object locations
 */
UCLASS(Abstract)
class SMARTOBJECTSMODULE_API USmartObjectSpacePartition : public UObject
{
	GENERATED_BODY()

public:
	virtual void SetBounds(const FBox& Bounds) {}
	virtual FInstancedStruct Add(const FSmartObjectHandle Handle, const FBox& Bounds) { return FInstancedStruct(); }
	virtual void Remove(const FSmartObjectHandle Handle, FStructView EntryData) {}
	virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) {}

#if UE_ENABLE_DEBUG_DRAWING
	virtual void Draw(FDebugRenderSceneProxy* DebugProxy) {}
#endif
};


/**
 * Helper struct to wrap basic functionalities to store the index of a slot in a SmartObject definition
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectSlotIndex
{
	GENERATED_BODY()

	explicit FSmartObjectSlotIndex(const int32 InSlotIndex = INDEX_NONE) : Index(InSlotIndex) {}

	bool IsValid() const { return Index != INDEX_NONE; }
	void Invalidate() { Index = INDEX_NONE; }

	operator int32() const { return Index; }

	bool operator==(const FSmartObjectSlotIndex& Other) const { return Index == Other.Index; }
	friend FString LexToString(const FSmartObjectSlotIndex& SlotIndex) { return FString::Printf(TEXT("[Slot:%d]"), SlotIndex.Index); }

private:
	UPROPERTY(Transient)
	int32 Index = INDEX_NONE;
};

/**
 * Reference to a specific Smart Object slot in a Smart Object Definition.
 * When placed on a slot definition data, the Index is resolved automatically when changed, on load and save. 
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotReference
{
	GENERATED_BODY()

	static constexpr uint8 InvalidValue = 0xff;

	bool IsValid() const { return Index != InvalidValue; }

	int32 GetIndex() const { return Index == InvalidValue ? INDEX_NONE : Index; }
	
	void SetIndex(const int32 InIndex)
	{
		if (InIndex >= 0 && InIndex < InvalidValue)
		{
			Index = (uint8)InIndex;
		}
		else
		{
			Index = InvalidValue; 
		}
	}

#if WITH_EDITORONLY_DATA
	const FGuid& GetSlotID() const { return SlotID; }
#endif
	
private:
	UPROPERTY()
	uint8 Index = InvalidValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid SlotID;
#endif // WITH_EDITORONLY_DATA

	friend class FSmartObjectSlotReferenceDetails;
};

/**
 * Describes how Smart Object or slot was changed.
 */
UENUM()
enum class ESmartObjectChangeReason : uint8
{
	/** No Change. */
	None,
	/** External event sent. */
	OnEvent,
	/** A tag was added. */
	OnTagAdded,
	/** A tag was removed. */
	OnTagRemoved,
	/** Slot was claimed. */
	OnClaimed,
	/** Slot claim was released. */
	OnReleased,
	/** Object or slot was enabled. */
	OnEnabled,
	/** Object or slot was disabled. */
	OnDisabled,
};

/**
 * Strict describing a change in Smart Object or Slot. 
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectEventData
{
	GENERATED_BODY()

	/** Handle to the changed Smart Object. */
	UPROPERTY(Transient)
	FSmartObjectHandle SmartObjectHandle;

	/** Handle to the changed slot, if invalid, the event is for the object. */
	UPROPERTY(Transient)
	FSmartObjectSlotHandle SlotHandle;

	/** Change reason. */
	UPROPERTY(Transient)
	ESmartObjectChangeReason Reason = ESmartObjectChangeReason::None;

	/** Added/Removed tag, or event tag, depending on Reason. */
	UPROPERTY(Transient)
	FGameplayTag Tag;

	/** Event payload. */
	FConstStructView EventPayload;
};

/** Delegate called when Smart Object or Slot is changed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartObjectEvent, const FSmartObjectEventData& /*Event*/);
