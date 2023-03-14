// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Templates/PimplPtr.h"
#include "Templates/RefCounting.h"
#include "Iris/IrisConfig.h"
#include "Iris/ReplicationSystem/NetHandle.h"

#include "ReplicationSystem.generated.h"

class UDataStreamManager;
class UFunction;
class UNetBlobHandler;
class UNetDriver;
class UNetObjectFilter;
class UNetObjectPrioritizer;
class UReplicationBridge;
class UReplicationSystem;
namespace UE::Net
{
	class FNetBitArray;
	enum class ENetFilterStatus : uint32;
	class FNetObjectAttachment;
	enum class ENetObjectDeltaCompressionStatus : unsigned;
	typedef uint32 FNetObjectFilterHandle;
	typedef uint16 FNetObjectGroupHandle;
	typedef uint32 FNetObjectPrioritizerHandle;
	class FNetObjectReference;
	enum class EReplicationCondition : uint32;
	struct FReplicationProtocol;
	class FReplicationSystemFactory;
	struct FReplicationSystemUtil;
	struct FReplicationView;
	class FStringTokenStore;
	class FWorldLocations;
	namespace Private
	{
		class FReplicationSystemImpl;
		class FReplicationSystemInternal;
	}
}

UCLASS(transient)
class UReplicationSystem : public UObject
{
	GENERATED_BODY()

	using FNetHandle = UE::Net::FNetHandle;
	using FNetObjectGroupHandle = UE::Net::FNetObjectGroupHandle;

public:

	struct FReplicationSystemParams
	{
		UReplicationBridge* ReplicationBridge = nullptr;
		uint32 MaxReplicatedObjectCount = 65535U;
		uint32 MaxDeltaCompressedObjectCount = 2048U;
		uint32 MaxNetObjectGroupCount = 2048U;
		bool bIsServer = false;
		bool bAllowObjectReplication = false;
	};

	/** @return The unique ID of the ReplicationSystem. */
	uint32 GetId() const { return Id; }
	
	/** @return The Max number of connections that is supported. */
	IRISCORE_API uint32 GetMaxConnectionCount() const;

	/** @return Whether the system is run on a server. */
	bool IsServer() const { return bIsServer; }

	/**
	 * PreSendUpdate performs all the necessary work, such as filtering and prioritization of objects,
	 * so that each connection will be properly updated with all the information needed in order to replicate.
	 */
	IRISCORE_API void PreSendUpdate(float DeltaSeconds);

	/**
	 * SendUpdate is currently more of a placeholder for a future where the ReplicationSystem itself is responsible for
	 * the low level protocol and sending, rather than having the DataStreamChannel.
	 * @see UDataStreamChannel.
	 */
	IRISCORE_API void SendUpdate();

	/**
	 * Cleanup temporaries and prepare for the next send update.
	 */
	IRISCORE_API void PostSendUpdate();

	/**
	 * Notify that a connection was added.
	 * @param ConnectionId The ID of the added connection. Must not collide with an existing ID. Must be <= the maximum number of connections.
	 */
	IRISCORE_API void AddConnection(uint32 ConnectionId);

	/**
	 * Notify that a connection was removed.
	 * @param ConnectionId The ID of the removed connection. Must have previously been added.
	 */
	IRISCORE_API void RemoveConnection(uint32 ConnectionId);

	/**
	 * Verify if a connection is valid, that is has been added to the system.
	 * @param ConnectionId The ID of the connection to check for validness.
	 * @return Whether the connection is valid.
	 */
	IRISCORE_API bool IsValidConnection(uint32 ConnectionId) const;

	/**
	 * Enable or disable the ReplicationDataStream to transmit data for a particular connection.
	 * @param ConnectionId The ID of the connection to enable or disable object replication for.
	 * @param bReplicationEnabled Whether to enable or disable object replication.
	 */
	IRISCORE_API void SetReplicationEnabledForConnection(uint32 ConnectionId, bool bReplicationEnabled);

	/**
	 * Check whether object replication is enabled for a particular connection.
	 * @param ConnectionId The ID of the connection to check if object replication is enabled for.
	 * @return Whether object replication is enabled for the connection.
	 */
	IRISCORE_API bool IsReplicationEnabledForConnection(uint32 ConnectionId) const;

	// Prioritization

