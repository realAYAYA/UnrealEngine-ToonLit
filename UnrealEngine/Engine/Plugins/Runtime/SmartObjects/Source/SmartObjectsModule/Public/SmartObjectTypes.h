// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollisionShape.h"
#include "Containers/ArrayView.h"
#include "Engine/ActorInstanceHandle.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "Math/Box.h"
#include "StructView.h"
#include "SmartObjectTypes.generated.h"

class FDebugRenderSceneProxy;
class UNavigationQueryFilter;
class USmartObjectSlotValidationFilter;
class USmartObjectComponent;
class UWorld;

#define WITH_SMARTOBJECT_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && 1)

SMARTOBJECTSMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogSmartObject, Warning, All);

/** Delegate called when Smart Object or Slot is changed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartObjectEvent, const FSmartObjectEventData& /*Event*/);


namespace UE::SmartObject
{
#if WITH_EDITORONLY_DATA
	inline const FName WithSmartObjectTag = FName("WithSmartObject");
#endif // WITH_EDITORONLY_DATA
}

namespace UE::SmartObject::EnabledReason
{
SMARTOBJECTSMODULE_API extern FGameplayTag Gameplay;
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
 * Enum indicating if we're looking for a location to enter or exit the Smart Object slot.
 */
UENUM()
enum class ESmartObjectSlotNavigationLocationType : uint8
{
	/** Find a location to enter the slot. */
	Entry,
	
	/** Find a location to exit the slot. */
	Exit,
};

/** Enum indicating the claim priority of a Smart Object slot. */
UENUM(BlueprintType)
enum class ESmartObjectClaimPriority : uint8
{
	None UMETA(Hidden),

	Low,
	BelowNormal,
	Normal,
	AboveNormal,
	High,

	MIN = None UMETA(Hidden),
	MAX = High UMETA(Hidden)
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
		return FString::Printf(TEXT("0x%016llX:%c"), Handle.ID, (Handle.ID & DynamicIdsBitMask) != 0 ? 'D' : 'P');
	}

	bool operator==(const FSmartObjectHandle Other) const { return ID == Other.ID; }
	bool operator!=(const FSmartObjectHandle Other) const { return !(*this == Other); }

	/** Has meaning only for sorting purposes */
	bool operator<(const FSmartObjectHandle Other) const { return ID < Other.ID; }

	friend uint32 GetTypeHash(const FSmartObjectHandle Handle)
	{
		return CityHash32(reinterpret_cast<const char*>(&Handle.ID), sizeof Handle.ID);
	}

private:
	/** Valid Id must be created by the collection */
	friend struct FSmartObjectHandleFactory;

	explicit FSmartObjectHandle(const uint64 InID) : ID(InID) {}

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	uint64 ID = InvalidID;

	/**
	 * All Ids with this bit set were assigned for dynamic ('D') entries that rely on the component lifetime.
	 * Otherwise their Ids are from persistent collections ('P').
	 */
	static constexpr uint64 DynamicIdsBitMask = 1ULL << 63;
	static constexpr uint64 InvalidID = 0;
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
	bool IsValid() const { return SmartObjectHandle.IsValid(); }
	void Invalidate()
	{
		SmartObjectHandle = {};
		SlotIndex = INDEX_NONE;
	}

	bool operator==(const FSmartObjectSlotHandle Other) const { return SmartObjectHandle == Other.SmartObjectHandle && SlotIndex == Other.SlotIndex; }
	bool operator!=(const FSmartObjectSlotHandle Other) const { return !(*this == Other); }
	
	/** Has meaning only for sorting purposes */
	bool operator<(const FSmartObjectSlotHandle Other) const
	{
		if (SmartObjectHandle == Other.SmartObjectHandle)
		{
			return SlotIndex < Other.SlotIndex;
		}
		return SmartObjectHandle < Other.SmartObjectHandle;
	}

