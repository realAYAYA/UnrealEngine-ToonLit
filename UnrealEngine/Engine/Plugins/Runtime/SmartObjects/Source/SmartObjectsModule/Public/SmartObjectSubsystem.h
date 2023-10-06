// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectPersistentCollection.h"
#include "SmartObjectRuntime.h"
#include "WorldConditionContext.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "SmartObjectSubsystem.generated.h"

class UCanvas;
class USmartObjectBehaviorDefinition;

class USmartObjectComponent;
class UWorldPartitionSmartObjectCollectionBuilder;
struct FMassEntityManager;
class ASmartObjectSubsystemRenderingActor;
class FDebugRenderSceneProxy;
class ADEPRECATED_SmartObjectCollection;
class UNavigationQueryFilter;
class ANavigationData;

#if WITH_EDITOR
/** Called when an event related to the main collection occured. */
DECLARE_MULTICAST_DELEGATE(FOnMainCollectionEvent);
#endif

/**
 * Struct that can be used to filter results of a smart object request when trying to find or claim a smart object
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectRequestFilter
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with BehaviorDefinitionClass_DEPRECATED being copied or created in the default methods
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectRequestFilter() = default;
	
	FSmartObjectRequestFilter(const FSmartObjectRequestFilter&) = default;
	FSmartObjectRequestFilter(FSmartObjectRequestFilter&&) = default;
	FSmartObjectRequestFilter& operator=(const FSmartObjectRequestFilter&) = default;
	FSmartObjectRequestFilter& operator=(FSmartObjectRequestFilter&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Pointer to the Actor requesting the Smart Object slot (Optional). */
	UE_DEPRECATED(5.3, "User actor is no longer provided by the request filter. Instead you must provide user data struct to claim find objects or use filter methods.")
	UPROPERTY(Transient)
	TObjectPtr<AActor> UserActor = nullptr;

	/** Gameplay tags of the Actor or Entity requesting the Smart Object slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagContainer UserTags;

	/** Only return slots whose activity tags are matching this query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagQuery ActivityRequirements;

	UE_DEPRECATED(5.1, "Use BehaviorDefinitionClasses instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use BehaviorDefinitionClasses instead"))
	TSubclassOf<USmartObjectBehaviorDefinition> BehaviorDefinitionClass;

	/** If set, will filter out any SmartObject that uses different BehaviorDefinition classes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	TArray<TSubclassOf<USmartObjectBehaviorDefinition>> BehaviorDefinitionClasses;

	/** If true will evaluate the slot and object conditions, otherwise will skip them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	bool bShouldEvaluateConditions = true;

	/** Is set, will filter out any SmartObject that does not pass the predicate. */
	TFunction<bool(FSmartObjectHandle)> Predicate;

	/**
	 * If set, will pass the context data for the selection preconditions.
	 * Any SmartObject that has selection preconditions but does not match the schema set the in the context data will be skipped.
	 */
	UE_DEPRECATED(5.3, "Condition context data is no longer provided by the request filter. Instead you must provide user data struct to find objects or use filter methods.")
	FWorldConditionContextData ConditionContextData;
};

/**
 * Struct used to find a smart object within a specific search range and with optional filtering
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectRequest
{
	GENERATED_BODY()

	FSmartObjectRequest() = default;
	FSmartObjectRequest(const FBox& InQueryBox, const FSmartObjectRequestFilter& InFilter)
		: QueryBox(InQueryBox)
		, Filter(InFilter)
	{}

	/** Box defining the search range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FBox QueryBox = FBox(ForceInitToZero);

	/** Struct used to filter out some results (all results allowed by default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FSmartObjectRequestFilter Filter;
};

/**
 * Struct that holds the object and slot selected by processing a smart object request.
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectRequestResult
{
	GENERATED_BODY()

	explicit FSmartObjectRequestResult(const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectSlotHandle InSlotHandle = {})
		: SmartObjectHandle(InSmartObjectHandle)
		, SlotHandle(InSlotHandle)
	{}

	FSmartObjectRequestResult() = default;

	bool IsValid() const { return SmartObjectHandle.IsValid() && SlotHandle.IsValid(); }

	bool operator==(const FSmartObjectRequestResult& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle
			&& SlotHandle == Other.SlotHandle;
	}

	bool operator!=(const FSmartObjectRequestResult& Other) const
	{
		return !(*this == Other);
	}
	
	friend FString LexToString(const FSmartObjectRequestResult& Result)
	{
		return FString::Printf(TEXT("Object:%s Slot:%s"), *LexToString(Result.SmartObjectHandle), *LexToString(Result.SlotHandle));
	}

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = SmartObject)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = SmartObject)
	FSmartObjectSlotHandle SlotHandle;
};


/**
 * Defines method for selecting slot entry from multiple candidates.
 */
UENUM()
enum class FSmartObjectSlotEntrySelectionMethod : uint8
{
	/** Return first entry location (in order defined in the slot definition). */
	First,
	
	/** Return nearest entry to specified search location. */
	NearestToSearchLocation,
};

