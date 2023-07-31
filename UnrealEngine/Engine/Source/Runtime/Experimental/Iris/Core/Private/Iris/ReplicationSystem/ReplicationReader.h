// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// This class will never be included from public headers
#include "Iris/ReplicationSystem/AttachmentReplication.h"
#include "Iris/ReplicationSystem/ChangeMaskUtil.h"
#include "Iris/ReplicationSystem/NetHandleManager.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/ReplicationSystem/ReplicationTypes.h"
#include "Containers/Map.h"
#include "Misc/MemStack.h"

// Forward declaration
class UReplicationBridge;
namespace UE::Net
{
	class FNetSerializationContext;
	class FNetTokenStoreState;
	class FReplicationStateStorage;
	namespace Private
	{
		class FReplicationSystemInternal;
		class FResolveAndCollectUnresolvedAndResolvedReferenceCollector;
		class FNetBlobHandlerManager;
		class FNetHandleManager;
	}
}

namespace UE::Net::Private
{

enum class ENetObjectAttachmentDispatchFlags : uint32
{
	None = 0,
	Reliable = 1U << 0U,
	Unreliable = Reliable << 1U,
};
ENUM_CLASS_FLAGS(ENetObjectAttachmentDispatchFlags);

/** Deals with Incoming replication Data and dispatch */
class FReplicationReader
{
public:
	FReplicationReader();
	~FReplicationReader();

	// Init
	void Init(const FReplicationParameters& Parameters);
	void Deinit();

	// Read incoming replication data
	void Read(FNetSerializationContext& Context);

	// Read index part of handle
	FNetHandle ReadNetHandleId(FNetBitStreamReader& Reader) const;

	void SetRemoteNetTokenStoreState(FNetTokenStoreState* RemoteTokenStoreState);
	
	// Mark objects pending destroy as unresolvable.
	void UpdateUnresolvableReferenceTracking();

private:
	// ChangeMaskOffset -> FNetHandle
	enum EConstants : uint32
	{
		FakeInitChangeMaskOffset = 0xFFFFFFFFU,
	};
	typedef TMultiMap<uint32, FNetHandle> FObjectReferenceTracker;

	struct FReplicatedObjectInfo
	{
		FReplicatedObjectInfo();

		// We accumulate unresolved changes in this changemask
		FChangeMaskStorageOrPointer UnresolvedChangeMaskOrPointer;

		/* In order to be able to do partial updates the changemask bit is stored with the reference.
		 * That also means a reference can have many entries, but at most one per changemask bit. */
		FObjectReferenceTracker UnresolvedObjectReferences;
		FObjectReferenceTracker ResolvedDynamicObjectReferences;

		// Baselines
		uint8* StoredBaselines[2];

		uint32 InternalIndex;							// InternalIndex
		union
		{
			uint32 Value;
			struct	 
			{
				uint32 ChangeMaskBitCount : 16;					// This is cached to avoid having to look it up in the protocol		
				uint32 bHasUnresolvedReferences : 1;			// Do we have unresolved references in the changemask?
				uint32 bHasUnresolvedInitialReferences : 1;		// Do we have unresolved initial only references
				uint32 bHasAttachments : 1;
				uint32 bDestroy : 1;
				uint32 bTearOff : 1;
				uint32 bIsDeltaCompressionEnabled : 1;
				uint32 LastStoredBaselineIndex : 2;				// Last stored baseline, as soon as we receive data compressed against the baseline we know that we can release older baselines
				uint32 PrevStoredBaselineIndex : 2;				// Previous stored baselines index
				uint32 Padding : 7;
			};
		};
	};

	// Temporary Data to dispatch
	struct FDispatchObjectInfo
	{
		uint32 InternalIndex;
		FChangeMaskStorageOrPointer ChangeMaskOrPointer;
		uint32 bIsInitialState : 1;
		uint32 bHasState : 1;
		uint32 bHasAttachments : 1;
		uint32 bDestroy : 1;
		uint32 bTearOff : 1;
		uint32 bDeferredEndReplication : 1;
	};

	enum : uint32
	{
		ObjectIndexForOOBAttachment = 0U,
		// Try to avoid reallocations for dispatch in the case we need to process a huge object
		ObjectsToDispatchSlackCount = 16U,
	};

	bool IsObjectIndexForOOBAttachment(uint32 InternalIndex) const { return InternalIndex == ObjectIndexForOOBAttachment; }

	// Read index reference to replicated object
	uint32 ReadInternalIndex(FNetBitStreamReader& Reader) const;

	// Read a new or updated object
	void ReadObject(FNetSerializationContext& Context);

	// Read all objects pending destroy
	uint16 ReadObjectsPendingDestroy(FNetSerializationContext& Context);

	// Read state data for all incoming objects
	void ReadObjects(FNetSerializationContext& Context, uint32 ObjectCountToRead);

