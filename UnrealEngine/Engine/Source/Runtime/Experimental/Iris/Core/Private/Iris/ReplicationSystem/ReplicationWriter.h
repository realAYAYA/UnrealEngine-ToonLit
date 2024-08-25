// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// This class will never be included from public headers
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationTypes.h"
#include "Iris/ReplicationSystem/ReplicationRecord.h"
#include "Iris/DataStream/DataStream.h"
#include "Iris/ReplicationSystem/AttachmentReplication.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlob.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/NetExports.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/Stats/NetStats.h"
#include "Containers/Array.h"
#include "Containers/List.h"
#include "Containers/Set.h"
#include "Misc/EnumClassFlags.h"

// Forward declaration
class UReplicationSystem;
class UNetObjectBlobHandler;
class UPartialNetObjectAttachmentHandler;
class UReplicationBridge;

namespace UE::Net
{
	class FNetBitStreamReader;
	class FNetBitStreamWriter;
	class FNetObjectAttachment;
	class FNetSerializationContext;
	struct FReplicationProtocol;
	
	namespace Private
	{
		struct FChangeMaskCache;
		class FNetRefHandleManager;
		class FReliableNetBlobQueue;
		class FReplicationConditionals;
		class FReplicationFiltering;
		class FReplicationSystemInternal;
		class FDeltaCompressionBaselineManager;
		class FDeltaCompressionBaseline;
	}
}

namespace UE::Net::Private
{

class FReplicationWriter
{
public:
	// Scheduling constants
	static constexpr float CreatePriority = 1.f;
	static constexpr float TearOffPriority = 1.f;
	static constexpr float LostStatePriorityBump = 1.f;
	static constexpr float SchedulingThresholdPriority = 1.f;

	// When scheduling there is no point in scheduling more objects than we can fit in a packet
	static const uint32 PartialSortObjectCount = 128u;

public:

	// State
	enum class EReplicatedObjectState : uint8
	{
		Invalid = 0,

		// Special states
		AttachmentToObjectNotInScope,	// Special state for object index used for sending attachments to remote owned objects
		HugeObject,						// Special state for object index used for sending parts of huge object payloads

		// Normal states
		PendingCreate,					// Not yet created, no data in flight
		WaitOnCreateConfirmation,		// Waiting for confirmation from remote, we can send state data, but if we do we must also include creation info until the object is Created
		Created,						// Confirmed by remote, this is the normal replicating state
		WaitOnFlush,					// Pending flush, we are waiting for all in-flight state data to be acknowledged
		PendingTearOff,					// TearOff should be sent
		SubObjectPendingDestroy,		// SubObject destroy should be sent
		CancelPendingDestroy,			// Destroy was sent but object wants to start replicating again
		PendingDestroy,					// Object is set to be destroyed
		WaitOnDestroyConfirmation,		// Destroy has been sent, waiting for response from client
		Destroyed,						// Confirmed as Destroyed,
		PermanentlyDestroyed,			// DestructionInfo has been confirmed as received

		Count
	};

	static_assert((uint8)(EReplicatedObjectState::Count) <= 32, "EReplicatedObjectState must fit in 5 bits. See FReplicationInfo::State and FReplicationRecord::FRecordInfo::ReplicatedObjectState members.");

	static const TCHAR* LexToString(const EReplicatedObjectState State);

	enum EFlushFlags : uint32
	{
		FlushFlags_None				= 0U,
		FlushFlags_FlushState		= 1U << 0U,											// Make sure that all current state data is acknowledged before we stop replicating the object
		FlushFlags_FlushReliable	= FlushFlags_FlushState << 1U,						// Make sure that all enqueued Reliable RPCs are delivered before we stop replicating the object
		FlushFlags_FlushTornOffSubObjects	= FlushFlags_FlushReliable << 1U,					// Make sure that we flush TearOff and replicated destroy properly
		FlushFlags_All				= FlushFlags_FlushState | FlushFlags_FlushReliable | FlushFlags_FlushTornOffSubObjects,
		FlushFlags_Default			= FlushFlags_FlushReliable,
	};

	// Bookkeeping info required for a replicated object
	// Keep as small as possible since there is one per replicated object per connection
	// Changemask can and will most likely be replaced by a smaller index into a pool to reduce overhead
	struct FReplicationInfo
	{
		inline FReplicationInfo();

