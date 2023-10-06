// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationReader.h"
#include "Iris/IrisConfigInternal.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/IrisDebugging.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Trace/NetDebugName.h"
#include "Iris/DataStream/DataStream.h"
#include "Iris/ReplicationSystem/DeltaCompression/DeltaCompressionBaselineManager.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/DequantizeAndApplyHelper.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetBlob/NetObjectBlobHandler.h"
#include "Iris/ReplicationSystem/NetBlob/PartialNetObjectAttachmentHandler.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializer.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Class.h"

#if UE_NET_ENABLE_REPLICATIONREADER_LOG
#	define UE_LOG_REPLICATIONREADER(Format, ...)  UE_LOG(LogIris, Log, Format, ##__VA_ARGS__)
#	define UE_LOG_REPLICATIONREADER_CONN(Format, ...)  UE_LOG(LogIris, Log, TEXT("Conn: %u ") Format, Parameters.ConnectionId, ##__VA_ARGS__)
#else
#	define UE_LOG_REPLICATIONREADER(...)
#	define UE_LOG_REPLICATIONWRITER_CONN(Format, ...)
#endif

#define UE_LOG_REPLICATIONREADER_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)
#define UE_LOG_REPLICATIONREADER_CONN_WARNING(Format, ...) UE_LOG(LogIris, Warning, TEXT("Conn: %u ") Format, Parameters.ConnectionId, ##__VA_ARGS__)
#define UE_LOG_REPLICATIONREADER_ERROR(Format, ...)  UE_LOG(LogIris, Error, Format, ##__VA_ARGS__)

namespace UE::Net::Private
{

static bool bExecuteReliableRPCsBeforeOnReps = false;
static FAutoConsoleVariableRef CVarExecuteReliableRPCsBeforeOnReps(
		TEXT("net.Iris.ExecuteReliableRPCsBeforeOnReps"),
		bExecuteReliableRPCsBeforeOnReps,
		TEXT("If true and Iris runs in backwards compatibility mode then reliable RPCs will be executed before OnReps on the target object. Default is false."
		));

static bool bDeferEndReplication = true;
static FAutoConsoleVariableRef CVarDeferEndReplication(
	TEXT("net.Iris.DeferEndReplication"),
	bDeferEndReplication,
	TEXT("bDeferEndReplication if true calls to EndReplication will be defered until after we have applied statedata. Default is true."
	));

static const FName NetError_FailedToFindAttachmentQueue("Failed to find attachment queue");

class FResolveAndCollectUnresolvedAndResolvedReferenceCollector
{
public:
	typedef FNetReferenceCollector::FReferenceInfoArray FReferenceInfoArray;

	void CollectReferences(FObjectReferenceCache& ObjectReferenceCache,
		const FNetObjectResolveContext& ResolveContext,
		bool bInIncludeInitState, 
		const FNetBitArrayView* ChangeMask,
		uint8* RESTRICT InternalBuffer, const FReplicationProtocol* Protocol)
	{
		bIncludeInitState = bInIncludeInitState;

		// Setup context
		FNetSerializationContext Context;
		Context.SetChangeMask(ChangeMask);
		Context.SetIsInitState(bInIncludeInitState);

		FNetReferenceCollector Collector;
		FReplicationProtocolOperationsInternal::CollectReferences(Context, Collector, InternalBuffer, Protocol);

		// Iterate over result and process results
		for (const FNetReferenceCollector::FReferenceInfo& Info : MakeArrayView(Collector.GetCollectedReferences()))
		{
			if (!ObjectReferenceCache.ResolveObjectReference(Info.Reference, ResolveContext))
			{
				UnresolvedReferenceInfos.Add(Info);
			}
			else
			{
				ResolvedReferenceInfos.Add(Info);
			}		
		}		
	}

	void Reset()
	{ 
		UnresolvedReferenceInfos.Reset();
		ResolvedReferenceInfos.Reset();
	}

	const FReferenceInfoArray& GetResolvedReferences() const { return ResolvedReferenceInfos; }
	const FReferenceInfoArray& GetUnresolvedReferences() const { return UnresolvedReferenceInfos; }
	bool IsInitStateIncluded() const { return bIncludeInitState; }

private:
	FReferenceInfoArray UnresolvedReferenceInfos;
	FReferenceInfoArray ResolvedReferenceInfos;
	bool bIncludeInitState = false;
};

// Helper class to deal with management of ObjectsToDispatch allocations from our temporary allocator
class FReplicationReader::FObjectsToDispatchArray
{
public:

	FObjectsToDispatchArray(uint32 InitialCapacity, FMemStackBase& Allocator)
	: ObjectsToDispatchCount(0U)
	, Capacity(InitialCapacity + ObjectsToDispatchSlackCount)
	{			
		ObjectsToDispatch = new (Allocator) FDispatchObjectInfo[Capacity];
	}

	void Grow(uint32 Count, FMemStackBase& Allocator)
	{
		if (Capacity < (ObjectsToDispatchCount + Count))
		{
			Capacity = ObjectsToDispatchCount + Count + ObjectsToDispatchSlackCount;
			FDispatchObjectInfo* NewObjectsToDispatch = new (Allocator) FDispatchObjectInfo[Capacity];

			// If we already had allocated data we need to copy the old elements
			if (ObjectsToDispatchCount)
			{
				FPlatformMemory::Memcpy(NewObjectsToDispatch, ObjectsToDispatch, ObjectsToDispatchCount*sizeof(FDispatchObjectInfo));
			}
			ObjectsToDispatch = NewObjectsToDispatch;
		}		
	}

	FDispatchObjectInfo& AddPendingDispatchObjectInfo(FMemStackBase& Allocator)
	{
		Grow(1, Allocator);
		ObjectsToDispatch[ObjectsToDispatchCount] = FDispatchObjectInfo();

		return ObjectsToDispatch[ObjectsToDispatchCount];
	}

	void CommitPendingDispatchObjectInfo()
	{
		checkSlow(ObjectsToDispatchCount < Capacity);
		++ObjectsToDispatchCount;
	}

	uint32 Num() const { return ObjectsToDispatchCount; }
	TArrayView<FDispatchObjectInfo> GetObjectsToDispatch() { return MakeArrayView(ObjectsToDispatch, ObjectsToDispatchCount); }

private:

	FDispatchObjectInfo* ObjectsToDispatch;
	uint32 ObjectsToDispatchCount;
	uint32 Capacity;		
};

FReplicationReader::FReplicatedObjectInfo::FReplicatedObjectInfo()
: InternalIndex(FNetRefHandleManager::InvalidInternalIndex)
, Value(0U)
{
	FMemory::Memzero(StoredBaselines);
	LastStoredBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	PrevStoredBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
}

FReplicationReader::FReplicationReader()
: TempLinearAllocator()
, TempChangeMaskAllocator(&TempLinearAllocator)
, ReplicationSystemInternal(nullptr)
, NetRefHandleManager(nullptr)
, StateStorage(nullptr)
, ObjectsToDispatchArray(nullptr)
, NetBlobHandlerManager(nullptr)
, NetObjectBlobType(InvalidNetBlobType)
, DelayAttachmentsWithUnresolvedReferences(IConsoleManager::Get().FindConsoleVariable(TEXT("net.DelayUnmappedRPCs"), false /* bTrackFrequentCalls */))
{
}

FReplicationReader::~FReplicationReader()
{
	Deinit();
}

void FReplicationReader::Init(const FReplicationParameters& InParameters)
{
	// Store copy of parameters
	Parameters = InParameters;

	ResolveContext.ConnectionId = InParameters.ConnectionId;

	// Cache internal systems
	ReplicationSystemInternal = Parameters.ReplicationSystem->GetReplicationSystemInternal();
	NetRefHandleManager = &ReplicationSystemInternal->GetNetRefHandleManager();
	StateStorage = &ReplicationSystemInternal->GetReplicationStateStorage();
	NetBlobHandlerManager = &ReplicationSystemInternal->GetNetBlobHandlerManager();
	ObjectReferenceCache = &ReplicationSystemInternal->GetObjectReferenceCache();
	ReplicationBridge = Parameters.ReplicationSystem->GetReplicationBridge();

	// Find out if there's a PartialNetObjectAttachmentHandler so we can re-assemble split blobs
	if (const UPartialNetObjectAttachmentHandler* Handler = ReplicationSystemInternal->GetNetBlobManager().GetPartialNetObjectAttachmentHandler())
	{
		FNetObjectAttachmentsReaderInitParams InitParams;
		InitParams.PartialNetObjectAttachmentHandler = Handler;
		Attachments.Init(InitParams);
	}

	if (const UNetBlobHandler* Handler = ReplicationSystemInternal->GetNetBlobManager().GetNetObjectBlobHandler())
	{
		NetObjectBlobType = Handler->GetNetBlobType();
	}

	// reserve index 0
	StartReplication(0);
}

void FReplicationReader::Deinit()
{
	for (FPendingBatchData& PendingBatchData : PendingBatches)
	{
		UE_LOG_REPLICATIONREADER_WARNING(TEXT("FReplicationReader::Deinit NetHandle %s has %d unprocessed data batches"), *PendingBatchData.Handle.ToString(), PendingBatchData.QueuedDataChunks.Num());

		// Make sure to release all references that we are holding on to
		if (ObjectReferenceCache)
		{
			for (const FNetRefHandle& RefHandle : PendingBatchData.ResolvedReferences)
			{
				ObjectReferenceCache->RemoveTrackedQueuedBatchObjectReference(RefHandle);
			}
		}		
	}

	// Cleanup any allocation stored in the per object info
	for (auto& ObjectIt : ReplicatedObjects)
	{
		CleanupObjectData(ObjectIt.Value);
	}
	ReplicatedObjects.Empty();
}

// Read incomplete handle
FNetRefHandle FReplicationReader::ReadNetRefHandleId(FNetSerializationContext& Context, FNetBitStreamReader& Reader) const
{
	UE_NET_TRACE_NAMED_OBJECT_SCOPE(ReferenceScope, FNetRefHandle(), *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Verbose);

	const uint64 NetId = ReadPackedUint64(&Reader);
	FNetRefHandle RefHandle = FNetRefHandleManager::MakeNetRefHandleFromId(NetId);

	UE_NET_TRACE_SET_SCOPE_OBJECTID(ReferenceScope, RefHandle);

	if (RefHandle.GetId() != NetId)
	{
		Context.SetError(GNetError_InvalidNetHandle);
		return FNetRefHandle();
	}

	return RefHandle;
}
	
uint32 FReplicationReader::ReadObjectsPendingDestroy(FNetSerializationContext& Context)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	UE_NET_TRACE_SCOPE(ObjectsPendingDestroy, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Read how many destroyed objects we have
	const uint32 ObjectsToRead = Reader.ReadBits(16);
	
	if (!Context.HasErrorOrOverflow())
	{
		const bool bHasPendingBatches = !PendingBatches.IsEmpty();
	
		for (uint32 It = 0; It < ObjectsToRead; ++It)
		{
			UE_NET_TRACE_NAMED_OBJECT_SCOPE(DestroyedObjectScope, FNetRefHandle(), Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

			FNetRefHandle IncompleteHandle = ReadNetRefHandleId(Context, Reader);
			FNetRefHandle SubObjectRootOrHandle = IncompleteHandle;
			if (Reader.ReadBool())
			{
				SubObjectRootOrHandle = ReadNetRefHandleId(Context, Reader);
			}
			const bool bShouldDestroyInstance = Reader.ReadBool();
			if (Context.HasErrorOrOverflow())
			{
				break;
			}

			if (FPendingBatchData* PendingBatchData = bHasPendingBatches ? PendingBatches.FindByPredicate([&SubObjectRootOrHandle](const FPendingBatchData& Entry) { return Entry.Handle == SubObjectRootOrHandle; }) : nullptr)
			{
				EnqueueEndReplication(PendingBatchData, bShouldDestroyInstance, IncompleteHandle);
				continue;
			}

			{
				// Resolve handle and destroy using bridge
				const uint32 InternalIndex = NetRefHandleManager->GetInternalIndex(IncompleteHandle);
				if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex)
				{
					UE_NET_TRACE_SET_SCOPE_OBJECTID(DestroyedObjectScope, IncompleteHandle);

					// Defer EndReplication until after applying state data
					if (bDeferEndReplication)
					{
						FDispatchObjectInfo& Info = ObjectsToDispatchArray->AddPendingDispatchObjectInfo(TempLinearAllocator);

						Info.bDestroy = bShouldDestroyInstance;
						Info.bTearOff = false;
						Info.bDeferredEndReplication = true;
						Info.InternalIndex = InternalIndex;
						Info.bIsInitialState = 0U;
						Info.bHasState = 0U;
						Info.bHasAttachments = 0U;

						// Mark for dispatch
						ObjectsToDispatchArray->CommitPendingDispatchObjectInfo();
					}
					else
					{
						EndReplication(InternalIndex, false, bShouldDestroyInstance);
					}
				}
				else
				{
					// If we did not find the object or associated bridge, the packet that would have created the object may have been lost.
					UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::Read Tried to destroy object with %s (This can occur if the server sends destroy for an object that has not yet been confirmed as created)"), *IncompleteHandle.ToString());
				}
			}
		}
	}

	return ObjectsToRead;
}

FReplicationReader::FReplicatedObjectInfo& FReplicationReader::StartReplication(uint32 InternalIndex)
{
	check(!ReplicatedObjects.Contains(InternalIndex));

	// Create ReadObjectInfo
	FReplicatedObjectInfo& ObjectInfo = ReplicatedObjects.Emplace(InternalIndex);
	ObjectInfo = FReplicatedObjectInfo();
	ObjectInfo.InternalIndex = InternalIndex;

	// Allocate changemask (if needed)
	if (InternalIndex != 0U)
	{		
		const FNetRefHandleManager::FReplicatedObjectData& Data = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
		ObjectInfo.ChangeMaskBitCount = Data.Protocol->ChangeMaskBitCount;

		// Alloc and init changemask
		FNetBitArrayView ChangeMask = FChangeMaskStorageOrPointer::AllocAndInitBitArray(ObjectInfo.UnresolvedChangeMaskOrPointer, ObjectInfo.ChangeMaskBitCount, PersistentChangeMaskAllocator);
	}

	return ObjectInfo;
}

FReplicationReader::FReplicatedObjectInfo* FReplicationReader::GetReplicatedObjectInfo(uint32 InternalIndex)
{
	return ReplicatedObjects.Find(InternalIndex);
}

void FReplicationReader::CleanupObjectData(FReplicatedObjectInfo& ObjectInfo)
{
	// Remove from pending resolve
	if (ObjectInfo.InternalIndex != 0U)
	{
		FChangeMaskStorageOrPointer::Free(ObjectInfo.UnresolvedChangeMaskOrPointer, ObjectInfo.ChangeMaskBitCount, PersistentChangeMaskAllocator);
	}

	// Release stored baselines
	if (ObjectInfo.LastStoredBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		StateStorage->FreeBaseline(ObjectInfo.InternalIndex, ObjectInfo.StoredBaselines[ObjectInfo.LastStoredBaselineIndex]);
	}
	if (ObjectInfo.PrevStoredBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		StateStorage->FreeBaseline(ObjectInfo.InternalIndex, ObjectInfo.StoredBaselines[ObjectInfo.PrevStoredBaselineIndex]);
	}
}

void FReplicationReader::EndReplication(uint32 InternalIndex, bool bTearOff, bool bDestroyInstance)
{
	if (FReplicatedObjectInfo* ObjectInfo = ReplicatedObjects.Find(InternalIndex))
	{
		const FNetRefHandleManager::FReplicatedObjectData& Data = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);

		CleanupReferenceTracking(ObjectInfo);
		Attachments.DropAllAttachments(ENetObjectAttachmentType::Normal, InternalIndex);

		EReplicationBridgeDestroyInstanceReason DestroyReason = EReplicationBridgeDestroyInstanceReason::DoNotDestroy;
		if (bTearOff)
		{
			DestroyReason = EReplicationBridgeDestroyInstanceReason::TearOff;
		}
		else if (bDestroyInstance)
		{
			DestroyReason = EReplicationBridgeDestroyInstanceReason::Destroy;
		}
		EReplicationBridgeDestroyInstanceFlags DestroyFlags = (Data.bAllowDestroyInstanceFromRemote ? EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote : EReplicationBridgeDestroyInstanceFlags::None);
		ReplicationBridge->DestroyNetObjectFromRemote(Data.RefHandle, DestroyReason, DestroyFlags);

		CleanupObjectData(*ObjectInfo);

		ReplicatedObjects.Remove(InternalIndex);
	}
}

