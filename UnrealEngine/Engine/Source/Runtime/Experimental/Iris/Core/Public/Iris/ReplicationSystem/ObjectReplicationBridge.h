// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Net/Core/NetBitArray.h"
#include "Net/Core/NetHandle/NetHandle.h"
#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "Delegates/IDelegateInstance.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"

#include "ObjectReplicationBridge.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogIrisFilterConfig, Log, All);

namespace UE::Net
{
	struct FNetObjectResolveContext;
	typedef uint32 FNetObjectFilterHandle;
	typedef uint32 FNetObjectPrioritizerHandle;
	class FNetObjectReference;
	namespace Private
	{
		typedef uint32 FInternalNetRefIndex;
		class FObjectPollFrequencyLimiter;
		class FObjectPoller;
	}
}

struct FObjectReplicationBridgeInstantiateResult
{
	UObject* Object = nullptr;
	EReplicationBridgeCreateNetRefHandleResultFlags Flags = EReplicationBridgeCreateNetRefHandleResultFlags::None;
};

/*
* Partial implementation of ReplicationBridge that can be used as a foundation for 
* implementing support for replicating objects derived from UObject
*/
UCLASS(Transient, MinimalApi)
class UObjectReplicationBridge : public UReplicationBridge
{
	GENERATED_BODY()

public:
	using UReplicationBridge::EndReplication;
	using EGetRefHandleFlags = UE::Net::EGetRefHandleFlags;

	struct FCreateNetRefHandleParams
	{
		//$IRIS TODO: These need documentation
		bool bCanReceive = false;
		bool bNeedsPreUpdate = false;
		bool bNeedsWorldLocationUpdate = false;

		/** Whether the object is dormant or not */
		bool bIsDormant = false;

		/** When true we ask the class config if a dynamic filter was assigned to this class or one of it's parent inherited class. */
		bool bUseClassConfigDynamicFilter = false;

		/** When enabled we ignore the class config for this object and instead use the one specified in ExplicitDynamicFilter */
		bool bUseExplicitDynamicFilter = false;

		/** 
		 * The name of the dynamic filter to use for this object (instead of asking the class config). 
		 * Can be none so that no dynamic filter is assigned to the object.
		 * Only used when bUseExplicitDynamicFilter is true.
		 */
		FName ExplicitDynamicFilterName;

		/**
		 * If StaticPriority is > 0 the ReplicationSystem will use that as priority when scheduling objects. 
		 * If it's <= 0.0f one will look for a world location support and then use the default spatial prioritizer.
		 */
		float StaticPriority = 0.0f;

		/**
		 * How often per second the object should be polled for dirtiness, including calling the InstancePreUpdate function. 
		 * When set to zero it will be polled every frame.
		 */
		float PollFrequency = 0.0f;
	};

	IRISCORE_API static FCreateNetRefHandleParams DefaultCreateNetRefHandleParams;

	IRISCORE_API UObjectReplicationBridge();

	/** Get the Object from a replicated handle, if the handle is invalid or not is a replicated handle the function will return nullptr */
	IRISCORE_API UObject* GetReplicatedObject(FNetRefHandle Handle) const;

	/** Get NetRefHandle from a replicated UObject. */
	IRISCORE_API FNetRefHandle GetReplicatedRefHandle(const UObject* Object, EGetRefHandleFlags GetRefHandleFlags = EGetRefHandleFlags::None) const;

	/** Get NetRefHandle from a NetHandle. */
	IRISCORE_API FNetRefHandle GetReplicatedRefHandle(FNetHandle Handle) const;

	/** Try to resolve UObject from NetObjectReference, this function tries to resolve the object by loading if necessary. */
	IRISCORE_API UObject* ResolveObjectReference(const UE::Net::FNetObjectReference& ObjectRef, const UE::Net::FNetObjectResolveContext& ResolveContext);

	/** Describe the NetObjectReference */
	IRISCORE_API FString DescribeObjectReference(const UE::Net::FNetObjectReference& ObjectRef, const UE::Net::FNetObjectResolveContext& ResolveContext);

	/** Get or create NetObjectReference for object instance. */
	IRISCORE_API UE::Net::FNetObjectReference GetOrCreateObjectReference(const UObject* Instance) const;