		FChangeMaskStorageOrPointer ChangeMaskOrPtr;			// Changemask storage or pointer to storage	
		union 
		{
			uint64 Value;
			struct 
			{
				uint64 ChangeMaskBitCount : 16;							// This is cached to avoid having to look it up in the protocol
				uint64 State : 5;										// Current state
				uint64 HasDirtySubObjects : 1;							// Set if this object has dirty subobjects
				uint64 IsSubObject : 1;									// Set if this object is a subobject
				uint64 HasDirtyChangeMask : 1;							// Set if the ChangeMask might be dirty, if not set the changemask should be zero!
				uint64 HasAttachments : 1;								// Set if there are attachments, such as RPCs, waiting to be sent
				uint64 HasChangemaskFilter : 1;							// Do we need to filter our changemask or not
				uint64 IsDestructionInfo : 1;							// If this is a destruction info
				uint64 IsCreationConfirmed : 1;							// We know that this object has been created on the receiving end
				uint64 TearOff : 1;										// This object should be torn off
				uint64 SubObjectPendingDestroy : 1;						// This object is a subobject that should be destroyed when we replicate owner
				uint64 IsDeltaCompressionEnabled : 1;					// Set to 1 if deltacompression is enabled for this object
				uint64 LastAckedBaselineIndex : 2;						// Last acknowledged baseline index which we can use for deltacompresion
				uint64 PendingBaselineIndex : 2;						// Baseline index pending acknowledgment from client
				uint64 FlushFlags : 3;									// Flags indicating what we are waiting for when flushing
			};
		};

		static const uint32 LocalChangeMaskMaxBitCount = 64u;

		void SetState(EReplicatedObjectState NewState);
		EReplicatedObjectState GetState() const { return (EReplicatedObjectState)State; }

		ChangeMaskStorageType* GetChangeMaskStoragePointer() { return ChangeMaskOrPtr.GetPointer(ChangeMaskBitCount); }
		const ChangeMaskStorageType* GetChangeMaskStoragePointer() const { return ChangeMaskOrPtr.GetPointer(ChangeMaskBitCount); }
	};

	static_assert(sizeof(FReplicationInfo) == 16, "Expected sizeof FReplicationInfo to be 16 bytes");

public:
	~FReplicationWriter();

	// Init
	void Init(const FReplicationParameters& InParameters);

	// Update new or existing/destroyed 
	void UpdateScope(const FNetBitArrayView& ScopedObjects);

	// Force update DirtyChangeMasks and mark objects for flush and/or tearoff depending on flags
	void ForceUpdateDirtyChangeMasks(const FChangeMaskCache& CachedChangeMasks, EFlushFlags ExtraFlushFlags, bool bMarkForTearOff) { InternalUpdateDirtyChangeMasks(CachedChangeMasks, ExtraFlushFlags, bMarkForTearOff); }

	// Called if an object first being teared off and then explicitly destroyed before it has been removed from scope
	void NotifyDestroyedObjectPendingTearOff(FInternalNetRefIndex ObjectInternalIndex);

	// Propagate dirty changemasks
	void UpdateDirtyChangeMasks(const FChangeMaskCache& CachedChangeMasks) { InternalUpdateDirtyChangeMasks(CachedChangeMasks, EFlushFlags::FlushFlags_None, false); }

	// Returns objects that are in need of a priority update. It could be dirty, new or objects in need of resending.
	const FNetBitArray& GetObjectsRequiringPriorityUpdate() const;

	// UpdatedPriorities contains priorities for all objects. Objects in need of a priority update should use the newly calculated priorities.
	void UpdatePriorities(const float* UpdatedPriorities);

	UDataStream::EWriteResult BeginWrite(const UDataStream::FBeginWriteParameters& Params);

	// WriteData to Packet, returns true for now if data was written
	UDataStream::EWriteResult Write(FNetSerializationContext& Context);

	void EndWrite();

	// Deal with processing of lost and delivered data.
	void ProcessDeliveryNotification(EPacketDeliveryStatus PacketDeliveryStatus);

	void SetReplicationEnabled(bool bInReplicationEnabled);
	bool IsReplicationEnabled() const;

	void SetNetExports(FNetExports& InNetExports);