void FReplicationReader::DeserializeObjectStateDelta(FNetSerializationContext& Context, uint32 InternalIndex, FDispatchObjectInfo& Info, FReplicatedObjectInfo& ObjectInfo, const FNetRefHandleManager::FReplicatedObjectData& ObjectData, uint32& OutNewBaselineIndex)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	uint32 BaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	BaselineIndex = Reader.ReadBits(FDeltaCompressionBaselineManager::BaselineIndexBitCount);
	if (BaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{ 
		const bool bIsNewBaseline = Reader.ReadBool();

		if (Reader.IsOverflown())
		{
			UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::DeserializeObjectStateDelta Bitstream corrupted."));
			return;
		}

		if (bIsNewBaseline)
		{
			OutNewBaselineIndex = (BaselineIndex + 1) % FDeltaCompressionBaselineManager::MaxBaselineCount;
		}

		// If we are compressing against the LastStoredBaselineIndex we can release older baselines to reduce memory overhead
		if (!bIsNewBaseline && BaselineIndex == ObjectInfo.LastStoredBaselineIndex && ObjectInfo.PrevStoredBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
		{
			StateStorage->FreeBaseline(InternalIndex, ObjectInfo.StoredBaselines[ObjectInfo.PrevStoredBaselineIndex]);
			ObjectInfo.StoredBaselines[ObjectInfo.PrevStoredBaselineIndex] = nullptr;
			ObjectInfo.PrevStoredBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
		}

		check(ObjectInfo.StoredBaselines[BaselineIndex]);

		UE_NET_TRACE_SCOPE(DeltaCompressed, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		FReplicationProtocolOperations::DeserializeWithMaskDelta(Context, Info.ChangeMaskOrPointer.GetPointer(ObjectInfo.ChangeMaskBitCount), ObjectData.ReceiveStateBuffer, ObjectInfo.StoredBaselines[BaselineIndex], ObjectData.Protocol);
	}
	else
	{
		const uint32 NewBaselineIndex = Reader.ReadBits(FDeltaCompressionBaselineManager::BaselineIndexBitCount);
		if (Reader.IsOverflown())
		{
			UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::DeserializeObjectStateDelta Bitstream corrupted."));
			return;
		}
		OutNewBaselineIndex = NewBaselineIndex;
		FReplicationProtocolOperations::DeserializeWithMask(Context, Info.ChangeMaskOrPointer.GetPointer(ObjectInfo.ChangeMaskBitCount), ObjectData.ReceiveStateBuffer, ObjectData.Protocol);
	}
}

FReplicationReader::FPendingBatchData* FReplicationReader::UpdateUnresolvedMustBeMappedReferences(FNetRefHandle InHandle, TArray<FNetRefHandle>& MustBeMappedReferences)
{
	FPendingBatchData* PendingBatch = PendingBatches.FindByPredicate([&InHandle](const FPendingBatchData& Entry) { return Entry.Handle == InHandle; });
	// If we already have a pending batch we append any new must be mapped references to it.
	if (PendingBatch)
	{
		for (const FNetRefHandle Ref : PendingBatch->PendingMustBeMappedReferences)
		{
			MustBeMappedReferences.AddUnique(Ref);
		}
	}

	// Resolve
	TArray<FNetRefHandle, TInlineAllocator<4>> Unresolved;
	TArray<TPair<FNetRefHandle, UObject*>, TInlineAllocator<4>> QueuedObjectsToTrack;

	Unresolved.Reserve(MustBeMappedReferences.Num());
	QueuedObjectsToTrack.SetNum(MustBeMappedReferences.Num());
	
	for (FNetRefHandle Handle : MustBeMappedReferences)
	{
		UObject* ResolvedObject = nullptr;
		// TODO: Report broken status in same call to avoid map lookup
		ENetObjectReferenceResolveResult ResolveResult = ObjectReferenceCache->ResolveObjectReference(FObjectReferenceCache::MakeNetObjectReference(Handle), ResolveContext, ResolvedObject);
		if (EnumHasAnyFlags(ResolveResult, ENetObjectReferenceResolveResult::HasUnresolvedMustBeMappedReferences) && !ObjectReferenceCache->IsNetRefHandleBroken(Handle, true))
		{
			Unresolved.Add(Handle);
		}
		else if (ResolveResult == ENetObjectReferenceResolveResult::None)
		{
			QueuedObjectsToTrack.Emplace(Handle, ResolvedObject);
		}
	}

	if (Unresolved.Num())
	{
		FPendingBatchData* Batch = PendingBatch;
		// We must create a new batch
		if (!Batch)
		{
			Batch = &PendingBatches.AddDefaulted_GetRef();
			Batch->Handle = InHandle;
		}

		// Update
		Batch->PendingMustBeMappedReferences = MoveTemp(Unresolved);
	
		// If we resolved more references, add them to tracking list
		for (TPair<FNetRefHandle, UObject*>& NetRefHandleObjectPair : QueuedObjectsToTrack)
		{
			if (!Batch->ResolvedReferences.Contains(NetRefHandleObjectPair.Key))
			{
				Batch->ResolvedReferences.Add(NetRefHandleObjectPair.Key);
				ObjectReferenceCache->AddTrackedQueuedBatchObjectReference(NetRefHandleObjectPair.Key, NetRefHandleObjectPair.Value);
			}
		}

		return Batch;
	}
	else if (PendingBatch)
	{
		PendingBatch->PendingMustBeMappedReferences.Reset();

		// If we resolved more references, add them to tracking list
		for (TPair<FNetRefHandle, UObject*>& NetRefHandleObjectPair : QueuedObjectsToTrack)
		{
			if (!PendingBatch->ResolvedReferences.Contains(NetRefHandleObjectPair.Key))
			{
				PendingBatch->ResolvedReferences.Add(NetRefHandleObjectPair.Key);
				ObjectReferenceCache->AddTrackedQueuedBatchObjectReference(NetRefHandleObjectPair.Key, NetRefHandleObjectPair.Value);
			}
		}
		
		return PendingBatch;
	}
	
	return nullptr;
}

uint32 FReplicationReader::ReadObjectsInBatch(FNetSerializationContext& Context, FNetRefHandle IncompleteHandle, bool bHasBatchOwnerData, uint32 BatchEndBitPosition)
{
	uint32 ReadObjectCount = 0;
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	// If the batch owner had state, we read it now
	if (bHasBatchOwnerData)
	{
		ReadObjectInBatch(Context, IncompleteHandle, false);
		if (Context.HasErrorOrOverflow())
		{
			return 0U;
		}
		++ReadObjectCount;
	}

	ensureAlways(Reader.GetPosBits() <= BatchEndBitPosition);

	// ReadSubObjects 
	while (Reader.GetPosBits() < BatchEndBitPosition)
	{
		ReadObjectInBatch(Context, IncompleteHandle, true);
		if (Context.HasErrorOrOverflow())
		{
			return 0U;
		}
		++ReadObjectCount;
	}

	return ReadObjectCount;
}

uint32 FReplicationReader::ReadObjectBatch(FNetSerializationContext& Context)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	UE_NET_TRACE_SCOPE(Batch, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Special handling for destruction infos
	if (const bool bIsDestructionInfo = Reader.ReadBool())
	{
		FReplicationBridgeSerializationContext BridgeContext(Context, Parameters.ConnectionId, true);

		// For destruction infos we inline the exports
		FForceInlineExportScope ForceInlineExportScope(Context.GetInternalContext());
		ReplicationBridge->ReadAndExecuteDestructionInfoFromRemote(BridgeContext);

#if UE_NET_USE_READER_WRITER_SENTINEL
		ReadAndVerifySentinelBits(&Reader, TEXT("DestructionInfo"), 8);
#endif
		
		return 1U;
	}

#if UE_NET_USE_READER_WRITER_SENTINEL
	ReadAndVerifySentinelBits(&Reader, TEXT("ReadObject"), 8);
#endif

	uint32 ObjectsReadInBatch = 0U;
	
	// A batch starts with (RefHandleId | BatchSize | bHasBatchObjectData | bHasExports)
	// If the batch has exports we must to seek to the end of the batch to read and process exports before reading/processing batch data
	const FNetRefHandle IncompleteHandle = ReadNetRefHandleId(Context, Reader);

	uint32 BatchSize = 0U;
	// Read Batch size
	{
		const uint32 NumBitsUsedForBatchSize = Parameters.NumBitsUsedForBatchSize;

		UE_NET_TRACE_SCOPE(BatchSize, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		BatchSize = Reader.ReadBits(NumBitsUsedForBatchSize);
	}

	if (Context.HasErrorOrOverflow() || BatchSize > Reader.GetBitsLeft())
	{
		Context.SetError(GNetError_InvalidValue);
		return 0U;
	}

	// This either marks the end of the data associated with this batch or the offset in the stream where exports are stored.
	const uint32 BatchEndOrStartOfExportsPos = Reader.GetPosBits() + BatchSize;

	// Do we have state data or attachments for the owner of the batch?
	const bool bHasBatchOwnerData = Reader.ReadBool();

	// Do we have exports or not?
	const bool bHasExports = Reader.ReadBool();
	
	uint32 ReadObjectCount = 0U;

	// First we need to read exports, they are stored at the end of the batch
	uint32 BatchEndPos = BatchEndOrStartOfExportsPos;

	TempMustBeMappedReferences.Reset();
	if (bHasExports)
	{
		const uint32 ReturnPos = Reader.GetPosBits();

		// Seek to the export section
		Reader.Seek(BatchEndPos);

		// Read exports and any must be mapped references
		ObjectReferenceCache->ReadExports(Context, &TempMustBeMappedReferences);
		if (Context.HasErrorOrOverflow())
		{
			return 0U;
		}

		// Update BatchEndPos if we successfully read exports
		BatchEndPos = Reader.GetPosBits();

		// Seek back to state data
		Reader.Seek(ReturnPos);
	}

	// This object has pending must be mapped references that must be resolved before we can process the data.
	FPendingBatchData* PendingBatchData = ObjectReferenceCache->ShouldAsyncLoad() ? UpdateUnresolvedMustBeMappedReferences(IncompleteHandle, TempMustBeMappedReferences) : nullptr;
	if (PendingBatchData)
	{
		UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::ReadObjectBatch Handle %s will be defered as it has unresolved must be mapped references"), *IncompleteHandle.ToString());				

		UE_NET_TRACE_OBJECT_SCOPE(IncompleteHandle, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		UE_NET_TRACE_SCOPE(QueuedBatch, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

		// Enqueue BatchData
		FQueuedDataChunk DataChunk;
		
		const uint32 NumDataBits = BatchEndOrStartOfExportsPos - Reader.GetPosBits();
		const uint32 NumDataWords = (NumDataBits + 31U) / 32U;

		DataChunk.NumBits = NumDataBits;
		DataChunk.StorageOffset = PendingBatchData->DataChunkStorage.Num();
		DataChunk.bHasBatchOwnerData = bHasBatchOwnerData;
		DataChunk.bIsEndReplicationChunk = false;

		// Make sure we have space
		PendingBatchData->DataChunkStorage.AddUninitialized(NumDataWords);
	
		// Store batch data
		Reader.ReadBitStream(PendingBatchData->DataChunkStorage.GetData() + DataChunk.StorageOffset, DataChunk.NumBits);

		if (Context.HasErrorOrOverflow())
		{
			return 0U;
		}

		PendingBatchData->QueuedDataChunks.Add(MoveTemp(DataChunk));
	}
	else
	{
		ReadObjectCount = ReadObjectsInBatch(Context, IncompleteHandle, bHasBatchOwnerData, BatchEndOrStartOfExportsPos);

		if (Context.HasErrorOrOverflow())
		{
			return 0U;
		}
	}

	// Skip to the end as we already have read any exports
	Reader.Seek(BatchEndPos);

	return ReadObjectCount;
}

void FReplicationReader::ReadObjectInBatch(FNetSerializationContext& Context, FNetRefHandle BatchHandle, bool bIsSubObject)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	// If we are reading 
	FNetRefHandle IncompleteHandle = BatchHandle;
	if (bIsSubObject)
	{
		IncompleteHandle = ReadNetRefHandleId(Context, Reader);
	}
	
	// Read replicated destroy header if necessary. We don't know the internal index yet so can't do the more appropriate check IsObjectIndexForOOBAttachment.
	const bool bReadReplicatedDestroyHeader = IncompleteHandle.IsValid();
	const uint32 ReplicatedDestroyHeaderFlags = bReadReplicatedDestroyHeader ? Reader.ReadBits(ReplicatedDestroyHeaderFlags_BitCount) : ReplicatedDestroyHeaderFlags_None;

	const bool bHasState = Reader.ReadBool();
	if (bHasState)
	{
#if UE_NET_USE_READER_WRITER_SENTINEL
		ReadAndVerifySentinelBits(&Reader, TEXT("HasState"), 8);
#endif
	}

	uint32 NewBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;

	const bool bIsInitialState = bHasState && Reader.ReadBool();
	uint32 InternalIndex = ObjectIndexForOOBAttachment;

	UE_NET_TRACE_OBJECT_SCOPE(IncompleteHandle, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	//UE_LOG_REPLICATIONREADER(TEXT("FReplicationReader::Read Object with %s InitialState: %u"), *IncompleteHandle.ToString(), bIsInitialState ? 1u : 0u);

	bool bHasErrors = false;

	// Read creation data
	Context.SetIsInitState(bIsInitialState);
	if (bIsInitialState)
	{
		UE_NET_TRACE_SCOPE(CreationInfo, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

		// SubObject data for initial state
		FNetRefHandle SubObjectOwnerHandle;
		if (bIsSubObject)
		{
			// The owner is the same as the Batch owner
			const FNetRefHandle IncompleteOwnerHandle = BatchHandle;
				
			FInternalNetRefIndex SubObjectOwnerInternalIndex = NetRefHandleManager->GetInternalIndex(IncompleteOwnerHandle);
			if (Reader.IsOverflown() || SubObjectOwnerInternalIndex == FNetRefHandleManager::InvalidInternalIndex)
			{
				UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::ReadObject Invalid subobjectowner handle. %s"), ToCStr(IncompleteOwnerHandle.ToString()));
				const FName& NetError = (Reader.IsOverflown() ? GNetError_BitStreamOverflow : GNetError_InvalidNetHandle);
				Context.SetError(NetError);
				return;			
			}

			SubObjectOwnerHandle = NetRefHandleManager->GetReplicatedObjectDataNoCheck(SubObjectOwnerInternalIndex).RefHandle;
		}

		const bool bIsDeltaCompressed = Reader.ReadBool();
		if (bIsDeltaCompressed)
		{
			UE_LOG_REPLICATIONREADER(TEXT("DeltaCompression is enabled for Handle %s"), *IncompleteHandle.ToString());
			NewBaselineIndex = Reader.ReadBits(FDeltaCompressionBaselineManager::BaselineIndexBitCount);
		}
		
		// We got a read error
		if (Reader.IsOverflown() || !IncompleteHandle.IsValid())
		{
			UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::ReadObject Bitstream corrupted."));
			const FName& NetError = (Reader.IsOverflown() ? GNetError_BitStreamOverflow : GNetError_BitStreamError);
			Context.SetError(NetError);
			return;
		}
	
		// Get Bridge
		FReplicationBridgeSerializationContext BridgeContext(Context, Parameters.ConnectionId);

		const FReplicationBridgeCreateNetRefHandleResult CreateResult = ReplicationBridge->CallCreateNetRefHandleFromRemote(SubObjectOwnerHandle, IncompleteHandle, BridgeContext);
		FNetRefHandle NetRefHandle = CreateResult.NetRefHandle;
		if (!NetRefHandle.IsValid())
		{	
			UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::ReadObject Unable to create handle for %s."), *IncompleteHandle.ToString());
			Context.SetError(GNetError_InvalidNetHandle);
			bHasErrors = true;
			goto ErrorHandling;
		}

		InternalIndex = NetRefHandleManager->GetInternalIndex(NetRefHandle);
		FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
		ObjectData.bAllowDestroyInstanceFromRemote = EnumHasAnyFlags(CreateResult.Flags, EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote);

		FReplicatedObjectInfo& ObjectInfo = StartReplication(InternalIndex);

		ObjectInfo.bIsDeltaCompressionEnabled = bIsDeltaCompressed;
	}
	else
	{
		bHasErrors = bHasErrors || Context.HasErrorOrOverflow();
		if (bHasErrors || !IncompleteHandle.IsValid())
		{
			InternalIndex = ObjectIndexForOOBAttachment;
		}
		else
		{
			// If we get back an invalid internal index then either the object has been deleted or there's bitstream corruption.
			InternalIndex = NetRefHandleManager->GetInternalIndex(IncompleteHandle);

			// If this is a subobject that is being destroyed this was no error as we send destroy info for unconfirmed objects
			if ((ReplicatedDestroyHeaderFlags & ReplicatedDestroyHeaderFlags_EndReplication) == 0U)
			{
				bHasErrors = InternalIndex == FNetRefHandleManager::InvalidInternalIndex;
			}
		}
	}

	if (bHasErrors)
	{
		Context.SetError(GNetError_InvalidNetHandle);
		goto ErrorHandling;
	}

	// Read state data and attachments
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);

		// Add entry in our received data as we postpone state application until we have received all data in order to be able to properly resolve references
		FDispatchObjectInfo& Info = ObjectsToDispatchArray->AddPendingDispatchObjectInfo(TempLinearAllocator);

		// Update info based on ReplicatedDestroyHeader
		Info.bDestroy = !!(ReplicatedDestroyHeaderFlags & (ReplicatedDestroyHeaderFlags_TearOff | ReplicatedDestroyHeaderFlags_DestroyInstance));
		Info.bTearOff = !!(ReplicatedDestroyHeaderFlags & ReplicatedDestroyHeaderFlags_TearOff);
		Info.bDeferredEndReplication = !!(ReplicatedDestroyHeaderFlags & (ReplicatedDestroyHeaderFlags_TearOff | ReplicatedDestroyHeaderFlags_EndReplication));

		if (bHasState)
		{
			bHasErrors = IsObjectIndexForOOBAttachment(InternalIndex);
			if (bHasErrors)
			{
				UE_LOG_REPLICATIONREADER_WARNING(TEXT("FReplicationReader::ReadObject Bitstream corrupted. Getting state when only expecting RPCs."));
				Context.SetError(GNetError_BitStreamError);
				goto ErrorHandling;
			}

			FReplicatedObjectInfo* ObjectInfo = GetReplicatedObjectInfo(InternalIndex);
			checkSlow(ObjectInfo);

			const uint32 ChangeMaskBitCount = ObjectData.Protocol->ChangeMaskBitCount;

			// Temporary changemask
			FChangeMaskStorageOrPointer::Alloc(Info.ChangeMaskOrPointer, ChangeMaskBitCount, TempChangeMaskAllocator);

			if (bIsInitialState)
			{
				FReplicationProtocolOperations::DeserializeInitialStateWithMask(Context, Info.ChangeMaskOrPointer.GetPointer(ChangeMaskBitCount), ObjectData.ReceiveStateBuffer, ObjectData.Protocol);
			}
			else
			{
				if (ObjectInfo->bIsDeltaCompressionEnabled)
				{
					DeserializeObjectStateDelta(Context, InternalIndex, Info, *ObjectInfo, ObjectData, NewBaselineIndex);
				}
				else
				{
					FReplicationProtocolOperations::DeserializeWithMask(Context, Info.ChangeMaskOrPointer.GetPointer(ChangeMaskBitCount), ObjectData.ReceiveStateBuffer, ObjectData.Protocol);
				}
			}
		#if UE_NET_USE_READER_WRITER_SENTINEL
				ReadAndVerifySentinelBits(&Reader, TEXT("HasStateEnd"), 8);
		#endif

			// Should we store a new baseline?
			if (NewBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
			{
				// This object uses delta compression, store the last received state as a baseline with the specified index
				UE_LOG_REPLICATIONREADER(TEXT("Storing new baselineindex: %u for (:%u) Handle %s"), NewBaselineIndex, InternalIndex, *ObjectData.RefHandle.ToString());

				check(NewBaselineIndex < FDeltaCompressionBaselineManager::MaxBaselineCount);
				if (ObjectInfo->StoredBaselines[NewBaselineIndex])
				{
					// Clone into already allocated state, unfortunately we have to free dynamic state
					FReplicationProtocolOperations::FreeDynamicState(Context, ObjectInfo->StoredBaselines[NewBaselineIndex], ObjectData.Protocol);
					FReplicationProtocolOperationsInternal::CloneQuantizedState(Context, ObjectInfo->StoredBaselines[NewBaselineIndex], ObjectData.ReceiveStateBuffer, ObjectData.Protocol);
				}
				else
				{
					// Allocate new baseline and initialize from current RecvState
					ObjectInfo->StoredBaselines[NewBaselineIndex] = StateStorage->AllocBaseline(InternalIndex, EReplicationStateType::CurrentRecvState);
				}
	
				// Make sure that PrevStoredBaselineIndex is not set to the same as the NewBaselineIndex
				const uint32 OldPrevStoredBaselineIndex = ObjectInfo->PrevStoredBaselineIndex;
				const uint32 NewPrevStoredBaselineIndex = NewBaselineIndex != ObjectInfo->LastStoredBaselineIndex ? ObjectInfo->LastStoredBaselineIndex : FDeltaCompressionBaselineManager::InvalidBaselineIndex;
				ObjectInfo->PrevStoredBaselineIndex = NewPrevStoredBaselineIndex;
				if (NewPrevStoredBaselineIndex == FDeltaCompressionBaselineManager::InvalidBaselineIndex && NewPrevStoredBaselineIndex != OldPrevStoredBaselineIndex)
				{
					uint8* PrevBaseline = ObjectInfo->StoredBaselines[OldPrevStoredBaselineIndex];
					ObjectInfo->StoredBaselines[OldPrevStoredBaselineIndex] = nullptr;
					StateStorage->FreeBaseline(InternalIndex, PrevBaseline);
				}
				ObjectInfo->LastStoredBaselineIndex = NewBaselineIndex;
			}
		}

		const bool bHasAttachments = Reader.ReadBool();
		ENetObjectAttachmentType AttachmentType = ENetObjectAttachmentType::Normal;
		if (bHasAttachments)
		{
			if (IsObjectIndexForOOBAttachment(InternalIndex))
			{
				const bool bIsHugeObject = Reader.ReadBool();
				AttachmentType = (bIsHugeObject ? ENetObjectAttachmentType::HugeObject : ENetObjectAttachmentType::OutOfBand);
				bHasErrors = bHasErrors || (!Parameters.bAllowReceivingAttachmentsFromRemoteObjectsNotInScope && AttachmentType == ENetObjectAttachmentType::OutOfBand);
				if (bHasErrors)
				{
					Context.SetError(GNetError_InvalidNetHandle);
					goto ErrorHandling;
				}
			}

			Attachments.Deserialize(Context, AttachmentType, InternalIndex, ObjectData.RefHandle);
		}

		bHasErrors = bHasErrors || Context.HasErrorOrOverflow();
		if (bHasErrors)
		{
			goto ErrorHandling;
		}

		// Fill in ReadObjectInfo, we skip HugeObjects as they are not added to the dispatch list until they are fully assembled
		if (AttachmentType != ENetObjectAttachmentType::HugeObject)
		{
			Info.InternalIndex = InternalIndex;
			Info.bIsInitialState = bIsInitialState ? 1U : 0U;
			Info.bHasState = bHasState ? 1U : 0U;
			Info.bHasAttachments = bHasAttachments ? 1U : 0U;

			ObjectsToDispatchArray->CommitPendingDispatchObjectInfo();
		}
	}
	
ErrorHandling:
	if (bHasErrors)
	{
		UE_LOG_REPLICATIONREADER_ERROR(TEXT("FReplicationReader::ReadObject Failed to read replicated object with %s. Error '%s'."), *IncompleteHandle.ToString(), (Context.HasError() ? ToCStr(Context.GetError().ToString()) : TEXT("BitStream Overflow")));
	}
}

// Update reference tracking maps for the current object. It is assumed the ObjectReferenceTracker do no include duplicates for a given key.
void FReplicationReader::UpdateObjectReferenceTracking(FReplicatedObjectInfo* ReplicationInfo, FNetBitArrayView ChangeMask, bool bIncludeInitState, const FObjectReferenceTracker& NewUnresolvedReferences, const FObjectReferenceTracker& NewMappedDynamicReferences)
{
	IRIS_PROFILER_SCOPE(FReplicationReader_UpdateObjectReferenceTracking)

	/*
	 * As we store references per changemask we need to construct a set of all unresolved references and
	 * compare with the new set of unresolved references. The new set is found by first updating the 
	 * references that were found in the changemask.
	 */
	{
		// Try to avoid dynamic allocations during the update of the UnresolvedObjectReferences.
		ReplicationInfo->UnresolvedObjectReferences.Reserve(ReplicationInfo->UnresolvedObjectReferences.Num() + NewUnresolvedReferences.Num());

		TSet<FNetRefHandle> OldUnresolvedSet;
		OldUnresolvedSet.Reserve(ReplicationInfo->UnresolvedObjectReferences.Num());
		for (const FObjectReferenceTracker::ElementType& Element : ReplicationInfo->UnresolvedObjectReferences)
		{
			OldUnresolvedSet.Add(Element.Value);
		}

		// Replace each entry in UnresolvedObjectReferences for the given changemask
		auto UpdateUnresolvedReferencesForChange = [ReplicationInfo, &NewUnresolvedReferences](uint32 ChangeBit)
		{
			FObjectReferenceTracker& UnresolvedObjectReferences = ReplicationInfo->UnresolvedObjectReferences;

			// Use the Hash variants of the map methods to provide a tiny speedup.
			const uint32 KeyHash = ::GetTypeHash(ChangeBit);
			UnresolvedObjectReferences.RemoveByHash(KeyHash, ChangeBit);

			for (auto It = NewUnresolvedReferences.CreateConstKeyIterator(ChangeBit); It; ++It)
			{
				const FNetRefHandle RefHandle = It.Value();
				UnresolvedObjectReferences.AddByHash(KeyHash, ChangeBit, RefHandle);
			}
		};

		ChangeMask.ForAllSetBits(UpdateUnresolvedReferencesForChange);
		if (bIncludeInitState)
		{
			UpdateUnresolvedReferencesForChange(FakeInitChangeMaskOffset);
		}

		// The unresolved set is now updated with the current status of unresolved references.
		TSet<FNetRefHandle> NewUnresolvedSet;
		NewUnresolvedSet.Reserve(NewUnresolvedReferences.Num());
		for (const FObjectReferenceTracker::ElementType& Element : ReplicationInfo->UnresolvedObjectReferences)
		{
			NewUnresolvedSet.Add(Element.Value);
		}

		// Update ReplicationInfo with the status of unresolved references
		ReplicationInfo->bHasUnresolvedReferences = NewUnresolvedSet.Num() > 0;
		ReplicationInfo->bHasUnresolvedInitialReferences = ReplicationInfo->UnresolvedObjectReferences.Find(FakeInitChangeMaskOffset) != nullptr;

		// Remove resolved or no longer existing references
		const uint32 OwnerInternalIndex = ReplicationInfo->InternalIndex;
		for (FNetRefHandle Handle : OldUnresolvedSet)
		{
			if (!NewUnresolvedSet.Contains(Handle))
			{
				// Remove from tracking
				UnresolvedHandleToDependents.RemoveSingle(Handle, OwnerInternalIndex);
				UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::UpdateObjectReferenceTracking Removing unresolved reference %s for %s"), ToCStr(Handle.ToString()), ToCStr(NetRefHandleManager->GetNetRefHandleFromInternalIndex(OwnerInternalIndex).ToString()));
			}
		}

		// Add new unresolved references
		for (FNetRefHandle Handle : NewUnresolvedSet)
		{
			if (!OldUnresolvedSet.Contains(Handle))
			{
				// Add to tracking
				UnresolvedHandleToDependents.Add(Handle, OwnerInternalIndex);
				UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::UpdateObjectReferenceTracking Adding unresolved reference %s for %s"), ToCStr(Handle.ToString()), ToCStr(NetRefHandleManager->GetNetRefHandleFromInternalIndex(OwnerInternalIndex).ToString()));
			}
		}
	}

	// Update tracking for resolved dynamic references
#if 0
	{
		// Try to avoid dynamic allocations during the update of the ResolvedDynamicObjectReferences.
		ReplicationInfo->ResolvedDynamicObjectReferences.Reserve(ReplicationInfo->ResolvedDynamicObjectReferences.Num() + NewMappedDynamicReferences.Num());

		TSet<FNetRefHandle> OldResolvedSet;
		OldResolvedSet.Reserve(ReplicationInfo->ResolvedDynamicObjectReferences.Num());
		for (const FObjectReferenceTracker::ElementType& Element : ReplicationInfo->ResolvedDynamicObjectReferences)
		{
			OldResolvedSet.Add(Element.Value);
		}

		// Replace each entry in ResolvedDynamicObjectReferences for the given changemask
		auto UpdateResolvedReferencesForChange = [ReplicationInfo, &NewMappedDynamicReferences](uint32 ChangeBit)
		{
			FObjectReferenceTracker& ResolvedDynamicObjectReferences = ReplicationInfo->ResolvedDynamicObjectReferences;

			const uint32 KeyHash = ::GetTypeHash(ChangeBit);
			ResolvedDynamicObjectReferences.RemoveByHash(KeyHash, ChangeBit);

			for (auto It = NewMappedDynamicReferences.CreateConstKeyIterator(ChangeBit); It; ++It)
			{
				const FNetRefHandle RefHandle = It.Value();
				ResolvedDynamicObjectReferences.AddByHash(KeyHash, ChangeBit, RefHandle);
			}
		};

		ChangeMask.ForAllSetBits(UpdateResolvedReferencesForChange);
		// Intentionally leaving out init state. It seems weird to call rep notifies and update init only properties after
		// the initial state has already been applied.

		// The resolved set is now updated with the current status of resolved references.
		TSet<FNetRefHandle> NewResolvedSet;
		NewResolvedSet.Reserve(ReplicationInfo->ResolvedDynamicObjectReferences.Num());
		for (const FObjectReferenceTracker::ElementType& Element : ReplicationInfo->ResolvedDynamicObjectReferences)
		{
			NewResolvedSet.Add(Element.Value);
		}

		// Remove now unresolved or no longer existing references
		const uint32 OwnerInternalIndex = ReplicationInfo->InternalIndex;
		for (FNetRefHandle Handle : OldResolvedSet)
		{
			if (!NewResolvedSet.Contains(Handle))
			{
				// Remove from tracking
				ResolvedDynamicHandleToDependents.RemoveSingle(Handle, OwnerInternalIndex);
				UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::UpdateObjectReferenceTracking Removing resolved dynamic reference %s for %s"), ToCStr(Handle.ToString()), ToCStr(NetRefHandleManager->GetNetHandleFromInternalIndex(OwnerInternalIndex).ToString()));
			}
		}

		// Add new resolved dynamic references
		for (FNetRefHandle Handle : NewResolvedSet)
		{
			if (Handle.IsDynamic() && !OldResolvedSet.Contains(Handle))
			{
				// Add to tracking
				ResolvedDynamicHandleToDependents.Add(Handle, OwnerInternalIndex);
				UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::UpdateObjectReferenceTracking Adding resolved dynamic reference %s for %s"), ToCStr(Handle.ToString()), ToCStr(NetRefHandleManager->GetNetHandleFromInternalIndex(OwnerInternalIndex).ToString()));
			}
		}
	}
#endif
}

void FReplicationReader::RemoveUnresolvedObjectReferenceInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetRefHandle Handle)
{
	for (FObjectReferenceTracker::TIterator It = ReplicationInfo->UnresolvedObjectReferences.CreateIterator(); It; ++It)
	{
		const FNetRefHandle RefHandle = It->Value;
		if (RefHandle == Handle)
		{
			It.RemoveCurrent();
		}
	}
}

void FReplicationReader::RemoveResolvedObjectReferenceInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetRefHandle Handle)
{
	for (FObjectReferenceTracker::TIterator It = ReplicationInfo->ResolvedDynamicObjectReferences.CreateIterator(); It; ++It)
	{
		const FNetRefHandle RefHandle = It->Value;
		if (RefHandle == Handle)
		{
			It.RemoveCurrent();
		}
	}
}

bool FReplicationReader::MoveResolvedObjectReferenceToUnresolvedInReplicationInfo(FReplicatedObjectInfo* ReplicationInfo, FNetRefHandle UnresolvableHandle)
{
	bool bFoundHandle = false;
	bool bHasUnresolvedReferences = ReplicationInfo->bHasUnresolvedReferences;
	bool bHasUnresolvedInitialReferences = ReplicationInfo->bHasUnresolvedInitialReferences;
	FNetBitArrayView UnresolvedChangeMask = FChangeMaskUtil::MakeChangeMask(ReplicationInfo->UnresolvedChangeMaskOrPointer, ReplicationInfo->ChangeMaskBitCount);
	FObjectReferenceTracker& UnresolvedObjectReferences = ReplicationInfo->UnresolvedObjectReferences;
	for (FObjectReferenceTracker::TIterator It = ReplicationInfo->ResolvedDynamicObjectReferences.CreateIterator(); It; ++It)
	{
		const FNetRefHandle RefHandle = It->Value;
		if (RefHandle == UnresolvableHandle)
		{
			bFoundHandle = true;

			const uint32 ChangemaskOffset = It->Key;
			if (ChangemaskOffset == FakeInitChangeMaskOffset)
			{
				bHasUnresolvedInitialReferences = true;
			}
			else
			{
				bHasUnresolvedReferences = true;
				UnresolvedChangeMask.SetBit(ChangemaskOffset);
			}

			// At this point we'd like to skip iteration to the next key as a handle can only be found once per changemask.
			It.RemoveCurrent();

			// This handle should only have existed once in the ResolvedDynamicObjectReferences map and should not be able to
			// already exist in the UnresolvedObjectReferences map, so no need to call AddUnique.
			UnresolvedObjectReferences.Add(ChangemaskOffset, UnresolvableHandle);

			UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::MoveResolvedObjectReferenceToUnresolvedInReplicationInfo Moving from resolved to unresolved reference %s for %s"), ToCStr(UnresolvableHandle.ToString()), ToCStr(NetRefHandleManager->GetNetRefHandleFromInternalIndex(ReplicationInfo->InternalIndex).ToString()));
		}
	}

	ReplicationInfo->bHasUnresolvedInitialReferences = bHasUnresolvedInitialReferences;
	ReplicationInfo->bHasUnresolvedReferences = bHasUnresolvedReferences;

	return bFoundHandle;
}

// Remove all references for object
void FReplicationReader::CleanupReferenceTracking(FReplicatedObjectInfo* ObjectInfo)
{
	const uint32 ObjectIndex = ObjectInfo->InternalIndex;

	// Remove from unresolved references
	for (FObjectReferenceTracker::ElementType Element : ObjectInfo->UnresolvedObjectReferences)
	{
		// Remove from tracking
		FNetRefHandle Handle = Element.Value;
		UnresolvedHandleToDependents.RemoveSingle(Handle, ObjectIndex);
		UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::CleanupReferenceTracking Removing unresolved reference %s for %s"), *Handle.ToString(), *(NetRefHandleManager->GetNetRefHandleFromInternalIndex(ObjectIndex).ToString()));
	}
	ObjectInfo->UnresolvedObjectReferences.Reset();

	// Remove from resolved dynamic references
	for (FObjectReferenceTracker::ElementType Element : ObjectInfo->ResolvedDynamicObjectReferences)
	{
		// Remove from tracking
		const FNetRefHandle Handle = Element.Value;
		ResolvedDynamicHandleToDependents.RemoveSingle(Handle, ObjectIndex);
		UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::CleanupReferenceTracking Removing resolved dynamic reference %s for %s"), *Handle.ToString(), *(NetRefHandleManager->GetNetRefHandleFromInternalIndex(ObjectIndex).ToString()));
	}
	ObjectInfo->ResolvedDynamicObjectReferences.Reset();

	// Remove from attachment resolve
	ObjectsWithAttachmentPendingResolve.Remove(ObjectIndex);
}

void FReplicationReader::BuildUnresolvedChangeMaskAndUpdateObjectReferenceTracking(const FResolveAndCollectUnresolvedAndResolvedReferenceCollector& Collector, FNetBitArrayView CollectorChangeMask, FReplicatedObjectInfo* ReplicationInfo, FNetBitArrayView& OutUnresolvedChangeMask)
{
	OutUnresolvedChangeMask.Reset();
	bool bHasUnresolvedInitReferences = false;

	FObjectReferenceTracker UnresolvedReferences;
	for (const auto& RefInfo : Collector.GetUnresolvedReferences())
	{
		const FNetSerializerChangeMaskParam& ChangeMaskInfo = RefInfo.ChangeMaskInfo;
		if (ChangeMaskInfo.BitCount)
		{
			OutUnresolvedChangeMask.SetBit(ChangeMaskInfo.BitOffset);
		}
		else
		{
			bHasUnresolvedInitReferences = true;
		}

		const uint32 BitOffset = (ChangeMaskInfo.BitCount > 0U ? ChangeMaskInfo.BitOffset : FakeInitChangeMaskOffset);
		UnresolvedReferences.AddUnique(BitOffset, RefInfo.Reference.GetRefHandle());
	}

	FObjectReferenceTracker MappedDynamicReferences;
	for (const auto& RefInfo : Collector.GetResolvedReferences())
	{
		if (RefInfo.Reference.GetRefHandle().IsDynamic())
		{
			const uint32 BitOffset = (RefInfo.ChangeMaskInfo.BitCount > 0U ? RefInfo.ChangeMaskInfo.BitOffset : FakeInitChangeMaskOffset);
			MappedDynamicReferences.AddUnique(BitOffset, RefInfo.Reference.GetRefHandle());
		}
	}

	// Update object specific 
	UpdateObjectReferenceTracking(ReplicationInfo, CollectorChangeMask, Collector.IsInitStateIncluded(), UnresolvedReferences, MappedDynamicReferences);
}

void FReplicationReader::ResolveAndDispatchUnresolvedReferencesForObject(FNetSerializationContext& Context, uint32 InternalIndex)
{
	IRIS_PROFILER_SCOPE(FReplicationReader_ResolveAndDispatchUnresolvedReferencesForObject);

	FReplicatedObjectInfo* ReplicationInfo = GetReplicatedObjectInfo(InternalIndex);

	const bool bObjectHasAttachments = ReplicationInfo->bHasAttachments;
	const bool bObjectHasReferences = ReplicationInfo->bHasUnresolvedInitialReferences | ReplicationInfo->bHasUnresolvedReferences;

	ENetObjectAttachmentDispatchFlags AttachmentDispatchedFlags = ENetObjectAttachmentDispatchFlags::None;
	if (bObjectHasReferences)
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ReplicationInfo->InternalIndex);
		const uint32 ChangeMaskBitCount = ReplicationInfo->ChangeMaskBitCount;
		
		// Try to resolve references and collect unresolved references
		const bool bOldHasUnresolvedInitReferences = ReplicationInfo->bHasUnresolvedInitialReferences;

		FNetBitArrayView UnresolvedChangeMask = FChangeMaskUtil::MakeChangeMask(ReplicationInfo->UnresolvedChangeMaskOrPointer, ChangeMaskBitCount);
	
		// Need a temporary changemask for the unresolved changes due to UnresolvedChangeMask being written to by BuildUnresolvedChangeMaskAndUpdateObjectReferenceTracking
		FChangeMaskStorageOrPointer TempChangeMaskOrPointer;
		FNetBitArrayView TempChangeMask = FChangeMaskStorageOrPointer::AllocAndInitBitArray(TempChangeMaskOrPointer, ChangeMaskBitCount, TempChangeMaskAllocator);
		FNetBitArrayView TempUnresolvedChangeMask = TempChangeMask;
		TempUnresolvedChangeMask.Copy(UnresolvedChangeMask);

		// Try to resolve references and collect resolved and references still pending resolve
		FResolveAndCollectUnresolvedAndResolvedReferenceCollector Collector;
		Collector.CollectReferences(*ObjectReferenceCache, ResolveContext, ReplicationInfo->bHasUnresolvedInitialReferences, &UnresolvedChangeMask, ObjectData.ReceiveStateBuffer, ObjectData.Protocol);

		// Build UnresolvedChangeMask from collected data and update replication info
		BuildUnresolvedChangeMaskAndUpdateObjectReferenceTracking(Collector, TempUnresolvedChangeMask, ReplicationInfo, UnresolvedChangeMask);

		// Repurpose temp changemask for members that has resolved references.
		TempChangeMask.Reset();
		FNetBitArrayView ResolvedChangeMask = TempChangeMask;

		bool bHasResolvedInitReferences = false;
		for (const auto& RefInfo : Collector.GetResolvedReferences())
		{
			const FNetSerializerChangeMaskParam& ChangeMaskInfo = RefInfo.ChangeMaskInfo;
			if (ChangeMaskInfo.BitCount)
			{
				ResolvedChangeMask.SetBit(ChangeMaskInfo.BitOffset);
			}
			else
			{
				// If we had old unresolved init dependencies we need to include the init state when we update references
				bHasResolvedInitReferences = bOldHasUnresolvedInitReferences;
			}
		}

		if (ObjectData.InstanceProtocol)
		{
			// Apply resolved references, this is a blunt tool as we currently push out full dirty properties rather than only the resolved references
			if (ResolvedChangeMask.IsAnyBitSet() || bHasResolvedInitReferences)
			{
				if (bObjectHasAttachments && bExecuteReliableRPCsBeforeOnReps && !bHasResolvedInitReferences)
				{
					AttachmentDispatchedFlags = ENetObjectAttachmentDispatchFlags::Reliable;
					ResolveAndDispatchAttachments(Context, ReplicationInfo, ENetObjectAttachmentDispatchFlags::Reliable);
				}

				Context.SetIsInitState(bHasResolvedInitReferences);

				FDequantizeAndApplyParameters Params;
				Params.Allocator = &TempLinearAllocator;
				Params.ChangeMaskData = ResolvedChangeMask.GetData();
				Params.UnresolvedReferencesChangeMaskData = ReplicationInfo->bHasUnresolvedReferences ? ReplicationInfo->UnresolvedChangeMaskOrPointer.GetPointer(ChangeMaskBitCount) : nullptr;
				Params.InstanceProtocol = ObjectData.InstanceProtocol;
				Params.Protocol = ObjectData.Protocol;
				Params.SrcObjectStateBuffer = ObjectData.ReceiveStateBuffer;
				Params.bHasUnresolvedInitReferences = ReplicationInfo->bHasUnresolvedInitialReferences;

				FReplicationInstanceOperations::DequantizeAndApply(Context, Params);
			}
		}
		else
		{
			// $IRIS: $TODO: Figure out how to handle this, currently we do not crash but we probably want to
			// handle this properly by accumulating changemask for later instantiation
			UE_LOG_REPLICATIONREADER_WARNING(TEXT("Cannot dispatch state data for not instantiated %s"), *ObjectData.RefHandle.ToString());
		}
	}

	// Dispatch attachment and enqueue for later resolving
	if (bObjectHasAttachments)
	{
		// If we haven't dispatched reliable attachments for this object then do so now in addition to unreliable attachments.
		const ENetObjectAttachmentDispatchFlags AttachmentDispatchFlags = ENetObjectAttachmentDispatchFlags::Unreliable | (AttachmentDispatchedFlags ^ ENetObjectAttachmentDispatchFlags::Reliable);
		ResolveAndDispatchAttachments(Context, ReplicationInfo, AttachmentDispatchFlags);
	}
}

// Dispatch all data received for the frame, this includes trying to resolve object references
void FReplicationReader::DispatchStateData(FNetSerializationContext& Context)
{
	IRIS_PROFILER_SCOPE(FReplicationReader::DispatchStateData);

	// In order to execute PostNetRecv/PostRepNotifies after we have applied the actual state 
	// we need to cache some information during dispatch and execute the logic in multiple passes
	// Note: Currently all objects received in the packet are treated a a single batch
	struct FPostDispatchObjectInfo
	{
		FReplicatedObjectInfo* ReplicationInfo;
		FDispatchObjectInfo* Info;
		FDequantizeAndApplyHelper::FContext* DequantizeAndApplyContext;
		ENetObjectAttachmentDispatchFlags AttachmentDispatchedFlags;		
	};

	// Allocate temporary space for post dispatch
	FPostDispatchObjectInfo* PostDispatchObjectInfos = new (TempLinearAllocator) FPostDispatchObjectInfo[ObjectsToDispatchArray->Num()];
	uint32 NumObjectsPendingPostDistpatch = 0U;

	// Function to flush all objects pending post dispatch
	auto FlushPostDispatchForBatch = [PostDispatchObjectInfos, &NumObjectsPendingPostDistpatch, this, &Context]()
	{
		// When all received states have been applied we invoke PostReplicate and RepNotifies
		for (FPostDispatchObjectInfo& PostDispatchObjectInfo : MakeArrayView(PostDispatchObjectInfos, NumObjectsPendingPostDistpatch))
		{
			FDispatchObjectInfo& Info = *PostDispatchObjectInfo.Info;

			// If we are running in backwards compatibility mode, execute Reliable RPC`s before RepNotify callbacks
			if (Info.bHasAttachments && bExecuteReliableRPCsBeforeOnReps && !Info.bIsInitialState)
			{
				PostDispatchObjectInfo.AttachmentDispatchedFlags |= ENetObjectAttachmentDispatchFlags::Reliable;
				ResolveAndDispatchAttachments(Context, PostDispatchObjectInfo.ReplicationInfo, ENetObjectAttachmentDispatchFlags::Reliable);
			}

			// Execute legacy post replicate functions
			if (Info.bHasState && PostDispatchObjectInfo.DequantizeAndApplyContext)
			{
				Context.SetIsInitState(Info.bIsInitialState);
				FDequantizeAndApplyHelper::CallLegacyPostApplyFunctions(PostDispatchObjectInfo.DequantizeAndApplyContext, Context);
			}
		}

		// In the last pass, RPC`s and cleanup cached data
		for (FPostDispatchObjectInfo& PostDispatchObjectInfo : MakeArrayView(PostDispatchObjectInfos, NumObjectsPendingPostDistpatch))
		{
			FDispatchObjectInfo& Info = *PostDispatchObjectInfo.Info;

			// If object was created this frame it`s initial state is now applied
			if (Info.bHasState && Info.bIsInitialState)
			{
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(Info.InternalIndex);
				ReplicationBridge->CallPostApplyInitialState(ObjectData.RefHandle);
			}

			// Dispatch attachment and enqueue for later resolving
			if (Info.bHasAttachments)
			{
				// If we haven't dispatched reliable attachments for this object then do so now in addition to unreliable attachments.
				const ENetObjectAttachmentDispatchFlags AttachmentDispatchFlags = ENetObjectAttachmentDispatchFlags::Unreliable | (PostDispatchObjectInfo.AttachmentDispatchedFlags ^ ENetObjectAttachmentDispatchFlags::Reliable);
				ResolveAndDispatchAttachments(Context, PostDispatchObjectInfo.ReplicationInfo, AttachmentDispatchFlags);
			}

			// Cleanup temporary state data
			if (PostDispatchObjectInfo.DequantizeAndApplyContext)
			{
				FDequantizeAndApplyHelper::Deinitialize(PostDispatchObjectInfo.DequantizeAndApplyContext);
			}
		}

		NumObjectsPendingPostDistpatch = 0U;
	};

	// In order to properly execute legacy callbacks we need to batch apply state data for owner/subobjects
	FInternalNetRefIndex LastDispatchedRootInternalIndex = 0U;
	
	// Dispatch and apply received state data
	for (FDispatchObjectInfo& Info : ObjectsToDispatchArray->GetObjectsToDispatch())
	{
		FReplicatedObjectInfo* ReplicationInfo = GetReplicatedObjectInfo(Info.InternalIndex);

		FPostDispatchObjectInfo PostDispatchObjectInfo;
		PostDispatchObjectInfo.ReplicationInfo = ReplicationInfo;
		PostDispatchObjectInfo.Info = &Info;
		PostDispatchObjectInfo.DequantizeAndApplyContext = nullptr;
		PostDispatchObjectInfo.AttachmentDispatchedFlags = ENetObjectAttachmentDispatchFlags::None;

		// If we have any object references we want to update any unresolved ones, including previously unresolved references
		if (Info.bHasState)
		{
			const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(Info.InternalIndex);

			// We only need to flush if we are switching to a new root object with state data
			const FInternalNetRefIndex RootInternalIndex = ObjectData.SubObjectRootIndex == FNetRefHandleManager::InvalidInternalIndex ? Info.InternalIndex : ObjectData.SubObjectRootIndex;
			if (RootInternalIndex != LastDispatchedRootInternalIndex)
			{
				FlushPostDispatchForBatch();
				LastDispatchedRootInternalIndex = RootInternalIndex;
			}

			const uint32 ChangeMaskBitCount = ReplicationInfo->ChangeMaskBitCount;

			FNetBitArrayView ChangeMask = FChangeMaskUtil::MakeChangeMask(Info.ChangeMaskOrPointer, ChangeMaskBitCount);
			// If we have pending unresolved changes we include them as well 
			FNetBitArrayView UnresolvedChangeMask = FChangeMaskUtil::MakeChangeMask(ReplicationInfo->UnresolvedChangeMaskOrPointer, ChangeMaskBitCount);

			if (ReplicationInfo->bHasUnresolvedReferences)
			{
				ChangeMask.Combine(UnresolvedChangeMask, FNetBitArrayView::OrOp);
			}

			// Collect all unresolvable references, including old pending references
			FResolveAndCollectUnresolvedAndResolvedReferenceCollector Collector;
			Collector.CollectReferences(*ObjectReferenceCache, ResolveContext, Info.bIsInitialState | ReplicationInfo->bHasUnresolvedInitialReferences, &ChangeMask, ObjectData.ReceiveStateBuffer, ObjectData.Protocol);

			// If we have any object references we need to track them
			if (Collector.GetUnresolvedReferences().Num() > 0 || Collector.GetResolvedReferences().Num() > 0)
			{
				BuildUnresolvedChangeMaskAndUpdateObjectReferenceTracking(Collector, ChangeMask, ReplicationInfo, UnresolvedChangeMask);
			
				// $IRIS: $TODO: For now we always apply, even if we cannot resolve all references for a property
				// We need to investigate how this is handled best as we do not want to prevent arrays(fastarrays) from applying data just because a single element wont resolve?
				// Mask off any unresolvable states before we dispatch state data
				//ChangeMask.Combine(UnresolvedChangeMask, FNetBitArrayView::AndNotOp);
			}

			// Apply state data
			if (ObjectData.InstanceProtocol)
			{
				Context.SetIsInitState(Info.bIsInitialState);

				FDequantizeAndApplyParameters Params;
				Params.Allocator = &TempLinearAllocator;
				Params.ChangeMaskData = Info.ChangeMaskOrPointer.GetPointer(ChangeMaskBitCount);
				Params.UnresolvedReferencesChangeMaskData = ReplicationInfo->bHasUnresolvedReferences ? ReplicationInfo->UnresolvedChangeMaskOrPointer.GetPointer(ChangeMaskBitCount) : nullptr;
				Params.InstanceProtocol = ObjectData.InstanceProtocol;
				Params.Protocol = ObjectData.Protocol;
				Params.SrcObjectStateBuffer = ObjectData.ReceiveStateBuffer;
				Params.bHasUnresolvedInitReferences = ReplicationInfo->bHasUnresolvedInitialReferences;

				// Dequantize state data, call PreReplicate and apply received state
				PostDispatchObjectInfo.DequantizeAndApplyContext = FDequantizeAndApplyHelper::Initialize(Context, Params);
				FDequantizeAndApplyHelper::ApplyAndCallLegacyPreApplyFunction(PostDispatchObjectInfo.DequantizeAndApplyContext, Context);
			}
			else
			{
				// $IRIS: $TODO: Figure out how to handle this, currently we do not crash but we probably want to
				// handle this properly by accumulating changemask for later instantiation
				UE_LOG_REPLICATIONREADER_WARNING(TEXT("Cannot dispatch state data for not instantiated %s"), *(ObjectData.RefHandle.ToString()));
			}
		}

		// Add to post dispatch
		PostDispatchObjectInfos[NumObjectsPendingPostDistpatch++] = PostDispatchObjectInfo;
	}

	FlushPostDispatchForBatch();
}

void FReplicationReader::ResolveAndDispatchUnresolvedReferences()
{
	IRIS_PROFILER_SCOPE(FReplicationReader_ResolveAndDispatchUnresolvedReferences);

	// Setup context for dispatch
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = Parameters.ReplicationSystem;
	InternalContextInitParams.ObjectResolveContext = ResolveContext;
	InternalContextInitParams.PackageMap = ReplicationSystemInternal->GetIrisObjectReferencePackageMap();
	InternalContext.Init(InternalContextInitParams);

	FNetSerializationContext Context;
	Context.SetLocalConnectionId(ResolveContext.ConnectionId);
	Context.SetInternalContext(&InternalContext);

	// Currently we brute force this by iterating over all handles pending resolve and update all objects pending resolve
	TArray<FNetRefHandle> UpdatedHandles;
	UpdatedHandles.Reserve(128);
	UnresolvedHandleToDependents.GenerateKeyArray(UpdatedHandles);
	
	TSet<uint32> InternalObjectsToResolve;
	InternalObjectsToResolve.Reserve(UnresolvedHandleToDependents.Num());

	for (FNetRefHandle Handle : UpdatedHandles)
	{
		// Only make sense to update dependant objects if handle is resolvable
		if (ObjectReferenceCache->ResolveObjectReferenceHandle(Handle, ResolveContext) != nullptr)
		{
			for (auto It = UnresolvedHandleToDependents.CreateConstKeyIterator(Handle); It; ++It)
			{
				InternalObjectsToResolve.Add(It.Value());
			}
		}
	}

	// Add in any handles with pending attachments to resolve
	InternalObjectsToResolve.Append(ObjectsWithAttachmentPendingResolve);

	// Try to resolve objects with updated references
	for (uint32 InternalIndex : InternalObjectsToResolve)
	{
		ResolveAndDispatchUnresolvedReferencesForObject(Context, InternalIndex);
	}

	if (NumHandlesPendingResolveLastUpdate != UpdatedHandles.Num() || ObjectsWithAttachmentPendingResolve.Num() > 0)
	{
		UE_LOG_REPLICATIONREADER(TEXT("FReplicationReader::ResolveAndDispatchUnresolvedReferences NetHandles pending: %u Attachments pending: %u)"), UpdatedHandles.Num(), ObjectsWithAttachmentPendingResolve.Num());
		NumHandlesPendingResolveLastUpdate = UpdatedHandles.Num();
	}
}


void FReplicationReader::UpdateUnresolvableReferenceTracking()
{
	constexpr uint32 AssumedMaxDependentCount = 256;
	TArray<uint32, TInlineAllocator<AssumedMaxDependentCount>> Dependents;

	// Naively go through every object pending destroy, see if it's dynamic and update dependent's unresolved tracking
	TArrayView<const FInternalNetRefIndex> ObjectsPendingDestroy = NetRefHandleManager->GetObjectsPendingDestroy();
	for (const FInternalNetRefIndex InternalIndex : ObjectsPendingDestroy)
	{
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
		const FNetRefHandle DestroyedHandle = ObjectData.RefHandle;
		if (!DestroyedHandle.IsDynamic())
		{
			continue;
		}

		// For torn off objects we want to remove both resolved and unresolved references to it as it will never be replicated again.
		if (ObjectData.bTearOff)
		{
			constexpr bool bMaintainOrder = false;
			Dependents.Reset();
			UnresolvedHandleToDependents.MultiFind(DestroyedHandle, Dependents, bMaintainOrder);
			UnresolvedHandleToDependents.Remove(DestroyedHandle);
			for (const uint32 DependentObjectIndex : Dependents)
			{
				FReplicatedObjectInfo* ReplicationInfo = GetReplicatedObjectInfo(DependentObjectIndex);
				RemoveUnresolvedObjectReferenceInReplicationInfo(ReplicationInfo, DestroyedHandle);
			}
		}

		// For any previously resolved handles make sure to move them to unresolved status.
		constexpr bool bMaintainOrder = false;
		Dependents.Reset();
		ResolvedDynamicHandleToDependents.MultiFind(DestroyedHandle, Dependents, bMaintainOrder);
		if (Dependents.Num() > 0)
		{
			ResolvedDynamicHandleToDependents.Remove(DestroyedHandle);
			// Torn off objects will get new handles if replicated again so they can never be remapped.
			if (ObjectData.bTearOff)
			{
				for (const uint32 DependentObjectIndex : Dependents)
				{
					FReplicatedObjectInfo* ReplicationInfo = GetReplicatedObjectInfo(DependentObjectIndex);
					RemoveResolvedObjectReferenceInReplicationInfo(ReplicationInfo, DestroyedHandle);
				}
			}
			else
			{
				for (const uint32 DependentObjectIndex : Dependents)
				{
					FReplicatedObjectInfo* ReplicationInfo = GetReplicatedObjectInfo(DependentObjectIndex);
					if (MoveResolvedObjectReferenceToUnresolvedInReplicationInfo(ReplicationInfo, DestroyedHandle))
					{
						UnresolvedHandleToDependents.Add(DestroyedHandle, DependentObjectIndex);
					}
				}
			}
		}
	}
}

void FReplicationReader::DispatchEndReplication(FNetSerializationContext& Context)
{
	for (FDispatchObjectInfo& Info : ObjectsToDispatchArray->GetObjectsToDispatch())
	{
		if (Info.bDeferredEndReplication)
		{
			// Detach destroy object
			EndReplication(Info.InternalIndex, Info.bTearOff, Info.bDestroy);
		}
	}
}


void FReplicationReader::ReadObjects(FNetSerializationContext& Context, uint32 ObjectBatchCountToRead)
{
	IRIS_PROFILER_SCOPE(ReplicationReader_ReadObjects);

	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();
	
	while (ObjectBatchCountToRead && !Context.HasErrorOrOverflow())
	{
		ReadObjectBatch(Context);
		--ObjectBatchCountToRead;
	}

	ensureAlwaysMsgf(!Context.HasErrorOrOverflow(), TEXT("Overflow: %c Error: %s Bit stream bits left: %u position: %u"), "YN"[Context.HasError()], ToCStr(Context.GetError().ToString()), Reader.GetBitsLeft(), Reader.GetPosBits());
}

void FReplicationReader::ProcessHugeObjectAttachment(FNetSerializationContext& Context, const TRefCountPtr<FNetBlob>& Attachment)
{
	if (Attachment->GetCreationInfo().Type != NetObjectBlobType)
	{
		Context.SetError(GNetError_UnsupportedNetBlob);
		return;
	}

	FNetTraceCollector* HugeObjectTraceCollector = nullptr;
#if UE_NET_TRACE_ENABLED
	FNetTraceCollector HugeObjectTraceCollectorOnStack;
	HugeObjectTraceCollector = &HugeObjectTraceCollectorOnStack;
#endif

	const FNetObjectBlob& NetObjectBlob = *static_cast<FNetObjectBlob*>(Attachment.GetReference());

	FNetBitStreamReader HugeObjectReader;
	HugeObjectReader.InitBits(NetObjectBlob.GetRawData().GetData(), NetObjectBlob.GetRawDataBitCount());
	FNetSerializationContext HugeObjectSerializationContext = Context.MakeSubContext(&HugeObjectReader);
	HugeObjectSerializationContext.SetTraceCollector(HugeObjectTraceCollector);

	UE_NET_TRACE_NAMED_SCOPE(HugeObjectTraceScope, HugeObjectState, HugeObjectReader, HugeObjectTraceCollector, ENetTraceVerbosity::Trace);

	// Find out how many objects to read so we can reserve object dispatch infos.
	FNetObjectBlob::FHeader HugeObjectHeader = {};
	FNetObjectBlob::DeserializeHeader(HugeObjectSerializationContext, HugeObjectHeader);
	if (HugeObjectSerializationContext.HasErrorOrOverflow() || HugeObjectHeader.ObjectCount < 1U)
	{
		if (!Context.HasError())
		{
			Context.SetError(GNetError_BitStreamError);
			return;
		}
	}

	// Reserve space for more dispatch infos as needed, we allocate some extra to account for subobjects etc
	ObjectsToDispatchArray->Grow(HugeObjectHeader.ObjectCount + ObjectsToDispatchSlackCount, TempLinearAllocator);

	ReadObjects(HugeObjectSerializationContext, HugeObjectHeader.ObjectCount);
	if (HugeObjectSerializationContext.HasErrorOrOverflow())
	{
		Context.SetError(GNetError_BitStreamError);
		return;
	}

#if UE_NET_TRACE_ENABLED
	UE_NET_TRACE_EXIT_NAMED_SCOPE(HugeObjectTraceScope);

	// Append huge object state at end of stream.
	if (FNetTraceCollector* TraceCollector = Context.GetTraceCollector())
	{
		FNetBitStreamReader& Reader = *Context.GetBitStreamReader();
		// Inject after all other trace events
		FNetTrace::FoldTraceCollector(TraceCollector, HugeObjectTraceCollector, GetBitStreamPositionForNetTrace(Reader));
	}
#endif
}

bool FReplicationReader::EnqueueEndReplication(FPendingBatchData* PendingBatchData, bool bShouldDestroyInstance, FNetRefHandle NetRefHandleToEndReplication)
{
	UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::EnqueueEndReplication for %s since %s has queued batches"), *NetRefHandleToEndReplication.ToString(), *PendingBatchData->Handle.ToString());
		
	const uint32 MaxNumDataBits = 65U;
	const uint32 NumDataWords = (MaxNumDataBits + 31U) / 32U;

	// Enqueue BatchData
	FQueuedDataChunk DataChunk;

	DataChunk.NumBits = MaxNumDataBits;
	DataChunk.StorageOffset = PendingBatchData->DataChunkStorage.Num();
	DataChunk.bHasBatchOwnerData = false;
	DataChunk.bIsEndReplicationChunk = true;

	// Make sure we have space
	PendingBatchData->DataChunkStorage.AddUninitialized(NumDataWords);

	FNetBitStreamWriter Writer;
	Writer.InitBytes(PendingBatchData->DataChunkStorage.GetData() + DataChunk.StorageOffset, NumDataWords*sizeof(uint32));

	// Write data
	WriteUint64(&Writer, NetRefHandleToEndReplication.GetId());
	Writer.WriteBool(bShouldDestroyInstance);
	Writer.CommitWrites();

	if (Writer.IsOverflown())
	{
		ensureAlwaysMsgf(false, TEXT("Failed to EnqueueEndReplication for %s, Should never occur unless size of NetRefHandle has been increased."), *NetRefHandleToEndReplication.ToString());
		return false;
	}

	PendingBatchData->QueuedDataChunks.Add(MoveTemp(DataChunk));

	return true;
}

void FReplicationReader::ProcessQueuedBatches()
{
	UE_NET_TRACE_FRAME_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), ReplicationReader.PendingQueuedBatches, PendingBatches.Num(), ENetTraceVerbosity::Trace);

	if (PendingBatches.IsEmpty())
	{
		//Nothing to do.
		return;
	}

	// Setup context for dispatch
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = Parameters.ReplicationSystem;
	InternalContextInitParams.ObjectResolveContext = ResolveContext;
	InternalContextInitParams.PackageMap = ReplicationSystemInternal->GetIrisObjectReferencePackageMap();
	InternalContext.Init(InternalContextInitParams);

	FNetBitStreamReader Reader;
	FNetSerializationContext Context(&Reader);
	Context.SetLocalConnectionId(ResolveContext.ConnectionId);
	Context.SetInternalContext(&InternalContext);
	Context.SetNetBlobReceiver(&ReplicationSystemInternal->GetNetBlobHandlerManager());

	for (int BatchIt = 0; BatchIt < PendingBatches.Num(); )
	{
		FPendingBatchData& PendingBatchData = PendingBatches[BatchIt];

		// Try to resolve remaining must be mapped references
		TempMustBeMappedReferences.Reset();
		UpdateUnresolvedMustBeMappedReferences(PendingBatchData.Handle, TempMustBeMappedReferences);

		// If we have no more pending must be referenes we can apply the received state
		if (PendingBatchData.PendingMustBeMappedReferences.IsEmpty())		
		{
			UE_LOG(LogIris, Verbose, TEXT("ProcessQueuedBatches processing %d queued batches for Handle %s "), PendingBatchData.QueuedDataChunks.Num(), *PendingBatchData.Handle.ToString());

			// Process batched data & dispatch data
			for (const FQueuedDataChunk& CurrentChunk : PendingBatchData.QueuedDataChunks)
			{
				Reader.InitBits(PendingBatchData.DataChunkStorage.GetData() + CurrentChunk.StorageOffset, CurrentChunk.NumBits);

				// Chunks marked as bIsEndReplicationChunk are dispatched immediately as we do not know if the next chunk tries to re-create the instance
				if (CurrentChunk.bIsEndReplicationChunk)
				{
					// Read data stored for objects ending replication, 
					// this can be the batch root or a subobject owned by the batched root.
					const uint64 NetRefHandleIdToEndReplication = ReadUint64(&Reader);
					const bool bShouldDestroyInstance = Reader.ReadBool();
					
					const FNetRefHandle NetRefHandleToEndReplication = FNetRefHandleManager::MakeNetRefHandleFromId(NetRefHandleIdToEndReplication);
					
					// End replication for object
					const uint32 InternalIndex = NetRefHandleManager->GetInternalIndex(NetRefHandleToEndReplication);
					if (InternalIndex != FNetRefHandleManager::InvalidInternalIndex)
					{
						EndReplication(InternalIndex, false, bShouldDestroyInstance);
					}

					UE_LOG(LogIris, Verbose, TEXT("FReplicationReader::ProcessQueuedBatches EndReplication for %s while processing queued batches for %s"), *NetRefHandleToEndReplication.ToString(), *PendingBatchData.Handle.ToString());
					continue;
				}

				// Read and process chunk as it was a received packet for now at least
				FMemMark TempAllocatorScope(TempLinearAllocator);

				// We need to set this up to store temporary dispatch data, the array will grow if needed
				FObjectsToDispatchArray TempObjectsToDispatchArray(ObjectsToDispatchSlackCount, TempLinearAllocator);

				// Need to set this pointer as we are dealing with temporary linear allocations
				ObjectsToDispatchArray = &TempObjectsToDispatchArray;

				// $IRIS: $TODO: Implement special dispatch to defer RepNotifies if we are processing multiple batches for the same object.
				ReadObjectsInBatch(Context, PendingBatchData.Handle, CurrentChunk.bHasBatchOwnerData, CurrentChunk.NumBits);

				// $IRIS: $TODO: What to do if we fail to process this batch? Just delete it? Might need to report this to server as a broken object and build logic to reset replication of the object
				ensureAlwaysMsgf(!Context.HasErrorOrOverflow(), TEXT("FReplicationReader::ProcessQueuedBatches - Failed to process enqueued batch for %s - %s"), *PendingBatchData.Handle.ToString(), *Context.GetError().ToString());
			
				// Apply received data and resolve dependencies
				DispatchStateData(Context);

				// Resolve
				ResolveAndDispatchUnresolvedReferences();

				// EndReplication for all objects in the batch that should no longer replicate
				DispatchEndReplication(Context);

				// Drop temporary data
				ObjectsToDispatchArray = nullptr;
			}
	
			// Make sure to release all references that we hold on to
			for (const FNetRefHandle& RefHandle : PendingBatchData.ResolvedReferences)
			{
				ObjectReferenceCache->RemoveTrackedQueuedBatchObjectReference(RefHandle);
			}

			// Not optimal, but we want to preserve the order if we can as there might be batches waiting for the same reference
			PendingBatches.RemoveAt(BatchIt);
		}
		else
		{
			++BatchIt;
		}
	}
}