/**
 * Handle describing a specific entrance on a Smart Object slot.
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectSlotEntranceHandle
{
	GENERATED_BODY()

	FSmartObjectSlotEntranceHandle() = default;

	FSmartObjectSlotHandle GetSlotHandle() const { return SlotHandle; }
	
	bool IsValid() const { return Type != EType::Invalid; }

	bool operator==(const FSmartObjectSlotEntranceHandle& Other) const
	{
		return SlotHandle == Other.SlotHandle && Type == Other.Type && Index == Other.Index;
	}

	bool operator!=(const FSmartObjectSlotEntranceHandle& Other) const
	{
		return !(*this == Other);
	}

private:

	enum class EType : uint8
	{
		Invalid,	// Handle is invalid
		Entrance,	// The handle points to a specific entrance, index is slot data index.
		Slot,		// The handle points to the slot itself.
	};
	
	explicit FSmartObjectSlotEntranceHandle(const FSmartObjectSlotHandle InSlotHandle, const EType InType, const uint8 InIndex = 0)
		: SlotHandle(InSlotHandle)
		, Type(InType)
		, Index(InIndex)
	{
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject", meta = (AllowPrivateAccess = "true"))
	FSmartObjectSlotHandle SlotHandle;
	
	EType Type = EType::Invalid;
	uint8 Index = 0;
	
	friend class USmartObjectSubsystem;
};

/**
 * Struct used to request slot entry or exit location.
 *
 * When used with actor, it is generally enough to set the UserActor. In that case NavigationData, ValidationFilter,
 * and UserCapsule are queried via the INavAgentInterface and USmartObjectUserComponent on the actor if they are _not_ set.
 * 
 * If the user actor is not available (e.g. with Mass), then ValidationFilter and UserCapsule must be defined, and if bProjectNavigationLocation is set NavigationData must be valid. 
 * 
 * The location validation is done in following order:
 *  - navigation projection
 *  - trace ground location (uses altered location from navigation test if applicable)
 *  - check transition trajectory (test between unmodified navigation location and slow location)
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotEntranceLocationRequest
{
	GENERATED_BODY()

	/** Actor that is using the smart object slot. (Optional) */
	TObjectPtr<const AActor> UserActor = nullptr;

	/** Filter to use for the validation. If not set and UserActor is valid, the filter is queried via USmartObjectUserComponent. */
	TSubclassOf<USmartObjectSlotValidationFilter> ValidationFilter = nullptr;

	/** Navigation data to use for the navigation queries. If not set and UserActor is valid, the navigation data is queried via INavAgentInterface. */
	TObjectPtr<const ANavigationData> NavigationData = nullptr;
	
	/** Size of the user of the smart object. If not set and UserActor is valid, the dimensions are queried via INavAgentInterface. */
	TOptional<FSmartObjectUserCapsuleParams> UserCapsule;
	
	/** Search location that may be used to select an entry from multiple candidates. (e.g. user actor location). */
	FVector SearchLocation = FVector::ZeroVector;

	/** How to select an entry when a slot has multiple entries. */
	FSmartObjectSlotEntrySelectionMethod SelectMethod = FSmartObjectSlotEntrySelectionMethod::First;

	/** Enum indicating if we're looking for a location to enter or exit the Smart Object slot. */
	ESmartObjectSlotNavigationLocationType LocationType = ESmartObjectSlotNavigationLocationType::Entry;

	/** If true, try to project the location on navigable area. If projection fails, an entry is discarded. */
	bool bProjectNavigationLocation = true;

	/** If true, try to trace the location on ground. If trace fails, an entry is discarded. */
	bool bTraceGroundLocation = true;

	/** If true, check collisions between navigation location and slot location. If collisions are found, an entry is discarded. */
	bool bCheckTransitionTrajectory = true;

	/** If true, check user capsule collisions at the entrance location. Uses capsule dimensions set in the validation filter. */
	bool bCheckEntranceLocationOverlap = true;

	/** If true, check user capsule collisions at the slot location. Uses capsule dimensions set in an annotation on the slot. */
	bool bCheckSlotLocationOverlap = true;

	/** If true, include slot location as a candidate if no navigation annotation is present. */
	bool bUseSlotLocationAsFallback = false;
};

/**
 * Validated result from FindEntranceLocationForSlot().
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectSlotEntranceLocationResult
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with "Tag" being copied or created in the default methods
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectSlotEntranceLocationResult() = default;
	FSmartObjectSlotEntranceLocationResult(const FSmartObjectSlotEntranceLocationResult&) = default;
	FSmartObjectSlotEntranceLocationResult(FSmartObjectSlotEntranceLocationResult&&) = default;
	FSmartObjectSlotEntranceLocationResult& operator=(const FSmartObjectSlotEntranceLocationResult&) = default;
	FSmartObjectSlotEntranceLocationResult& operator=(FSmartObjectSlotEntranceLocationResult&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** @returns entry as nav location. */
	FNavLocation GetNavLocation() const { return FNavLocation(Location, NodeRef); }

	/** @returns true if the result contains valid navigation node reference. */
	bool HasNodeRef() const { return NodeRef != INVALID_NAVNODEREF; }

	/** The location to enter the slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FVector Location = FVector::ZeroVector;

	/** The expected direction to enter the slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FRotator Rotation = FRotator::ZeroRotator;
	
	/** Node reference in navigation data (if requested with bMustBeNavigable). */
	NavNodeRef NodeRef = INVALID_NAVNODEREF;

	/** Gameplay tag associated with the entrance. */
	UE_DEPRECATED(5.3, "Use Tags instead.")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FGameplayTag Tag;

	/** Gameplay tags associated with the entrance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FGameplayTagContainer Tags;

	/** Handle identifying the entrance that was found. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SmartObject")
	FSmartObjectSlotEntranceHandle EntranceHandle;
};

using FSmartObjectSlotNavigationLocationResult = FSmartObjectSlotEntranceLocationResult; 

/**
 * Result code indicating if the Collection was successfully registered or why it was not.
 */
UENUM()
enum class ESmartObjectCollectionRegistrationResult : uint8
{
	Failed_InvalidCollection,
	Failed_AlreadyRegistered,
	Failed_NotFromPersistentLevel,
	Succeeded
};

/**
 * Mode that indicates how the unregistration of the SmartObjectComponent affects its runtime instance.
 */
UENUM()
enum class UE_DEPRECATED(5.2, "This type is deprecated and no longer being used.") ESmartObjectUnregistrationMode : uint8
{
	KeepRuntimeInstanceActiveIfPartOfCollection,
	DestroyRuntimeInstance
};

/**
 * Subsystem that holds all registered smart object instances and offers the API for spatial queries and reservations.
 */
