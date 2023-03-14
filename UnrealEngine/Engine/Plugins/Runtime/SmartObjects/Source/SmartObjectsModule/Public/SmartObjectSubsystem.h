// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectCollection.h"
#include "Templates/SubclassOf.h"
#include "SmartObjectTypes.h"
#include "SmartObjectRuntime.h"
#include "Subsystems/WorldSubsystem.h"
#include "SmartObjectSubsystem.generated.h"

class USmartObjectComponent;
struct FMassEntityManager;
class ASmartObjectSubsystemRenderingActor;
class FDebugRenderSceneProxy;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagContainer UserTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagQuery ActivityRequirements;

	UE_DEPRECATED(5.1, "Use BehaviorDefinitionClasses instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use BehaviorDefinitionClasses instead"))
	TSubclassOf<USmartObjectBehaviorDefinition> BehaviorDefinitionClass;

	/** If set will filter out any SmartObject that uses different BehaviorDefinition classes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	TArray<TSubclassOf<USmartObjectBehaviorDefinition>> BehaviorDefinitionClasses;

	TFunction<bool(FSmartObjectHandle)> Predicate;
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
enum class ESmartObjectUnregistrationMode : uint8
{
	KeepRuntimeInstanceActive,
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
	static USmartObjectSubsystem* GetCurrent(const UWorld* World);

	ESmartObjectCollectionRegistrationResult RegisterCollection(ASmartObjectCollection& InCollection);
	void UnregisterCollection(ASmartObjectCollection& InCollection);
	ASmartObjectCollection* GetMainCollection() const { return MainCollection; }

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
	 * Registers a SmartObject components to the runtime simulation.
	 * @param SmartObjectComponent SmartObject component to register
	 * @return true when component is successfully registered, false otherwise
	 */
	bool RegisterSmartObject(USmartObjectComponent& SmartObjectComponent);
	
	/**
	 * Unregisters a SmartObject components from the runtime simulation.
	 * @param SmartObjectComponent SmartObject component to unregister
	 * @return true when component is successfully unregistered, false otherwise
	 */
	bool UnregisterSmartObject(USmartObjectComponent& SmartObjectComponent);

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
	bool IsSmartObjectSlotValid(const FSmartObjectSlotHandle SlotHandle) const { return SlotHandle.IsValid() && RuntimeSlotStates.Find(SlotHandle) != nullptr; }

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

#if WITH_EDITOR
	mutable FOnMainCollectionEvent OnMainCollectionChanged;
	mutable FOnMainCollectionEvent OnMainCollectionDirtied;
#endif