	/**
	 * Set view information for a connection. The views are used by some prioritizers, typically to make
	 * objects closer to any of the view positions higher priority and thus more likely to replicate
	 * on a given frame. The information will persist until the next call. This is normally handled by
	 * a NetDriver.
	 * @param ConnectionId The ID of the connection to set the view information for.
	 * @param View The view information for the connection and its subconnections.
	 * @see UNetDriver::UpdateReplicationViews
	 */
	IRISCORE_API void SetReplicationView(uint32 ConnectionId, const UE::Net::FReplicationView& View);

	/**
	 * Sets a fixed priority for a replicated object which will be used until the next call to SetStaticPriority or SetPrioritizer.
	 * An object which never gets a call to either SetPrioritizer or SetStaticPriority will have a priority of 1.0.
	 * @param Handle Which object to set the priority for.
	 * @param Priority A value >= 0. 1.0 means the object will be considered for replication every frame, if it has updated replicated properties.
	 * @see SetPrioritizer
	 */
	IRISCORE_API void SetStaticPriority(FNetHandle Handle, float Priority);

	/**
	 * Sets a prioritizer for a replicated object which will be used until the next call to SetPrioritizer or SetStaticPriority.
	 * An object which never gets a call to either SetPrioritizer or SetStaticPriority will have a priority of 1.0.
	 * @param Handle A valid handle to an object.
	 * @param PrioritizerHandle A valid handle to a prioritizer, retrieved via a call to GetPrioritizer, or DefaultSpatialNetObjectPrioritizerHandle.
	 * @return true if the prioritizer was successfully set and false if it was not. It can fail for various reasons,
	 * such as the prioritizer not supporting the object in question for implementation defined reasons. If the function fails 
	 * the prioritization of the object is unspecified, it could be using a previous prioritizer or get a default static priority.
	 * @see GetPrioritizerHandle
	 * @see SetStaticPriority
	 */
	IRISCORE_API bool SetPrioritizer(FNetHandle Handle, UE::Net::FNetObjectPrioritizerHandle PrioritizerHandle);


	/**
	 * Gets the handle for a prioritizer with a given name. The handle can be used in subsequent calls to SetPrioritizer.
	 * @param PrioritizerName The name of the prioritizer. Names of valid prioritizers are configured in UNetObjectPrioritizerDefinitions.
	 * @return A valid handle if a prioritizer with the given name has been successfully created in this system or InvalidNetObjectPrioritizerHandle
	 * if not.
	 * @see SetPrioritizer
	 * @see UNetObjectPrioritizerDefinitions
	 */
	IRISCORE_API UE::Net::FNetObjectPrioritizerHandle GetPrioritizerHandle(const FName PrioritizerName) const;


	/**
	 * Gets the prioritizer with a given name. Can be useful for special initialization for a custom prioritizer.
	 * @param PrioritizerName The name of the prioritizer. Names of valid filters are configured in UNetObjectPrioritizerDefinitions.
	 * @return A pointer to the filter if it exists, nullptr if not.
	 * @see UNetObjectPrioritizerDefinitions
	 */
	IRISCORE_API UNetObjectPrioritizer* GetPrioritizer(const FName PrioritizerName) const;
	
	// NetBlob 

	/**
	 * Registers a NetBlobHandler so that its NetBlob type can be sent and received. The user must be sure to keep the handler valid
	 * for the lifetime of the ReplicationSystem. The handler class must have been configured in UNetBlobHandlerDefinitions in order
	 * for the call to succeed. Only one instance per class is allowed to be registered.
	 * @param Handler The UNetBlobHandler to register.
	 * @return Whether the handler was succesfully registered or not.
	 */
	IRISCORE_API bool RegisterNetBlobHandler(UNetBlobHandler* Handler);

	/**
	 * Queue an attachment for replication with an object. The attachment will be sent when the target is scheduled for replication.
	 * @param ConnectionId A valid connection ID. Only this connection will receive the attachment.
	 * @param TargetRef A valid FNetObjectReference representing object to replicate the attachment with.
	 * @param Attachment The attachment to replicate. Note that the attachment's NetObjectReference will be modified based on TargetRef.
	 * @return Whether the attachment was properly queued or not.
	 */
	IRISCORE_API bool QueueNetObjectAttachment(uint32 ConnectionId, const UE::Net::FNetObjectReference& TargetRef, const TRefCountPtr<UE::Net::FNetObjectAttachment>& Attachment);