	// Attachments
	// Queue NetObjectAttachments, returns whether the attachments was enqueued or not.
	bool QueueNetObjectAttachments(FInternalNetRefIndex OwnerInternalIndex, FInternalNetRefIndex SubObjectInternalIndex, TArrayView<const TRefCountPtr<FNetBlob>> Attachments, ENetObjectAttachmentSendPolicyFlags SendFlags);

private:
	// Various types

	enum : uint32
	{
		ObjectIndexForOOBAttachment = 0U,
	};

	// Propagate dirty changemasks
	void InternalUpdateDirtyChangeMasks(const FChangeMaskCache& CachedChangeMasks, EFlushFlags ExtraFlushFlags, bool bTearOff);

	struct FScheduleObjectInfo
	{
		uint32 Index;
		float SortKey;
	};

	// We persist some information during write so that we can support writing multiple packets with data without re-doing scheduling work.
	struct FWriteContext
	{
		FWriteContext() : bIsValid(0) {}

		// Objects we have written in this packet batch to avoid sending same object multiple times
		FNetBitArray ObjectsWrittenThisPacket;
		// DependentObjets that we should try to write this packet batch, aka. allow overcommit if we have pending DependentObjects when the packet is full
		TArray<uint32, TInlineAllocator<32>> DependentObjectsPendingSend;
		// Scheduled objects
		FScheduleObjectInfo* ScheduledObjectInfos;
		uint32 ScheduledObjectCount;

		// For performance sake we do partial sorting so we need to track how many object we have sorted.
		uint32 SortedObjectCount;
		// The index into the scheduled objects array to attempt to replicate next.
		uint32 CurrentIndex;

		// Written batch count, which excludes objects pending destroy and subobjects
		uint32 WrittenBatchCount;

		// How many objects that were attempted to be replicated but which ultimately didn't fit in the packet.
		uint32 FailedToWriteSmallObjectCount;

		// How many packets have we written to?
		uint32 NumWrittenPacketsInThisBatch = 0U;
		bool bCanWriteMoreData = false;
		
		EDataStreamWriteMode WriteMode = EDataStreamWriteMode::Full;	
	
		uint32 bHasDestroyedObjectsToSend : 1;
		uint32 bHasUpdatedObjectsToSend : 1;
		uint32 bHasHugeObjectToSend : 1;
		uint32 bHasOOBAttachmentsToSend : 1;
		uint32 bIsValid : 1;

		FNetSendStats Stats;
	};

	struct FBatchObjectInfo
	{
		FNetRefHandle Handle;
		uint32 InternalIndex;
		FNetObjectAttachmentsWriter::FCommitRecord AttachmentRecord;
		ENetObjectAttachmentType AttachmentType;
		bool bHasUnsentAttachments;
		uint32 NewBaselineIndex : 2;
		uint32 bIsInitialState : 1;
		uint32 bSentState : 1;
		uint32 bSentAttachments : 1;
		uint32 bHasDirtySubObjects : 1;
		uint32 bSentTearOff : 1;
		uint32 bSentDestroySubObject : 1;
		uint32 bSentBatchData : 1;
	};

	enum class EBatchInfoType : uint32
	{
		Object,
		HugeObject,
		OOBAttachment,
		Internal,
		// Currently unused as destruction infos don't create a BatchInfo.
		DestructionInfo,
	};

	struct FBatchInfo
	{
		TArray<FBatchObjectInfo, TInlineAllocator<16>> ObjectInfos;
		uint32 ParentInternalIndex;
		EBatchInfoType Type;
	};

	struct FObjectRecord
	{
		FReplicationRecord::FRecordInfo Record;
		FNetObjectAttachmentsWriter::FReliableReplicationRecord AttachmentRecord;
	};

	struct FBatchRecord
	{
		TArray<FObjectRecord, TInlineAllocator<16>> ObjectReplicationRecords;
		uint32 BatchCount = 0U;
	};

	struct FBitStreamInfo
	{
		uint32 ReplicationStartPos;
		uint32 BatchStartPos;
		uint32 ReplicationCapacity;
	};

	enum class EHugeObjectSendStatus : uint32
	{
		Idle,
		Sending,
	};

	struct FHugeObjectContext
	{
		FHugeObjectContext();
		~FHugeObjectContext();

		FInternalNetRefIndex RootObjectInternalIndex = 0;
		FBatchRecord BatchRecord;
		FNetExportContext::FBatchExports BatchExports;