void FReplicationReader::ProcessHugeObject(FNetSerializationContext& Context)
{
	if (!Attachments.HasUnprocessedAttachments(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment))
	{
		return;
	}

	FNetObjectAttachmentReceiveQueue* AttachmentQueue = Attachments.GetQueue(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment);
	while (const TRefCountPtr<FNetBlob>* Attachment = AttachmentQueue->PeekReliable())
	{
		ProcessHugeObjectAttachment(Context, *Attachment);
		AttachmentQueue->PopReliable();
		if (Context.HasError())
		{
			return;
		}
	}
	while (const TRefCountPtr<FNetBlob>* Attachment = AttachmentQueue->PeekUnreliable())
	{
		ProcessHugeObjectAttachment(Context, *Attachment);
		AttachmentQueue->PopUnreliable();
		if (Context.HasError())
		{
			return;
		}
	}
}

void FReplicationReader::Read(FNetSerializationContext& Context)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	// Setup internal context
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = Parameters.ReplicationSystem;
	InternalContextInitParams.PackageMap = ReplicationSystemInternal->GetIrisObjectReferencePackageMap();
	InternalContextInitParams.ObjectResolveContext = ResolveContext;
	InternalContext.Init(InternalContextInitParams);
	
	Context.SetLocalConnectionId(Parameters.ConnectionId);
	Context.SetInternalContext(&InternalContext);
	Context.SetNetBlobReceiver(&ReplicationSystemInternal->GetNetBlobHandlerManager());

	UE_NET_TRACE_SCOPE(ReplicationData, *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	FMemMark TempAllocatorScope(TempLinearAllocator);

	// Sanity check received object count
	const uint32 MaxObjectBatchCountToRead = 8192U;
	const uint32 ReceivedObjectBatchCountToRead = Reader.ReadBits(16);
	uint32 ObjectBatchCountToRead = ReceivedObjectBatchCountToRead;

	if (Reader.IsOverflown() || ObjectBatchCountToRead >= MaxObjectBatchCountToRead)
	{
		const FName& NetError = (Reader.IsOverflown() ? GNetError_BitStreamOverflow : GNetError_BitStreamError);
		Context.SetError(NetError);

		return;
	}

	if (ObjectBatchCountToRead == 0)
	{
		return;
	}

	// Allocate tracking info for objects we receive this packet from temporary allocator
	// We need to set this up to store temporary dispatch data, the array will grow if needed
	FObjectsToDispatchArray TempObjectsToDispatchArray(ObjectBatchCountToRead + ObjectsToDispatchSlackCount, TempLinearAllocator);

	// Need to set this pointer as we are dealing with temporary linear allocations
	ObjectsToDispatchArray = &TempObjectsToDispatchArray;

	uint32 DestroyedObjectCount = ReadObjectsPendingDestroy(Context);

	ObjectBatchCountToRead -= DestroyedObjectCount;

	// Nothing more to do or we failed and should disconnect
	if (Context.HasErrorOrOverflow() || (ObjectBatchCountToRead == 0 && ObjectsToDispatchArray->Num() == 0))
	{
		return;
	}

	ReadObjects(Context, ObjectBatchCountToRead);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	// Assemble and deserialize huge object if present
	ProcessHugeObject(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	// Stats
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationReader.ReadObjectBatchCount, ReceivedObjectBatchCountToRead, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationReader.ReadObjectsAndSubObjectsToDispatchCount, ObjectsToDispatchArray->Num(), ENetTraceVerbosity::Trace);

	// Apply received data and resolve dependencies
	DispatchStateData(Context);
	if (Context.HasErrorOrOverflow())
	{
		return;
	}

	// Resolve
	ResolveAndDispatchUnresolvedReferences();

	// EndReplication for all objects that should no longer replicate
	DispatchEndReplication(Context);

	// Drop temporary dispatch data
	ObjectsToDispatchArray = nullptr;
}