	/**
	 * Multicast an RPC targeting a object/subobject. 
	 * @param Object A valid Owner/Actor. If no SubObject is specified the function will be called in this instance on the remote side.
	 * @param SubObject Optional SubObject that the function will be called in on the remote side.
	 * @param Function The function to call.
	 * @param Parameters The function parameters.
	 * @return Whether the RPC was successfully queued for replication or not.
	 */
	IRISCORE_API bool SendRPC(const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters);

	/**
	 * Unicast an RPC targeting a object/subobject.
	 * @param ConnectionId A valid connection ID. Only this connection will replicate the RPC.
	 * @param Object A valid Owner/Actor. If no SubObject is specified the function will be called in this instance on the remote side.
	 * @param SubObject Optional SubObject that the function will be called in on the remote side.
	 * @param Function The function to call.
	 * @param Parameters The function parameters.
	 * @return Whether the RPC was successfully queued for replication or not.
	 */
	IRISCORE_API bool SendRPC(uint32 ConnectionId, const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters);

	/** @return The UReplicationBridge that was passed with the system creation parameters. */
	IRISCORE_API UReplicationBridge* GetReplicationBridge() const;

	/** @return The UReplicationBridge that was passed with the system creation parameters. Will return nullptr if it cannot be cast to the desired type. */
	template<typename T>
	T* GetReplicationBridgeAs() const { return Cast<T>(GetReplicationBridge()); }

	/**
	 * @return A const version of the string token store.
	 * @see UE::Net::FStringTokenStore
	 */
	IRISCORE_API const UE::Net::FStringTokenStore* GetStringTokenStore() const;

	/**
	 * @return The string token store.
	 * @see UE::Net::FStringTokenStore
	 */
	IRISCORE_API UE::Net::FStringTokenStore* GetStringTokenStore();

	/**
	 * Check whether a FNetHandle is still associated with a replicated object.
	 * @param Handle The handle check.
	 * @return true if the handle is still valid, false if not. 
	 */
	IRISCORE_API bool IsValidHandle(FNetHandle Handle) const;

	/**
	 * Get the ReplicationProtocol for a handle.
	 * @param Handle The handle to retrieve the protocol for.
	 * @return A valid pointer to the protocol if the handle is valid, nullptr if not.
	 */
	IRISCORE_API const UE::Net::FReplicationProtocol* GetReplicationProtocol(FNetHandle Handle) const;

	// Groups

	/**
	 * Create a group which can be used to logically group objects together. The group must be
	 * destroyed when it's not needed anymore.
	 * @return A handle to the group, or InvalidNetObjectGroupHandle if no more groups could be created.
	 * @see DestroyGroup
	 */
	IRISCORE_API FNetObjectGroupHandle CreateGroup();

	/**
	 * Destroy a group.
	 * @see CreateGroup
	 */
	IRISCORE_API void DestroyGroup(FNetObjectGroupHandle GroupHandle);

	/**
	 * Add an object to a group.
	 * @param GroupHandle A valid group handle.
	 * @param Handle A valid handle to an object.
	 */
	IRISCORE_API void AddToGroup(FNetObjectGroupHandle GroupHandle, FNetHandle Handle);

	/**
	 * Removes an object from a group.
	 * @param GroupHandle A valid group handle.
	 * @param Handle A valid handle to an object.
	 */
	IRISCORE_API void RemoveFromGroup(FNetObjectGroupHandle GroupHandle, FNetHandle Handle);

	/**
	 * Removes an object from all groups it's part of.
	 * @param Handle A valid handle to an object.
	 */
	IRISCORE_API void RemoveFromAllGroups(FNetHandle Handle);

	/**
	 * Check whether an objects belongs to a particular group or not.
	 * @param GroupHandle A valid group handle.
	 * @param Handle A valid handle to an object.
	 * @return true if both handles are valid and the object belongs to the group, false otherwise.
	 */
	IRISCORE_API bool IsInGroup(FNetObjectGroupHandle GroupHandle, FNetHandle Handle) const;