	friend uint32 GetTypeHash(const FSmartObjectSlotHandle SlotHandle)
	{
		return HashCombineFast(GetTypeHash(SlotHandle.SmartObjectHandle), GetTypeHash(SlotHandle.SlotIndex));
	}

	friend FString LexToString(const FSmartObjectSlotHandle SlotHandle)
	{
		return LexToString(SlotHandle.SmartObjectHandle) + TEXT(":") + LexToString(SlotHandle.SlotIndex);
	}

	FSmartObjectHandle GetSmartObjectHandle() const { return SmartObjectHandle; }
	int32 GetSlotIndex() const { return SlotIndex; }
	
protected:
	/** Do not expose the EntityHandle anywhere else than SlotView or the Subsystem. */
	friend class USmartObjectSubsystem;
	friend struct FSmartObjectSlotView;

	FSmartObjectSlotHandle(const FSmartObjectHandle InSmartObjectHandle, const int32 SlotIndex)
		: SmartObjectHandle(InSmartObjectHandle)
		, SlotIndex(SlotIndex)
	{
	}

	FSmartObjectHandle SmartObjectHandle;
	int32 SlotIndex = INDEX_NONE;
};


/**
 * This is the base struct to inherit from to store custom definition data within the main slot definition
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionData
{
	GENERATED_BODY()
	virtual ~FSmartObjectDefinitionData() {}
};

using FSmartObjectSlotDefinitionData UE_DEPRECATED(5.4, "Deprecated struct. Please use FSmartObjectDefinitionData instead.") = FSmartObjectDefinitionData;

/**
 * This is the base struct to inherit from to store custom state data associated to a slot
 */
USTRUCT(meta=(Hidden))
struct SMARTOBJECTSMODULE_API FSmartObjectSlotStateData
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
	
	virtual void Add(const FSmartObjectHandle Handle, const FBox& Bounds, FInstancedStruct& OutHandle) {}
	virtual void Remove(const FSmartObjectHandle Handle, FStructView EntryData) {}
	virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) {}

#if UE_ENABLE_DEBUG_DRAWING
	virtual void Draw(FDebugRenderSceneProxy* DebugProxy) {}
#endif

	UE_DEPRECATED(5.4, "Use version of the Add that takes the instanced struct as parameter.")
	virtual FInstancedStruct Add(const FSmartObjectHandle Handle, const FBox& Bounds) final { return FInstancedStruct(); }
};


/**
 * Helper struct to wrap basic functionalities to store the index of a slot in a SmartObject definition
 */
USTRUCT(BlueprintType)
struct UE_DEPRECATED(5.3, "This type is deprecated and no longer being used.") SMARTOBJECTSMODULE_API FSmartObjectSlotIndex
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



/** Indicates how TagQueries from slots and parent object will be processed against Tags from a find request. */
UENUM()
enum class ESmartObjectTraceType : uint8
{
	ByChannel,
	ByProfile,
	ByObjectTypes,
};

/** Struct used to define how traces should be handled. */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectTraceParams
{
	GENERATED_BODY()

	FSmartObjectTraceParams() = default;
	
	explicit FSmartObjectTraceParams(const ETraceTypeQuery InTraceChanel)
		: Type(ESmartObjectTraceType::ByChannel)
		, TraceChannel(InTraceChanel)
	{
	}

	explicit FSmartObjectTraceParams(TConstArrayView<EObjectTypeQuery> InObjectTypes)
		: Type(ESmartObjectTraceType::ByObjectTypes)
	{
		for (const EObjectTypeQuery ObjectType : InObjectTypes)
		{
			ObjectTypes.Add(ObjectType);
		}
	}

	explicit FSmartObjectTraceParams(const FCollisionProfileName InCollisionProfileName)
		: Type(ESmartObjectTraceType::ByProfile)
		, CollisionProfile(InCollisionProfileName)
	{
	}

	/** Type of trace to use. */
	UPROPERTY(EditAnywhere, Category = "Default")
	ESmartObjectTraceType Type = ESmartObjectTraceType::ByChannel;

	/** Trace channel to use to determine collisions. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (EditCondition = "Type == ESmartObjectTraceType::ByChannel", EditConditionHides))
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	/** Object types to use to determine collisions. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (EditCondition = "Type == ESmartObjectTraceType::ByObjectTypes", EditConditionHides))
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	/** Collision profile to use to determine collisions. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (EditCondition = "Type == ESmartObjectTraceType::ByProfile", EditConditionHides))
	FCollisionProfileName CollisionProfile;

	/** Whether we should trace against complex collision */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bTraceComplex = false;
};

