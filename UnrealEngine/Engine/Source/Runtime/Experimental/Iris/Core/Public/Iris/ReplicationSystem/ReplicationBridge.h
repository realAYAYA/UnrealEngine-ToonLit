// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Map.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"

#include "ReplicationBridge.generated.h"

class UObjectReplicationBridge;
class UReplicationSystem;
class UNetDriver;

namespace UE::Net
{
	struct FNetDependencyInfo;
	typedef uint16 FNetObjectGroupHandle;
	class FNetTokenStoreState;
	class FReplicationFragment;
	struct FReplicationInstanceProtocol;
	struct FReplicationProtocol;
	class FNetSerializationContext;
	namespace Private
	{
		typedef uint32 FInternalNetHandle;
		class FNetHandleManager;
		class FNetObjectGroups;
		struct FNetPushObjectHandle;
		class FObjectReferenceCache;
		class FReplicationProtocolManager;
		class FReplicationReader;
		class FReplicationStateDescriptorRegistry;
		class FReplicationSystemImpl;
		class FReplicationSystemInternal;
		class FReplicationWriter;
	}

	typedef TArray<FNetDependencyInfo, TInlineAllocator<32> > FNetDependencyInfoArray;
}

#if UE_BUILD_SHIPPING
	IRISCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogIrisBridge, All, Display);
#else
	IRISCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogIrisBridge, Log, All);
#endif

struct FReplicationBridgeSerializationContext
{
	FReplicationBridgeSerializationContext(UE::Net::FNetSerializationContext& InSerialiazationContext, uint32 InConnectionId, bool bInIsDestructionInfo = false);

	UE::Net::FNetSerializationContext& SerializationContext;
	uint32 ConnectionId;
	bool bIsDestructionInfo;
};

enum class EEndReplicationFlags : uint32
{
	None								= 0U,
	/** Destroy remote instance. Default for dynamic objects unless they have TearOff flag set. */
	Destroy								= 1U,				
	/** Stop replication object without destroying instance on the remote end. */
	TearOff								= Destroy << 1U,
	/** Complete replication of pending state to all clients before ending replication. */
	Flush								= TearOff << 1U,
};
ENUM_CLASS_FLAGS(EEndReplicationFlags);

UCLASS(Transient, MinimalAPI)
class UReplicationBridge : public UObject
{
	GENERATED_BODY()

protected:
	using FNetHandle = UE::Net::FNetHandle;
	using FNetDependencyInfoArray = UE::Net::FNetDependencyInfoArray;

public:
	IRISCORE_API UReplicationBridge();
	IRISCORE_API virtual ~UReplicationBridge();

	// Local interface
	
	struct FEndReplicationParameters
	{
		/** The location of the object. Used for distance based prioritization. */
		FVector Location;
		/** The level the object is placed in. */
		const UObject* Level = nullptr;
		/** Whether to use distance based priority for the destruction of the object. */
		bool bUseDistanceBasedPrioritization = false;
	};

	enum class ESubObjectInsertionOrder : uint8
	{
		None,
		ReplicateWith,
	};

	/**
	 * Stop replicating the NetObject associated with the handle and mark the handle to be destroyed.
	 * If EEndReplication::TearOff is set the remote instance will be Torn-off rather than being destroyed on the receiving end, after the call, any state changes will not be replicated
     * If EEndReplication::Flush is set all pending states will be delivered before the remote instance is destroyed, final state will be immediately copied so it is safe to remove the object after this call
	 * If EEndReplication::Destroy is set the remote instance will be destroyed, if this is set for a static instance and the EndReplicationParameters are set a permanent destruction info will be added
	 * Dynamic instances are always destroyed unless the TearOff flag is set.
	 */
	IRISCORE_API void EndReplication(FNetHandle Handle, EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::Destroy, FEndReplicationParameters* Parameters = nullptr);

	/** Returns true if the handle is replicated. */
	IRISCORE_API bool IsReplicatedHandle(FNetHandle Handle) const;

	/** Set the NetDriver used by the bridge. Called during creation if the NetDriver is recreated. */
	IRISCORE_API virtual void SetNetDriver(UNetDriver* NetDriver);

	/** Get the group associated with the level in order to control connection filtering for it. */
	IRISCORE_API UE::Net::FNetObjectGroupHandle GetLevelGroup(const UObject* Level) const;

protected:
	/** Initializes the bridge. Is called during ReplicationSystem initialization. */
	IRISCORE_API virtual void Initialize(UReplicationSystem* InReplicationSystem);

	/** Deinitializes the bridge. Is called during ReplicationSystem deinitialization. */
	IRISCORE_API virtual void Deinitialize();