UCLASS(config = SmartObjects, defaultconfig, Transient)
class SMARTOBJECTSMODULE_API USmartObjectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	USmartObjectSubsystem();

	static USmartObjectSubsystem* GetCurrent(const UWorld* World);

	ESmartObjectCollectionRegistrationResult RegisterCollection(ASmartObjectPersistentCollection& InCollection);
	void UnregisterCollection(ASmartObjectPersistentCollection& InCollection);

	const FSmartObjectContainer& GetSmartObjectContainer() const { return SmartObjectContainer; }
	FSmartObjectContainer& GetMutableSmartObjectContainer() { return SmartObjectContainer; }

	/**
	 * Enables or disables the entire smart object represented by the provided handle.
	 * Delegate 'OnEvent' is broadcasted with ESmartObjectChangeReason::OnEnabled/ESmartObjectChangeReason::OnDisabled if state changed.
	 * @param Handle Handle to the smart object.
	 * @param bEnabled If true enables the smart object, disables otherwise.
	 * @return True when associated smart object is found and set (or already set) to desired state; false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool SetEnabled(const FSmartObjectHandle Handle, const bool bEnabled);

	/**
	 * Returns the enabled state of the smart object represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @return True when associated smart object is found and set to be enabled. False otherwise.
	 */
	bool IsEnabled(const FSmartObjectHandle Handle) const;

	/**
	 * Enables or disables all smart objects associated to the provided actor (multiple components).
	 * @param SmartObjectActor Smart object(s) parent actor.
	 * @param bEnabled If true enables, disables otherwise.
	 * @return True if all (at least one) smartobject components are found with their associated
	 * runtime and values set (or already set) to desired state; false otherwise.
	 */
	bool SetSmartObjectActorEnabled(const AActor& SmartObjectActor, bool bEnabled);

	/**
	 * Registers to the runtime simulation all SmartObject components for a given actor.
	 * @param SmartObjectActor Actor owning the components to register
	 * @return true when components are found and all successfully registered, false otherwise
	 */
	bool RegisterSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Unregisters from the simulation all SmartObject components for a given actor.
	 * @param SmartObjectActor Actor owning the components to unregister
	 * @return true when components are found and all successfully unregistered, false otherwise
	 */
	bool UnregisterSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Removes all data associated to SmartObject components of a given actor from the simulation.
	 * @param SmartObjectActor Actor owning the components to delete
	 * @return whether components are found and all successfully deleted
	 */
	bool RemoveSmartObjectActor(const AActor& SmartObjectActor);

	/**
	 * Registers a SmartObject components to the runtime simulation.
	 * @param SmartObjectComponent SmartObject component to register
	 * @return true when component is successfully registered, false otherwise
	 */
	bool RegisterSmartObject(USmartObjectComponent& SmartObjectComponent);
	
	/**
	 * Unbinds a SmartObject component from the runtime simulation and handles the associated runtime data
	 * according to the component registration type (i.e. runtime data associated to components from persistent collections
	 * will remain in the simulation).
	 * @param SmartObjectComponent SmartObject component to unregister
	 * @return true when component is successfully unregistered, false otherwise
	 */
	bool UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Unbinds a SmartObject component from the runtime simulation and removes its runtime data.
	 * @param SmartObjectComponent SmartObject component to remove
	 * @return whether SmartObject data has been successfully found and removed
	 */
	bool RemoveSmartObject(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Binds a smartobject component to an existing instance in the simulation. If a given SmartObjectComponent has not 
	 * been registered yet an ensure will trigger.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime instance must exist
	*/
	void BindComponentToSimulation(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Returns the component associated to the claim handle if still
	 * accessible. In some scenarios the component may no longer exist
	 * but its smart object data could (e.g. streaming)
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return A pointer to the USmartObjectComponent* associated to the handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	USmartObjectComponent* GetSmartObjectComponent(const FSmartObjectClaimHandle& ClaimHandle) const;

	/**
	 * Returns the component associated to the  given request result
	 * In some scenarios the component may no longer exist
	 * but its smart object data could (e.g. streaming)
	 * @param Result A request result returned by any of the Find methods .
	 * @return A pointer to the USmartObjectComponent* associated to the handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	USmartObjectComponent* GetSmartObjectComponentByRequestResult(const FSmartObjectRequestResult& Result) const;

	/**
	 * Spatial lookup for first slot in range respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return First valid smart object in range. Not the closest one, just the one
	 *		that happens to be retrieved first from space partition
	 */
	[[nodiscard]] FSmartObjectRequestResult FindSmartObject(const FSmartObjectRequest& Request, const FConstStructView UserData) const;

	/**
	 * Spatial lookup for slot candidates respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return True if at least one candidate was found.
	 */
	bool FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const FConstStructView UserData) const;
	
	/**
	 * Spatial lookup for first slot in range respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param UserActor Actor claiming the smart object used to evaluate selection conditions
	 * @return First valid smart object in range. Not the closest one, just the one
	 *		that happens to be retrieved first from space partition
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	FSmartObjectRequestResult FindSmartObject(const FSmartObjectRequest& Request, const AActor* UserActor = nullptr) const
	{
		return FindSmartObject(Request, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	/**
	 * Spatial lookup for slot candidates respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param OutResults List of smart object slot candidates
	 * @param UserActor Actor claiming the smart object
	 * @return All valid smart objects in range.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Find Smart Objects (Pure)", DeprecatedFunction, DeprecationMessage="The pure version is deprecated, place a new Find Smart Objects node and connect the exec pin"))
	bool FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr) const
	{
		return FindSmartObjects(Request, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	/**
	 * Blueprint function for spatial lookup for slot candidates respecting request criteria and selection conditions.
	 * @param Request Parameters defining the search area and criteria
	 * @param OutResults List of smart object slot candidates
	 * @param UserActor Actor claiming the smart object
	 * @return All valid smart objects in range.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", meta=(DisplayName="Find Smart Objects"))
	bool FindSmartObjects_BP(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr) const
	{
		return FindSmartObjects(Request, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	/**
	 * Returns slots of a given smart object matching the filter.
	 * @param Handle Handle to the smart object.
	 * @param Filter Filter to apply on object and slots.
	 * @param OutSlots Available slots found that match the filter
	 * @param UserData Optional additional data that could be provided to bind values in the conditions evaluation context
	 */
	void FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutSlots, const FConstStructView UserData = {}) const;

	/**
	 * Return all slots of a given smart object.
	 * @param Handle Handle to the smart object.
	 * @param OutSlots All slots of the smart object
	 */
	void GetAllSlots(const FSmartObjectHandle Handle, TArray<FSmartObjectSlotHandle>& OutSlots) const;

	/**
	 * Evaluates conditions of each slot and add to the result array on success.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param SlotsToFilter List of slot handles to apply selection conditions on
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return List of slot handles that pass all their selection conditions
	 */
	[[nodiscard]] TArray<FSmartObjectSlotHandle> FilterSlotsBySelectionConditions(
		const TConstArrayView<FSmartObjectSlotHandle>& SlotsToFilter,
		const FConstStructView UserData
		) const;
	
	/**
	 * Evaluates conditions of the slot specified by each request result and add to the result array on success.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param ResultsToFilter List of request results to apply selection conditions on
	 * @param UserData The additional data that could be bound in the conditions evaluation context
	 * @return List of request results that pass all their selection conditions
	 */
	[[nodiscard]] TArray<FSmartObjectRequestResult> FilterResultsBySelectionConditions(
		const TConstArrayView<FSmartObjectRequestResult>& ResultsToFilter,
		const FConstStructView UserData
		) const;

	/**
	 * Evaluates conditions of the specified slot and its parent smart object.
	 * Optional user data can be provided to bind parameters in evaluation context based
	 * on the schema used by the object definition.
	 * @param SlotHandle Handle to the smart object slot
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 * @return True if all conditions are met; false otherwise
	 */
	[[nodiscard]] bool EvaluateSelectionConditions(const FSmartObjectSlotHandle SlotHandle, const FConstStructView UserData) const;

	/**
	 * Finds entrance location for a specific slot. Each slot can be annotated with multiple entrance locations, and the request can be configured to also consider the slot location as one entry.
	 * Additionally the entrance locations can be checked to be on navigable surface (does not check that the point is reachable, though), traced on ground, and without of collisions.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Request Request describing how to select the entry.
	 * @param Result Entry location result (in world space).
	 * @return True if valid entry found.
	 */
	bool FindEntranceLocationForSlot(const FSmartObjectSlotHandle SlotHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const;
	
	/**
	 * Runs the same logic as FindEntranceLocationForSlot() but for a specific entrance location. The entrance handle can be get from entrance location result.
	 * @param EntranceHandle Handle to a specific smart object slot entrance.
	 * @param Request Request describing how to select the entry.
	 * @param Result Entry location result (in world space).
	 * @return True if result is valid.
	 */
	bool UpdateEntranceLocation(const FSmartObjectSlotEntranceHandle EntranceHandle, const FSmartObjectSlotEntranceLocationRequest& Request, FSmartObjectSlotEntranceLocationResult& Result) const;

	/**
	 * Checks whether given slot is free and can be claimed (i.e. slot and its parent are both enabled)
	 * @note This methods doesn't evaluate the selection conditions. EvaluateSelectionConditions must be called separately.
	 * @return true if the indicated slot can be claimed, false otherwise
	 * @see EvaluateSelectionConditions
	 */
	[[nodiscard]] bool CanBeClaimed(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Claims smart object from a request result.
	 * @param RequestResult Request result for given smart object and slot index.
	 * @param UserActor Actor claiming the smart object
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UE_DEPRECATED(5.3, "Please use MarkSmartObjectSlotAsClaimed instead")
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeprecatedFunction, DeprecationMessage = "Use MarkSmartObjectSlotAsClaimed instead."))
	FSmartObjectClaimHandle Claim(const FSmartObjectRequestResult& RequestResult, const AActor* UserActor = nullptr)
	{
		return MarkSlotAsClaimed(RequestResult.SlotHandle, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	/**
	 * Claim smart object slot.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param UserData Instanced struct that represents the interacting agent.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UE_DEPRECATED(5.3, "Please use MarkSlotAsClaimed instead")
	[[nodiscard]] FSmartObjectClaimHandle Claim(const FSmartObjectSlotHandle SlotHandle, const FConstStructView UserData = {})
	{
		return MarkSlotAsClaimed(SlotHandle, UserData);
	}

	/**
	 * Claim smart object from object and slot handles.
	 * @param Handle Handle to the smart object.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UE_DEPRECATED(5.3, "Please use MarkSlotAsClaimed passing only the slot handle")
	[[nodiscard]] FSmartObjectClaimHandle Claim(const FSmartObjectHandle Handle, FSmartObjectSlotHandle SlotHandle) { return MarkSlotAsClaimed(SlotHandle, {}); }

	/**
	 * Claim smart object from object and slot handles.
	 * @param Handle Handle to the smart object.
	 * @param Filter Optional filter to apply on object and slots.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	 UE_DEPRECATED(5.3, "Please use both FindSlots and MarkSlotAsClaimed using only the slot handle. This will allow proper support of selection conditions.")
	[[nodiscard]] FSmartObjectClaimHandle Claim(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter);

	/**
	 * Marks a smart object slot as claimed.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param UserData Instanced struct that represents the interacting agent.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	[[nodiscard]] FSmartObjectClaimHandle MarkSlotAsClaimed(const FSmartObjectSlotHandle SlotHandle, const FConstStructView UserData = {});

	/**
	 * Indicates if the object referred to by the given handle is still accessible in the simulation.
	 * This should only be required when a handle is stored and used later.
	 * @param Handle Handle to the smart object.
	 * @return True if the handle is valid and its associated object is accessible; false otherwise.
	 */
	bool IsSmartObjectValid(const FSmartObjectHandle Handle) const;

	/**
	 * Indicates if the object/slot referred to by the given handle are still accessible in the simulation.
	 * This should only be required when a handle is stored and later needed to access slot or object information (e.g. SlotView)
	 * Otherwise a valid ClaimHandle can be use directly after calling 'Claim'.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return True if the claim handle is valid and its associated object is accessible; false otherwise.
	 */
	bool IsClaimedSmartObjectValid(const FSmartObjectClaimHandle& ClaimHandle) const;

	/**
	 * Indicates if the slot referred to by the given handle is still accessible in the simulation.
	 * This should only be required when a handle is stored and later needed to access slot information (e.g. SlotView)
	 * Otherwise a valid SlotHandle can be use directly after calling any of the 'Find' or 'Claim' methods.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return True if the handle is valid and its associated slot is accessible; false otherwise.
	 */
	bool IsSmartObjectSlotValid(const FSmartObjectSlotHandle SlotHandle) const
	{
		if (!SlotHandle.IsValid())
		{
			return false;
		}
		const FSmartObjectRuntime* SmartObjectRuntime = RuntimeSmartObjects.Find(SlotHandle.SmartObjectHandle);
		return SmartObjectRuntime != nullptr && SmartObjectRuntime->Slots.IsValidIndex(SlotHandle.GetSlotIndex());
	}

	/**
	 * Start using a claimed smart object slot.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition the user wants to use.
	 * @return The base class pointer of the requested behavior definition class associated to the slot
	 */
	UE_DEPRECATED(5.3, "Please use MarkSmartObjectSlotAsOccupied instead")
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeprecatedFunction, DeprecationMessage = "Use MarkSmartObjectSlotAsOccupied instead."))
	const USmartObjectBehaviorDefinition* Use(const FSmartObjectClaimHandle& ClaimHandle, TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass)
	{
		return MarkSlotAsOccupied(ClaimHandle, DefinitionClass);
	}

	/**
	 * Marks a previously claimed smart object slot as occupied.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition the user wants to use.
	 * @return The base class pointer of the requested behavior definition class associated to the slot
	 */
	const USmartObjectBehaviorDefinition* MarkSlotAsOccupied(const FSmartObjectClaimHandle& ClaimHandle, TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass);

	/**
	 * Start using a claimed smart object slot.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the slot
	 */
	template <typename DefinitionType>
	UE_DEPRECATED(5.3, "Please use MarkSlotAsOccupied instead")
	const DefinitionType* Use(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(MarkSlotAsOccupied(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Marks a previously claimed smart object slot as occupied.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the slot
	 */
	template <typename DefinitionType>
	const DefinitionType* MarkSlotAsOccupied(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(MarkSlotAsOccupied(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Release claim on a smart object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Whether the claim was successfully released or not
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DeprecatedFunction, DeprecationMessage = "Use MarkSmartObjectSlotAsFree instead."))
	bool Release(const FSmartObjectClaimHandle& ClaimHandle)
	{
		return MarkSlotAsFree(ClaimHandle);
	}

	/**
	 * Marks a claimed or occupied smart object as free.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Whether the claim was successfully released or not
	 */
	bool MarkSlotAsFree(const FSmartObjectClaimHandle& ClaimHandle);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition.
	 * @return The base class pointer of the requested behavior definition class associated to the slotClaim handle
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const USmartObjectBehaviorDefinition* GetBehaviorDefinition(
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the Claim handle
	 */
	template <typename DefinitionType>
	const DefinitionType* GetBehaviorDefinition(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(GetBehaviorDefinition(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Return the behavior definition of a given type from a request result.
	 * @param RequestResult A request result returned by any of the Find methods.
	 * @param DefinitionClass The type of behavior definition.
	 * @return The base class pointer of the requested behavior definition class associated to the request result
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const USmartObjectBehaviorDefinition* GetBehaviorDefinitionByRequestResult(
		const FSmartObjectRequestResult& RequestResult,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param RequestResult A request result returned by any of the Find methods.
	 * @return The requested behavior definition class pointer associated to the request result
	 */
	template <typename DefinitionType>
	const DefinitionType* GetBehaviorDefinition(const FSmartObjectRequestResult& RequestResult)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(GetBehaviorDefinitionByRequestResult(RequestResult, DefinitionType::StaticClass()));
	}
	
	ESmartObjectSlotState GetSlotState(const FSmartObjectSlotHandle SlotHandle) const;

	UE_DEPRECATED(5.3, "Data is now added synchronously, use AddSlotData instead.")
	void AddSlotDataDeferred(const FSmartObjectClaimHandle& ClaimHandle, FConstStructView InData) { AddSlotData(ClaimHandle, InData); }
	
	/**
	 * Adds state data to a slot instance. Data must be a struct that inherits
	 * from FSmartObjectSlotStateData and passed as a struct view (e.g. FConstStructView::Make(FSomeStruct))
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param InData A view on the struct to add.
	 */
	void AddSlotData(const FSmartObjectClaimHandle& ClaimHandle, FConstStructView InData);

	/**
	 * Creates and returns a view to the data associated to a slot handle.
	 * @return A view on the slot associated to SlotHandle. Caller should use IsValid() on the view
	 * before using it since the provided handle might no longer refer to a slot registered in the simulation.
	 */
	FSmartObjectSlotView GetSlotView(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Returns the position (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Position (in world space) of the slot associated to ClaimHandle.
	 */
	TOptional<FVector> GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle) const { return GetSlotLocation(ClaimHandle.SlotHandle); }
	
	/**
	 * Returns the position (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param OutSlotLocation Position (in world space) of the slot associated to ClaimHandle.
	 * @return Whether the location was found and assigned to 'OutSlotLocation'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool GetSlotLocation(const FSmartObjectClaimHandle& ClaimHandle, FVector& OutSlotLocation) const;

	/**
	 * Returns the position (in world space) of the slot associated to the given request result.
	 * @param Result A request result returned by any of the Find methods.
	 * @return Position (in world space) of the slot associated to Result.
	 */
	TOptional<FVector> GetSlotLocation(const FSmartObjectRequestResult& Result) const { return GetSlotLocation(Result.SlotHandle); }

	/**
	 * Returns the position (in world space) of the slot represented by the provided slot handle.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Position (in world space) of the slot associated to SlotHandle.
	 */
	TOptional<FVector> GetSlotLocation(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Returns the transform (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Transform (in world space) of the slot associated to ClaimHandle.
	 */
	TOptional<FTransform> GetSlotTransform(const FSmartObjectClaimHandle& ClaimHandle) const { return GetSlotTransform(ClaimHandle.SlotHandle);	}

	/**
	 * Returns the transform (in world space) of the slot associated to the given claim handle.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param OutSlotTransform Transform (in world space) of the slot associated to ClaimHandle.
	 * @return Whether the transform was found and assigned to 'OutSlotTransform'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool GetSlotTransform(const FSmartObjectClaimHandle& ClaimHandle, FTransform& OutSlotTransform) const;

	/**
	 * Returns the transform (in world space) of the slot associated to the given request result.
	 * @param Result A request result returned by any of the Find methods.
	 * @return Transform (in world space) of the slot associated to Result.
	 */
	TOptional<FTransform> GetSlotTransform(const FSmartObjectRequestResult& Result) const {	return GetSlotTransform(Result.SlotHandle);	}

	/**
	 * Returns the transform (in world space) of the slot associated to the given RequestResult.
	 * @param RequestResult Result returned by any of the Find Smart Object(s) methods.
	 * @param OutSlotTransform Transform (in world space) of the slot associated to the RequestResult.
	 * @return Whether the transform was found and assigned to 'OutSlotTransform'
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool GetSlotTransformFromRequestResult(const FSmartObjectRequestResult& RequestResult, FTransform& OutSlotTransform) const;

	/**
	 * Returns the transform (in world space) of the slot represented by the provided slot handle.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Transform (in world space) of the slot associated to SlotHandle.
	 */
	TOptional<FTransform> GetSlotTransform(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Similarly to GetSlotTransform fetches the transform (in world space) of the indicated slot, but assumes the slot 
	 * handle is valid and that the EntityManager is known. The burden of ensuring that's the case is on the caller. 
	 * @param SlotHandle Handle to a smart object slot.
	 * @return Transform (in world space) of the slot associated to SlotHandle.
	 */
	FTransform GetSlotTransformChecked(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Returns the list of tags associated to the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @return Container of tags associated to the smart object instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const FGameplayTagContainer& GetInstanceTags(const FSmartObjectHandle Handle) const;

	/**
	 * Adds a single tag to the smart object instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @param Tag Tag to add to the smart object instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void AddTagToInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag);

	/**
	 * Removes a single tag from the smartobject instance represented by the provided handle.
	 * @param Handle Handle to the smart object.
	 * @param Tag Tag to remove from the SmartObject instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void RemoveTagFromInstance(const FSmartObjectHandle Handle, const FGameplayTag& Tag);

	/**
	 * Returns pointer to the smart object instance event delegate.
	 * @param SmartObjectHandle Handle to the smart object.
	 * @return Pointer to object's delegate, or nullptr if instance doesn't exists.
	 */
	FOnSmartObjectEvent* GetEventDelegate(const FSmartObjectHandle SmartObjectHandle);
	
	/**
	 * Returns the list of tags associated to the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @return Container of tags associated to the smart object instance, or empty container if slot was not valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const FGameplayTagContainer& GetSlotTags(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Adds a single tag to the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Tag Tag to add to the smart object slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void AddTagToSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag);

	/**
	 * Removes a single tag from the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param Tag Tag to remove from the smart object slot.
	 * @return True if the tag was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool RemoveTagFromSlot(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag& Tag);

	/**
	 * Enables or disables the smart object slot represented by the provided handle.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param bEnabled If true enables the slot, if false, disables the slot.
	 * @return Previous enabled state. 
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool SetSlotEnabled(const FSmartObjectSlotHandle SlotHandle, const bool bEnabled);

	/**
	 * Sends event to a Smart Object slot.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param EventTag Gameplay Tag describing the event type.
	 * @param Payload Struct payload for the event.
	 * @return True if the event was successfully sent. 
	 */
	bool SendSlotEvent(const FSmartObjectSlotHandle SlotHandle, const FGameplayTag EventTag, const FConstStructView Payload = FConstStructView());

	/**
	 * Returns pointer to the smart object changed delegate associated to the provided handle.
	 * The delegate is shared for all slots so listeners must filter using 'Event.SlotHandle'.
	 * @param SlotHandle Handle to the smart object slot.
	 * @return Pointer to slot's delegate, or nullptr if slot does not exists.
	 */
	FOnSmartObjectEvent* GetSlotEventDelegate(const FSmartObjectSlotHandle SlotHandle);
	
	/**
	 * Register a callback to be notified if the claimed slot is no longer available and user need to perform cleanup.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param Callback Delegate that will be called to notify that a slot gets invalidated and can no longer be used.
	 */
	void RegisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle, const FOnSlotInvalidated& Callback);

	/**
	 * Unregisters a callback to be notified if the claimed slot is no longer available and user need to perform cleanup.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 */
	void UnregisterSlotInvalidationCallback(const FSmartObjectClaimHandle& ClaimHandle);

#if UE_ENABLE_DEBUG_DRAWING
	void DebugDraw(FDebugRenderSceneProxy* DebugProxy) const;
	void DebugDrawCanvas(UCanvas* Canvas, APlayerController* PlayerController) const {}
#endif

#if WITH_EDITORONLY_DATA
	/** 
	 * Special-purpose function used to set up an instance of ASmartObjectPersistentCollection with data from a given
	 * instance of ADEPRECATED_SmartObjectCollection 
	 */
	static void CreatePersistentCollectionFromDeprecatedData(UWorld& World, const ADEPRECATED_SmartObjectCollection& DeprecatedCollection);

	TConstArrayView<TWeakObjectPtr<ASmartObjectPersistentCollection>> GetRegisteredCollections() const { return RegisteredCollections; }
	TArrayView<TWeakObjectPtr<ASmartObjectPersistentCollection>> GetMutableRegisteredCollections() { return RegisteredCollections; }
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	mutable FOnMainCollectionEvent OnMainCollectionChanged;
	mutable FOnMainCollectionEvent OnMainCollectionDirtied;
#endif

protected:
	friend UWorldPartitionSmartObjectCollectionBuilder;

	bool RegisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent);
	bool UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const bool bDestroyRuntimeState);

	UE_DEPRECATED(5.2, "This flavor of UnregisterSmartObjectInternal is deprecated. Please use UnregisterSmartObjectInternal(USmartObjectComponent&, const bool) instead.")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const ESmartObjectUnregistrationMode UnregistrationMode);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	/**
	 * Callback overridden to gather loaded collections, spawn missing one and set the main collection.
	 * @note we use this method instead of `Initialize` or `PostInitialize` so active level is set and actors registered.
	 */
	virtual void OnWorldComponentsUpdated(UWorld& World) override;

	/**
	 * BeginPlay will push all objects stored in the collection to the runtime simulation
	 * and initialize octree using collection bounds.
	 */
	virtual void OnWorldBeginPlay(UWorld& World) override;

	// USubsystem BEGIN
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// USubsystem END

	/** Creates all runtime data using main collection */
	void InitializeRuntime();

	UE_DEPRECATED(5.3, "Use InitializeRuntime() without Mass entity manager.")
	void InitializeRuntime(const TSharedPtr<FMassEntityManager>& InEntityManager)
	{
	}

	/** Removes all runtime data */
	virtual void CleanupRuntime();

	/** Returns the runtime instance associated to the provided handle */
	FSmartObjectRuntime* GetRuntimeInstance(const FSmartObjectHandle SmartObjectHandle) { return RuntimeSmartObjects.Find(SmartObjectHandle); }

	/** Returns the const runtime instance associated to the provided handle */
	const FSmartObjectRuntime* GetRuntimeInstance(const FSmartObjectHandle SmartObjectHandle) const { return RuntimeSmartObjects.Find(SmartObjectHandle); }

	/**
	 * Indicates if the handle is set and the slot referred to is still accessible in the simulation.
	 * Log is produced for any failing condition using provided LogContext.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param LogContext String describing the context in which the method is called (e.g. caller function name)
	 * @return True if the handle is valid and its associated slot is accessible; false otherwise.
	 */
	bool IsSlotValidVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext) const;

	/**
	 * Returns the const runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	bool GetValidatedRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, const FSmartObjectRuntime*& OutSmartObjectRuntime, const FSmartObjectRuntimeSlot*& OutSlot, const TCHAR* Context) const;

	/**
	 * Returns the mutable runtime instance associated to the provided handle
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	bool GetValidatedMutableRuntimeAndSlot(const FSmartObjectSlotHandle SlotHandle, FSmartObjectRuntime*& OutSmartObjectRuntime, FSmartObjectRuntimeSlot*& OutSlot, const TCHAR* Context);

	UE_DEPRECATED(5.3, "Use GetValidatedRuntimeAndSlot() instead.")
	const FSmartObjectRuntimeSlot* GetSlotVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext) const
	{
		return nullptr;
	}

	UE_DEPRECATED(5.3, "Use GetValidatedMutableRuntimeAndSlot() instead.")
	FSmartObjectRuntimeSlot* GetMutableSlotVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext)
	{
		return nullptr;
	}

	UE_DEPRECATED(5.3, "Use GetValidatedMutableRuntimeAndSlot() instead.")
	FSmartObjectRuntimeSlot* GetMutableSlot(const FSmartObjectClaimHandle& ClaimHandle)
	{
		return nullptr;
	}
	
	/**
	 * Returns the const runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	const FSmartObjectRuntime* GetValidatedRuntime(const FSmartObjectHandle Handle, const TCHAR* Context) const;

	/**
	 * Returns the mutable runtime instance associated to the provided handle
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	FSmartObjectRuntime* GetValidatedMutableRuntime(const FSmartObjectHandle Handle, const TCHAR* Context) const;

	static void AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	static void RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	static void OnSlotChanged(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRuntimeSlot& Slot,
		const FSmartObjectSlotHandle SlotHandle,
		const ESmartObjectChangeReason Reason,
		const FConstStructView Payload = {},
		const FGameplayTag ChangedTag = FGameplayTag()
		);

	/** Goes through all defined slots of smart object represented by SmartObjectRuntime and finds the ones matching the filter. */
	void FindSlots(
		const FSmartObjectHandle Handle,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRequestFilter& Filter,
		 TArray<FSmartObjectSlotHandle>& OutResults,
		 const FConstStructView UserData) const;

	UE_DEPRECATED(5.3, "FindSlots() was changed to require object handle as parameter too.")
	void FindSlots(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectRequestFilter& Filter,
		 TArray<FSmartObjectSlotHandle>& OutResults,
		 const FConstStructView UserData) const
	{
	}

	/** Applies filter on provided definition and fills OutValidIndices with indices of all valid slots. */
	static void FindMatchingSlotDefinitionIndices(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices);

	static const USmartObjectBehaviorDefinition* GetBehaviorDefinition(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	const USmartObjectBehaviorDefinition* MarkSlotAsOccupied(
		FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		);

	UE_DEPRECATED(5.3, "MarkSlotAsOccupied() was changed to require SmartObjectRuntime to be passed as mutable.")
	const USmartObjectBehaviorDefinition* MarkSlotAsOccupied(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectClaimHandle& ClaimHandle,
		TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass
		)
	{
		return nullptr;
	}

	void AbortAll(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime) const;

	UE_DEPRECATED(5.3, "AbortAll() was changed to require object handle as parameter too.")
	void AbortAll(const FSmartObjectRuntime& SmartObjectRuntime)
	{
	}
	
	/** Make sure that all SmartObjectCollection actors from our associated world are registered. */
	void RegisterCollectionInstances();

	void AddContainerToSimulation(const FSmartObjectContainer& InSmartObjectContainer);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 */
	FSmartObjectRuntime* AddCollectionEntryToSimulation(
		const FSmartObjectCollectionEntry& Entry,
		const USmartObjectDefinition& Definition,
		USmartObjectComponent* OwnerComponent
		);

	UE_DEPRECATED(5.3, "bCommitChanges is not used anymore, use the version without.")
	FSmartObjectRuntime* AddCollectionEntryToSimulation(
		const FSmartObjectCollectionEntry& Entry,
		const USmartObjectDefinition& Definition,
		USmartObjectComponent* OwnerComponent,
		const bool bCommitChanges)
	{
		return nullptr;
	}

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime entry might be created or an existing one found
	 * @param CollectionEntry The associated collection entry that got created to add the component to the simulation.
	 * @param bCommitChanges Indicates if deferred commands must be flushed. Set to 'true' by default but could be set to 'false' when adding batch of components.
	 */
	FSmartObjectRuntime* AddComponentToSimulation(
		USmartObjectComponent& SmartObjectComponent,
		const FSmartObjectCollectionEntry& CollectionEntry
		);

	UE_DEPRECATED(5.3, "bCommitChanges is not used anymore, use the version without.")
	FSmartObjectRuntime* AddComponentToSimulation(
		USmartObjectComponent& SmartObjectComponent,
		const FSmartObjectCollectionEntry& CollectionEntry,
		const bool bCommitChanges
		)
	{
		return nullptr;
	}

	/** Notifies SmartObjectComponent that it has been bound to a runtime instance, and sets SmartObjectComponent-related properties of SmartObjectRuntime */
	void BindComponentToSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime);
	
	/**
	 * Unbinds a smartobject component from an existing instance in the simulation.
	 * @param SmartObjectComponent The component to remove from the simulation
	 */
	void UnbindComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Unbinds a smartobject component from the given FSmartObjectRuntime instance. Note that unlike UnbindComponentFromSimulation
	 * this function blindly assumes that SmartObjectRuntime does indeed represent SmartObjectComponent
	 * @param SmartObjectComponent The component to remove from the simulation
	 * @param SmartObjectRuntime runtime data representing the component being removed
	 */
	static void UnbindComponentFromSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime);

	/** @return whether the removal was successful */
	bool RemoveRuntimeInstanceFromSimulation(const FSmartObjectHandle Handle, USmartObjectComponent* SmartObjectComponent);
	bool RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry);
	void RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

	/** Destroy SmartObjectRuntime contents as Handle's representation. */
	void DestroyRuntimeInstanceInternal(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime);

	UE_DEPRECATED(5.3, "EntityManagerRef is not used anymore, use the version without.")
	void DestroyRuntimeInstanceInternal(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime, const FMassEntityManager& EntityManagerRef)
	{
	}
	
	/**
	 * Fills the provided context data with the smartobject actor and handle associated to 'SmartObjectRuntime' and the subsystem. 
	 * @param ContextData The context data to fill
	 * @param SmartObjectRuntime The runtime instance of the SmartObject for which the context must be filled 
	 */
	void SetupConditionContextCommonData(FWorldConditionContextData& ContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;

	/**
	 * Binds properties of the context data to property values of the user data struct when they match type and name.
	 * @param ContextData The context data to fill
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 */
	void BindPropertiesFromStruct(FWorldConditionContextData& ContextData, const FConstStructView& UserData) const;

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and adds the slot related part. It then evaluates all conditions associated to the specified slot.  
	 * @param ConditionContextData The context data to fill and use for conditions evaluation
	 * @param SmartObjectRuntime Runtime struct associated to the smart object slot
	 * @param SlotHandle Handle to the smart object slot
	 * @return True if all conditions are met; false otherwise
	 * @see SetupDefaultConditionsContext
	 */
	[[nodiscard]] bool EvaluateSlotConditions(
		FWorldConditionContextData& ConditionContextData,
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle
		) const;

	UE_DEPRECATED(5.3, "Use the version that takes smart object runtime and slot handle.")
	[[nodiscard]] bool EvaluateSlotConditions(
		FWorldConditionContextData& ConditionContextData,
		const FSmartObjectSlotHandle SlotHandle,
		const FSmartObjectRuntimeSlot& Slot
		) const
	{
		return false;
	}

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and evaluates all conditions associated to the specified object.
	 * @param ConditionContextData The context data to use for conditions evaluation
	 * @param SmartObjectRuntime Runtime data representing the smart object for which the conditions must be evaluated
	 * @return True if all conditions are met; false otherwise
	 */
	[[nodiscard]] bool EvaluateObjectConditions(const FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;

	/**
	 * Internal helper for filter methods to build the list of accepted slots
	 * by reusing context data and schema as much as possible.
	 */
	[[nodiscard]] bool EvaluateConditionsForFiltering(
		const FSmartObjectRuntime& SmartObjectRuntime,
		const FSmartObjectSlotHandle SlotHandle,
		FWorldConditionContextData& ContextData,
		const FConstStructView UserData,
		TPair<const FSmartObjectRuntime*, bool>& LastEvaluatedRuntime
		) const;	

	UE_DEPRECATED(5.3, "EvaluateConditionsForFiltering() was changed to rewuire smart object runtime as parameter.")
	[[nodiscard]] bool EvaluateConditionsForFiltering(
		const FSmartObjectSlotHandle SlotHandle,
		FWorldConditionContextData& ContextData,
		const FConstStructView UserData,
		TPair<const FSmartObjectRuntime*, bool>& LastEvaluatedRuntime
		) const
	{
		return false;
	}

	/**
	 * Finds entrance location for a specific slot. Each slot can be annotated with multiple entrance locations, and the request can be configured to also consider the slot location as one entry.
	 * Additionally the entrance locations can be checked to be on navigable surface (does not check that the point is reachable, though), traced on ground, and without of collisions.
	 * @param SlotHandle Handle to the smart object slot.
	 * @param SlotEntranceHandle Handle to specific entrance if just one entrance should be checked. (Optional)
	 * @param Request Request describing how to select the entry.
	 * @param Result Entry location result (in world space).
	 * @return True if valid entry found.
	 */
	bool FindEntranceLocationInternal(
		const FSmartObjectSlotHandle SlotHandle,
		const FSmartObjectSlotEntranceHandle SlotEntranceHandle,
		const FSmartObjectSlotEntranceLocationRequest& Request,
		FSmartObjectSlotEntranceLocationResult& Result
		) const;

	/**
	 * Name of the Space partition class to use.
	 * Usage:
	 *		[/Script/SmartObjectsModule.SmartObjectSubsystem]
	 *		SpacePartitionClassName=/Script/SmartObjectsModule.<SpacePartitionClassName>
	 */
	UPROPERTY(config, meta=(MetaClass="/Script/SmartObjectsModule.SmartObjectSpacePartition", DisplayName="Spatial Representation Structure Class"))
	FSoftClassPath SpacePartitionClassName;

	UPROPERTY()
	TSubclassOf<USmartObjectSpacePartition> SpacePartitionClass;

	UPROPERTY()
	TObjectPtr<USmartObjectSpacePartition> SpacePartition;

	UPROPERTY()
	TObjectPtr<ASmartObjectSubsystemRenderingActor> RenderingActor;

	UPROPERTY(Transient)
	FSmartObjectContainer SmartObjectContainer;

	TArray<TWeakObjectPtr<ASmartObjectPersistentCollection>> RegisteredCollections;

	UPROPERTY(Transient)
	TMap<FSmartObjectHandle, FSmartObjectRuntime> RuntimeSmartObjects;
	
	/** List of registered components. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> RegisteredSOComponents;

	/** smart objects that attempted to register while no collection was being present */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> PendingSmartObjectRegistration;

	uint32 NextFreeUserID = 1;

	bool bRuntimeInitialized = false;

#if WITH_EDITOR
	bool bAutoInitializeEditorInstances = true;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Set in OnWorldComponentsUpdated and used to control special logic required to build collections in Editor mode */
	bool bIsPartitionedWorld = false;

	friend class ASmartObjectPersistentCollection;

	void PopulateCollection(ASmartObjectPersistentCollection& InCollection) const;

	/** Iteratively adds items to registered collections. Expected to be called in World Partitined worlds. */
	void IterativelyBuildCollections();

	int32 GetRegisteredSmartObjectsCompatibleWithCollection(const ASmartObjectPersistentCollection& InCollection, TArray<USmartObjectComponent*>& OutRelevantComponents) const;

	/**
	 * Compute bounds from the given world 
	 * @param World World from which the bounds must be computed
	 */
	FBox ComputeBounds(const UWorld& World) const;
#endif // WITH_EDITORONLY_DATA

#if WITH_SMARTOBJECT_DEBUG
public:
	uint32 DebugGetNumRuntimeObjects() const { return RuntimeSmartObjects.Num(); }
	const TMap<FSmartObjectHandle, FSmartObjectRuntime>& DebugGetRuntimeObjects() const { return RuntimeSmartObjects; }
	uint32 DebugGetNumRegisteredComponents() const { return RegisteredSOComponents.Num(); }

	UE_DEPRECATED(5.3, "DebugGetRuntimeSlots() is not supported anymore, slots are accessed via object runtime instead.")
	const TMap<FSmartObjectSlotHandle, FSmartObjectRuntimeSlot>& DebugGetRuntimeSlots() const
	{
		static TMap<FSmartObjectSlotHandle, FSmartObjectRuntimeSlot> Dummy;
		return Dummy;
	}

	/** Debugging helper to remove all registered smart objects from the simulation */
	void DebugUnregisterAllSmartObjects();

	/** Debugging helpers to add all registered smart objects to the simulation */
	void DebugRegisterAllSmartObjects();

	/** Debugging helper to emulate the start of the simulation to create all runtime data */
	void DebugInitializeRuntime();

	/** Debugging helper to emulate the stop of the simulation to destroy all runtime data */
	void DebugCleanupRuntime();
#endif // WITH_SMARTOBJECT_DEBUG
};