	/** Get or create NetObjectReference for object identified by path relative to outer. */
	IRISCORE_API UE::Net::FNetObjectReference GetOrCreateObjectReference(const FString& Path, const UObject* Outer) const;

	/** Begin replicating the Instance and return a valid NetRefHandle for the Instance if successful. */
	IRISCORE_API FNetRefHandle BeginReplication(UObject* Instance, const FCreateNetRefHandleParams& Params = DefaultCreateNetRefHandleParams);

	/**
	 * Begin replicating the Instance as a subobject of the OwnerHandle. If InsertRelativeSubObjectHandle is valid
	 * the new subobject will be inserted in the subobject replication list next to the specified handle and the wanted insertion order.
	 * Default behavior is to always add new subobjects at the end of the list. Returns a valid NetRefHandle for the Instance if successful.
	 */
	IRISCORE_API FNetRefHandle BeginReplication(FNetRefHandle OwnerHandle, UObject* Instance, FNetRefHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder = UReplicationBridge::ESubObjectInsertionOrder::None, const FCreateNetRefHandleParams& Params = DefaultCreateNetRefHandleParams);

	/** Create handle and start replicating the Instance as a SubObject of the OwnerHandle. */
	/** Begin replicating the Instance as a subobject of the OwnerHandle and return a valid NetRefHandle for the Instance if successful. */
	IRISCORE_API FNetRefHandle BeginReplication(FNetRefHandle OwnerHandle, UObject* Instance, const FCreateNetRefHandleParams& Params = DefaultCreateNetRefHandleParams);

	/** 
	 * Set NetCondition for a subobject, the condition is used to determine if the SubObject should replicate or not.
	 * @note As the filtering is done at the serialization level it is typically more efficient to use a separate NetObject for connection 
	 * specific data as filtering can then be done at a higher level.
	 */
	IRISCORE_API void SetSubObjectNetCondition(FNetRefHandle SubObjectHandle, ELifetimeCondition Condition);

	/** Get the handle of the root object of any replicated subobject. */
	IRISCORE_API FNetRefHandle GetRootObjectOfSubObject(FNetRefHandle SubObjectHandle) const;

	/** Stop replicating the object. */
	IRISCORE_API void EndReplication(UObject* Instance, EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::Destroy, FEndReplicationParameters* Parameters = nullptr);

	/** Add static destruction info, this is used when stably named objects are destroyed prior to starting replication */
	IRISCORE_API void AddStaticDestructionInfo(const FString& ObjectPath, const UObject* Outer, const FEndReplicationParameters& Parameters);
	
	/** PIE package name remapping support. */
	virtual bool RemapPathForPIE(uint32 ConnectionId, FString& Path, bool bReading) const { return false; }

	/** Returns true of the level that the Object belongs to has finished loading. */
	virtual bool ObjectLevelHasFinishedLoading(UObject* Object) const { return true; }

	struct FCreationHeader
	{
		virtual ~FCreationHeader() {};
	};
	
	/**
	 * Adds a dependent object. A dependent object can replicate separately or if a parent replicates.
	 * Dependent objects cannot be filtered out by dynamic filtering unless the parent is also filtered out.
	 * @note: There is no guarantee that the data will end up in the same packet so it is a very loose form of dependency.
	 */
	IRISCORE_API void AddDependentObject(FNetRefHandle Parent, FNetRefHandle DependentObject, UE::Net::EDependentObjectSchedulingHint SchedulingHint = UE::Net::EDependentObjectSchedulingHint::Default);

	/** Remove dependent object from parent. The dependent object will function as a standard standalone replicated object. */
	IRISCORE_API void RemoveDependentObject(FNetRefHandle Parent, FNetRefHandle DependentObject);

	// Dormancy support

	/** Set whether object should go dormant. If dormancy is enabled any dirty state will be replicated first. */
	IRISCORE_API void SetObjectWantsToBeDormant(FNetRefHandle Handle, bool bWantsToBeDormant);

	/** Returns whether the object wants to be dormant. */
	IRISCORE_API bool GetObjectWantsToBeDormant(FNetRefHandle Handle) const;

