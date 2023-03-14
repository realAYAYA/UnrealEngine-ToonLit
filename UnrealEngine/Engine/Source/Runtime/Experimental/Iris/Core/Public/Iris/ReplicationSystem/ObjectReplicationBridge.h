// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationSystem/ReplicationBridge.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"

#include "ObjectReplicationBridge.generated.h"

namespace UE::Net
{
	struct FNetObjectResolveContext;
	typedef uint32 FNetObjectFilterHandle;
	typedef uint16 FNetObjectGroupHandle;
	typedef uint32 FNetObjectPrioritizerHandle;
	class FNetObjectReference;
	namespace Private
	{
		typedef uint32 FInternalNetHandle;
		class FObjectPollFrequencyLimiter;
	}
}

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

	struct FCreateNetHandleParams
	{
		uint32 bCanReceive : 1;
		uint32 bNeedsPreUpdate : 1;
		uint32 bNeedsWorldLocationUpdate : 1;
		uint32 bAllowDynamicFilter : 1;

		/**
		  * If StaticPriority is > 0 the ReplicationSystem will use that as priority when scheduling objects.
		  * If it's <= 0.0f one will look for a world location support and then use the default spatial prioritizer.
		  */
		float StaticPriority;

		/**
		  * How often the object should be polled for dirtiness, including calling the InstancePreUpdate function.
		  * The period is zero based so 0 means poll every frame and 255 means poll every 256th frame.
		  */
		uint8 PollFramePeriod;
	};
	IRISCORE_API static FCreateNetHandleParams DefaultCreateNetHandleParams;

	IRISCORE_API UObjectReplicationBridge();

	/** Get the Object from a replicated handle, if the handle is invalid or not is a replicated handle the function will return nullptr */
	IRISCORE_API UObject* GetReplicatedObject(FNetHandle Handle) const;

	/** Get NetHandle from a Replicated UObject */
	IRISCORE_API FNetHandle GetReplicatedHandle(const UObject* Object) const;

	/** Try to resolve UObject from NetObjectReference, this function tries to resolve the object by loading if necessary.*/
	IRISCORE_API UObject* ResolveObjectReference(const UE::Net::FNetObjectReference& ObjectRef, const UE::Net::FNetObjectResolveContext& ResolveContext);

	/** Get or create NetObjectReference for object instance. */
	IRISCORE_API UE::Net::FNetObjectReference GetOrCreateObjectReference(const UObject* Instance) const;

	/** Get or create NetObjectReference for object identified by path relative to outer. */
	IRISCORE_API UE::Net::FNetObjectReference GetOrCreateObjectReference(const FString& Path, const UObject* Outer) const;

	/** Create NetHandle and start replicating the Instance */
	IRISCORE_API FNetHandle BeginReplication(UObject* Instance, const FCreateNetHandleParams& Params = DefaultCreateNetHandleParams);

	/**
	 * Create NetHandle and start replicating the Instance as a SubObject of the OwnerHandle. If InsertRelativeSubObjectHandle is valid
	 * the new subobject will be inserted in the subobject replication list next to the specified handle and the wanted insertion order.
	 * Default behavior is to always add new subobjects at the end of the list.
	 */
	IRISCORE_API FNetHandle BeginReplication(FNetHandle OwnerHandle, UObject* Instance, FNetHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder = UReplicationBridge::ESubObjectInsertionOrder::None, const FCreateNetHandleParams& Params = DefaultCreateNetHandleParams);

	/** Create NetHandle and start replicating the Instance as a SubObject of the OwnerHandle */
	FNetHandle BeginReplication(FNetHandle OwnerHandle, UObject* Instance, const FCreateNetHandleParams& Params = DefaultCreateNetHandleParams) { return BeginReplication(OwnerHandle, Instance, FNetHandle(), ESubObjectInsertionOrder::None, Params); };

	/** 
	 * Set NetCondition for a subobject, the condition is used to determine if the SubObject should replicated or not.
	 * @note: As the filtering is done at the serialization level it is typically more efficient to use a separate NetObject for connection 
	 * specific data as filtering can then be done at a higher level.
	 */
	IRISCORE_API void SetSubObjectNetCondition(FNetHandle SubObjectHandle, ELifetimeCondition Condition);

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

	// Dependent objects supprt 

	enum class EDependentWarnFlags : uint32
	{
		None = 1 << 0U,
		WarnAlreadyDependent = 1 << None, 
	};
	
	/**
	 * Adds a dependent object. A dependent object can replicate separately or if a parent replicates.
	 * Dependent objects cannot be filtered out by dynamic filtering unless the parent is also filtered out.
	 * @note: There is no guarantee that the data will end up in the same packet so it is a very loose form of dependency.
	 */
	IRISCORE_API void AddDependentObject(FNetHandle Parent, FNetHandle DependentObject, EDependentWarnFlags WarnFlags = EDependentWarnFlags::WarnAlreadyDependent);

	/** Remove dependent object from parent. The dependent object will function as a standard standalone replicated object. */
	IRISCORE_API void RemoveDependentObject(FNetHandle Parent, FNetHandle DependentObject);

	// Dormancy support

	/** Set whether object should go dormant. If dormancy is enabled any dirty state will be replicated first. */
	IRISCORE_API void SetObjectWantsToBeDormant(FNetHandle Handle, bool bWantsToBeDormant);

	/** Returns whether the object wants to be dormant. */
	IRISCORE_API bool GetObjectWantsToBeDormant(FNetHandle Handle) const;

	/** Trigger replication of dirty state for object wanting to be dormant. */
	IRISCORE_API void ForceUpdateWantsToBeDormantObject(FNetHandle Handle);	