/** Struct defining a collider in world space. */
struct SMARTOBJECTSMODULE_API FSmartObjectAnnotationCollider
{
	/** Location of the collision shape. */
	FVector Location = FVector::ZeroVector;
	
	/** Rotation of the collision shape. */
	FQuat Rotation = FQuat::Identity;
	
	/** Shape of the collider. */
	FCollisionShape CollisionShape;
};

/** Struct defining Smart Object user capsule size. */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectUserCapsuleParams
{
	GENERATED_BODY()

	FSmartObjectUserCapsuleParams() = default;
	FSmartObjectUserCapsuleParams(const float InRadius, const float InHeight, const float InStepHeight)
		: Radius(InRadius)
		, Height(InHeight)
		, StepHeight(InStepHeight)
	{
	}

	/** Invalid capsule. */
	static const FSmartObjectUserCapsuleParams Invalid;
	
	bool IsValid() const
	{
		return Radius > 0.f && Height > 0.f && StepHeight > 0.f;
	}
	
	/**
	 * Returns the capsule as an annotation collider at specified world location and rotation.
	 * The capsule is placed so that Z-axis of the rotation is considered up.
	 * The values specified in the struct will be constrained to create valid collider (and thus can differ from the set values).
	 * @param Location Location of the collider.
	 * @param Rotation Rotation of the collider.
	 * @return annotation collider representing the capsule. */
	FSmartObjectAnnotationCollider GetAsCollider(const FVector& Location, const FQuat& Rotation) const;
	
	/** Radius of the capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0.0"))
	float Radius = 35.0f;

	/** Full height of the capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0.0"))
	float Height = 180.0f;

	/** Step up height. This space is ignored when testing collisions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta = (ClampMin = "0.0"))
	float StepHeight = 50.0f;
};

/**
 * Parameters for Smart Object navigation and collision validation. 
 */
USTRUCT()
struct FSmartObjectSlotValidationParams
{
	GENERATED_BODY()

public:
	/** @return navigation filter class to be used for navigation checks. */
	TSubclassOf<UNavigationQueryFilter> GetNavigationFilter() const { return NavigationFilter; }

	/** @return search extents used to define how far the validation can move the points. */
	FVector GetSearchExtents() const { return SearchExtents; }

	/** @return trace parameters for finding ground location. */
	const FSmartObjectTraceParams& GetGroundTraceParameters() const { return GroundTraceParameters; }

	/** @return trace parameters for testing if there are collision transitioning from navigation location to slot location. */
	const FSmartObjectTraceParams& GetTransitionTraceParameters() const { return TransitionTraceParameters; }

	/** @return reference to user capsule parameters. */
	const FSmartObjectUserCapsuleParams& GetUserCapsule() const { return UserCapsule; }

	/**
	 * Selects between specified NavigationCapsule size and capsule size defined in the params based on bUseNavigationCapsuleSize.
	 * @param NavigationCapsule Size of the navigation capsule.
	 * @return reference to selected capsule. */
	const FSmartObjectUserCapsuleParams& GetUserCapsule(const FSmartObjectUserCapsuleParams& NavigationCapsule) const;

	/**
	 * Gets user capsule for a specified actor, if bUseNavigationCapsuleSize is specified uses INavAgentInterface to forward the values from navigation system.
	 * The method can fail if the navigation capsule is requested, but we fail to get the navigation properties from the actor. 
	 * @param UserActor Actor used to look up navigation settings from.
	 * @param OutCapsule Dimensions of the user capsule.
	 * @return true operation succeeds. */
	bool GetUserCapsuleForActor(const AActor& UserActor, FSmartObjectUserCapsuleParams& OutCapsule) const;

	/**
	 * Gets default user capsule size used for preview when the user actor is now known.
	 * The method can fail if the navigation capsule is requested, but we fail to get the navigation properties from the world.
	 * @param World where to look for default navigation settings.
	 * @param OutCapsule Dimensions of the user capsule.
	 * @return true if operation succeeds. */
	bool GetPreviewUserCapsule(const UWorld& World, FSmartObjectUserCapsuleParams& OutCapsule) const;

protected:
	/** Navigation filter used to validate entrance locations. */
	UPROPERTY(EditAnywhere, Category = "Default")
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	/** How far we allow the validated location to be from the specified navigation location. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FVector SearchExtents = FVector(5.0f, 5.0f, 40.0f);

	/** Trace parameters used for finding navigation location on ground. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FSmartObjectTraceParams GroundTraceParameters;

	/** Trace parameters user for checking if the transition between navigation location and slot is unblocked. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FSmartObjectTraceParams TransitionTraceParameters;

	/** If true, the capsule size is queried from the user actor via INavAgentInterface. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bUseNavigationCapsuleSize = false;

	/**
	 * Dimensions of the capsule used for testing if user can fit into a specific location.
	 * If bUseNavigationCapsuleSize is set, the capsule size from the Actor navigation settings is used if the actor is present (otherwise we fallback to the UserCapsule). 
	 */
	UPROPERTY(EditAnywhere, Category = "Default")
	FSmartObjectUserCapsuleParams UserCapsule;
};

/**
 * Class used to define settings for Smart Object navigation and collision validation.
 * It is possible to specify two set of validation parameters. The one labeled "entry" is used for validating
 * entry locations and other general collision validation.
 * A separate set can be defined for checking exit locations. This allows the exit location checking to be relaxed.
 * E.g. we might not allow to enter the SO on water area, but it is fine to exit on water.
 * The values of the CDO are used, the users are expected to derive from this class to create custom settings. 
 */
UCLASS(Blueprintable, Abstract)
class SMARTOBJECTSMODULE_API USmartObjectSlotValidationFilter : public UObject
{
	GENERATED_BODY()

public:

	/** @return validation parameters based on location type (enter & exit) */
	const FSmartObjectSlotValidationParams& GetValidationParams(const ESmartObjectSlotNavigationLocationType LocationType) const
	{
		return LocationType == ESmartObjectSlotNavigationLocationType::Entry ? GetEntryValidationParams() : GetExitValidationParams();
	}
	
	/** @return validation parameters for entry validation, and general use. */
	const FSmartObjectSlotValidationParams& GetEntryValidationParams() const
	{
		return EntryParameters;
	}

	/** @return validation parameters for exit validation. */
	const FSmartObjectSlotValidationParams& GetExitValidationParams() const
    {
    	return bUseEntryParametersForExit ? EntryParameters : ExitParameters;
    }

protected:
	/** Parameters to use for validating entry locations or general collision validation. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FSmartObjectSlotValidationParams EntryParameters;

	/** If true, use separate settings for validating exit locations. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bUseEntryParametersForExit = true;

	/** Parameters to use for validating exit locations. The separate set allows to specify looser settings on exits. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (EditCondition = "bUseEntryParametersForExit == false", EditConditionHides))
	FSmartObjectSlotValidationParams ExitParameters;
};

/**
 * Describes how Smart Object or slot was changed.
 */
UENUM(BlueprintType)
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
	/** Slot is now occupied*/
	OnOccupied,
	/** Slot claim was released. */
	OnReleased,
	/** Slot was enabled. */
	OnSlotEnabled,
	/** Slot was disabled. */
	OnSlotDisabled,
	/** Object was enabled. */
	OnObjectEnabled,
	/** Object was disabled. */
	OnObjectDisabled,
	/** Related Smart Object Component is bound to simulation. */
	OnComponentBound,
	/** Related Smart Object Component is unbound from simulation. */
	OnComponentUnbound,
};

/**
 * Struct describing a change in Smart Object or Slot. 
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectEventData
{
	GENERATED_BODY()

	/** Handle to the changed Smart Object. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SmartObject")
	FSmartObjectHandle SmartObjectHandle;

	/** Handle to the changed slot, if invalid, the event is for the object. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SmartObject")
	FSmartObjectSlotHandle SlotHandle;

	/** Change reason. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SmartObject")
	ESmartObjectChangeReason Reason = ESmartObjectChangeReason::None;

	/** Added/Removed tag, or event tag, depending on Reason. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SmartObject")
	FGameplayTag Tag;

	/**
	 * Event payload.
	 * For external event (i.e. SendSlotEvent) payload is provided by the caller.
	 * For internal event types (e.g. OnClaimed, OnReleased, etc.)
	 * payload is the user data struct provided on claim.
	 **/
	FConstStructView EventPayload;
};