	/**
	 * Check if a group handle is valid.
	 * @param GroupHandle A group handle.
	 * @return true if the group is valid, false if not.
	 */
	IRISCORE_API bool IsValidGroup(FNetObjectGroupHandle GroupHandle) const;

	// Filtering

	/**
	 * Sets the owning connection of an object. This can determine which properties are replicated 
	 * to which connections or affect filtering such that the object is only replicated to the owning connection.
	 * @param Handle A valid handle to an object.
	 * @param ConnectionId A valid connection ID to be set as the owner.
	 */
	IRISCORE_API void SetOwningNetConnection(FNetHandle Handle, uint32 ConnectionId);

	/** 
	 * Get the owning net connection for an object.
	 * @see SetOwningNetConnection
	 */
	IRISCORE_API uint32 GetOwningNetConnection(FNetHandle Handle) const;

	/**
	 * Sets a filter for a replicated object which will be used until the next call to SetFilter or SetConnectionFilter.
	 * Filters are used prevent objects from being replicated to certain connections. An object that is filtered
	 * out will cause the object to be destroyed on the remote side.
	 * @param Handle A valid handle to an object.
	 * @param FilterHandle A valid handle to a filter, retrieved via a call to GetFilter or one of the two special handles
	 * InvalidNetObjectFilterHandle to clear filtering or ToOwnerFilterHandle for owner filtering.
	 * @return true if the filter was successfully set and false if it was not. It can fail for various reasons,
	 * such as the filter not supporting the object in question for implementation defined reasons. If the function fails
	 * the filter of the object is unspecified, it could be using a previous filter or use no filtering.
	 * @see GetFilterHandle
	 * @see SetConnectionFilter
	 */
	IRISCORE_API bool SetFilter(FNetHandle Handle, UE::Net::FNetObjectFilterHandle FilterHandle);

	/**
	 * Gets the handle for a filter with a given name. The handle can be used in subsequent calls to SetFilter.
	 * @param Handle A valid handle to an object.
	 * @param FilterName The name of the filter. Names of valid filters are configured in UNetObjectFilterDefinitions.
	 * @return A valid handle if a filter with the given name has been successfully created in this system or InvalidNetObjectFilterHandle if not.
	 * @see SetFilter
	 * @see UNetObjectFilterDefinitions
	 */
	IRISCORE_API UE::Net::FNetObjectFilterHandle GetFilterHandle(const FName FilterName) const;

	/**
	 * Gets the filter with a given name.
	 * @param FilterName The name of the filter. Names of valid filters are configured in UNetObjectFilterDefinitions.
	 * @return A pointer to the filter if it exists, nullptr if not.
	 * @see UNetObjectFilterDefinitions
	 */
	IRISCORE_API UNetObjectFilter* GetFilter(const FName FilterName) const;

	// Group Filtering

	/**
	 * Add a group to the filtering system. By default the filter disallows replication for all objects in the group.
	 * @param GroupHandle A valid handle to a group.
	 * @see CreateGroup 
	 */
	IRISCORE_API void AddGroupFilter(FNetObjectGroupHandle GroupHandle);

	/** Remove group from filtering system, will cancel effects of the group. */
	IRISCORE_API void RemoveGroupFilter(FNetObjectGroupHandle GroupHandle);

