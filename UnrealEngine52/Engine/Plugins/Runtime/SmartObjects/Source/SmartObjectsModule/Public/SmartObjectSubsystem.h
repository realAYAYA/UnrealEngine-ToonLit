// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectPersistentCollection.h"
#include "SmartObjectRuntime.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldConditionContext.h"
#include "SmartObjectSubsystem.generated.h"

class UCanvas;
class USmartObjectBehaviorDefinition;

class USmartObjectComponent;
class UWorldPartitionSmartObjectCollectionBuilder;
struct FMassEntityManager;
class ASmartObjectSubsystemRenderingActor;
class FDebugRenderSceneProxy;
class ADEPRECATED_SmartObjectCollection;

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

	/** Is set, will filter out any SmartObject that does not pass the predicate. */
	TFunction<bool(FSmartObjectHandle)> Predicate;

	/**
	 * If set, will pass the context data for the selection preconditions.
	 * Any SmartObject that has selection preconditions but does not match the schema set the in the context data will be skipped.
	 */
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

	UPROPERTY(Transient, VisibleAnywhere, Category = SmartObject)
	FSmartObjectSlotHandle SlotHandle;
};

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
	 * Enables or disables all smart objects associated to the provided actor (multiple components).
	 * @param SmartObjectActor Smart object(s) parent actor.
	 * @param bEnabled If true enables, disables otherwise.
	 * @return True if all (at least one) smartobject components are found with their associated runtime and values set (or alredy set) to desired state; false otherwise.
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
	 * Unregisters a SmartObject component from the runtime simulation.
	 * @param SmartObjectComponent SmartObject component to unregister
	 * @return true when component is successfully unregistered, false otherwise
	 */
	bool UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Removes a SmartObject component from the runtime simulation.
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
	 * Spatial lookup
	 * @return First valid smart object in range. Not the closest one, just the one
	 *		that happens to be retrieved first from space partition
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	FSmartObjectRequestResult FindSmartObject(const FSmartObjectRequest& Request) const;

	/**
	 * Spatial lookup
	 * @return All valid smart objects in range.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool FindSmartObjects(const FSmartObjectRequest& Request, TArray<FSmartObjectRequestResult>& OutResults) const;

	/**
	 * Returns slots of a given smart object matching the filter.
	 * @param Handle Handle to the smart object.
	 * @param Filter Filter to apply on object and slots.
	 * @param OutSlots Available slots found that match the filter
	 */
	void FindSlots(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutSlots) const;

	/**
	 * Return all slots of a given smart object.
	 * @param Handle Handle to the smart object.
	 * @param OutSlots All slots of the smart object
	 */
	void GetAllSlots(const FSmartObjectHandle Handle, TArray<FSmartObjectSlotHandle>& OutSlots) const;

	/**
	 * Claims smart object from a request result.
	 * @param RequestResult Request result for given smart object and slot index.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	FSmartObjectClaimHandle Claim(const FSmartObjectRequestResult& RequestResult) { return Claim(RequestResult.SmartObjectHandle, RequestResult.SlotHandle); }

	/**
	 * Claim smart object from object and slot handles.
	 * @param Handle Handle to the smart object.
	 * @param SlotHandle Handle to a smart object slot.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UE_NODISCARD FSmartObjectClaimHandle Claim(const FSmartObjectHandle Handle, FSmartObjectSlotHandle SlotHandle);

	/**
	 * Claim smart object from object and slot handles.
	 * @param Handle Handle to the smart object.
	 * @param Filter Optional filter to apply on object and slots.
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UE_NODISCARD FSmartObjectClaimHandle Claim(const FSmartObjectHandle Handle, const FSmartObjectRequestFilter& Filter = {});
	
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
	bool IsSmartObjectSlotValid(const FSmartObjectSlotHandle SlotHandle) const { return SlotHandle.IsValid() && RuntimeSlots.Find(SlotHandle) != nullptr; }

	/**
	 * Start using a claimed smart object slot.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition the user wants to use.
	 * @return The base class pointer of the requested behavior definition class associated to the slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const USmartObjectBehaviorDefinition* Use(const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	/**
	 * Checks whether given slot is free and can be claimed
	 * @return true if the indicated slot can be claimed, false otherwise
	 */
	UE_NODISCARD bool CanBeClaimed(const FSmartObjectSlotHandle SlotHandle) const;

	/**
	 * Start using a claimed smart object slot.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return The requested behavior definition class pointer associated to the slot
	 */
	template <typename DefinitionType>
	const DefinitionType* Use(const FSmartObjectClaimHandle& ClaimHandle)
	{
		static_assert(TIsDerivedFrom<DefinitionType, USmartObjectBehaviorDefinition>::IsDerived, "DefinitionType must derive from USmartObjectBehaviorDefinition");
		return Cast<const DefinitionType>(Use(ClaimHandle, DefinitionType::StaticClass()));
	}

	/**
	 * Release claim on a smart object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Whether the claim was successfully released or not
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool Release(const FSmartObjectClaimHandle& ClaimHandle);

	/**
	 * Return the behavior definition of a given type from a claimed object.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition.
	 * @return The base class pointer of the requested behavior definition class associated to the slotClaim handle
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	const USmartObjectBehaviorDefinition* GetBehaviorDefinition(const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

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
	const USmartObjectBehaviorDefinition* GetBehaviorDefinitionByRequestResult(const FSmartObjectRequestResult& RequestResult, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

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

	/**
	 * Adds state data (through a deferred command) to a slot instance. Data must be a struct that inherits
	 * from FSmartObjectSlotStateData and passed as a struct view (e.g. FConstStructView::Make(FSomeStruct))
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param InData A view on the struct to add.
	 */
	void AddSlotDataDeferred(const FSmartObjectClaimHandle& ClaimHandle, FConstStructView InData) const;

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
	const FTransform& GetSlotTransformChecked(const FSmartObjectSlotHandle SlotHandle) const;

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
	 * Returns pointer to the smart object slot changed delegate by the provided handle.
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
	void InitializeRuntime(const TSharedPtr<FMassEntityManager>& InEntityManager);

	/** Removes all runtime data */
	virtual void CleanupRuntime();

	/** Returns the runtime instance associated to the provided handle */
	FSmartObjectRuntime* GetRuntimeInstance(const FSmartObjectHandle SmartObjectHandle) { return RuntimeSmartObjects.Find(SmartObjectHandle); }

	/**
	 * Indicates if the handle is set and the slot referred to is still accessible in the simulation.
	 * Log is produced for any failing condition using provided LogContext.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param LogContext String describing the context in which the method is called (e.g. caller function name)
	 * @return True if the handle is valid and its associated slot is accessible; false otherwise.
	 */
	bool IsSlotValidVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext) const;
	
	/**
	 * Returns pointer to the specified slot if it exists. 
	 * Log is produced for any failing condition using provided LogContext.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param LogContext String describing the context in which the method is called (e.g. caller function name)
	 * @return Pointer to the slot state if the handle is valid and its associated slot is accessible; nullptr otherwise.
	 */
	FSmartObjectRuntimeSlot* GetMutableSlotVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext);

	/**
	 * Returns pointer to the specified slot if it exists. 
	 * Log is produced for any failing condition using provided LogContext.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param LogContext String describing the context in which the method is called (e.g. caller function name)
	 * @return Pointer to the slot state if the handle is valid and its associated slot is accessible; nullptr otherwise.
	 */
	const FSmartObjectRuntimeSlot* GetSlotVerbose(const FSmartObjectSlotHandle SlotHandle, const TCHAR* LogContext) const;

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

	void AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	void RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	void OnSlotChanged(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectRuntimeSlot& Slot, const FSmartObjectSlotHandle SlotHandle, const ESmartObjectChangeReason Reason, const FGameplayTag ChangedTag = FGameplayTag()) const;

	/** Goes through all defined slots of smart object represented by SmartObjectRuntime and finds the ones matching the filter. */
	void FindSlots(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutResults) const;

	/** Applies filter on provided definition and fills OutValidIndices with indices of all valid slots. */
	static void FindMatchingSlotDefinitionIndices(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices);

	static const USmartObjectBehaviorDefinition* GetBehaviorDefinition(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectSlotHandle SlotHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	const USmartObjectBehaviorDefinition* Use(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	void AbortAll(const FSmartObjectRuntime& SmartObjectRuntime);

	FSmartObjectRuntimeSlot* GetMutableSlot(const FSmartObjectClaimHandle& ClaimHandle);

	/** Make sure that all SmartObjectCollection actors from our associated world are registered. */
	void RegisterCollectionInstances();

	void AddContainerToSimulation(const FSmartObjectContainer& InSmartObjectContainer);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 */
	FSmartObjectRuntime* AddCollectionEntryToSimulation(const FSmartObjectCollectionEntry& Entry, const USmartObjectDefinition& Definition, USmartObjectComponent* OwnerComponent, const bool bCommitChanges = true);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime entry might be created or an existing one found
	 * @param CollectionEntry The associated collection entry that got created to add the component to the simulation.
	 */
	FSmartObjectRuntime* AddComponentToSimulation(USmartObjectComponent& SmartObjectComponent, const FSmartObjectCollectionEntry& CollectionEntry, const bool bCommitChanges = true);

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
	void UnbindComponentFromSimulationInternal(USmartObjectComponent& SmartObjectComponent, FSmartObjectRuntime& SmartObjectRuntime);

	/** @return whether the removal was successful */
	bool RemoveRuntimeInstanceFromSimulation(const FSmartObjectHandle Handle);
	bool RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry);
	void RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

	/** Destroy SmartObjectRuntime contents as Handle's representation. */
	void DestroyRuntimeInstanceInternal(const FSmartObjectHandle Handle, FSmartObjectRuntime& SmartObjectRuntime, FMassEntityManager& EntityManagerRef);

	/**
	 * Fills the provided context data with the smartobject actor and handle associated to 'SmartObjectRuntime' and the subsystem. 
	 * @param ConditionContextData The context data to fill
	 * @param SmartObjectRuntime The runtime instance of the SmartObject for which the context must be filled 
	 */
	void SetupConditionContextCommonData(FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and adds the slot related part. It then evaluates all conditions associated to the specified slot.  
	 * @param ConditionContextData The context data to fill and use for conditions evaluation
	 * @param SlotHandle Handle to the smart object slot
	 * @param Slot Runtime struct associated to the smart object slot
	 * @return True if all conditions are met; false otherwise
	 * @see SetupDefaultConditionsContext
	 */
	UE_NODISCARD bool EvaluateSlotConditions(FWorldConditionContextData& ConditionContextData, const FSmartObjectSlotHandle& SlotHandle, const FSmartObjectRuntimeSlot& Slot) const;\

	/**
	 * Use the provided context data that is expected to be already filled by calling 'SetupConditionContextCommonData'
	 * and evaluates all conditions associated to the specified object.
	 * @param ConditionContextData The context data to use for conditions evaluation
	 * @param SmartObjectRuntime Runtime data representing the smart object for which the conditions must be evaluated
	 * @return True if all conditions are met; false otherwise
	 */
	UE_NODISCARD bool EvaluateObjectConditions(const FWorldConditionContextData& ConditionContextData, const FSmartObjectRuntime& SmartObjectRuntime) const;

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

	TSharedPtr<FMassEntityManager> EntityManager;

	UPROPERTY(Transient)
	TMap<FSmartObjectHandle, FSmartObjectRuntime> RuntimeSmartObjects;
	
	UPROPERTY(Transient)
	TMap<FSmartObjectSlotHandle, FSmartObjectRuntimeSlot> RuntimeSlots;

	/** Keep track of Ids associated to objects entirely created at runtime (i.e. not part of the initial collection) */
	TArray<FSmartObjectHandle> RuntimeCreatedEntries;

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

	void PopulateCollection(ASmartObjectPersistentCollection& InCollection);

	/** Iteratively adds items to registered collections. Expected to be called in World Partitined worlds. */
	void IterativelyBuildCollections();

	int32 GetRegisteredSmartObjectsCompatibleWithCollection(ASmartObjectPersistentCollection& InCollection, TArray<USmartObjectComponent*>& OutRelevantComponents) const;

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
	const TMap<FSmartObjectSlotHandle, FSmartObjectRuntimeSlot>& DebugGetRuntimeSlots() const { return RuntimeSlots; }
	uint32 DebugGetNumRegisteredComponents() const { return RegisteredSOComponents.Num(); }

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