		// The entire payload. When refcount reaches one for all blobs the object has been fully acked.
		TArray<TRefCountPtr<FNetBlob>> Blobs;
	};

	class FHugeObjectSendQueue
	{
	public:
		FHugeObjectSendQueue();
		~FHugeObjectSendQueue();

		// If the queue is full we can't start another send.
		bool IsFull() const;
		bool IsEmpty() const;
		uint32 NumRootObjectsInTransit() const;

		// Enqueue huge object info and return true if it can be sent.
		bool EnqueueHugeObject(const FHugeObjectContext& Context);

		// Returns true if the object is a huge object root object or part of any huge object's payload. The latter is an expensive operation.
		bool IsObjectInQueue(FInternalNetRefIndex ObjectIndex, bool bFullSearch) const;

		// Best effort implementation of getting a valid index for trace.
		FInternalNetRefIndex GetRootObjectInternalIndexForTrace() const;

		// Call AckHugeObject on all objects determined to have been fully processed.
		void AckObjects(TFunctionRef<void (const FHugeObjectContext& Context)> AckHugeObject);

		void FreeContexts(TFunctionRef<void (const FHugeObjectContext& Context)> FreeHugeObject);

	public:
		// Public members
		struct FStats
		{
			// Cycle counter for when the huge object context went from idle to sending.
			uint64 StartSendingTime = 0;
			// Cycle counter for when the last part of huge object was sent.
			uint64 EndSendingTime = 0;
			// Cycle counter for when it was detected that no more parts of the huge object could be sent until some of the first parts have been acked.
			uint64 StartStallTime = 0;
		};

		FStats Stats;

		FNetTraceCollector* TraceCollector = nullptr;
		const FNetDebugName* DebugName = nullptr;

	private:
		TSet<FInternalNetRefIndex> RootObjectsInTransit;
		TDoubleLinkedList<FHugeObjectContext> SendContexts;
	};

	enum EWriteObjectFlag : unsigned
	{
		WriteObjectFlag_State = 1U,
		WriteObjectFlag_Attachments = WriteObjectFlag_State << 1U,
		WriteObjectFlag_HugeObject = WriteObjectFlag_Attachments << 1U,
	};

	enum class EWriteObjectRetryMode : unsigned
	{
		// Stop trying to write more object this frame.
		Abort,
		// Continue with something smaller, it might succeed.
		TrySmallObject,
		// The object is probably huge. Enter special mode for huge objects.
		SplitHugeObject,
	};

	enum class EWriteObjectStatus : unsigned
	{
		Success,

		// The object is in an invalid state and won't be written. This is not considered a failure.
		InvalidState,

		// BitStream overflow.
		BitStreamOverflow,

		// A detached instance, which no longer has an instance protocol. This object cannot be replicated.
		NoInstanceProtocol,

		// A subobject with an invalid owner.
		InvalidOwner,

		// Some error occurred while serializing the object.
		Error,

	};

private:

	uint32 GetDefaultFlushFlags() const;
	uint32 GetFlushStatus(uint32 InternalIndex, const FReplicationInfo& Info, uint32 FlushFlagsToTest = EFlushFlags::FlushFlags_Default) const;
	void SetPendingDestroyOrSubObjectPendingDestroyState(uint32 Index, FReplicationInfo& Info);

	bool IsObjectIndexForOOBAttachment(uint32 InternalIndex) const { return InternalIndex == ObjectIndexForOOBAttachment; }

	void GetInitialChangeMask(ChangeMaskStorageType* ChangeMaskStorage, const FReplicationProtocol* Protocol);

	// Start replication of new object with the specified InternalIndex
	void StartReplication(uint32 InternalIndex);

	// Stop replication object with the specified InternalIndex
	void StopReplication(uint32 InternalIndex);

	// Get ReplicationInfo for specified InternalIndex, prefer to use this method over direct access.
	FReplicationInfo& GetReplicationInfo(uint32 InternalIndex);
	const FReplicationInfo& GetReplicationInfo(uint32 InternalIndex) const;

	// Set the state of a ReplicatedObject, prefer this method to enable logging
	void SetState(uint32 InternalIndex, EReplicatedObjectState NewState);

	// Write index part of handle
	void WriteNetRefHandleId(FNetSerializationContext& Context, FNetRefHandle RefHandle);
		