	/** Invoked before ReplicationSystem copies dirty state data. */
	IRISCORE_API virtual void PreSendUpdate();

	/** Invoked before ReplicationSystem copies dirty state data for a single replicated object. */
	IRISCORE_API virtual void PreSendUpdateSingleHandle(FNetHandle Handle);

	/** Update world locations in FWorldLocations for objects that support it. */
	IRISCORE_API virtual void UpdateInstancesWorldLocation();

	// Remote interface, invoked from Replication code during serialization
	
	/** Write data required to instantiate NetObject remotely to bitstream. */
	IRISCORE_API virtual bool WriteNetHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetHandle Handle);

	/** Read data required to instantiate NetObject from bitstream. */
	IRISCORE_API virtual FNetHandle CreateNetHandleFromRemote(FNetHandle SubObjectOwnerNetHandle, FNetHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context);

	/** Invoke after we have applied the initial state for an object.*/
	IRISCORE_API virtual void PostApplyInitialState(FNetHandle Handle);

	/**
	 * Called when the instance is detached from the protocol on request by the remote. 
	 * @param Handle The handle of the object to destroy or tear off.
	 * @param bTearOff Whether the object should be torn off, i.e. not destroyed. 
	 * @param bShouldDestroyInstance Whether the object should be destroyed.
	 */
	IRISCORE_API virtual void DetachInstanceFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance);

	/** Called when we detach instance protocol from the local instance */
	IRISCORE_API virtual void DetachInstance(FNetHandle Handle);

	/** Invoked post garbage collect to allow us to detect stale objects */
	IRISCORE_API virtual void PruneStaleObjects();

	/** Invoked when we start to replicate an object for a specific connection to fill in any initial dependencies */
	IRISCORE_API virtual void GetInitialDependencies(FNetHandle Handle, FNetDependencyInfoArray& OutDependencies) const;

protected:
	// Forward calls to internal operations that we allow replication bridges to access

	/** Create a local NetHandle / NetObject using the ReplicationProtocol. */
	IRISCORE_API FNetHandle InternalCreateNetObject(FNetHandle AllocatedHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol);

	/** Create a NetHandle / NetObject on request from the authoritative end. */
	IRISCORE_API FNetHandle InternalCreateNetObjectFromRemote(FNetHandle WantedNetHandle, const UE::Net::FReplicationProtocol* ReplicationProtocol);

	/** Attach instance to NetHandle. */
	IRISCORE_API void InternalAttachInstanceToNetHandle(FNetHandle Handle, bool bBindInstanceProtocol, UE::Net::FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance);

	// Detach instance from NetObject/Handle and destroy the instance protocol
	IRISCORE_API void InternalDetachInstanceFromNetHandle(FNetHandle Handle);

	/** Destroy the handle and all internal book keeping associated with it. */
	IRISCORE_API void InternalDestroyNetObject(FNetHandle Handle);
	
	/** Get the owner handle of a subobject handle. */
	IRISCORE_API FNetHandle InternalGetSubObjectOwner(FNetHandle SubObjectHandle) const;

	/** Add SubObjectHandle as SubObject to OwnerHandle. */
	IRISCORE_API void InternalAddSubObject(FNetHandle OwnerHandle, FNetHandle SubObjectHandle, FNetHandle InsertRelativeToSubObjectHandle, ESubObjectInsertionOrder InsertionOrder);

	/** Add destruction info for the referenced object. */
	FNetHandle InternalAddDestructionInfo(FNetHandle Handle, const FEndReplicationParameters& Parameters);

	inline UE::Net::Private::FReplicationProtocolManager* GetReplicationProtocolManager() const { return ReplicationProtocolManager; }
	inline UReplicationSystem* GetReplicationSystem() const { return ReplicationSystem; }
	inline UE::Net::Private::FReplicationStateDescriptorRegistry* GetReplicationStateDescriptorRegistry() const { return ReplicationStateDescriptorRegistry; }
	inline UE::Net::Private::FObjectReferenceCache* GetObjectReferenceCache() const { return ObjectReferenceCache; }

	/** Creates a group for a level for object filtering purposes. */
	IRISCORE_API UE::Net::FNetObjectGroupHandle CreateLevelGroup(const UObject* Level);

	/** Destroys the group associated with the level. */
	IRISCORE_API void DestroyLevelGroup(const UObject* Level);