	/** Trigger replication of dirty state for object wanting to be dormant. */
	IRISCORE_API void ForceUpdateWantsToBeDormantObject(FNetRefHandle Handle);	

	/** Set poll frequency on root object and its subobjects. They will be polled on the same frame. */
	IRISCORE_API void SetPollFrequency(FNetRefHandle RootHandle, float PollFrequency);

	/** Set the object filter to use for objects of this class and any derived classes without an explicit config. */
	IRISCORE_API void SetClassDynamicFilterConfig(FName ClassPathName, const UE::Net::FNetObjectFilterHandle FilterHandle, FName FilterProfile=NAME_None);
	IRISCORE_API void SetClassDynamicFilterConfig(FName ClassPathName, FName FilterName, FName FilterProfile=NAME_None);

	/** Set the TypeStats to use for specified class and any derived classes without explicit config */
	IRISCORE_API void SetClassTypeStatsConfig(FName ClassPathName, FName TypeStatsName);
	IRISCORE_API void SetClassTypeStatsConfig(const FString& ClassPathName, const FString& TypeStatsName);

public:

	// Debug functions exposed that are triggered via console commands
	void PrintDynamicFilterClassConfig() const;
	void PrintReplicatedObjects(uint32 ArgTraits) const;
	void PrintAlwaysRelevantObjects(uint32 ArgTraits) const;
	void PrintRelevantObjects(uint32 ArgTraits) const;
	void PrintRelevantObjectsForConnections(const TArray<FString>& Args) const;
	void PrintNetCullDistances(const TArray<FString>& Args) const;

protected:
	IRISCORE_API virtual ~UObjectReplicationBridge();

	// UReplicationBridge

	IRISCORE_API virtual void Initialize(UReplicationSystem* InReplicationSystem) override;
	IRISCORE_API virtual void Deinitialize() override;
	IRISCORE_API virtual void PreSendUpdateSingleHandle(FNetRefHandle RefHandle) override;	
	IRISCORE_API virtual void PreSendUpdate() override;	
	IRISCORE_API virtual void OnStartPreSendUpdate() override;
	IRISCORE_API virtual void OnPostSendUpdate() override;
	IRISCORE_API virtual void UpdateInstancesWorldLocation() override;
	IRISCORE_API virtual void PruneStaleObjects() override;	
	IRISCORE_API virtual bool WriteNetRefHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetRefHandle Handle) override;
	IRISCORE_API virtual FReplicationBridgeCreateNetRefHandleResult CreateNetRefHandleFromRemote(FNetRefHandle RootObjectNetHandle, FNetRefHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context) override;
	IRISCORE_API virtual void SubObjectCreatedFromReplication(FNetRefHandle SubObjectHandle) override;
	IRISCORE_API virtual void PostApplyInitialState(FNetRefHandle Handle) override;
	IRISCORE_API virtual void DetachInstanceFromRemote(FNetRefHandle Handle, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags) override;
	IRISCORE_API virtual void DetachInstance(FNetRefHandle Handle) override;
	IRISCORE_API virtual void OnProtocolMismatchReported(FNetRefHandle RefHandle, uint32 ConnectionId) override;
	IRISCORE_API virtual void OnErrorWithNetRefHandleReported(uint32 ErrorType, FNetRefHandle RefHandle, uint32 ConnectionId) override;
	