/**
 * Struct that can be used to pass data to the find or filtering methods.
 * Properties will be used as user data to fill values expected by the world condition schema
 * specified by the smart object definition.
 *		e.g. FilterSlotsBySelectionConditions(SlotHandles, FConstStructView::Make(FSmartObjectActorUserData(Pawn)));
 *
 * It can be inherited from to provide additional data to another world condition schema inheriting
 * from USmartObjectWorldConditionSchema.
 *	e.g.
 *		UCLASS()
 *		class USmartObjectWorldConditionExtendedSchema : public USmartObjectWorldConditionSchema
 *		{
 *			...
 *			USmartObjectWorldConditionExtendedSchema(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
 *			{
 *				OtherActorRef = AddContextDataDesc(TEXT("OtherActor"), AActor::StaticClass(), EWorldConditionContextDataType::Dynamic);
 *			}
 *			
 *			FWorldConditionContextDataRef OtherActorRef;
 *		};
 *
 *		USTRUCT()
 *		struct FSmartObjectActorExtendedUserData : public FSmartObjectActorUserData
 *		{
 *			UPROPERTY()
 *			TWeakObjectPtr<const AActor> OtherActor = nullptr;
 *		}
 *
 * The struct can also be used to be added to a Smart Object slot when it gets claimed.
 *		e.g. Claim(SlotHandle, FConstStructView::Make(FSmartObjectActorUserData(Pawn)));
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectActorUserData
{
	GENERATED_BODY()

	FSmartObjectActorUserData() = default;
	explicit FSmartObjectActorUserData(const AActor* InUserActor);

	UPROPERTY()
	TWeakObjectPtr<const AActor> UserActor = nullptr;
};

/**
 * Struct that can be used to pass data related to the owner of a created SmartObject.
 * It identifies an instanced actor that could be in its lightweight representation or a normal actor.
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectActorOwnerData
{
	GENERATED_BODY()

	FSmartObjectActorOwnerData() = default;
	explicit FSmartObjectActorOwnerData(AActor* Actor): Handle(Actor)
	{
	}
	explicit FSmartObjectActorOwnerData(const FActorInstanceHandle& Handle) : Handle(Handle)
	{
	}

	UPROPERTY()
	FActorInstanceHandle Handle;
};

/**
 * Struct used as a friend to FSmartObjectHandle that is the only caller allowed to create a handle from a uint64.
 */
struct FSmartObjectHandleFactory
{
	static FSmartObjectHandle CreateHandleForDynamicObject();
	static FSmartObjectHandle CreateHandleForComponent(const UWorld& World, const USmartObjectComponent& Component);
};