void FReplicationReader::ResolveAndDispatchAttachments(FNetSerializationContext& Context, FReplicatedObjectInfo* ReplicationInfo, ENetObjectAttachmentDispatchFlags DispatchFlags)
{
	if (DispatchFlags == ENetObjectAttachmentDispatchFlags::None)
	{
		return;
	}

	// Cache configurables before processing attachments
	const bool bDispatchReliableAttachments = EnumHasAnyFlags(DispatchFlags, ENetObjectAttachmentDispatchFlags::Reliable);
	const bool bDispatchUnreliableAttachments = EnumHasAnyFlags(DispatchFlags, ENetObjectAttachmentDispatchFlags::Unreliable);
	const bool bCanDelayAttachments = Parameters.bAllowDelayingAttachmentsWithUnresolvedReferences && (DelayAttachmentsWithUnresolvedReferences != nullptr && DelayAttachmentsWithUnresolvedReferences->GetInt() > 0);
	const uint32 InternalIndex = ReplicationInfo->InternalIndex;

	/**
	* This code path handles all cases where the initial state has already been applied. An object can have multiple entries in ObjectsPendingResolve.
	* Reliable attachments will be delivered if they can be resolved or if CVarDelayUnmappedRPCs is <= 0
	*/
	bool bHasUnresolvedReferences = false;
	const ENetObjectAttachmentType AttachmentType = (IsObjectIndexForOOBAttachment(InternalIndex) ? ENetObjectAttachmentType::OutOfBand : ENetObjectAttachmentType::Normal);
	if (FNetObjectAttachmentReceiveQueue* AttachmentQueue = Attachments.GetQueue(AttachmentType, InternalIndex))
	{
		if (bDispatchReliableAttachments)
		{
			while (const TRefCountPtr<FNetBlob>* Attachment = AttachmentQueue->PeekReliable())
			{
				// Delay attachments with unresolved references
				if (bCanDelayAttachments)
				{
					// Only block reliable stream if we have unresolved references that must be mapped
					const ENetObjectReferenceResolveResult ResolveResult = Attachment->GetReference()->ResolveObjectReferences(Context);
					if (EnumHasAnyFlags(ResolveResult, ENetObjectReferenceResolveResult::HasUnresolvedMustBeMappedReferences))
					{
						const FReplicationStateDescriptor* Descriptor = Attachment->GetReference()->GetReplicationStateDescriptor();

						UE_LOG(LogIris, Warning, TEXT("Unable to resolve references in %s for InternalIndex %u"), (Descriptor != nullptr ? ToCStr(Descriptor->DebugName) : TEXT("N/A")), InternalIndex);
						break;
					}
				}

				NetBlobHandlerManager->OnNetBlobReceived(Context, *reinterpret_cast<const TRefCountPtr<FNetBlob>*>(Attachment));
				AttachmentQueue->PopReliable();

				if (Context.HasError())
				{
					return;
				}
			}
		}

		if (bDispatchUnreliableAttachments)
		{
			while (const TRefCountPtr<FNetBlob>* Attachment = AttachmentQueue->PeekUnreliable())
			{
				NetBlobHandlerManager->OnNetBlobReceived(Context, *Attachment);
				AttachmentQueue->PopUnreliable();

				if (Context.HasError())
				{
					return;
				}
			}
		}
		
		if (AttachmentQueue->IsSafeToDestroy())
		{
			// N.B. AttachmentQueue is no longer valid after this call
			Attachments.DropAllAttachments(AttachmentType, InternalIndex);
		}
		else
		{
			bHasUnresolvedReferences = AttachmentQueue->HasUnprocessed();
		}
	}
	else
	{
		// Should not get here, if we do something is out of sync and we should disconnect
		Context.SetError(NetError_FailedToFindAttachmentQueue);
		ensure(AttachmentType == ENetObjectAttachmentType::OutOfBand);
	}

	// Update tracking of objects with attachments pending resolve
	if (bHasUnresolvedReferences && !ReplicationInfo->bHasAttachments)
	{
		ObjectsWithAttachmentPendingResolve.Add(InternalIndex);
	}
	else if (!bHasUnresolvedReferences && ReplicationInfo->bHasAttachments)
	{
		ObjectsWithAttachmentPendingResolve.RemoveSwap(InternalIndex);
	}
	ReplicationInfo->bHasAttachments = bHasUnresolvedReferences;
}

void FReplicationReader::SetRemoteNetTokenStoreState(FNetTokenStoreState* InRemoteTokenStoreState)
{
	ResolveContext.RemoteNetTokenStoreState = InRemoteTokenStoreState;
}

}