	// Create new ObjectRecord
	// Note: be aware that it will allocate a copy of the ChangeMask that needs to be handled if the record is not Committed
	void CreateObjectRecord(const FNetBitArrayView* ChangeMask, const FReplicationInfo& Info, const FBatchObjectInfo& ObjectInfo, FObjectRecord& OutRecord);
	
	// Push and link info for written object to ReplicationRecord
	void CommitObjectRecord(uint32 InternalObjectIndex, const FObjectRecord& Record);

	void CommitBatchRecord(const FBatchRecord& BatchRecord);

	void ScheduleDependentObjects(uint32 Index, float ParentPriority, TArray<float>& LocalPriorities, FScheduleObjectInfo* ScheduledObjectIndices, uint32& OutScheduledObjectCount);

	uint32 ScheduleObjects(FScheduleObjectInfo* ScheduledObjectIndices);
	
	// Partial sort of OutScheduledObjectIndices, will sort at most PartialSortObjectCount objects
	uint32 SortScheduledObjects(FScheduleObjectInfo* ScheduledObjectIndices, uint32 ScheduledObjectCount, uint32 StartIndex);

	// Write all objects pending destroy (or as many as we fit in the current packet)
	uint32 WriteObjectsPendingDestroy(FNetSerializationContext& Context);

	// Write object and SubObjects
	EWriteObjectStatus WriteObjectAndSubObjects(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags, FBatchInfo& OutBatchInfo);

	// Write objects recursive
	EWriteObjectStatus WriteObjectInBatch(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags, FBatchInfo& OutBatchInfo);

	// Write destruction info for an object that should be destroyed
	// returns > 1 if Objects was written, 0 if the objects was skipped (failed dependency or waiting for creation confirmation) -1 if the stream is full and we should stop
	int WriteDestructionInfo(FNetSerializationContext& Context, uint32 InternalIndex);

	// Write Object and any subobject(s) to stream as an atomic batch
	// returns > 1 if Objects was written, 0 if the objects was skipped (failed dependency or waiting for creation confirmation) -1 if the stream is full and we should stop
	int WriteObjectBatch(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags);

	int PrepareAndSendHugeObjectPayload(FNetSerializationContext& Context, uint32 InternalIndex);

	// Write OOBAttachments
	uint32 WriteOOBAttachments(FNetSerializationContext& Context);

	// Write as many scheduled objects to stream as we can fit.
	uint32 WriteObjects(FNetSerializationContext& Context);

	// Updates ReplicationInfos, pushes ReplicationRecords etc after a successful call to WriteObjectInBatch() on a top level object
	int HandleObjectBatchSuccess(const FBatchInfo& BatchInfo, FBatchRecord& OutRecord);

	// Determines the best course of action after a WriteObjectBatch() call failed.
	EWriteObjectRetryMode HandleObjectBatchFailure(EWriteObjectStatus WriteObjectStatus, const FBatchInfo& BatchInfo, const FBitStreamInfo& BatchBitStreamInfo);

	// Update logic for dropped RecordInfo
	void HandleDroppedRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord);
	template<EReplicatedObjectState LostState> void HandleDroppedRecord(EReplicatedObjectState CurrentState, const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord);