protected:
	/**
	* $IRIS TODO:
	* Consider moving these methods to separate interface to allow for a single ObjectReplicationBridge to support different type specific methods to its own interface and allow for a single bridge to support multiple subtypes? 
	* We will see what we need when we start to Serialize data we might not even want to expose virtual methods for doing this, and instead provide support for specifying a "creation state" that works exactly as any other state
	* but only ever is replicated when we first instantiate an object of the specific type 
	*/

	/** Write data required to instantiate the Handle. */
	virtual bool WriteCreationHeader(UE::Net::FNetSerializationContext& Context, FNetRefHandle Handle) { return false; };

	/** Create Header and read data required to instantiate the Handle. */
	virtual FCreationHeader* ReadCreationHeader(UE::Net::FNetSerializationContext& Context) { return nullptr; };

	/** Called when we instantiate/find object instance requested by remote. */
	virtual FObjectReplicationBridgeInstantiateResult BeginInstantiateFromRemote(FNetRefHandle RootObjectOfSubObject, const UE::Net::FNetObjectResolveContext& ResolveContext, const FCreationHeader* Header) { return FObjectReplicationBridgeInstantiateResult(); };

	/** Invoked before we start applying state data to instance on remote end. */
	virtual bool OnInstantiatedFromRemote(UObject* Instance, const FCreationHeader* InHeader, uint32 ConnectionId) const { return true; }

	/** Invoked for new replicated SubObjects after state has been applied to owner */
	virtual void OnSubObjectCreatedFromReplication(FNetRefHandle SubObjectHandle) {};

	/** Invoked after remote NetHandle has been created and initial state is applied. */
	virtual void EndInstantiateFromRemote(FNetRefHandle Handle) {};

	struct FDestroyInstanceParams
	{
		UObject* Instance = nullptr;
		UObject* RootObject = nullptr;
		EReplicationBridgeDestroyInstanceReason DestroyReason = EReplicationBridgeDestroyInstanceReason::DoNotDestroy;
		EReplicationBridgeDestroyInstanceFlags DestroyFlags = EReplicationBridgeDestroyInstanceFlags::None;
	};

	/** Destroy or tear-off the game instance on request from remote. */
	virtual void DestroyInstanceFromRemote(const FDestroyInstanceParams& Params) {}

	/** Called when we found a divergence between the local and remote protocols when trying to instantiate a remote replicated object. */
	virtual void OnProtocolMismatchDetected(FNetRefHandle ObjectHandle) {}

protected:
	/** Lookup the UObject associated with the provided Handle. This function will not try to resolve the reference. */
	IRISCORE_API UObject* GetObjectFromReferenceHandle(FNetRefHandle RefHandle) const;

	/** Helper method that calls provided PreUpdateFunction and polls state data for all replicated instances with the NeedsPoll trait. */
	using FInstancePreUpdateFunction = TFunction<void(FNetRefHandle, UObject*, const UReplicationBridge*)>;

	/** Set the function that we should call before copying state data. */
	IRISCORE_API void SetInstancePreUpdateFunction(FInstancePreUpdateFunction InPreUpdateFunction);
	
	/** Helper method to get the world location & cull distance for replicated instances with the HasWorldLocation trait. */
	using FInstanceGetWorldObjectInfoFunction = TFunction<void(UE::Net::FNetRefHandle, const UObject*, FVector&, float&)>;

	/** Set the function that we should call to get the world location of an object. */
	IRISCORE_API void SetInstanceGetWorldObjectInfoFunction(FInstanceGetWorldObjectInfoFunction InGetWorldObjectInfoFunction);
	
	// Poll frequency support

	/** Force polling of Object when ObjectToPollWith is polled. */
	IRISCORE_API void SetPollWithObject(FNetRefHandle ObjectToPollWith, FNetRefHandle Object);

	/** Transform a poll frequency (in updates per second) to an equivalent number of frames. */
	IRISCORE_API uint8 ConvertPollFrequencyIntoFrames(float PollFrequency) const;

	/** Returns the poll frequency of a specific root object. */
	IRISCORE_API virtual float GetPollFrequencyOfRootObject(const UObject* ReplicatedObject) const;

	/** Re-initialize the poll frequency of all replicated root objects. */
	IRISCORE_API void ReinitPollFrequency();

	/** Re-initialize config-driven parameters found in ObjectReplicationBridgeConfig. */
	IRISCORE_API void LoadConfig();

	/**
	 * Set the function used to determine whether an object should use the default spatial filter
	 * unless the class is configured to use some other filter.
	 */
	IRISCORE_API void SetShouldUseDefaultSpatialFilterFunction(TFunction<bool(const UClass*)>);

	/** Set the function used to determine whether two classes are to be considered equal when it comes to filtering. Used on subclasses. */
	IRISCORE_API void SetShouldSubclassUseSameFilterFunction(TFunction<bool(const UClass* Class, const UClass* Subclass)>);

	/** Finds the final poll frequency for a class and cache it for future lookups. OutPollPeriod will only be modified if this method returns true. */
	IRISCORE_API bool FindOrCachePollFrequency(const UClass* Class, float& OutPollPeriod);

	/**
	 * Find the poll period of a class if it was configured with an override. 
	 * Use this only if all class configs have been properly cached.
	 * OutPollPeriod will only be modified if this method returns true.
	 */
	IRISCORE_API bool GetClassPollFrequency(const UClass* Class, float& OutPollPeriod) const;

	/** Returns true if the class is considered critical and we force a disconnection if a protocol mismatch prevents instances of this class from replicating. */
	IRISCORE_API bool IsClassCritical(const UClass* Class);

	/** Returns the most relevant description of the client tied to this connection id. */
	[[nodiscard]] IRISCORE_API virtual FString PrintConnectionInfo(uint32 ConnectionId) const;

	/** Current max tick rate set by the engine */
	float GetMaxTickRate() const { return MaxTickRate; }

	/** Change the max tick rate to match the one from the engine */
	void SetMaxTickRate(float InMaxTickRate) { MaxTickRate = InMaxTickRate; }

	/**
	 * Parses a list of arguments and returns a list of Connection's that match them
	 * Ex: ConnectionId=1 or ConnectionId=1,5,7
	 */
	IRISCORE_API virtual TArray<uint32> FindConnectionsFromArgs(const TArray<FString>& Args) const;