	/** Set status of GroupFilter for specific connection. */
	IRISCORE_API void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, UE::Net::ENetFilterStatus ReplicationStatus);

	/** Set status of GroupFilter for connection marked in the Connections BitArray. */
	IRISCORE_API void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, const UE::Net::FNetBitArray& Connections, UE::Net::ENetFilterStatus ReplicationStatus);

	/** Set status of GroupFilter for all connnections. */
	IRISCORE_API void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, UE::Net::ENetFilterStatus ReplicationStatus);


	// SubObject filtering
	/**
	 * Add a group to the filtering system. By default the filter disallows replication for all objects in the group.
	 * @param GroupHandle A valid handle to a group.
	 * @see CreateGroup
	 */
	IRISCORE_API FNetObjectGroupHandle GetOrCreateSubObjectFilter(FName GroupName);

	/** Returns the FNetObjectGroupHandle used by a named SubObjectFilter */
	IRISCORE_API FNetObjectGroupHandle GetSubObjectFilterGroupHandle(FName GroupName) const;

	/** Set status of GroupFilter for specific connection. */
	IRISCORE_API void SetSubObjectFilterStatus(FName GroupName, uint32 ConnectionId, UE::Net::ENetFilterStatus ReplicationStatus);

	/** Remove group from filtering system, will cancel effects of the group. */
	IRISCORE_API void RemoveSubObjectFilter(FName GroupName);

	/**
	 * Set which connections the object is allowed to be replicated to. This will cancel the effect
	 * of any previous SetFilter or SetConnectionFilter calls on the object.
	 * @param Handle A valid handle to a replicated object.
	 * @param Connections Set bits indicates the connection IDs that the object is allowed or not allowed to be replicated to depending on ReplicationStatus.
	 * @param ReplicationStatus Whether the set bits in Connections indicate if the object is allowed or not allowed to be replicated to those connections.
	 * @return true if the connection filter was properly set, false if not.
	 * @see SetFilter
	 */
	IRISCORE_API bool SetConnectionFilter(FNetHandle Handle, const TBitArray<>& Connections, UE::Net::ENetFilterStatus ReplicationStatus);

	/**
	 * Enable or disable a replication condition for a single connection and do the inverse for all other connections. This will affect
	 * the replication of properties with conditions that are dependent on this condition.
	 * Calling this function will cancel the effect of previous calls to this function with this condition, i.e. only a single
	 * connection can have RoleAutonomous set.
	 * 
	 * @param Handle A valid handle to a replicated object.
	 * @param Condition The ReplicationCondition to modify. Only EReplicationCondition::RoleAutonomous is supported.
	 * @param ConnectionId The ID of the connection to enable or disable the condition for or 0 to disable the condition.
	 * @param bEnable Whether the the condition should be enabled or disabled for the specified connection.
	 * If ConnectionId is zero this parameter is ignored.
	 * @return true if the condition was successfully set, false if not.
	 * @see SetReplicationCondition
	 */
	IRISCORE_API bool SetReplicationConditionConnectionFilter(FNetHandle Handle, UE::Net::EReplicationCondition Condition, uint32 ConnectionId, bool bEnable);

	/**
	 * Enable or disable a replication condition for all connections. This will affect the replication of properties with conditions.
	 *
	 * @param Handle A valid handle to a replicated object.
	 * @param Condition The ReplicationCondition to modify. Only EReplicationCondition::ReplicatePhysics is supported.
	 * @param bEnable Whether the the condition should be enabled or disabled.
	 * @return true if the condition was successfully set, false if not.
	 * @see SetReplicationCondition, SetReplicationConditionConnectionFilter
	 */
	IRISCORE_API bool SetReplicationCondition(FNetHandle Handle, UE::Net::EReplicationCondition Condition, bool bEnable);


	/**
	 * Set whether the object allows delta compression when serializing. This does not guarantee that the object
	 * will use delta compression depending on other factors, such as the maximum number of delta compressed objects,
	 * whether the delta compression feature is enabled or not and other reasons.
	 * @param Handle A valid handle to a replicated object.
	 */
	IRISCORE_API void SetDeltaCompressionStatus(FNetHandle Handle, UE::Net::ENetObjectDeltaCompressionStatus Status);

	 /**
	  * Mark an object as a net temporary. Such objects only replicate its
	  * initial state and ignores future state changes.
	  */
	IRISCORE_API void SetIsNetTemporary(FNetHandle Handle);

	/**
	 * Mark an object to be torn off next update.
	 * @param Handle A valid handle to an object.
	 */
	IRISCORE_API void TearOffNextUpdate(FNetHandle Handle);

	/**
	 * Explicitly mark object as dirty. This will make it considered for replication this frame
	 * if the object has any modified properties/data.
	 * Normally an object is checked for dirtiness based on poll frequency.
	 * @param Handle A valid handle to an object.
	 * @see FObjectReplicationBridgePollConfig
	 */
	IRISCORE_API void MarkDirty(FNetHandle Handle);

	/**
	 * Retrieve the WorldLocations instance which holds world locations for all objects that support it. 
	 * @return The WorldLocations instance.
	 * @see UE::Net::FWorldLocations
	 */
	IRISCORE_API const UE::Net::FWorldLocations& GetWorldLocations() const;

	/* Init data streams for a connection. For internal use by UDataStreamChannel. */
	IRISCORE_API void InitDataStreams(uint32 ConnectionId, UDataStreamManager* DataStreamManager);

	/**
	 * Associate data with a connection. Only a single piece of user data is supported per connection.
	 * The last call determines which data is associated with the connection.
	 * @param ConnectionId A valid connection ID.
	 * @param UserData What data to associate with the connection.
	 * @see GetConnectionUserData
	 */
	IRISCORE_API void SetConnectionUserData(uint32 ConnectionId, UObject* UserData);

	/**
	 * Retrieve the user data associated with a connection.
	 * @param ConnectionId A valid connection ID.
	 * @return The user data associated with the connection.
	 * @see SetConnectionUserData
	 */
	IRISCORE_API UObject* GetConnectionUserData(uint32 ConnectionId) const;