protected:
	IRISCORE_API virtual ~UObjectReplicationBridge();

	// UReplicationBridge

	IRISCORE_API virtual void Initialize(UReplicationSystem* InReplicationSystem) override;
	IRISCORE_API virtual void Deinitialize() override;
	IRISCORE_API virtual void PreSendUpdateSingleHandle(FNetHandle Handle) override;	
	IRISCORE_API virtual void PreSendUpdate() override;	
	IRISCORE_API virtual void UpdateInstancesWorldLocation() override;
	IRISCORE_API virtual void PruneStaleObjects() override;	
	IRISCORE_API virtual bool WriteNetHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetHandle Handle) override;
	IRISCORE_API virtual FNetHandle CreateNetHandleFromRemote(FNetHandle SubObjectOwnerNetHandle, FNetHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context) override;
	IRISCORE_API virtual void PostApplyInitialState(FNetHandle Handle) override;
	IRISCORE_API virtual void DetachInstanceFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance) override;
	IRISCORE_API virtual void DetachInstance(FNetHandle Handle) override;

protected:
	/**
	* $IRIS TODO:
	* Consider moving these methods to separate interface to allow for a single ObjectReplicationBridge to support different type specific methods to its own interface and allow for a single bridge to support multiple subtypes? 
	* We will see what we need when we start to Serialize data we might not even want to expose virtual methods for doing this, and instead provide support for specifying a "creation state" that works exactly as any other state
	* but only ever is replicated when we first instantiate an object of the specific type 
	*/

	/** Write data required to instantiate the Handle. */
	virtual bool WriteCreationHeader(UE::Net::FNetSerializationContext& Context, FNetHandle Handle) { return false; };

	/** Create Header and read data required to instantiate the Handle. */
	virtual FCreationHeader* ReadCreationHeader(UE::Net::FNetSerializationContext& Context) { return nullptr; };

	/** Called when we instantiate/find object instance requested by remote. */
	virtual UObject* BeginInstantiateFromRemote(FNetHandle SubObjectOwnerHandle, const UE::Net::FNetObjectResolveContext& ResolveContext, const FCreationHeader* Header) { return nullptr; };

	/** Invoked before we start applying state data to instance on remote end. */
	virtual bool OnInstantiatedFromRemote(UObject* Instance, const FCreationHeader* InHeader, uint32 ConnectionId) const { return true; }

	/** Invoked after remote NetHandle has been created and initial state is applied. */
	virtual void EndInstantiateFromRemote(FNetHandle Handle) {};

	/** Destroy or tear-off the game instance on request from remote. */
	virtual void DestroyInstanceFromRemote(UObject* Instance, bool bTearOff) {};