	// Process a single huge object attachment
	void ProcessHugeObjectAttachment(FNetSerializationContext& Context, const TRefCountPtr<FNetBlob>& Attachment);

	// Assemble and deserialize huge object if present
	void ProcessHugeObject(FNetSerializationContext& Context);

	// Resolve and dispatch unresolved references
	void ResolveAndDispatchUnresolvedReferencesForObject(FNetSerializationContext& Context, uint32 InternalIndex);

	// Resolve and dispatch unresolved references
	void ResolveAndDispatchUnresolvedReferences();

	// Dispatch all data received for the frame, this includes trying to resolve object references
	void DispatchStateData(FNetSerializationContext& Context);

	// Dispatch resolved attachments
	void ResolveAndDispatchAttachments(FNetSerializationContext& Context, FReplicatedObjectInfo* ReplicationInfo, ENetObjectAttachmentDispatchFlags DispatchFlags);

	// End replication for all objects that the server has told us to destroy or tear off
	void DispatchEndReplication(FNetSerializationContext& Context);

	// Create tracking info for the object with the given InternalInfo
	FReplicatedObjectInfo& StartReplication(uint32 InternalIndex);

	// Remove tracking info for the object with InternalIndex
	void EndReplication(uint32 InternalIndex, bool bTearOff, bool bDestroyInstance);

	// Free any data allocated per object
	void CleanupObjectData(FReplicatedObjectInfo& ObjectInfo);

	// Lookup the tracking info for the object with IntnernalIndex
	FReplicatedObjectInfo* GetReplicatedObjectInfo(uint32 InternalIndex);

	// Update reference tracking maps for the current object
	void UpdateObjectReferenceTracking(FReplicatedObjectInfo* ReplicationInfo, FNetBitArrayView ChangeMask, bool bIncludeInitState, const FObjectReferenceTracker& NewUnresolvedReferences, const FObjectReferenceTracker& NewMappedDynamicReferences);

	// Remove all references for object
	void CleanupReferenceTracking(FReplicatedObjectInfo* ObjectInfo);

	// Update ReplicationInfo and OutUnresolvedChangeMask based on data collected by the Collector
	void BuildUnresolvedChangeMaskAndUpdateObjectReferenceTracking(const FResolveAndCollectUnresolvedAndResolvedReferenceCollector& Collector, FNetBitArrayView CollectorChangeMask, FReplicatedObjectInfo* ReplicationInfo, FNetBitArrayView& OutUnresolvedChangeMask);

	void RemoveUnresolvedObjectReferenceInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetHandle Handle);
	void RemoveResolvedObjectReferenceInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetHandle Handle);

	// A previously resolved dynamic reference is now unresolvable. The ReplicationInfo needs to be updated to reflect this.
	// Returns true if the reference was found.
	bool MoveResolvedObjectReferenceToUnresolvedInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetHandle UnresolvableHandle);

	void DeserializeObjectStateDelta(FNetSerializationContext& Context, uint32 InternalIndex, FDispatchObjectInfo& Info, FReplicatedObjectInfo& ObjectInfo, const FNetHandleManager::FReplicatedObjectData& ObjectData, uint32& OutNewBaselineIndex);

private:
	FReplicationParameters Parameters;

	FMemStackBase TempLinearAllocator;
	FMemStackChangeMaskAllocator TempChangeMaskAllocator;

	FGlobalChangeMaskAllocator PersistentChangeMaskAllocator;

	FReplicationSystemInternal* ReplicationSystemInternal;
	FNetHandleManager* NetHandleManager;
	FReplicationStateStorage* StateStorage;
	UReplicationBridge* ReplicationBridge;

	// We track some data about incoming objects
	// Stored in a map for now
	TMap<uint32, FReplicatedObjectInfo> ReplicatedObjects;

	// Temporary data valid during receive
	FDispatchObjectInfo* ObjectsToDispatch;
	uint32 ObjectsToDispatchCount;
	uint32 ObjectsToDispatchCapacity;

	// We need to keep some data around for objects with pending dependencies
	// For now just use array and brute force the updates
	TArray<uint32> ObjectsWithAttachmentPendingResolve;

	// Track all objects waiting for this handle to be resolvable
	TMultiMap<FNetHandle, uint32> UnresolvedHandleToDependents;
	
	// Track all objects with a dynamic handle in case it becomes unresolvable
	TMultiMap<FNetHandle, uint32> ResolvedDynamicHandleToDependents;

	FNetBlobHandlerManager* NetBlobHandlerManager;
	FNetBlobType NetObjectBlobType;
	FNetObjectAttachmentsReader Attachments;
	FObjectReferenceCache* ObjectReferenceCache;
	FNetObjectResolveContext ResolveContext;

	IConsoleVariable const* DelayAttachmentsWithUnresolvedReferences;
	uint32 NumHandlesPendingResolveLastUpdate = 0U;
};

}