private:

	/** Forcibly poll a single replicated object */
	void ForcePollObject(FNetRefHandle RefHandle);

	/** Build the list of relevant objects who hit their polling period or were flagged ForceNetUpdate. */
	void BuildPollList(UE::Net::FNetBitArrayView ObjectsConsideredForPolling);

	/** Call the user function PreUpdate (aka PreReplication) on objects about to be polled. */
	void PreUpdate(const UE::Net::FNetBitArrayView ObjectsConsideredForPolling);

	/** Find any objects that got set dirty by user code during PreUpdate then lock future modifications to the global dirty list */
	void FinalizeDirtyObjects();

	/** Find any new subobjects created inside PreUpdate and ensure they will be replicated this frame */
	void ReconcileNewSubObjects(UE::Net::FNetBitArrayView ObjectsConsideredForPolling);

	/** Poll all objects in the list and copy the dirty source data into the ReplicationState buffers*/
	void PollAndCopy(const UE::Net::FNetBitArrayView ObjectsConsideredForPolling);

	/** Remove mapping between handle and object instance. */
	void UnregisterInstance(FNetRefHandle RefHandle);

	void RegisterRemoteInstance(FNetRefHandle RefHandle, UObject* InstancePtr, const UE::Net::FReplicationProtocol* Protocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, const FCreationHeader* Header, uint32 ConnectionId);

	void SetNetPushIdOnInstance(UE::Net::FReplicationInstanceProtocol* InstanceProtocol, FNetHandle NetHandle);

	/** Tries to load the classes used in poll period overrides. */
	void FindClassesInPollPeriodOverrides();

protected:
	/** Retrieves the dynamic filter to set for the given class. Will return an invalid handle if no dynamic filter should be set. */
	IRISCORE_API UE::Net::FNetObjectFilterHandle GetDynamicFilter(const UClass* Class, bool bRequireForceEnabled, FName& OutFilterProfile);

private:
	/** Retrieves the prioritizer to set for the given class. If bRequireForceEnabled the config needs to have bForceEnableOnAllInstances set in order for this method to return the configured prioritizer. Returns an invalid handle if no prioritizer should be set. */
	UE::Net::FNetObjectPrioritizerHandle GetPrioritizer(const UClass* Class, bool bRequireForceEnabled);

	/** Assign the proper dynamic filter to a new object */
	void AssignDynamicFilter(UObject* Instance, const FCreateNetRefHandleParams& Params, FNetRefHandle RefHandle);

	/** Returns true if instances of this class should be delta compressed */
	bool ShouldClassBeDeltaCompressed(const UClass* Class);

	/** Marks a spatially filtered object as requiring or not requiring frequent world location updates independent of it having dirty replicated properties */
	void OptionallySetObjectRequiresFrequentWorldLocationUpdate(FNetRefHandle RefHandle, bool bDesiresFrequentWorldLocationUpdate);

	/** Returns the TypeStatsIndex this class should use */
	int32 GetTypeStatsIndex(const UClass* Class);

	FInstancePreUpdateFunction PreUpdateInstanceFunction;
	FInstanceGetWorldObjectInfoFunction GetInstanceWorldObjectInfoFunction;
	
	FName GetConfigClassPathName(const UClass* Class);

	void InitConditionalPropertyDelegates();