protected:
	/** Lookup the UObject associated with the provided Handle. This function will not try to resolve the reference. */
	IRISCORE_API UObject* GetObjectFromReferenceHandle(FNetHandle NetHandle) const;

	/** Helper method that calls provided PreUpdateFunction and polls state data for all replicated instances with the NeedsPoll trait. */
	using FInstancePreUpdateFunction = void(*)(FNetHandle, UObject*, const UReplicationBridge*);

	/** Set the function that we should call before copying state data. */
	IRISCORE_API void SetInstancePreUpdateFunction(FInstancePreUpdateFunction InPreUpdateFunction);
	
	/** Helper method to get the world location for replicated instances with the HasWorldLocation trait. */
	using FInstanceGetWorldLocationFunction = FVector(*)(FNetHandle, const UObject*);

	/** Set the function that we should call to get the world location of an object. */
	IRISCORE_API void SetInstanceGetWorldLocationFunction(FInstanceGetWorldLocationFunction InGetWorldLocationFunction);
	
	// Poll frequency support

	/** Set poll frame period for a specific object. Zero based period where 0 means every frame and 255 means every 256th frame. */
	IRISCORE_API void SetPollFramePeriod(UE::Net::Private::FInternalNetHandle InternalReplicationIndex, uint8 FramePeriod);

	/** Force polling of Object when ObjectToPoll is polled. */
	IRISCORE_API void SetPollWithObject(FNetHandle ObjectToPollWith, FNetHandle Object);

	/** Re-initialize config-driven parameters found in ObjectReplicationBridgeConfig. */
	IRISCORE_API void LoadConfig();

	/**
	 * Set the function used to determine whether an object should use the default spatial filter
	 * unless the class is configured to use some other filter.
	 */
	IRISCORE_API void SetShouldUseDefaultSpatialFilterFunction(TFunction<bool(const UClass*)>);

	/** Set the function used to determine whether two classes are to be considered equal when it comes to filtering. Used on subclasses. */
	IRISCORE_API void SetShouldSubclassUseSameFilterFunction(TFunction<bool(const UClass* Class, const UClass* Subclass)>);

private:

	void PreUpdateAndPollImpl(FNetHandle Handle);

	void RegisterRemoteInstance(FNetHandle Handle, UObject* InstancePtr, const UE::Net::FReplicationProtocol* Protocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, const FCreationHeader* Header, uint32 ConnectionId);

	void UnregisterRemoteInstance(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance);

	/** Finds a poll period override if it exists. OutPollPeriod will only be modified if this method returns true. */
	bool FindPollInfo(const UClass* Class, uint8& OutPollPeriod);

	/** Tries to load the classes used in poll period overrides. */
	void FindClassesInPollPeriodOverrides();

	/** Retrieves the dynamic filter to set for the given class. Will return an invalid handle if no dynamic filter should be set. */
	UE::Net::FNetObjectFilterHandle GetDynamicFilter(const UClass* Class);

	/** Retrieves the prioritizer to set for the given class. Will return an invalid handle if no prioritizer should be set. */
	UE::Net::FNetObjectPrioritizerHandle GetPrioritizer(const UClass* Class);

	/** Returns true if instances of this class should be delta compressed */
	bool ShouldClassBeDeltaCompressed(const UClass* Class);

	FInstancePreUpdateFunction PreUpdateInstanceFunction;
	FInstanceGetWorldLocationFunction GetInstanceWorldLocationFunction;

	FName GetConfigClassPathName(const UClass* Class);

private:

	TMap<const UClass*, FName> ConfigClassPathNameCache;

	// Polling
	struct FPollInfo
	{
		uint8 PollFramePeriod = 0;
		TWeakObjectPtr<const UClass> Class;
	};
	UE::Net::Private::FObjectPollFrequencyLimiter* PollFrequencyLimiter;

	// Class hierarchies with poll period overrides
	TMap<FName, FPollInfo> ClassHierarchyPollPeriodOverrides;
	// Exact classes with poll period overrides
	TMap<FName, FPollInfo> ClassesWithPollPeriodOverride;
	// Exact classes without poll period override
	TSet<FName> ClassesWithoutPollPeriodOverride;

	// Filter mapping
	TMap<FName, UE::Net::FNetObjectFilterHandle> ClassesWithDynamicFilter;
	TFunction<bool(const UClass*)> ShouldUseDefaultSpatialFilterFunction;
	TFunction<bool(const UClass*,const UClass*)> ShouldSubclassUseSameFilterFunction;

	// Prioritizer mapping
	TMap<FName, UE::Net::FNetObjectPrioritizerHandle> ClassesWithPrioritizer;

	// Delta compression
	TMap<FName, bool> ClassesWithDeltaCompression;

	// Array of dormant objects that has requested a flush
	TArray<FNetHandle> DormantHandlesPendingFlush;

	// Objects which has object references and could be affected by garbage collection.
	UE::Net::FNetBitArray ObjectsWithObjectReferences;
	// Objects that needs to update their object references due to garbage collection.
	UE::Net::FNetBitArray GarbageCollectionAffectedObjects;

	UE::Net::FNetObjectFilterHandle DefaultSpatialFilterHandle;

	bool bHasPollOverrides = false;
	bool bHasDirtyClassesInPollPeriodOverrides = false;
};