protected:
	friend class USmartObjectComponent;

	bool RegisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent);
	bool UnregisterSmartObjectInternal(USmartObjectComponent& SmartObjectComponent, const ESmartObjectUnregistrationMode UnregistrationMode);
	
	/**
	 * Callback overriden to gather loaded collections, spawn missing one and set the main collection.
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
	// USubsystem END

	/** Creates all runtime data using main collection */
	void InitializeRuntime();

	/** Removes all runtime data */
	void CleanupRuntime();

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
	 * Returns the const runtime instance associated to the provided handle.
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	const FSmartObjectRuntime* GetValidatedRuntime(const FSmartObjectHandle Handle, const TCHAR* Context) const;

	/**
	 * Returns the mutable runtime instance associated to the provided handle
	 * Method produces log messages with provided context if provided handle is not set or associated instance can't be found.
	 */
	FSmartObjectRuntime* GetValidatedMutableRuntime(const FSmartObjectHandle Handle, const TCHAR* Context);

	void AddTagToInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	void RemoveTagFromInstance(FSmartObjectRuntime& SmartObjectRuntime, const FGameplayTag& Tag);
	void UpdateRuntimeInstanceStatus(FSmartObjectRuntime& SmartObjectRuntime);

	/** Goes through all defined slots of smart object represented by SmartObjectRuntime and finds the ones matching the filter. */
	void FindSlots(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectRequestFilter& Filter, TArray<FSmartObjectSlotHandle>& OutResults) const;

	/** Applies filter on provided definition and fills OutValidIndices with indices of all valid slots. */
	static void FindMatchingSlotDefinitionIndices(const USmartObjectDefinition& Definition, const FSmartObjectRequestFilter& Filter, TArray<int32>& OutValidIndices);

	static const USmartObjectBehaviorDefinition* GetBehaviorDefinition(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectSlotHandle SlotHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	const USmartObjectBehaviorDefinition* Use(const FSmartObjectRuntime& SmartObjectRuntime, const FSmartObjectClaimHandle& ClaimHandle, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	void AbortAll(FSmartObjectRuntime& SmartObjectRuntime, const ESmartObjectSlotState NewState);

	FSmartObjectSlotClaimState* GetMutableSlotState(const FSmartObjectClaimHandle& ClaimHandle);

	/** Make sure that all SmartObjectCollection actors from our associated world are registered. */
	void RegisterCollectionInstances();

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 * This method must be used only when the associated actor component is not available (e.g. not loaded).
	 */
	FSmartObjectRuntime* AddCollectionEntryToSimulation(const FSmartObjectCollectionEntry& Entry, const USmartObjectDefinition& Definition);

	/**
	 * Registers a collection entry to the simulation and creates its associated runtime instance.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime entry might be created or an existing one found
	 * @param CollectionEntry The associated collection entry that got created to add the component to the simulation.
	 */
	FSmartObjectRuntime* AddComponentToSimulation(USmartObjectComponent& SmartObjectComponent, const FSmartObjectCollectionEntry& CollectionEntry);

	/**
	 * Binds a smartobject component to an existing instance in the simulation.
	 * @param SmartObjectComponent The component to add to the simulation and for which a runtime instance must exist
	 */
	void BindComponentToSimulation(USmartObjectComponent& SmartObjectComponent);

	/**
	 * Unbinds a smartobject component from an existing instance in the simulation.
	 * @param SmartObjectComponent The component to remove from the simulation
	 */
	void UnbindComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

	void RemoveRuntimeInstanceFromSimulation(const FSmartObjectHandle Handle);
	void RemoveCollectionEntryFromSimulation(const FSmartObjectCollectionEntry& Entry);
	void RemoveComponentFromSimulation(USmartObjectComponent& SmartObjectComponent);

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

	UPROPERTY()
	TObjectPtr<ASmartObjectCollection> MainCollection;

	TSharedPtr<FMassEntityManager> EntityManager;

	TMap<FSmartObjectHandle, FSmartObjectRuntime> RuntimeSmartObjects;
	TMap<FSmartObjectSlotHandle, FSmartObjectSlotClaimState> RuntimeSlotStates;

	/** Keep track of Ids associated to objects entirely created at runtime (i.e. not part of the initial collection) */
	TArray<FSmartObjectHandle> RuntimeCreatedEntries;

	/** List of registered components. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> RegisteredSOComponents;

	/** smart objects that attempted to register while no collection was being present */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USmartObjectComponent>> PendingSmartObjectRegistration;

	uint32 NextFreeUserID = 1;

	/** Flag to indicate that all entries from the baked collection are registered and new registrations will be considered runtime entries (i.e. no persistence) */
	bool bInitialCollectionAddedToSimulation = false;

#if WITH_EDITOR
	friend class ASmartObjectCollection;
	void RebuildCollection(ASmartObjectCollection& InCollection);
	void SpawnMissingCollection() const;

	/**
	 * Compute bounds from given world and store result in provided collection
	 * @param World World from which the bounds must be computed
	 * @param Collection Collection that will store computed bounds
	 */
	void ComputeBounds(const UWorld& World, ASmartObjectCollection& Collection) const;
#endif // WITH_EDITOR

#if WITH_SMARTOBJECT_DEBUG
public:
	uint32 DebugGetNumRuntimeObjects() const { return RuntimeSmartObjects.Num(); }
	const TMap<FSmartObjectHandle, FSmartObjectRuntime>& DebugGetRuntimeObjects() const { return RuntimeSmartObjects; }
	const TMap<FSmartObjectSlotHandle, FSmartObjectSlotClaimState>& DebugGetRuntimeSlots() const { return RuntimeSlotStates; }
	uint32 DebugGetNumRegisteredComponents() const { return RegisteredSOComponents.Num(); }

	/** Debugging helper to remove all registered smart objects from the simulation */
	void DebugUnregisterAllSmartObjects();

	/** Debugging helpers to add all registered smart objects to the simulation */
	void DebugRegisterAllSmartObjects();

	/** Debugging helper to emulate the start of the simulation to create all runtime data */
	void DebugInitializeRuntime();
	
	/** Debugging helper to force rebuild collection for tests scenarios when collection is built on demand */
	void DebugRebuildCollection();

	/** Debugging helper to emulate the stop of the simulation to destroy all runtime data */
	void DebugCleanupRuntime();
#endif // WITH_SMARTOBJECT_DEBUG
};