private:

	// Internal operations invoked by ReplicationSystem/ReplicationWriter
	void ReadAndExecuteDestructionInfoFromRemote(FReplicationBridgeSerializationContext& Context);
	void DetachSubObjectInstancesFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance);
	void DestroyNetObjectFromRemote(FNetHandle Handle, bool bTearOff, bool bDestroyInstance);

	// Adds the Handle to the list of handles pending tear-off, if bIsImmediate is true the object will be destroyed after the next update, otherwise
	// it will be kept around until EndReplication is called.
	void TearOff(FNetHandle Handle, bool bIsImmediate);

	FNetHandle CallCreateNetHandleFromRemote(FNetHandle SubObjectOwnerHandle, FNetHandle WantedNetHandle, FReplicationBridgeSerializationContext& Context);
	void CallPreSendUpdate(float DeltaSeconds);	
	void CallPreSendUpdateSingleHandle(FNetHandle Handle);
	void CallUpdateInstancesWorldLocation();
	bool CallWriteNetHandleCreationInfo(FReplicationBridgeSerializationContext& Context, FNetHandle Handle);
	void CallPostApplyInitialState(FNetHandle Handle);
	void CallPruneStaleObjects();
	void CallGetInitialDependencies(FNetHandle Handle, FNetDependencyInfoArray& OutDependencies) const;
	void CallDetachInstance(FNetHandle Handle);
	void CallDetachInstanceFromRemote(FNetHandle Handle, bool bTearOff, bool bShouldDestroyInstance);

private:
	// Internal method to copy state data for Handle and any SubObjects and mark them as being torn-off
	void InternalTearOff(FNetHandle OwnerHandle);

	// Destroy all SubObjects owned by provided handle
	void InternalDestroySubObjects(FNetHandle OwnerHandle);

	/**
	 * Called from ReplicationSystem when a streaming level is about to unload.
	 * Will remove the group associated with the level and remove destruction infos.
	 */
	void NotifyStreamingLevelUnload(const UObject* Level);

	/**
	 * Remove destruction infos associated with group
	 * Passing in an invalid group handle indicates that we should remove all destruction infos
	 */
	void RemoveDestructionInfosForGroup(UE::Net::FNetObjectGroupHandle GroupHandle);

	void DestroyLocalNetHandle(FNetHandle Handle);

	// Tear-off all handles in the PendingTearOff list that has not yet been torn-off
	void TearOffHandlesPendingTearOff();

	// Update all the handles pending tear-off
	void UpdateHandlesPendingTearOff();

	void SetNetPushIdOnFragments(const TArrayView<const UE::Net::FReplicationFragment*const>& Fragments, const UE::Net::Private::FNetPushObjectHandle& PushHandle);
	void ClearNetPushIdOnFragments(const TArrayView<const UE::Net::FReplicationFragment*const>& Fragments);

	friend UReplicationSystem;
	friend UE::Net::Private::FReplicationSystemImpl;
	friend UE::Net::Private::FReplicationSystemInternal;
	friend UE::Net::Private::FReplicationWriter;
	friend UE::Net::Private::FReplicationReader;
	friend UObjectReplicationBridge;

	UReplicationSystem* ReplicationSystem;
	UE::Net::Private::FReplicationProtocolManager* ReplicationProtocolManager;
	UE::Net::Private::FReplicationStateDescriptorRegistry* ReplicationStateDescriptorRegistry;
	UE::Net::Private::FNetHandleManager* NetHandleManager;
	UE::Net::Private::FObjectReferenceCache* ObjectReferenceCache;
	UE::Net::Private::FNetObjectGroups* Groups;

	TMap<FObjectKey, UE::Net::FNetObjectGroupHandle> LevelGroups;

private:

	// DestructionInfos
	struct FDestructionInfo
	{
		UE::Net::FNetObjectReference StaticRef;
		UE::Net::FNetObjectGroupHandle LevelGroupHandle;
		UE::Net::Private::FInternalNetHandle InternalReplicationIndex;
	};

	const UE::Net::FReplicationProtocol* DestructionInfoProtocol;
	
	// Need to track the objects with destruction infos so that we can clean them up properly
	// We use this to be able ask remote to destroy static objects
	TMap<FNetHandle, FDestructionInfo> StaticObjectsPendingDestroy;

	struct FTearOffInfo
	{
		FTearOffInfo(FNetHandle InHandle, bool bInIsImmediate) : Handle(InHandle), bIsImmediate(bInIsImmediate) {}

		FNetHandle Handle;
		bool	bIsImmediate;
	};
	TArray<FTearOffInfo> HandlesPendingTearOff;
};

inline FReplicationBridgeSerializationContext::FReplicationBridgeSerializationContext(UE::Net::FNetSerializationContext& InSerialiazationContext, uint32 InConnectionId, bool bInIsDestructionInfo)
: SerializationContext(InSerialiazationContext)
, ConnectionId(InConnectionId)
, bIsDestructionInfo(bInIsDestructionInfo)
{
}