public:
	// For internal use and not exported.

	UE::Net::Private::FReplicationSystemInternal* GetReplicationSystemInternal();
	const UE::Net::Private::FReplicationSystemInternal* GetReplicationSystemInternal() const;

private:
	friend UE::Net::FReplicationSystemFactory;

	UReplicationSystem();
	~UReplicationSystem();

	void Init(uint32 InId, const FReplicationSystemParams& Params);
	void Shutdown();

	// UObject interface
	IRISCORE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	friend UNetDriver;

	IRISCORE_API void ResetGameWorldState();
	IRISCORE_API void NotifyStreamingLevelUnload(const UObject* Level);

private:
	friend UE::Net::FReplicationSystemUtil;
	friend class FIrisCoreModule;
	IRISCORE_API bool SetPropertyCustomCondition(FNetHandle Handle, const void* Owner, uint16 RepIndex, bool bEnable);

	void PostGarbageCollection();
	void CollectGarbage();

	TPimplPtr<UE::Net::Private::FReplicationSystemImpl> Impl;

	FDelegateHandle PostGarbageCollectHandle;

	UPROPERTY(transient)
	TObjectPtr<UReplicationBridge> ReplicationBridge;

	uint32 Id;
	uint32 bIsServer : 1;
	uint32 bAllowObjectReplication : 1;
	uint32 bDoCollectGarbage : 1;
};

namespace UE::Net
{

DECLARE_MULTICAST_DELEGATE_OneParam(FReplicationSystemLifeTime, UReplicationSystem*);

using FReplicationSystemCreatedDelegate = FReplicationSystemLifeTime;
using FReplicationSystemDestroyedDelegate = FReplicationSystemLifeTime;

class FReplicationSystemFactory
{
public:
	/**
	 * Creates a new ReplicationSystem.
	 * @param Params The settings for the ReplicationSystem.
	 * @return A pointer to the newly created ReplicationSystem if it was allowed to created, nullptr if not.
	 */
	IRISCORE_API static UReplicationSystem* CreateReplicationSystem(const UReplicationSystem::FReplicationSystemParams& Params);
	/**
	 * Destroys a ReplicationSystem.
	 * @param System A pointer to the system to destroy. Must have been created with CreateReplicationSystem.
	 * @see CreateReplicationSystem
	 */
	IRISCORE_API static void DestroyReplicationSystem(UReplicationSystem* System);

	/** Static delegate that is triggered just after creating and initializing a new replication system. */
	IRISCORE_API static FReplicationSystemCreatedDelegate& GetReplicationSystemCreatedDelegate();

	/** Static delegate that is triggered before we destroy a replication system. */
	IRISCORE_API static FReplicationSystemDestroyedDelegate& GetReplicationSystemDestroyedDelegate();

	enum ReplicationSystemConstants : uint32
	{
		MaxReplicationSystemCount = 16,
	};

private:

	friend UReplicationSystem* GetReplicationSystem(uint32);

#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	IRISCORE_API static UReplicationSystem* ReplicationSystems[];
#else
	IRISCORE_API static UReplicationSystem* ReplicationSystem;
#endif
};

#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
inline UReplicationSystem* GetReplicationSystem(uint32 Id)
{
	return Id >= FReplicationSystemFactory::MaxReplicationSystemCount ? nullptr : FReplicationSystemFactory::ReplicationSystems[Id];
}
#else
inline UReplicationSystem* GetReplicationSystem(uint32)
{ 
	return FReplicationSystemFactory::ReplicationSystem;
}
#endif

}