	// Update logic for delivered RecordInfo
	void HandleDeliveredRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord);

	// Update logic for discarded RecordInfo, for preventing memory leaks on disconnect and shutdown.
	void HandleDiscardedRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord);

	// Setup replication info to be able to send attachments to objects not in scope
	void SetupReplicationInfoForAttachmentsToObjectsNotInScope();

	void ApplyFilterToChangeMask(uint32 ParentInternalIndex, uint32 InternalIndex, FReplicationInfo& Info, const FReplicationProtocol* Protocol, const uint8* InternalStateBuffer, bool bIsInitialState);

	// Patchup changemask to include any in-flight changes. Returns true if in-flight changes were added.
	bool PatchupObjectChangeMaskWithInflightChanges(uint32 InternalIndex, FReplicationInfo& Info);

	// Invalidate all inflight baseline information
	void InvalidateBaseline(uint32 InternalIndex, FReplicationInfo& Info);

	// Returns true if the record chain starting from the provided RecordInfo contains any records with statedata
	// Note: Does not check if it is part of a huge object
	bool HasInFlightStateChanges(uint32 InternalIndex, const FReplicationInfo& Info) const;

	// Returns true if object has pending state changes in flight
	// Note: Does not check if it is part of a huge object
	bool HasInFlightStateChanges(const FReplicationRecord::FRecordInfo* RecordInfo) const;

	// Returns true object and subobjects can be created on remote
	bool CanSendObject(uint32 InternalIndex) const;

	inline bool IsInitialState(const EReplicatedObjectState State) const { return State == EReplicatedObjectState::PendingCreate || State == EReplicatedObjectState::WaitOnCreateConfirmation; }

	bool IsActiveHugeObject(uint32 InternalIndex) const;
	bool IsObjectPartOfActiveHugeObject(uint32 InternalIndex) const;

	bool CanQueueHugeObject() const;

	void FreeHugeObjectSendQueue();

	bool HasDataToSend(const FWriteContext& Context) const;

	void CollectAndAppendExports(FNetSerializationContext& Context, uint8* RESTRICT InternalBuffer, const FReplicationProtocol* Protocol) const;

	bool IsWriteObjectSuccess(EWriteObjectStatus Status) const;

	void SerializeObjectStateDelta(FNetSerializationContext& Context, uint32 InternalIndex, const FReplicationInfo& Info, const FNetRefHandleManager::FReplicatedObjectData& ObjectData, const uint8* RESTRICT ReplicatedObjectStateBuffer, FDeltaCompressionBaseline& CurrentBaseline, uint32 CreatedBaselineIndex);

	void DiscardAllRecords();
	void StopAllReplication();

	void MarkObjectDirty(FInternalNetRefIndex InternalIndex, const char* Caller);

	/* Grow internal buffers to support a given index. */
	void OnLargestIndexIncrease(uint32 InternalIndex);

private:
	// Replication parameters
	FReplicationParameters Parameters;

	// Record of all in-flight data
	FReplicationRecord ReplicationRecord;

	// Tracking information for the state of all objects
	TArray<FReplicationInfo> ReplicatedObjects;

	// Tracking information linking all in-flight data per object
	TArray<FReplicationRecord::FRecordInfoList> ReplicatedObjectsRecordInfoLists;

	// Each replicated object has a scheduling priority that is bumped every time we have a chance to send and zeroed out every time the object is successfully sent
	TArray<float> SchedulingPriorities;

	// Track Objects Pending Destroy?
	FNetBitArray ObjectsPendingDestroy;

	// Objects in this bitArray with dirty change masks
	FNetBitArray ObjectsWithDirtyChanges;

	// Track Objects That is in scope for this connection
	FNetBitArray ObjectsInScope;
	
	// Handles logic for all attachments to objects.
	FNetObjectAttachmentsWriter Attachments;

	FDelegateHandle OnLargestIndexIncreaseHandle;

	// Cached internal systems
	FReplicationSystemInternal* ReplicationSystemInternal = nullptr;
	FNetRefHandleManager* NetRefHandleManager = nullptr;
	UReplicationBridge* ReplicationBridge = nullptr;
	FDeltaCompressionBaselineManager* BaselineManager = nullptr;
	FObjectReferenceCache* ObjectReferenceCache = nullptr;
	const FReplicationFiltering* ReplicationFiltering = nullptr;
	FReplicationConditionals* ReplicationConditionals = nullptr;
	const UPartialNetObjectAttachmentHandler* PartialNetObjectAttachmentHandler = nullptr;
	const UNetObjectBlobHandler* NetObjectBlobHandler = nullptr;
	FNetExports* NetExports = nullptr;
	FNetTypeStats* NetTypeStats = nullptr;

	FWriteContext WriteContext;
	FBitStreamInfo WriteBitStreamInfo;
	FHugeObjectSendQueue HugeObjectSendQueue;

	// Is replication enabled?
	bool bReplicationEnabled = false;
	
	// Should we use high prio create?
	const bool bHighPrioCreate = false;
};

inline FReplicationWriter::FReplicationInfo::FReplicationInfo()
: Value(0U)
{
}

template<FReplicationWriter::EReplicatedObjectState LostState> void FReplicationWriter::HandleDroppedRecord(FReplicationWriter::EReplicatedObjectState CurrentState, const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationWriter::FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	//static_assert(false, "Expected specialization to exist.");
}

}