private:

	friend UE::Net::Private::FObjectPoller;

	TMap<const UClass*, FName> ConfigClassPathNameCache;

	// Prioritization
	struct FClassPrioritizerInfo
	{
		UE::Net::FNetObjectPrioritizerHandle PrioritizerHandle;
		bool bForceEnable = false;
	};
	
	// Filtering
	struct FClassFilterInfo
	{
		UE::Net::FNetObjectFilterHandle FilterHandle;
		FName FilterProfile;
		bool bForceEnable = false;
	};

	// Polling

	/** The maximum tick per second of the engine. Default is to use 30hz */
	float MaxTickRate = 30.0f;

	struct FPollInfo
	{
		float PollFrequency = 0.0f;
		TWeakObjectPtr<const UClass> Class;
	};
	UE::Net::Private::FObjectPollFrequencyLimiter* PollFrequencyLimiter;

	//$IRIS TODO: The poll class config management code should be moved into it's own class. Maybe in a class that handles any type of per-class settings.
	// Class hierarchies with poll period overrides
	TMap<FName, FPollInfo> ClassHierarchyPollPeriodOverrides;
	// Exact classes with poll period overrides
	TMap<FName, FPollInfo> ClassesWithPollPeriodOverride;
	// Exact classes without poll period override
	TSet<FName> ClassesWithoutPollPeriodOverride;

	// Filter mapping
	//$IRIS TODO: Look into improving this class map by balancing runtime speed (implicit addition of new classes) with runtime modifications of base classes.
    //		      Right-now any changes to say APawn's filter will not be read if a APlayerPawn entry was created based on the APawn entry.
	TMap<FName, FClassFilterInfo> ClassesWithDynamicFilter;
	TFunction<bool(const UClass*)> ShouldUseDefaultSpatialFilterFunction;
	TFunction<bool(const UClass*,const UClass*)> ShouldSubclassUseSameFilterFunction;

	// Prioritizer mapping
	TMap<FName, FClassPrioritizerInfo> ClassesWithPrioritizer;

	// Delta compression
	TMap<FName, bool> ClassesWithDeltaCompression;

	// Classes that may force a disconnection when a protocol mismatch is detected.
	TMap<FName, bool> ClassesFlaggedCritical;

	// Type stats
	TMap<FName, FName> ClassesWithTypeStats;

	// Array of dormant objects that has requested a flush
	TArray<FNetRefHandle> DormantHandlesPendingFlush;

	// Objects which has object references and could be affected by garbage collection.
	UE::Net::FNetBitArray ObjectsWithObjectReferences;
	// Objects that needs to update their object references due to garbage collection.
	UE::Net::FNetBitArray GarbageCollectionAffectedObjects;

	UE::Net::FNetObjectFilterHandle DefaultSpatialFilterHandle;

	FDelegateHandle OnCustomConditionChangedHandle;
	FDelegateHandle OnDynamicConditionChangedHandle;

	bool bHasPollOverrides = false;
	bool bHasDirtyClassesInPollPeriodOverrides = false;

	/** Set to true when the system does not allow new objects to begin replication at this moment. Useful when calling into user code and to warn them of illegal operations. */
	bool bBlockBeginReplication = false;

protected:
	bool bSuppressCreateInstanceFailedEnsure = false;
};


inline UE::Net::FNetRefHandle UObjectReplicationBridge::BeginReplication(UE::Net::FNetRefHandle OwnerHandle, UObject* Instance, const FCreateNetRefHandleParams& Params)
{
	return BeginReplication(OwnerHandle, Instance, FNetRefHandle::GetInvalid(), ESubObjectInsertionOrder::None, Params);
}
