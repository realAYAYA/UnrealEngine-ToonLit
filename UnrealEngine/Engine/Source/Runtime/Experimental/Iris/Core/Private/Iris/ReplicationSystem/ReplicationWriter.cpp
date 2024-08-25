// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Containers/Array.h"
#include "Iris/IrisConfigInternal.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Misc/ScopeExit.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Trace/NetDebugName.h"
#include "Iris/PacketControl/PacketNotification.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "Iris/ReplicationSystem/DeltaCompression/DeltaCompressionBaselineManager.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetBlob/NetObjectBlobHandler.h"
#include "Iris/ReplicationSystem/NetBlob/PartialNetObjectAttachmentHandler.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetExportContext.h"
#include "Iris/Stats/NetStatsContext.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include <algorithm>
#include <cmath> // std::nextafter

#if UE_NET_ENABLE_REPLICATIONWRITER_LOG
#	define UE_LOG_REPLICATIONWRITER(Format, ...)  UE_LOG(LogIris, Log, Format, ##__VA_ARGS__)
#	define UE_LOG_REPLICATIONWRITER_CONN(Format, ...)  UE_LOG(LogIris, Log, TEXT("Conn: %u ") Format, Parameters.ConnectionId, ##__VA_ARGS__)
#else
#	define UE_LOG_REPLICATIONWRITER(...)
#	define UE_LOG_REPLICATIONWRITER_CONN(Format, ...)
#endif

#define UE_LOG_REPLICATIONWRITER_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)
#define UE_CLOG_REPLICATIONWRITER_WARNING(Condition, Format, ...)  UE_CLOG(Condition, LogIris, Warning, Format, ##__VA_ARGS__)

namespace UE::Net::Private
{

static bool bWarnAboutDroppedAttachmentsToObjectsNotInScope = false;
static FAutoConsoleVariableRef CVarWarnAboutDroppedAttachmentsToObjectsNotInScope(
	TEXT("net.Iris.WarnAboutDroppedAttachmentsToObjectsNotInScope"),
	bWarnAboutDroppedAttachmentsToObjectsNotInScope,
	TEXT("Warn when attachments are dropped due to object not in scope. Default is false."
	));

static int32 GReplicationWriterMaxAllowedPacketsIfNotHugeObject = 3;
static FAutoConsoleVariableRef CVarReplicationWriterMaxAllowedPacketsIfNotHugeObject(TEXT("net.Iris.ReplicationWriterMaxAllowedPacketsIfNotHugeObject"), GReplicationWriterMaxAllowedPacketsIfNotHugeObject,
	TEXT("Allow ReplicationWriter to overcommit data if we have more data to write."));

/*
 * net.Iris.ReplicationWriterMaxHugeObjectsInTransit
 * There's a tradeoff mainly between the connection characteristics to support and normal object replication scheduling when tweaking this value.
 * On one hand you don't want to end up stalling object replication because the top priority objects are huge. So you want to be able to keep replicating huge objects during the maximum latency, including latency variation, and packet loss scenarios 
 * you want to provide the best experience possible for. On the other hand object deletion cannot be performed once the object is in the huge object queue. Consider this and how long time it will take to replicate the huge object queue depending on the average payload of a huge object.
 */
static int32 GReplicationWriterMaxHugeObjectsInTransit = 16;
static FAutoConsoleVariableRef CVarReplicationWriterMaxHugeObjectsInTransit(TEXT("net.Iris.ReplicationWriterMaxHugeObjectsInTransit"), GReplicationWriterMaxHugeObjectsInTransit,
	TEXT("How many very large objects, one whose payload doesn't fit in a single packet, is allowed to be scheduled for send. Needs to be at least 1."));

static bool bValidateObjectsWithDirtyChanges = true;
static FAutoConsoleVariableRef CvarValidateObjectsWithDirtyChanges(TEXT("net.Iris.ReplicationWriter.ValidateObjectsWithDirtyChanges"), bValidateObjectsWithDirtyChanges, TEXT("Ensure that we don't try to mark invalid objects as dirty when they shouldn't."));

static const FName NetError_ObjectStateTooLarge("Object state is too large to be split.");

const TCHAR* FReplicationWriter::LexToString(const EReplicatedObjectState State)
{
	static const TCHAR* Names[] = {
		TEXT("Invalid"),
		TEXT("AttachmentToObjectNotInScope"),
		TEXT("HugeObject"),
		TEXT("PendingCreate"),
		TEXT("WaitOnCreateConfirmation"),
		TEXT("Created"),
		TEXT("WaitOnFlush"),
		TEXT("PendingTearOff"),
		TEXT("SubObjectPendingDestroy"),
		TEXT("CancelPendingDestroy"),
		TEXT("PendingDestroy"),
		TEXT("WaitOnDestroyConfirmation"),
		TEXT("Destroyed"),
		TEXT("PermanentlyDestroyed"),
	};
	static_assert(UE_ARRAY_COUNT(Names) == uint32(EReplicatedObjectState::Count), "Missing names for one or more values of EReplicatedObjectState.");

	return State < EReplicatedObjectState::Count ? Names[(uint32)State] : TEXT("");
}

void FReplicationWriter::FReplicationInfo::SetState(EReplicatedObjectState NewState)
{
	EReplicatedObjectState CurrentState = GetState();
	switch (NewState)
	{
		case EReplicatedObjectState::PendingCreate:
		{
			checkf(CurrentState == EReplicatedObjectState::Invalid || CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::WaitOnCreateConfirmation:
		{
			checkf(CurrentState == EReplicatedObjectState::PendingCreate || CurrentState == EReplicatedObjectState::CancelPendingDestroy, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::Created:
		{
			checkf(CurrentState == EReplicatedObjectState::PendingCreate || CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation || CurrentState == EReplicatedObjectState::CancelPendingDestroy || CurrentState == EReplicatedObjectState::WaitOnFlush, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::PendingTearOff:
		{
			checkf(CurrentState == EReplicatedObjectState::PendingTearOff || CurrentState == EReplicatedObjectState::WaitOnFlush || CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation || CurrentState == EReplicatedObjectState::Created || CurrentState == EReplicatedObjectState::WaitOnDestroyConfirmation, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::SubObjectPendingDestroy:
		{
			checkf(CurrentState == EReplicatedObjectState::PendingDestroy || CurrentState == EReplicatedObjectState::SubObjectPendingDestroy || CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation || CurrentState == EReplicatedObjectState::Created || CurrentState == EReplicatedObjectState::WaitOnFlush || CurrentState == EReplicatedObjectState::WaitOnDestroyConfirmation, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::WaitOnFlush:
		{
			checkf(CurrentState != EReplicatedObjectState::Invalid, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::PendingDestroy:
		{
			checkf(CurrentState != EReplicatedObjectState::Invalid, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::WaitOnDestroyConfirmation:
		{
			checkf(CurrentState >= EReplicatedObjectState::PendingTearOff, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::CancelPendingDestroy:
		{
			checkf(CurrentState == EReplicatedObjectState::WaitOnDestroyConfirmation || CurrentState == EReplicatedObjectState::CancelPendingDestroy, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::Destroyed:
		{
			checkf(CurrentState == EReplicatedObjectState::WaitOnDestroyConfirmation || CurrentState == EReplicatedObjectState::PendingTearOff || CurrentState == EReplicatedObjectState::CancelPendingDestroy, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::PermanentlyDestroyed:
		{
			checkf(CurrentState == EReplicatedObjectState::Invalid || CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;
		case EReplicatedObjectState::Invalid:
		{
			checkf(CurrentState == EReplicatedObjectState::PermanentlyDestroyed || CurrentState == EReplicatedObjectState::Destroyed || CurrentState == EReplicatedObjectState::PendingCreate, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), LexToString(NewState), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
		}
		break;

		default:
			checkf(false, TEXT("Trying to set state %s when state is %s. IsDestructionInfo: %u IsSubObject: %u"), ToCStr(FString::FromInt(int(NewState))), LexToString(CurrentState), IsDestructionInfo, IsSubObject);
			break;
	};

	State = (uint32)NewState;
}


// Default allocator for changemasks
static FGlobalChangeMaskAllocator s_DefaultChangeMaskAllocator;

// Helper class to process all ReplicationInfos for a record
struct TReplicationRecordHelper
{
	typedef FReplicationWriter::FReplicationInfo FReplicationInfo;
	typedef FReplicationRecord::FRecordInfoList FRecordInfoList;
	typedef FReplicationWriter::EReplicatedObjectState EReplicatedObjectState;

	TArray<FReplicationInfo>& ReplicationInfos;
	TArray<FReplicationRecord::FRecordInfoList>& ReplicationInfosRecordInfoLists;
	FReplicationRecord* ReplicationRecord;

	TReplicationRecordHelper(TArray<FReplicationInfo>& InReplicationInfos, TArray<FReplicationRecord::FRecordInfoList>& InReplicationInfosRecordInfoLists, FReplicationRecord* InReplicationRecordRecord)
	: ReplicationInfos(InReplicationInfos)
	, ReplicationInfosRecordInfoLists(InReplicationInfosRecordInfoLists)
	, ReplicationRecord(InReplicationRecordRecord)
	{
	}
	
	template <typename T>
	void Process(uint32 RecordInfoCount, T&& Functor)
	{
		for (uint32 It = 0; It < RecordInfoCount; ++It)
		{
 			const FReplicationRecord::FRecordInfo& RecordInfo = ReplicationRecord->PeekInfo();
			FNetObjectAttachmentsWriter::FReliableReplicationRecord AttachmentRecord(RecordInfo.HasAttachments ? ReplicationRecord->DequeueAttachmentRecord() : uint64(0));
			FReplicationInfo& Info = ReplicationInfos[RecordInfo.Index];
			FReplicationRecord::FRecordInfoList& RecordInfoList = ReplicationInfosRecordInfoLists[RecordInfo.Index];

			// We Need to cache this as the the ReplicationInfo might be invalidated by the functor
			const uint32 ChangeMaskBitCount = Info.ChangeMaskBitCount;

			// Invoke function
			Functor(RecordInfo, Info, AttachmentRecord);

			// We must free any dynamic memory allocated in PushRecordInfo
			if (RecordInfo.HasChangeMask)
			{
				FChangeMaskStorageOrPointer::Free(RecordInfo.ChangeMaskOrPtr, ChangeMaskBitCount, s_DefaultChangeMaskAllocator);
			}

			// We must remove the record and unlink it
			// It is safe to call even if we have stopped replicating the object		
			ReplicationRecord->PopInfoAndRemoveFromList(RecordInfoList);
		}
	}
};

#if UE_NET_VALIDATE_REPLICATION_RECORD

static bool s_ValidateReplicationRecord(const FReplicationRecord* ReplicationRecord, uint32 MaxInternalIndexCount, bool bVerifyFirstRecord)
{
	if (ReplicationRecord->GetRecordCount() == 0U)
	{
		return true;
	}

	// validate count
	{
		uint32 TotalPushedInfos = 0U;
		for (uint32 It = 0U, EndIt = ReplicationRecord->GetRecordCount(); It < EndIt; ++It)
		{
			TotalPushedInfos += ReplicationRecord->PeekRecordAtOffset(It);
		}

		if (TotalPushedInfos != ReplicationRecord->GetInfoCount())
		{
			ensure(false);
			return false;
		}
	}
	
	// Verify last / first record
	const uint32 RecordInfoCount = bVerifyFirstRecord ? ReplicationRecord->PeekRecordAtOffset(0) : ReplicationRecord->PeekRecordAtOffset(ReplicationRecord->GetRecordCount() - 1);

	// check for duplicates
	{
		FNetBitArray BitArray;
		BitArray.Init(MaxInternalIndexCount);
		
		uint32 Offset = bVerifyFirstRecord ? 0U : ReplicationRecord->GetInfoCount() - RecordInfoCount;
		for (uint32 It = 0U; It < RecordInfoCount; ++It)
		{
 			const FReplicationRecord::FRecordInfo& RecordInfo = ReplicationRecord->PeekInfoAtOffset(It + Offset);
			// We allow multiple entries for the OOB attachments but do not expect multiple entires for normal replicated objects
			if (RecordInfo.Index != 0U && BitArray.GetBit(RecordInfo.Index))
			{
				ensure(false);
				return false;
			}
			BitArray.SetBit(RecordInfo.Index);
		}
	}

	return true;
}

#endif

FReplicationWriter::~FReplicationWriter()
{
	// NOTE: Currently disabled because FReplicationWriter until the performance impact of TNetChunkedArray can be measured on the server.
	//NetRefHandleManager->GetLargestIndexIncreaseDelegate().Remove(OnLargestIndexIncreaseHandle);

	DiscardAllRecords();

	// Freeing the huge object queue needs to be done before calling StopAllReplication() in order to be able to free any changemask allocations.
	FreeHugeObjectSendQueue();
	
	StopAllReplication();
}

void FReplicationWriter::SetReplicationEnabled(bool bInReplicationEnabled)
{
	bReplicationEnabled = bInReplicationEnabled;
}

bool FReplicationWriter::IsReplicationEnabled() const
{
	return bReplicationEnabled;
}

// $IRIS TODO : May need to introduce queue and send behaviors. For example one may want to send only with object.
// One may not want to send unless the object is replicated very soon etc.
bool FReplicationWriter::QueueNetObjectAttachments(FInternalNetRefIndex OwnerInternalIndex, FInternalNetRefIndex SubObjectInternalIndex, TArrayView<const TRefCountPtr<FNetBlob>> InAttachments, ENetObjectAttachmentSendPolicyFlags SendFlags)
{
	if (InAttachments.Num() <= 0)
	{
		ensureMsgf(false, TEXT("%s"), TEXT("QueueNetObjectAttachments expects at least one attachment."));
		return false;
	}

	const uint32 TargetIndex = SubObjectInternalIndex != FNetRefHandleManager::InvalidInternalIndex ? SubObjectInternalIndex : OwnerInternalIndex;
	const bool bTargetObjectInScope = ObjectsInScope.GetBit(TargetIndex);
	if (!bTargetObjectInScope && !Parameters.bAllowSendingAttachmentsToObjectsNotInScope)
	{
		UE_CLOG_REPLICATIONWRITER_WARNING(bWarnAboutDroppedAttachmentsToObjectsNotInScope, TEXT("Dropping %s attachment due to object ( InternalIndex: %u ) not in scope."), (EnumHasAnyFlags(InAttachments[0]->GetCreationInfo().Flags, ENetBlobFlags::Reliable) ? TEXT("reliable") : TEXT("unreliable")), TargetIndex);
		return false;
	}
	
	const bool bScheduleUsingOOBChannel = EnumHasAnyFlags(SendFlags, ENetObjectAttachmentSendPolicyFlags::ScheduleAsOOB);
	if (bScheduleUsingOOBChannel)
	{
		// Route attachments flagged with ScheduleAsOOB through OOB channel only if we have started replicating the target.
		const EReplicatedObjectState ReplicationState = GetReplicationInfo(TargetIndex).GetState();
		if (ReplicationState < EReplicatedObjectState::WaitOnCreateConfirmation || ReplicationState >= EReplicatedObjectState::PendingDestroy)
		{
			UE_CLOG_REPLICATIONWRITER_WARNING(bWarnAboutDroppedAttachmentsToObjectsNotInScope, TEXT("Dropping attachment scheduled as ScheduleAsOOB due to object ( InternalIndex: %u ) not in replicated state."),  OwnerInternalIndex);
			return false;
		}
	}

	const uint32 AttachmentQueueIndex = (bTargetObjectInScope && !bScheduleUsingOOBChannel) ? TargetIndex : ObjectIndexForOOBAttachment;
	const ENetObjectAttachmentType AttachmentType = ((bTargetObjectInScope && !bScheduleUsingOOBChannel) ? ENetObjectAttachmentType::Normal : ENetObjectAttachmentType::OutOfBand);
	if (!Attachments.Enqueue(AttachmentType, AttachmentQueueIndex, InAttachments))
	{
		return false;
	}

	// We do not have to mark anything dirty as there's a special case for out of band attachments
	if (!IsObjectIndexForOOBAttachment(AttachmentQueueIndex))
	{
		FReplicationInfo& TargetInfo = GetReplicationInfo(AttachmentQueueIndex);
		TargetInfo.HasAttachments = 1;

		MarkObjectDirty(AttachmentQueueIndex, "QueueAttachment");

		if (OwnerInternalIndex != AttachmentQueueIndex)
		{
			MarkObjectDirty(OwnerInternalIndex, "QueueAttachment2");
			FReplicationInfo& OwnerInfo = GetReplicationInfo(OwnerInternalIndex);
			OwnerInfo.HasDirtySubObjects = 1;
		}
	}

	return true;
}

void FReplicationWriter::SetState(uint32 InternalIndex, EReplicatedObjectState NewState)
{
	FReplicationInfo& Info = GetReplicationInfo(InternalIndex);

	UE_LOG_REPLICATIONWRITER_CONN(TEXT("ReplicationWriter.SetState for ( InternalIndex: %u ) %s -> %s"), InternalIndex, LexToString(Info.GetState()), LexToString(NewState));
	Info.SetState(NewState);
}

void FReplicationWriter::Init(const FReplicationParameters& InParameters)
{
	// Store copy of parameters
	Parameters = InParameters;

	UE_LOG(LogIris, Log, TEXT("ReplicationWriter: Configured with MaxActiveReplicatedObjectCount=%d, PreallocatedObjectCount=%d and MaxReplicatedWriterObjectCount=%d."), Parameters.MaxActiveReplicatedObjectCount, Parameters.PreAllocatedReplicatedObjectCount, Parameters.MaxReplicatedWriterObjectCount);

	// Cache internal systems
	ReplicationSystemInternal = Parameters.ReplicationSystem->GetReplicationSystemInternal();
	NetRefHandleManager = &ReplicationSystemInternal->GetNetRefHandleManager();
	ReplicationBridge = Parameters.ReplicationSystem->GetReplicationBridge();
	BaselineManager = &ReplicationSystemInternal->GetDeltaCompressionBaselineManager();
	ObjectReferenceCache = &ReplicationSystemInternal->GetObjectReferenceCache();
	ReplicationFiltering = &ReplicationSystemInternal->GetFiltering();
	ReplicationConditionals = &ReplicationSystemInternal->GetConditionals();
	const FNetBlobManager* NetBlobManager = &ReplicationSystemInternal->GetNetBlobManager();
	PartialNetObjectAttachmentHandler = NetBlobManager->GetPartialNetObjectAttachmentHandler();
	NetObjectBlobHandler = NetBlobManager->GetNetObjectBlobHandler();
	NetTypeStats = &ReplicationSystemInternal->GetNetTypeStats();

	// Init book keeping
	const int32 PreAllocatedBufferSize = Parameters.MaxReplicatedWriterObjectCount;
	ReplicatedObjects.SetNumZeroed(PreAllocatedBufferSize);
	ReplicatedObjectsRecordInfoLists.SetNumZeroed(PreAllocatedBufferSize);
	SchedulingPriorities.SetNumZeroed(PreAllocatedBufferSize);

	// NOTE: Currently disabled because FReplicationWriter until the performance impact of TNetChunkedArray can be measured on the server.
	//OnLargestIndexIncreaseHandle = NetRefHandleManager->GetLargestIndexIncreaseDelegate().AddRaw(this, &FReplicationWriter::OnLargestIndexIncrease);

	ObjectsPendingDestroy.Init(Parameters.MaxActiveReplicatedObjectCount);
	ObjectsWithDirtyChanges.Init(Parameters.MaxActiveReplicatedObjectCount);
	ObjectsInScope.Init(Parameters.MaxActiveReplicatedObjectCount);	
	WriteContext.ObjectsWrittenThisPacket.Init(Parameters.MaxActiveReplicatedObjectCount);

	// Attachments init
	SetupReplicationInfoForAttachmentsToObjectsNotInScope();

	bReplicationEnabled = false;
}

void FReplicationWriter::GetInitialChangeMask(ChangeMaskStorageType* ChangeMaskData, const FReplicationProtocol* Protocol)
{
	FNetBitArrayView ChangeMask(ChangeMaskData, Protocol->ChangeMaskBitCount, FNetBitArrayView::NoResetNoValidate);

	// Just fill with all dirty for now
	ChangeMask.SetAllBits();
}

void FReplicationWriter::StartReplication(uint32 InternalIndex)
{
	FReplicationInfo& Info = GetReplicationInfo(InternalIndex);

	ensureMsgf(Info.GetState() == EReplicatedObjectState::Invalid, TEXT("Object ( InternalIndex: %u ) is in state %s in StartReplication."), InternalIndex, LexToString(Info.GetState()));
	if (InternalIndex != ObjectIndexForOOBAttachment && Attachments.HasUnsentAttachments(ENetObjectAttachmentType::Normal, InternalIndex))
	{
		UE_LOG(LogIris, Error, TEXT("FReplicationWriter::StartReplication - Expected object %s to not to have any queued up attachments"), *NetRefHandleManager->PrintObjectFromIndex(InternalIndex));
		ensure(false);
		Attachments.DropAllAttachments(ENetObjectAttachmentType::Normal, InternalIndex);
	}

	// Reset info
	Info = FReplicationInfo();
	Info.LastAckedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	Info.PendingBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;

	const uint32 bIsDestructionInfo = NetRefHandleManager->GetIsDestroyedStartupObject(InternalIndex) ? 1U : 0U;	
	if (bIsDestructionInfo)
	{
		// Check status of original object about to be destroyed, if it has been confirmed as created we do not replicate the destruction info object at all
		if (const uint32 OriginalInternalIndex = NetRefHandleManager->GetOriginalDestroyedStartupObjectIndex(InternalIndex))
		{
			const FReplicationInfo& OriginalInfo = GetReplicationInfo(OriginalInternalIndex);
			if (OriginalInfo.GetState() != EReplicatedObjectState::Invalid && OriginalInfo.IsCreationConfirmed)
			{
				// We do not need to send the destruction info so we mark it as PermanentlyDestroyed
				SetState(InternalIndex, EReplicatedObjectState::PermanentlyDestroyed);

				Info.IsDestructionInfo = bIsDestructionInfo;
				Info.IsCreationConfirmed = 1U;

				NetRefHandleManager->AddNetObjectRef(InternalIndex);

				return;
			}
		}
	}

	// Pending create
	SetState(InternalIndex, EReplicatedObjectState::PendingCreate);

	const FNetRefHandleManager::FReplicatedObjectData& Data = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
	NetRefHandleManager->AddNetObjectRef(InternalIndex);

	Info.ChangeMaskBitCount = Data.Protocol->ChangeMaskBitCount;
	Info.HasDirtySubObjects = 1U;
	Info.IsSubObject = NetRefHandleManager->GetSubObjectInternalIndices().GetBit(InternalIndex);
	Info.HasDirtyChangeMask = 1U;
	Info.HasAttachments = 0U;
	Info.HasChangemaskFilter = EnumHasAnyFlags(Data.Protocol->ProtocolTraits, EReplicationProtocolTraits::HasConditionalChangeMask);
	Info.IsDestructionInfo = bIsDestructionInfo;
	Info.IsCreationConfirmed = 0U;
	Info.TearOff = Data.bTearOff;
	Info.FlushFlags = GetDefaultFlushFlags();
	Info.SubObjectPendingDestroy = 0U;
	Info.IsDeltaCompressionEnabled = BaselineManager->GetDeltaCompressionStatus(InternalIndex) == ENetObjectDeltaCompressionStatus::Allow ? 1U : 0U;
	Info.LastAckedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	Info.PendingBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;

	// Allocate storage for changemask (if needed)
	FChangeMaskStorageOrPointer::Alloc(Info.ChangeMaskOrPtr, Info.ChangeMaskBitCount, s_DefaultChangeMaskAllocator);

	// Get Initial ChangeMask
	GetInitialChangeMask(Info.GetChangeMaskStoragePointer(), Data.Protocol);

	// Reset record for object
	ReplicationRecord.ResetList(ReplicatedObjectsRecordInfoLists[InternalIndex]);

	// Set initial priority
	// Subobject are always set to have zero priority as they are replicated with owner
	// Currently we also do this for dependent objects to support objects with zero priority that should only replicate with parents
	SchedulingPriorities[InternalIndex] = (Data.IsDependentObject() || Info.IsSubObject) ? 0.f : CreatePriority;

	UE_LOG_REPLICATIONWRITER_CONN(TEXT("ReplicationWriter.StartReplication for ( InternalIndex: %u ) %s"), InternalIndex, ToCStr(Data.RefHandle.ToString()));

	ObjectsWithDirtyChanges.SetBit(InternalIndex);

	// Subobject needs to mark its owner as dirty as the subobject could have been filtered out and now allowed to replicate again.
	if (Info.IsSubObject)
	{
		const uint32 RootObjectInternalIndex = NetRefHandleManager->GetRootObjectInternalIndexOfSubObject(InternalIndex);
		if (ensure(RootObjectInternalIndex != FNetRefHandleManager::InvalidInternalIndex))
		{
			FReplicationInfo& OwnerInfo = ReplicatedObjects[RootObjectInternalIndex];
			if (OwnerInfo.GetState() != EReplicatedObjectState::Invalid && ensureMsgf(OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy, TEXT("Unsupported state %s"), LexToString(OwnerInfo.GetState())))
			{
				ObjectsWithDirtyChanges.SetBit(RootObjectInternalIndex);
				OwnerInfo.HasDirtySubObjects = 1U;
			}
		}
	}
}

void FReplicationWriter::StopReplication(uint32 InternalIndex)
{
	FReplicationInfo& Info = GetReplicationInfo(InternalIndex);

	// Invalidate state
	SetState(InternalIndex, EReplicatedObjectState::Invalid);

	// Need to free allocated ChangeMask (if it is allocated)
	FChangeMaskStorageOrPointer::Free(Info.ChangeMaskOrPtr, Info.ChangeMaskBitCount, s_DefaultChangeMaskAllocator);
	
	Info.IsCreationConfirmed = 0U;

	// Remove from objects with dirty changes
	ObjectsWithDirtyChanges.ClearBit(InternalIndex);

	// Remove from pending destroy
	ObjectsPendingDestroy.ClearBit(InternalIndex);

	// Explicitly remove from objects in scope since we might call StopReplication from outside ScopeUpdate
	ObjectsInScope.ClearBit(InternalIndex);

	UE_LOG_REPLICATIONWRITER_CONN(TEXT("ReplicationWriter.StopReplication for ( InternalIndex: %u )"), InternalIndex);
	NetRefHandleManager->ReleaseNetObjectRef(InternalIndex);

	Attachments.DropAllAttachments(ENetObjectAttachmentType::Normal, InternalIndex);

	// Release baselines
	if (Info.PendingBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		BaselineManager->DestroyBaseline(Parameters.ConnectionId, InternalIndex, Info.PendingBaselineIndex);
	}
	if (Info.PendingBaselineIndex != Info.LastAckedBaselineIndex && Info.LastAckedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		BaselineManager->DestroyBaseline(Parameters.ConnectionId, InternalIndex, Info.LastAckedBaselineIndex);
	}

	Info.PendingBaselineIndex = Info.LastAckedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
}

FReplicationWriter::FReplicationInfo& FReplicationWriter::GetReplicationInfo(uint32 InternalIndex)
{
	return ReplicatedObjects[InternalIndex];
}

const FReplicationWriter::FReplicationInfo& FReplicationWriter::GetReplicationInfo(uint32 InternalIndex) const
{
	return ReplicatedObjects[InternalIndex];
}

void FReplicationWriter::WriteNetRefHandleId(FNetSerializationContext& Context, FNetRefHandle Handle)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	UE_NET_TRACE_OBJECT_SCOPE(Handle, *Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Verbose);
	WritePackedUint64(Writer, Handle.GetId());
}

uint32 FReplicationWriter::GetDefaultFlushFlags() const
{
	// By default we currently always flush if we have pending reliable attachments when EndReplication is called for a NetObject.
	return EFlushFlags::FlushFlags_FlushReliable;
}

uint32 FReplicationWriter::GetFlushStatus(uint32 InternalIndex, const FReplicationInfo& Info, uint32 FlushFlagsToTest) const
{
	uint32 FlushFlags = EFlushFlags::FlushFlags_None;

	if (FlushFlagsToTest == EFlushFlags::FlushFlags_None)
	{
		return FlushFlags;
	}

	if (!!(FlushFlagsToTest & EFlushFlags::FlushFlags_FlushState) && (Info.HasDirtyChangeMask || HasInFlightStateChanges(InternalIndex, Info) || IsObjectPartOfActiveHugeObject(InternalIndex)))
	{
		FlushFlags |= EFlushFlags::FlushFlags_FlushState;
	}

	if (!!(FlushFlagsToTest & EFlushFlags::FlushFlags_FlushReliable) && !Attachments.IsAllReliableSentAndAcked(ENetObjectAttachmentType::Normal, InternalIndex))
	{
		FlushFlags |= EFlushFlags::FlushFlags_FlushReliable;
	}

	// Do we have a tear-off for the subobject in-flight?
	if (!!(FlushFlagsToTest & EFlushFlags::FlushFlags_FlushTornOffSubObjects) && (Info.IsSubObject && Info.TearOff && (Info.GetState() == EReplicatedObjectState::WaitOnDestroyConfirmation)))
	{
		FlushFlags |= EFlushFlags::FlushFlags_FlushTornOffSubObjects;
	}

	if (!Info.IsSubObject && FlushFlags != FlushFlagsToTest)
	{
		// Check status of SubObjects as well.
		for (uint32 SubObjectIndex : NetRefHandleManager->GetSubObjects(InternalIndex))
		{
			const FReplicationInfo& SubObjectInfo = GetReplicationInfo(SubObjectIndex);
			FlushFlags |= GetFlushStatus(SubObjectIndex, SubObjectInfo, FlushFlagsToTest);

			if (FlushFlags == FlushFlagsToTest)
			{
				break;
			}
		}
	}

	return FlushFlags;
}

void FReplicationWriter::SetPendingDestroyOrSubObjectPendingDestroyState(uint32 InternalIndex, FReplicationInfo& Info)
{
	if (Info.IsSubObject)
	{
		// Subobject destroyed before its owner is explicitly replicated as state data.
		FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
		if (ObjectData.IsSubObject())
		{
			// If owner is not pending destroy we mark the state of the SubObject to SubObjectPendingDestroy and mark owner as having dirty subobjects which will 
			// destroy the subobject using the replicated state path of the owner.
			FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
			if ((OwnerInfo.GetState() != EReplicatedObjectState::Invalid) && (OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy))
			{
				MarkObjectDirty(ObjectData.SubObjectRootIndex, "SetPendingDestroyOrSubObjectPendingDestroyState");
				OwnerInfo.HasDirtySubObjects = 1U;

				SetState(InternalIndex, EReplicatedObjectState::SubObjectPendingDestroy);
				MarkObjectDirty(InternalIndex, "SetPendingDestroyOrSubObjectPendingDestroyState2");
				ObjectsPendingDestroy.SetBit(InternalIndex);
				Info.SubObjectPendingDestroy = 1U;
				return;
			}
		}
	}
	else if (Info.HasDirtySubObjects)
	{
		// If the owner is destroyed, all subobjects in the EReplicatedObjectState::SubObjectPendingDestroy state must also be marked as PendingDestroy as owner no longer will be replicated
		for (uint32 SubObjectIndex : NetRefHandleManager->GetSubObjects(InternalIndex))
		{
			FReplicationInfo& SubObjectInfo = GetReplicationInfo(SubObjectIndex);
			if (SubObjectInfo.GetState() == EReplicatedObjectState::SubObjectPendingDestroy)
			{
				SubObjectInfo.SetState(EReplicatedObjectState::PendingDestroy);
				SubObjectInfo.SubObjectPendingDestroy = 0U;
				ObjectsWithDirtyChanges.ClearBit(SubObjectIndex);
			}
		}
	}
				
	ObjectsPendingDestroy.SetBit(InternalIndex);
	ObjectsWithDirtyChanges.ClearBit(InternalIndex);
	SetState(InternalIndex, EReplicatedObjectState::PendingDestroy);				
	Info.HasDirtyChangeMask = 0U;
}

void FReplicationWriter::UpdateScope(const FNetBitArrayView& UpdatedScope)
{
	//IRIS_PROFILER_SCOPE(FReplicationWriter_ScopeUpdate);

	auto NewObjectFunctor = [this](uint32 Index)
	{
		// We can only start replicating an object that is not currently replicated
		FReplicationInfo& Info = GetReplicationInfo(Index);
		const EReplicatedObjectState State = Info.GetState();

		if (State == EReplicatedObjectState::Invalid)
		{
			StartReplication(Index);
		}
		else if (State == EReplicatedObjectState::WaitOnFlush)
		{			
			if (ensureMsgf(!Info.TearOff, TEXT("We cannot cancel flush for an object pending tearoff ( InternalIndex: %u )"), Index))
			{
				// If we are waiting on flush but are re-added to scope we reset flush flags to default.
				ObjectsPendingDestroy.ClearBit(Index);
				Info.FlushFlags = GetDefaultFlushFlags();
				SetState(Index, EReplicatedObjectState::Created);

				// If we have accumulated changes while WaitingOnFlush, we should send them now
				Info.SubObjectPendingDestroy = 0U;
				Info.HasDirtyChangeMask |= FNetBitArrayView(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount).IsAnyBitSet();
				ObjectsWithDirtyChanges.SetBitValue(Index, Info.HasDirtyChangeMask);
			}
		}
		else if (State == EReplicatedObjectState::WaitOnCreateConfirmation)
		{
			// Need to restore as we might have been in case where we was pending destroy
			ObjectsPendingDestroy.ClearBit(Index);
			Info.FlushFlags = GetDefaultFlushFlags();

			// If we have accumulated changes while waiting on flush, we should send them now
			Info.SubObjectPendingDestroy = 0U;
			Info.HasDirtyChangeMask |= FNetBitArrayView(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount).IsAnyBitSet();
			ObjectsWithDirtyChanges.SetBitValue(Index, Info.HasDirtyChangeMask);
		}
		else if (State == EReplicatedObjectState::WaitOnDestroyConfirmation || State == EReplicatedObjectState::CancelPendingDestroy)
		{
			// Need to clear the pending destroy bit or else the object will be masked out of ObjectsInScope.
			// Keep the SubObjectPendingDestroy status as is until we know if the destroy packet was received or not.
			ObjectsPendingDestroy.ClearBit(Index);
			SetState(Index, EReplicatedObjectState::CancelPendingDestroy);
		}
		else if (State == EReplicatedObjectState::SubObjectPendingDestroy || State == EReplicatedObjectState::PendingDestroy)
		{
			// Object was waiting to be destroyed but should now resume replication.
			// If the object has been created we can go back to Created state, otherwise we go back to WaitOnCreateConfirmation
			SetState(Index, EReplicatedObjectState::WaitOnDestroyConfirmation);
			SetState(Index, EReplicatedObjectState::CancelPendingDestroy);
			SetState(Index, (Info.IsCreationConfirmed ? EReplicatedObjectState::Created : EReplicatedObjectState::WaitOnCreateConfirmation));

			Info.SubObjectPendingDestroy = 0U;
			Info.HasDirtyChangeMask |= FNetBitArrayView(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount).IsAnyBitSet();
			ObjectsWithDirtyChanges.SetBitValue(Index, Info.HasDirtyChangeMask);
			ObjectsPendingDestroy.ClearBit(Index);

			if (State == EReplicatedObjectState::SubObjectPendingDestroy)
			{
				// If owner is not pending destroy we mark it as dirty as appropriate
				FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(Index);
				FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
				if (OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy)
				{
					ensureMsgf(!bValidateObjectsWithDirtyChanges || OwnerInfo.GetState() != EReplicatedObjectState::Invalid, TEXT("Object ( InternalIndex: %u ) with Invalid state potentially marked dirty."), ObjectData.SubObjectRootIndex);
					ensureMsgf(!OwnerInfo.TearOff, TEXT("Parent is tearing off ( InternalIndex: %u ) currently in State: %s "), ObjectData.SubObjectRootIndex, LexToString(OwnerInfo.GetState()));
					OwnerInfo.HasDirtySubObjects |= Info.HasDirtyChangeMask;
					ObjectsWithDirtyChanges.SetBitValue(ObjectData.SubObjectRootIndex, ObjectsWithDirtyChanges.GetBit(ObjectData.SubObjectRootIndex) || Info.HasDirtyChangeMask);
				}
			}
			else if (!Info.IsSubObject)
			{
				// If there are subobjects pending destroy we should make sure they're once again resorting to getting destroyed via state replication.
				bool bHasSubObjectsPendingDestroy = false;
				for (uint32 SubObjectIndex : NetRefHandleManager->GetSubObjects(Index))
				{
					FReplicationInfo& SubObjectInfo = GetReplicationInfo(SubObjectIndex);
					if (SubObjectInfo.GetState() == EReplicatedObjectState::PendingDestroy)
					{
						SubObjectInfo.SetState(EReplicatedObjectState::SubObjectPendingDestroy);
						SubObjectInfo.SubObjectPendingDestroy = 1U;

						ObjectsWithDirtyChanges.SetBit(SubObjectIndex);

						bHasSubObjectsPendingDestroy = true;
					}
				}

				if (bHasSubObjectsPendingDestroy)
				{
					ObjectsWithDirtyChanges.SetBit(Index);
					Info.HasDirtySubObjects = 1U;
				}
			}
		}
		else
		{
			UE_LOG_REPLICATIONWRITER_CONN(TEXT("New object added to scope, Waiting to start replication for ( InternalIndex: %u ) currently in State: %s "), Index, LexToString(State));

			ensureMsgf(!ObjectsWithDirtyChanges.GetBit(Index) , TEXT("New object added to scope, Waiting to start replication for ( InternalIndex: %u ) currently in State: %s "), Index, LexToString(State));
			ensureMsgf(!Info.HasDirtyChangeMask, TEXT("New object added to scope, Waiting to start replication for ( InternalIndex: %u ) currently in State: %s "), Index, LexToString(State));
		}
	};

	auto DestroyedObjectFunctor = [this](uint32 Index) 
	{
		// Request object to be destroyed
		FReplicationInfo& Info = GetReplicationInfo(Index);

		// We handle objects marked for tear-off using the state update path.
		if (Info.TearOff)
		{
			return;
		}

		const EReplicatedObjectState State = Info.GetState();
		if (State < EReplicatedObjectState::PendingDestroy)
		{
			// We have not sent the object yet so we can just stop replication
			if (State == EReplicatedObjectState::PendingCreate)
			{
				StopReplication(Index);
			}
			else if (State == EReplicatedObjectState::CancelPendingDestroy)
			{
				// If we wanted to cancel the pending destroy but now want to destroy the object again we can resume waiting for the destroy.
				ObjectsPendingDestroy.SetBit(Index);
				SetState(Index, EReplicatedObjectState::WaitOnDestroyConfirmation);
			}
			else if (const uint32 FlushFlags = GetFlushStatus(Index, Info, Info.FlushFlags))
			{
				// Store info about what we need to flush
				Info.FlushFlags = FlushFlags;

				if (State != EReplicatedObjectState::WaitOnCreateConfirmation)
				{
					SetState(Index, EReplicatedObjectState::WaitOnFlush);

					// If we do not have any state data to flush we can clear the has dirty states flag
					if ((FlushFlags & FlushFlags_FlushState) == 0U)
					{
						Info.HasDirtyChangeMask = 0U;
					}
				}

				// Mark object as pending destroy so that we can poll the flush status in WriteObjectPendingDestroy
				ObjectsPendingDestroy.SetBit(Index);
			}
			else
			{
				SetPendingDestroyOrSubObjectPendingDestroyState(Index, Info);
			}
		}
		else if (State == EReplicatedObjectState::PermanentlyDestroyed)
		{
			StopReplication(Index);
		}
	};

	FNetBitArrayView CurrentScope = MakeNetBitArrayView(ObjectsInScope);
	FNetBitArrayView::ForAllExclusiveBits(UpdatedScope, CurrentScope, NewObjectFunctor, DestroyedObjectFunctor);
	CurrentScope.Copy(UpdatedScope);

	// No objects marked for destroy can be in scope
	ObjectsInScope.Combine(ObjectsPendingDestroy, FNetBitArrayBase::AndNotOp);
}

void FReplicationWriter::InternalUpdateDirtyChangeMasks(const FChangeMaskCache& CachedChangeMasks, EFlushFlags ExtraFlushFlags, bool bMarkForTearOff)
{
	//IRIS_PROFILER_SCOPE(FReplicationWriter_UpdateDirtyChangeMasks);

	const uint32 MarkForTearOff = bMarkForTearOff ? 1U : 0U;
	const ChangeMaskStorageType* StoragePtr = CachedChangeMasks.Storage.GetData();

	for (const auto& Entry : CachedChangeMasks.Indices)
	{
		FReplicationInfo& Info = ReplicatedObjects[Entry.InternalIndex];
		if (Info.GetState() == EReplicatedObjectState::Invalid)
		{
			continue;
		}

		// We want to accumulate dirty changes even if we are going out of scope in case we get re-added to scope before replication has ended.
		const bool bMarkScopedObjectDirty = ObjectsInScope.GetBit(Entry.InternalIndex);
		if (bMarkScopedObjectDirty)
		{
			MarkObjectDirty(Entry.InternalIndex, "UpdateDirtyChangeMasks");
		}

		if (Entry.bMarkSubObjectOwnerDirty == 0U)
		{		
			// Mark object for TearOff, that is that we will stop replication as soon as the tear-off is acknowledged
			Info.TearOff |= MarkForTearOff;

			// Update flush flags
			Info.FlushFlags = Info.FlushFlags | ExtraFlushFlags;

			// Merge in dirty changes
			if (Entry.bHasDirtyChangeMask)
			{
				const uint32 ChangeMaskBitCount = Info.ChangeMaskBitCount;

				// Merge updated changes
				FNetBitArrayView Changes(Info.GetChangeMaskStoragePointer(), ChangeMaskBitCount);

				const FNetBitArrayView UpdatedChanges = MakeNetBitArrayView(StoragePtr + Entry.StorageOffset, ChangeMaskBitCount);
				Changes.Combine(UpdatedChanges, FNetBitArrayView::OrOp);

				// Mark changemask as dirty
				Info.HasDirtyChangeMask = bMarkScopedObjectDirty ? 1U : 0U;
			}
		}
		else
		{
			Info.HasDirtySubObjects = 1U;
		}
	}

	//UE_LOG_REPLICATIONWRITER(TEXT("FReplicationWriter::UpdateDirtyChangeMasks() Updated %u Objects for ConnectionId:%u, ReplicationSystemId: %u."), CachedChangeMasks.Indices.Num(), Parameters.ConnectionId, Parameters.ReplicationSystem->GetId());	
}

void FReplicationWriter::NotifyDestroyedObjectPendingTearOff(FInternalNetRefIndex ObjectInternalIndex)
{
	const FReplicationInfo& ReplicationInfo = GetReplicationInfo(ObjectInternalIndex);
	if (ReplicationInfo.GetState() == EReplicatedObjectState::PendingCreate)
	{
		check(ReplicationInfo.TearOff == 1U);
		StopReplication(ObjectInternalIndex);
	}
}

const FNetBitArray& FReplicationWriter::GetObjectsRequiringPriorityUpdate() const
{
	return ObjectsWithDirtyChanges;
}

void FReplicationWriter::UpdatePriorities(const float* UpdatedPriorities)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_UpdatePriorities);

	auto UpdatePriority = [&LocalPriorities = SchedulingPriorities, UpdatedPriorities](uint32 Index)
	{
		LocalPriorities[Index] += UpdatedPriorities[Index];
	};

	ObjectsWithDirtyChanges.ForAllSetBits(UpdatePriority);
}

void FReplicationWriter::ScheduleDependentObjects(uint32 Index, float ParentPriority, TArray<float>& LocalPriorities, FScheduleObjectInfo* ScheduledObjectIndices, uint32& OutScheduledObjectCount)
{
	const float DependentObjectPriorityBump = UE_KINDA_SMALL_NUMBER;

	for (const FDependentObjectInfo& DependentObjectInfo : NetRefHandleManager->GetDependentObjectInfos(Index))
	{
		const FInternalNetRefIndex DependentInternalIndex = DependentObjectInfo.NetRefIndex;
		float UpdatedPriority = ParentPriority;

		if (ObjectsWithDirtyChanges.GetBit(DependentInternalIndex))
		{
			const FReplicationInfo& DependentInfo = this->GetReplicationInfo(DependentInternalIndex);

			const bool bReplicateBeforeParent =	(DependentObjectInfo.SchedulingHint == EDependentObjectSchedulingHint::ScheduleBeforeParent) || ((DependentObjectInfo.SchedulingHint == EDependentObjectSchedulingHint::ScheduleBeforeParentIfInitialState) && IsInitialState(DependentInfo.GetState()));

			if (bReplicateBeforeParent)
			{
				// Bump prio of dependent object to be scheduled before its parent.
				UpdatedPriority = FMath::Max(std::nextafter(ParentPriority, std::numeric_limits<float>::infinity()), LocalPriorities[DependentInternalIndex]);
				LocalPriorities[DependentInternalIndex] = UpdatedPriority;

				// Schedule it, it does not matter if we add it to the scheduled list multiple times
				FScheduleObjectInfo& ScheduledObjectInfo = ScheduledObjectIndices[OutScheduledObjectCount];
				ScheduledObjectInfo.Index = DependentInternalIndex;
				ScheduledObjectInfo.SortKey = UpdatedPriority;
				++OutScheduledObjectCount;								
			}
		}
		
		// We go through all dependent objects here even though it might not be 100% correct, but it will make sure that we respect
		// the scheduling order hint at least in relation to the parent, but a dependent object might also end up replicating before its parent`s parent
		if (NetRefHandleManager->GetObjectsWithDependentObjectsInternalIndices().GetBit(DependentInternalIndex))
		{
			ScheduleDependentObjects(DependentInternalIndex, UpdatedPriority, LocalPriorities, ScheduledObjectIndices, OutScheduledObjectCount);
		}
	}
}

uint32 FReplicationWriter::ScheduleObjects(FScheduleObjectInfo* OutScheduledObjectIndices)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_ScheduleObjects);

	uint32 ScheduledObjectCount = 0;

	FScheduleObjectInfo* ScheduledObjectIndices = OutScheduledObjectIndices;

	// Special index is handled later.
	ObjectsWithDirtyChanges.ClearBit(ObjectIndexForOOBAttachment);

	const FNetBitArray& UpdatedObjects = ObjectsWithDirtyChanges;
	const FNetBitArray& SubObjects = NetRefHandleManager->GetSubObjectInternalIndices();

	auto FillIndexListFunc = [&ScheduledObjectIndices, &ScheduledObjectCount, this](uint32 Index)
	{
		const float UpdatedPriority = SchedulingPriorities[Index];

		FScheduleObjectInfo& ScheduledObjectInfo = ScheduledObjectIndices[ScheduledObjectCount];
		ScheduledObjectInfo.Index = Index;
		ScheduledObjectInfo.SortKey = UpdatedPriority;

		if (UpdatedPriority >= FReplicationWriter::SchedulingThresholdPriority)
		{
			++ScheduledObjectCount;

			// If we have dependent objects that needs to replicate before parent we need to schedule them as well.
			if (NetRefHandleManager->GetObjectsWithDependentObjectsInternalIndices().GetBit(Index))
			{
				ScheduleDependentObjects(Index, UpdatedPriority, SchedulingPriorities, ScheduledObjectIndices, ScheduledObjectCount);
			}
		}
	};

	// Invoke functor for all updated objects that not are sub objects.
	FNetBitArray::ForAllSetBits(UpdatedObjects, SubObjects, FNetBitArray::AndNotOp, FillIndexListFunc);

	// we now have our list of objects to write.
	return ScheduledObjectCount;
}

uint32 FReplicationWriter::SortScheduledObjects(FScheduleObjectInfo* ScheduledObjectIndices, uint32 ScheduledObjectCount, uint32 StartIndex)
{
	check(ScheduledObjectCount > 0 && StartIndex <= ScheduledObjectCount);

	// Partial sort of scheduled objects
	{
		IRIS_PROFILER_SCOPE(FReplicationWriter_SortScheduledObjects);

		// We only need a partial sort of the highest priority objects as we wont be able to fit that much data in a packet anyway
		// $IRIS TODO: Implement and evaluate partial sort algorithm, currently we simply use std::partial_sort https://jira.it.epicgames.com/browse/UE-123444
		FScheduleObjectInfo* StartIt = ScheduledObjectIndices + StartIndex;
		FScheduleObjectInfo* EndIt = ScheduledObjectIndices + ScheduledObjectCount;
		FScheduleObjectInfo* SortIt = FMath::Min(StartIt + PartialSortObjectCount, EndIt);

		std::partial_sort(StartIt, SortIt, EndIt, [](const FScheduleObjectInfo& EntryA, const FScheduleObjectInfo& EntryB) { return EntryA.SortKey > EntryB.SortKey; });
	}

	return FMath::Min(ScheduledObjectCount - StartIndex, PartialSortObjectCount);
}

void FReplicationWriter::HandleDeliveredRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	EReplicatedObjectState DeliveredState = (EReplicatedObjectState)RecordInfo.ReplicatedObjectState;
	EReplicatedObjectState CurrentState = Info.GetState();
	const uint32 InternalIndex = RecordInfo.Index;
	
	if (CurrentState == EReplicatedObjectState::Invalid)
	{
		UE_LOG_REPLICATIONWRITER_WARNING(TEXT("FReplicationWriter::HandleDeliveredRecord - Warning Object ( InternalIndex: %u ) is invalid. DeliveredState %s WasDestroySubObject: %u"), InternalIndex, LexToString(DeliveredState), RecordInfo.WroteDestroySubObject)
		ensure(false);
		return;
	}
	
	// We confirmed a new baseline
	if (RecordInfo.NewBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		check(RecordInfo.NewBaselineIndex == Info.PendingBaselineIndex);

		// Destroy old baseline
		if (Info.LastAckedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
		{			
			BaselineManager->DestroyBaseline(Parameters.ConnectionId, InternalIndex, Info.LastAckedBaselineIndex);
			UE_LOG_REPLICATIONWRITER_CONN(TEXT("Destroyed old baseline %u for ( InternalIndex: %u )"), Info.LastAckedBaselineIndex, InternalIndex);			
		}
		Info.LastAckedBaselineIndex = RecordInfo.NewBaselineIndex;
		Info.PendingBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;

		UE_LOG_REPLICATIONWRITER_CONN(TEXT("Acknowledged baseline %u for ( InternalIndex: %u )"), RecordInfo.NewBaselineIndex, InternalIndex);
	}

	// Update state
	switch (DeliveredState)
	{
		case EReplicatedObjectState::WaitOnCreateConfirmation:
		{
			// if we are still waiting for CreateConfirmation
			if (CurrentState == EReplicatedObjectState::WaitOnCreateConfirmation)
			{
				// If this is a destruction info, just put it in the destroyed state
				if (Info.IsDestructionInfo)
				{
					SetState(InternalIndex, EReplicatedObjectState::PermanentlyDestroyed);
				}
				// If this object was teared off, it can now be considered as destroyed
				else if (RecordInfo.WroteTearOff)
				{
					SetState(InternalIndex, EReplicatedObjectState::PendingTearOff);
					SetState(InternalIndex, EReplicatedObjectState::Destroyed);
					StopReplication(InternalIndex);
				}
				else
				{
					SetState(InternalIndex, EReplicatedObjectState::Created);

					// Tear-off is marked as a flush
					if (Info.TearOff)
					{
						SetState(InternalIndex, EReplicatedObjectState::WaitOnFlush);
					}
					// so are objects marked for destroy requiring flush
					else if (ObjectsPendingDestroy.GetBit(InternalIndex))
					{
						SetState(InternalIndex, EReplicatedObjectState::WaitOnFlush);
					}
				}
			}
			Info.IsCreationConfirmed = 1U;
		}
		break;

		case EReplicatedObjectState::WaitOnDestroyConfirmation:
		{
			SetState(InternalIndex, EReplicatedObjectState::Destroyed);

			// It is now safe to stop tracking this object
			StopReplication(InternalIndex);

			if (CurrentState == EReplicatedObjectState::CancelPendingDestroy)
			{
				StartReplication(InternalIndex);
				ObjectsInScope.SetBit(InternalIndex);
			}
		}
		break;

		case EReplicatedObjectState::AttachmentToObjectNotInScope:
		{
			check(IsObjectIndexForOOBAttachment(InternalIndex));
			Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Delivered, ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment, AttachmentRecord);
		}
		return;

		case EReplicatedObjectState::HugeObject:
		{
			check(IsObjectIndexForOOBAttachment(InternalIndex));
			Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Delivered, ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment, AttachmentRecord);

			HugeObjectSendQueue.AckObjects([this](const FHugeObjectContext& HugeObjectContext)
			{
				// If we've sent an entire huge objects we can ack everything in the payload and continue replicating this object using normal means.
				const FReplicationInfo& HugeObjectReplicationInfo = this->GetReplicationInfo(HugeObjectContext.RootObjectInternalIndex);
				for (const FObjectRecord& ObjectRecord : HugeObjectContext.BatchRecord.ObjectReplicationRecords)
				{
					FReplicationInfo& ReplicationInfo = this->GetReplicationInfo(ObjectRecord.Record.Index);
					const uint32 ChangeMaskBitCount = ReplicationInfo.ChangeMaskBitCount;
					this->HandleDeliveredRecord(ObjectRecord.Record, ReplicationInfo, ObjectRecord.AttachmentRecord);
					if (ObjectRecord.Record.HasChangeMask)
					{
						FChangeMaskStorageOrPointer::Free(ObjectRecord.Record.ChangeMaskOrPtr, ChangeMaskBitCount, s_DefaultChangeMaskAllocator);
					}
				}

				// We need to explicitly acknowledge exports made through the huge object batch
				this->NetExports->AcknowledgeBatchExports(HugeObjectContext.BatchExports);
			});
		}
		return;

		default:
		break;
	}

	if (RecordInfo.HasAttachments)
	{
		Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Delivered, ENetObjectAttachmentType::Normal, InternalIndex, AttachmentRecord);
	}

	// Must process WaitOnflush after attachments in order to correctly evaluate flush-status if needed
	if (Info.GetState() == EReplicatedObjectState::WaitOnFlush)
	{
		bool bStillPendingFlush = false;
		if ((RecordInfo.HasChangeMask || Info.HasDirtyChangeMask) && !!(Info.FlushFlags & EFlushFlags::FlushFlags_FlushState))
		{
			bStillPendingFlush |= (Info.HasDirtyChangeMask || HasInFlightStateChanges(ReplicationRecord.GetInfoForIndex(RecordInfo.NextIndex)) || IsObjectPartOfActiveHugeObject(InternalIndex));
		}

		if ((RecordInfo.HasAttachments || Info.HasAttachments) && !!(Info.FlushFlags & FlushFlags_FlushReliable))
		{
			bStillPendingFlush |= !Attachments.IsAllReliableSentAndAcked(ENetObjectAttachmentType::Normal, InternalIndex);
		}

		// This is a bit blunt as subobjects might be "acked" later but in this case it will be captured in WriteObjectsPendingDestroy
		if (!bStillPendingFlush && !Info.IsSubObject)
		{
			// Check status of SubObjects as well.
			for (uint32 SubObjectIndex : NetRefHandleManager->GetSubObjects(InternalIndex))
			{
				const FReplicationInfo& SubObjectInfo = GetReplicationInfo(SubObjectIndex);
				if (GetFlushStatus(SubObjectIndex, SubObjectInfo, Info.FlushFlags) != FlushFlags_None)
				{
					bStillPendingFlush = true;
					break;
				}
			}
		}

		if (!bStillPendingFlush)
		{
			if (Info.TearOff)
			{
				SetState(InternalIndex, EReplicatedObjectState::PendingTearOff);
				ObjectsWithDirtyChanges.SetBit(InternalIndex);

				// Must also mark owner dirty to make sure that we send the tearoff
				if (Info.IsSubObject)
				{
					FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
					if (ObjectData.IsSubObject())
					{
						FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
						MarkObjectDirty(ObjectData.SubObjectRootIndex, "HandleDeliveredRecordTearOff");
						OwnerInfo.HasDirtySubObjects = 1U;
					}
				}
			}
			else
			{
				SetPendingDestroyOrSubObjectPendingDestroyState(InternalIndex, Info);
			}
		}
	}
}

void FReplicationWriter::HandleDiscardedRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	const EReplicatedObjectState DeliveredState = (EReplicatedObjectState)RecordInfo.ReplicatedObjectState;
	const uint32 InternalIndex = RecordInfo.Index;

	// There are a couple of special cases we need to handle. Regular attachments are ignored since they don't require special handling at the moment.
	switch (DeliveredState)
	{
		// If we need to handle attachments this should return rather than fallback on some default path like HandleDeliveredRecord.
		case EReplicatedObjectState::AttachmentToObjectNotInScope:
		{
		}
		return;

		case EReplicatedObjectState::HugeObject:
		{
			check(IsObjectIndexForOOBAttachment(InternalIndex));

			// Deal with it similar to if the entire state has been sent as we need to go through all records.
			Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Discard, ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment, AttachmentRecord);

			HugeObjectSendQueue.AckObjects([this](const FHugeObjectContext& HugeObjectContext)
			{
				const FReplicationInfo& HugeObjectReplicationInfo = this->GetReplicationInfo(HugeObjectContext.RootObjectInternalIndex);
				for (const FObjectRecord& ObjectRecord : HugeObjectContext.BatchRecord.ObjectReplicationRecords)
				{
					FReplicationInfo& ReplicationInfo = this->GetReplicationInfo(ObjectRecord.Record.Index);
					const uint32 ChangeMaskBitCount = ReplicationInfo.ChangeMaskBitCount;
					this->HandleDiscardedRecord(ObjectRecord.Record, ReplicationInfo, ObjectRecord.AttachmentRecord);
					if (ObjectRecord.Record.HasChangeMask)
					{
						FChangeMaskStorageOrPointer::Free(ObjectRecord.Record.ChangeMaskOrPtr, ChangeMaskBitCount, s_DefaultChangeMaskAllocator);
					}
				}
			});
		}
		return;
	}
}

template<>
void FReplicationWriter::HandleDroppedRecord<FReplicationWriter::EReplicatedObjectState::WaitOnCreateConfirmation>(FReplicationWriter::EReplicatedObjectState CurrentState, const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	const uint32 InternalIndex = RecordInfo.Index;

	if (CurrentState < EReplicatedObjectState::Created)
	{
		// Until we have implemented cached creation info we cannot send creation info for destroyed objects
		// So we just have to StopReplication
		const bool bCanSendCreationInfo = !ObjectsPendingDestroy.GetBit(InternalIndex);
		if (bCanSendCreationInfo)
		{
			// Mark object as having dirty changes
			MarkObjectDirty(InternalIndex, "DroppedWaitOnCreate");

			// Resend creation data
			SetState(InternalIndex, EReplicatedObjectState::PendingCreate);

			// Must also restore changemask
			FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
			FNetBitArrayView LostChangeMask = FChangeMaskUtil::MakeChangeMask(RecordInfo.ChangeMaskOrPtr, Info.ChangeMaskBitCount);
			ChangeMask.Combine(LostChangeMask, FNetBitArrayView::OrOp);

			// Mark changemask dirty
			Info.HasDirtyChangeMask = 1U;

			// Indicate that we have dirty subobjects
			Info.HasDirtySubObjects = 1U;

			// Mark attachments as dirty
			Info.HasAttachments |= RecordInfo.HasAttachments;

			if (Info.IsSubObject)
			{
				// Mark owner dirty as well as subobjects only are scheduled together with owner
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectData(InternalIndex);
				uint32 SubObjectOwnerInternalIndex = ObjectData.SubObjectRootIndex;

				FReplicationInfo& SubObjectOwnerReplicationInfo = GetReplicationInfo(SubObjectOwnerInternalIndex);
				if (ensure(SubObjectOwnerReplicationInfo.GetState() < EReplicatedObjectState::PendingDestroy))
				{
					// Mark owner as dirty
					MarkObjectDirty(SubObjectOwnerInternalIndex, "DroppedWaitOnCreate2");

					// Indicate that we have dirty subobjects
					SubObjectOwnerReplicationInfo.HasDirtySubObjects = 1U;

					// Give slight priority bump to owner
					SchedulingPriorities[SubObjectOwnerInternalIndex] += FReplicationWriter::LostStatePriorityBump;
				}
			}
		}
		else
		{
			SetState(InternalIndex, EReplicatedObjectState::PendingCreate);
			StopReplication(InternalIndex);
		}
	}
	else if (CurrentState == EReplicatedObjectState::SubObjectPendingDestroy || CurrentState == EReplicatedObjectState::PendingDestroy)
	{
		// If Object has been destroyed while we where waiting for creation ack we can just stop replication
		SetState(InternalIndex, EReplicatedObjectState::WaitOnDestroyConfirmation);
		SetState(InternalIndex, EReplicatedObjectState::Destroyed);
		StopReplication(InternalIndex);
	}
}

template<>
void FReplicationWriter::HandleDroppedRecord<FReplicationWriter::EReplicatedObjectState::Created>(FReplicationWriter::EReplicatedObjectState CurrentState, const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	const uint32 InternalIndex = RecordInfo.Index;

	// An object in PendingDestroy/WaitOnDestroyConfirmation can end up being replicated again via CancelPendingDestroy.
	if (CurrentState < EReplicatedObjectState::Destroyed)
	{
		// Mask in any lost changes
		bool bNeedToResendAttachments = RecordInfo.HasAttachments;
		bool bNeedToResendState = false;

		FNetBitArrayView LostChangeMask = FChangeMaskUtil::MakeChangeMask(RecordInfo.ChangeMaskOrPtr, static_cast<uint32>(RecordInfo.HasChangeMask ? Info.ChangeMaskBitCount : 1U));
		if (RecordInfo.HasChangeMask)
		{
			// Iterate over all data in flight for this object and mask away any already re-transmitted changes
			// N.B. We don't check if this object is in huge object mode and check to see if any of these changes were part of that payload.
			const FReplicationRecord::FRecordInfo* CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(RecordInfo.NextIndex);
			while (CurrentRecordInfo)
			{
				if (CurrentRecordInfo->HasChangeMask)
				{
					const FNetBitArrayView CurrentRecordInfoChangeMask(FChangeMaskUtil::MakeChangeMask(CurrentRecordInfo->ChangeMaskOrPtr, Info.ChangeMaskBitCount));
					LostChangeMask.Combine(CurrentRecordInfoChangeMask, FNetBitArrayView::AndNotOp);
				}

				CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(CurrentRecordInfo->NextIndex);
			};

			bNeedToResendState = LostChangeMask.IsAnyBitSet();
		}

		// if we lost changes that are not already retransmitted we update the changemask
		if (bNeedToResendState | bNeedToResendAttachments)
		{
			if (bNeedToResendState)
			{
				FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
				ChangeMask.Combine(LostChangeMask, FNetBitArrayView::OrOp);
			}

			if (CurrentState < EReplicatedObjectState::PendingDestroy)
			{
				// Mark object as having dirty changes
				MarkObjectDirty(InternalIndex, "DroppedCreated");

				// Mark changemask as dirty
				Info.HasDirtyChangeMask |= bNeedToResendState;

				// Mark attachments as dirty
				Info.HasAttachments |= bNeedToResendAttachments;

				// Give slight priority bump
				SchedulingPriorities[InternalIndex] += FReplicationWriter::LostStatePriorityBump;

				if (Info.IsSubObject)
				{
					// Mark owner dirty as well as subobjects only are scheduled together with owner
					const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectData(InternalIndex);
					uint32 SubObjectOwnerInternalIndex = ObjectData.SubObjectRootIndex;

					FReplicationInfo& SubObjectOwnerReplicationInfo = GetReplicationInfo(SubObjectOwnerInternalIndex);

					if (ensure(SubObjectOwnerReplicationInfo.GetState() < EReplicatedObjectState::PendingDestroy))
					{
						// Mark owner as dirty
						MarkObjectDirty(SubObjectOwnerInternalIndex, "DroppedCreated2");

						// Indicate that we have dirty subobjects
						SubObjectOwnerReplicationInfo.HasDirtySubObjects = 1U;

						// Give slight priority bump to owner
						SchedulingPriorities[SubObjectOwnerInternalIndex] += FReplicationWriter::LostStatePriorityBump;
					}
				}
			}
		}
	}
}

template<>
void FReplicationWriter::HandleDroppedRecord<FReplicationWriter::EReplicatedObjectState::WaitOnDestroyConfirmation>(FReplicationWriter::EReplicatedObjectState CurrentState, const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	const uint32 InternalIndex = RecordInfo.Index;

	ensureMsgf(CurrentState == EReplicatedObjectState::WaitOnDestroyConfirmation || CurrentState == EReplicatedObjectState::CancelPendingDestroy, TEXT("Expected object ( InternalIndex: %u ) not to be in state %s"), InternalIndex, LexToString(CurrentState));

	// If we want to cancel the destroy and lost the destroy packet we can resume normal replication.
	if (CurrentState == EReplicatedObjectState::CancelPendingDestroy)
	{
		checkfSlow(!RecordInfo.WroteTearOff, TEXT("Torn off objects can't cancel destroy. ( InternalIndex: %u ) %s"), InternalIndex, ToCStr(NetRefHandleManager->GetReplicatedObjectData(InternalIndex).RefHandle.ToString()));

		if (RecordInfo.WroteDestroySubObject && Info.SubObjectPendingDestroy)
		{
			// If the subobject owner still is replicated and valid
			FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
			check(ObjectData.IsSubObject());

			FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
			Info.HasDirtyChangeMask |= ChangeMask.IsAnyBitSet();
			Info.SubObjectPendingDestroy = 0U;
			ObjectsPendingDestroy.ClearBit(InternalIndex);
			SetState(InternalIndex, EReplicatedObjectState::Created);

			ObjectsWithDirtyChanges.SetBitValue(InternalIndex, Info.HasDirtyChangeMask);

			// If owner is not pending destroy we mark it as dirty as appropriate.
			FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
			if (OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy)
			{
				OwnerInfo.HasDirtySubObjects |= Info.HasDirtyChangeMask;
				ensureMsgf(!bValidateObjectsWithDirtyChanges || OwnerInfo.GetState() != EReplicatedObjectState::Invalid, TEXT("Object (InternalIndex: % u) with Invalid state potentially marked dirty."), ObjectData.SubObjectRootIndex);
				ObjectsWithDirtyChanges.SetBitValue(ObjectData.SubObjectRootIndex, ObjectsWithDirtyChanges.GetBit(ObjectData.SubObjectRootIndex) || OwnerInfo.HasDirtySubObjects);
			}
		}
		else
		{
			// Check whether there are any dirty changes and mark object as dirty as appropriate.
			FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
			Info.HasDirtyChangeMask |= ChangeMask.IsAnyBitSet();

			ObjectsWithDirtyChanges.SetBitValue(InternalIndex, Info.HasDirtyChangeMask);

			ObjectsPendingDestroy.ClearBit(InternalIndex);

			SetState(InternalIndex, EReplicatedObjectState::Created);
		}	
	}
	else
	{
		// We dropped a packet with tear-off data, that is a destroy with state data so we need to resend that state
		if (RecordInfo.WroteTearOff)
		{
			ensureMsgf(Info.TearOff, TEXT("Expected object ( InternalIndex: %u ) to have TearOff set. Current state %s."), InternalIndex, LexToString(CurrentState));

			SetState(InternalIndex, EReplicatedObjectState::PendingTearOff);

			if (RecordInfo.HasChangeMask)
			{
				// Must also restore changemask
				FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
				FNetBitArrayView LostChangeMask = FChangeMaskUtil::MakeChangeMask(RecordInfo.ChangeMaskOrPtr, Info.ChangeMaskBitCount);
				ChangeMask.Combine(LostChangeMask, FNetBitArrayView::OrOp);

				// Mark changemask dirty
				Info.HasDirtyChangeMask = 1U;
			}

			// Mark attachments as dirty
			Info.HasAttachments |= RecordInfo.HasAttachments;

			// Mark object as having dirty changes
			MarkObjectDirty(InternalIndex, "DroppedWaitOnDestroy");

			// Mark parent as dirty
			uint32 ParentInternalIndex = InternalIndex;
			if (Info.IsSubObject)
			{
				const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
				if (ensure(ObjectData.SubObjectRootIndex != FNetRefHandleManager::InvalidInternalIndex))
				{
					ParentInternalIndex = ObjectData.SubObjectRootIndex;
					MarkObjectDirty(ParentInternalIndex, "DroppedWaitOnDestroy2");

					FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
					if (ensure(OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy))
					{
						// Indicate that we have dirty subobjects
						OwnerInfo.HasDirtySubObjects = 1U;
					}
				}
			}

			// Bump prio
			SchedulingPriorities[ParentInternalIndex] += FReplicationWriter::TearOffPriority;
		}
		else if (RecordInfo.WroteDestroySubObject && Info.SubObjectPendingDestroy)
		{
			// If the subobject owner still is replicated and valid
			FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
			check(ObjectData.IsSubObject());

			// If owner is not pending destroy we mark it as dirty so that we can replicate subobject destruction properly
			// We might get away with not doing this if owner or subobject does not have any unconfirmed changes in flight.
			FReplicationInfo& OwnerInfo = ReplicatedObjects[ObjectData.SubObjectRootIndex];
			if (OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy)
			{
				MarkObjectDirty(ObjectData.SubObjectRootIndex, "DroppedWaitOnDestroy2");
				OwnerInfo.HasDirtySubObjects = 1U;

				SetState(InternalIndex, EReplicatedObjectState::SubObjectPendingDestroy);
				ObjectsWithDirtyChanges.SetBit(InternalIndex);
				ObjectsPendingDestroy.SetBit(InternalIndex);
			}
		}
		else
		{
			// Mark for resend of Destroy
			ObjectsPendingDestroy.SetBit(InternalIndex);
			ObjectsWithDirtyChanges.ClearBit(InternalIndex);
			Info.HasDirtyChangeMask = 0U;

			SetState(InternalIndex, EReplicatedObjectState::PendingDestroy);
		}
	}
}

void FReplicationWriter::HandleDroppedRecord(const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
{
	EReplicatedObjectState LostObjectState = (EReplicatedObjectState)RecordInfo.ReplicatedObjectState;
	EReplicatedObjectState CurrentState = Info.GetState();
	const uint32 InternalIndex = RecordInfo.Index;

	check(CurrentState != EReplicatedObjectState::Invalid);

	UE_LOG_REPLICATIONWRITER_CONN(TEXT("Handle dropped data for ( InternalIndex: %u ) %s, LostState %s, CurrentState is %s"), InternalIndex, ToCStr(NetRefHandleManager->GetReplicatedObjectData(InternalIndex).RefHandle.ToString()), LexToString(LostObjectState), LexToString(CurrentState));

	// If we loose a baseline we must notify the BaselineManager and invalidate our pendingbaselineindex
	if (RecordInfo.NewBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		check(RecordInfo.NewBaselineIndex == Info.PendingBaselineIndex);
		UE_LOG_REPLICATIONWRITER_CONN(TEXT("Lost baseline %u for ( InternalIndex: %u )"), RecordInfo.NewBaselineIndex, InternalIndex);
		
		BaselineManager->LostBaseline(Parameters.ConnectionId, InternalIndex, RecordInfo.NewBaselineIndex);
		Info.PendingBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	}

	switch (LostObjectState)
	{
		// We dropped creation state, restore state to PendingCreate and bump priority to make sure we sent it again
		case EReplicatedObjectState::WaitOnCreateConfirmation:
		{
			HandleDroppedRecord<EReplicatedObjectState::WaitOnCreateConfirmation>(CurrentState, RecordInfo, Info, AttachmentRecord);
		}
		break;

		// Object is created, update lost state data unless object are currently being flushed/teared-off or destroyed
		case EReplicatedObjectState::Created:
		case EReplicatedObjectState::WaitOnFlush:
		{
			HandleDroppedRecord<EReplicatedObjectState::Created>(CurrentState, RecordInfo, Info, AttachmentRecord);
		}
		break;

		case EReplicatedObjectState::WaitOnDestroyConfirmation:
		{
			HandleDroppedRecord<EReplicatedObjectState::WaitOnDestroyConfirmation>(CurrentState, RecordInfo, Info, AttachmentRecord);
		}
		break;

		case EReplicatedObjectState::CancelPendingDestroy:
		{
			checkf(false, TEXT("%s"), TEXT("CancelPendingDestroy is not a state that should be replicated."));
		}
		break;

		case EReplicatedObjectState::AttachmentToObjectNotInScope:
		{
			check(IsObjectIndexForOOBAttachment(InternalIndex));
			Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Lost, ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment, AttachmentRecord);
		}
		return;

		case EReplicatedObjectState::HugeObject:
		{
			check(IsObjectIndexForOOBAttachment(InternalIndex));
			Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Lost, ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment, AttachmentRecord);
		}
		return;

		
		default:
		break;
	};

	if (RecordInfo.HasAttachments)
	{
		Attachments.ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Lost, ENetObjectAttachmentType::Normal, InternalIndex, AttachmentRecord);
	}
}

void FReplicationWriter::ProcessDeliveryNotification(EPacketDeliveryStatus PacketDeliveryStatus)
{
#if UE_NET_VALIDATE_REPLICATION_RECORD
	check(s_ValidateReplicationRecord(&ReplicationRecord, Parameters.MaxActiveReplicatedObjectCount + 1U, true));
#endif

	const uint32 RecordCount = ReplicationRecord.PopRecord();

	if (RecordCount > 0)
	{
		TReplicationRecordHelper Helper(ReplicatedObjects, ReplicatedObjectsRecordInfoLists, &ReplicationRecord);

		if (PacketDeliveryStatus == EPacketDeliveryStatus::Delivered)
		{
			Helper.Process(RecordCount,
				[this](const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
				{ 
					HandleDeliveredRecord(RecordInfo, Info, AttachmentRecord);
				}
			);
		}
		else if (PacketDeliveryStatus == EPacketDeliveryStatus::Lost)
		{
			Helper.Process(RecordCount,
				[this](const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
				{
					HandleDroppedRecord(RecordInfo, Info, AttachmentRecord);
				}
			);
		}
		else if (PacketDeliveryStatus == EPacketDeliveryStatus::Discard)
		{
			Helper.Process(RecordCount,
				[this](const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
				{
					HandleDiscardedRecord(RecordInfo, Info, AttachmentRecord);
				}
			);
		}
		else
		{
			checkf(false, TEXT("Unknown packet delivery status %u"), unsigned(PacketDeliveryStatus));
		}
	}
}

void FReplicationWriter::CreateObjectRecord(const FNetBitArrayView* ChangeMask, const FReplicationInfo& Info, const FBatchObjectInfo& ObjectInfo, FReplicationWriter::FObjectRecord& OutRecord)
{
	OutRecord.AttachmentRecord = ObjectInfo.AttachmentRecord.ReliableReplicationRecord;

	FReplicationRecord::FRecordInfo& RecordInfo = OutRecord.Record;

	RecordInfo.Index = ObjectInfo.InternalIndex;
	RecordInfo.ReplicatedObjectState = ObjectInfo.AttachmentType == ENetObjectAttachmentType::HugeObject ? uint8(EReplicatedObjectState::HugeObject) : (uint8)Info.GetState();
	RecordInfo.HasChangeMask = ChangeMask ? 1U : 0U;
	RecordInfo.HasAttachments = (OutRecord.AttachmentRecord.IsValid() ? 1U : 0U);
	RecordInfo.WroteTearOff = ObjectInfo.bSentTearOff;
	RecordInfo.WroteDestroySubObject = Info.SubObjectPendingDestroy;
	
	// If we wrote a new baseline we need to store it in the record
	RecordInfo.NewBaselineIndex = ObjectInfo.bSentState ? ObjectInfo.NewBaselineIndex : FDeltaCompressionBaselineManager::InvalidBaselineIndex;

	if (ChangeMask)
	{
		// $IRIS: TODO: Implement other type of changemask allocator that utilizes the the fifo nature of the record
		// https://jira.it.epicgames.com/browse/UE-127372
		// Allocate and copy changemask
		FChangeMaskStorageOrPointer::Alloc(RecordInfo.ChangeMaskOrPtr, ChangeMask->GetNumBits(), s_DefaultChangeMaskAllocator);
		FChangeMaskUtil::CopyChangeMask(RecordInfo.ChangeMaskOrPtr, *ChangeMask);
	}
	else
	{
		// Clear change mask
		RecordInfo.ChangeMaskOrPtr = FChangeMaskStorageOrPointer();
	}
}

void FReplicationWriter::CommitObjectRecord(uint32 InternalObjectIndex, const FObjectRecord& ObjectRecord)
{
	// Push and link replication record to data already in-flight
	ReplicationRecord.PushInfoAndAddToList(ReplicatedObjectsRecordInfoLists[InternalObjectIndex], ObjectRecord.Record, ObjectRecord.AttachmentRecord.ToUint64());
}

void FReplicationWriter::CommitBatchRecord(const FBatchRecord& BatchRecord)
{
	for (const FObjectRecord& ObjectRecord : BatchRecord.ObjectReplicationRecords)
	{
		 CommitObjectRecord(ObjectRecord.Record.Index, ObjectRecord);
	}
}

uint32 FReplicationWriter::WriteObjectsPendingDestroy(FNetSerializationContext& Context)
{
	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	UE_NET_TRACE_SCOPE(ObjectsPendingDestroy, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Write bit indicating that we did write, we also need one trailing bit to indicate the end
	uint16 WrittenCount = 0;

	// Write how many destroyed objects we have
	const uint32 HeaderPos = Writer.GetPosBits();
	Writer.WriteBits(WrittenCount, 16);

	if (Writer.IsOverflown())
	{
		return 0U;
	}

	bool bWroteAllDestroyedObjects = true;

	for (uint32 InternalIndex = 0U; (InternalIndex = ObjectsPendingDestroy.FindFirstOne(InternalIndex + 1U)) != FNetBitArray::InvalidIndex; )
	{
		FReplicationInfo& Info = GetReplicationInfo(InternalIndex);
		const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);

		// Don't send destroy until object creation has been acked.
		if (!Info.IsCreationConfirmed)
		{
			continue;
		}

		// Already waiting on destroy confirmation
		if (Info.GetState() == EReplicatedObjectState::WaitOnDestroyConfirmation)
		{
			continue;
		}

		if (Info.GetState() == EReplicatedObjectState::WaitOnFlush)
		{
			if (GetFlushStatus(InternalIndex, Info, Info.FlushFlags) != EFlushFlags::FlushFlags_None)
			{
				continue;
			}
			
			// Object and subobjects are now flushed and can be destroyed
			SetPendingDestroyOrSubObjectPendingDestroyState(InternalIndex, Info);
		}
		
		if (ObjectData.IsSubObject())
		{
			if (Info.SubObjectPendingDestroy)
			{
				// If the owner is pending destroy or already destroyed then we need to replicate the destruction for the subobject here.
				if (ObjectsPendingDestroy.GetBit(ObjectData.SubObjectRootIndex) || (ReplicatedObjects[ObjectData.SubObjectRootIndex].GetState() == EReplicatedObjectState::Invalid))
				{
					//			UE_LOG(LogTemp, Warning, TEXT("Object %s was a subobject but owner was destroyed so we need to destroy object normally"), ToCStr(ObjectData.RefHandle.ToString()));
					SetState(InternalIndex, EReplicatedObjectState::PendingDestroy);
					Info.SubObjectPendingDestroy = 0U;
				}
				else
				{
					// Replicate subobject destruction via state replication.
					continue;
				}
			}
			else
			{
				// If the owner is valid and isn't pending destroy we need to replicate destruction via state replication.
				if (!ObjectsPendingDestroy.GetBit(ObjectData.SubObjectRootIndex))
				{
					FReplicationInfo& OwnerInfo = GetReplicationInfo(ObjectData.SubObjectRootIndex);
					if (OwnerInfo.GetState() != EReplicatedObjectState::Invalid && OwnerInfo.GetState() < EReplicatedObjectState::PendingDestroy)
					{
						//			UE_LOG(LogTemp, Warning, TEXT("SubObject %s Should be destroyed by replicating owner"), ToCStr(ObjectData.RefHandle.ToString()));
						SetState(InternalIndex, EReplicatedObjectState::SubObjectPendingDestroy);
						Info.SubObjectPendingDestroy = 1U;

						OwnerInfo.HasDirtySubObjects = 1U;

						ObjectsWithDirtyChanges.SetBit(ObjectData.SubObjectRootIndex);
						ObjectsWithDirtyChanges.SetBit(InternalIndex);
						continue;
					}
				}
			}
		}

		// Unexpected. Get more info.
		if (Info.GetState() != EReplicatedObjectState::PendingDestroy)
		{
			ensureMsgf(Info.GetState() == EReplicatedObjectState::PendingDestroy, TEXT("Skipping writing destroy for object %s which is in unexpected state %s. IsSubObject: %u IsDestructionInfo: %u"), ToCStr(NetRefHandleManager->PrintObjectFromIndex(InternalIndex)), LexToString(Info.GetState()), Info.IsSubObject, Info.IsDestructionInfo);
			continue;
		}

		// We do not support destroying an object that is currently being sent as a huge object.
		if (IsObjectPartOfActiveHugeObject(InternalIndex))
		{
			UE_LOG(LogIris, Verbose, TEXT("Skipping writing destroy for object ( InternalIndex: %u ) which is part of active huge object."), InternalIndex);
			bWroteAllDestroyedObjects = false;
			continue;
		}

		UE_NET_TRACE_OBJECT_SCOPE(ObjectData.RefHandle,  Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

		FNetBitStreamRollbackScope RollbackScope(Writer);

		// Write handle with the needed bitCount
		WriteNetRefHandleId(Context, ObjectData.RefHandle);

		// If this is a explicit subobject we must also send the owner handle in the case that the client is currently queued data due to async loading.
		if (Writer.WriteBool(ObjectData.IsSubObject()))
		{			
			WriteNetRefHandleId(Context, NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectData.SubObjectRootIndex).RefHandle);
		}

		// Write bit indicating if the static instance should be destroyed or not (could skip the bit for dynamic objects)
		const bool bShouldDestroyInstance = ObjectData.RefHandle.IsDynamic() || NetRefHandleManager->GetIsDestroyedStartupObject(InternalIndex);
		Writer.WriteBool(bShouldDestroyInstance);

		if (!Writer.IsOverflown())
		{
			// Must update state before pushing record
			SetState(InternalIndex, EReplicatedObjectState::WaitOnDestroyConfirmation);
			Info.HasDirtyChangeMask = 0U;

			// update transmission record
			FBatchObjectInfo ObjectInfo = {};
			ObjectInfo.InternalIndex = InternalIndex;
			FObjectRecord ObjectRecord;
			CreateObjectRecord(nullptr, Info, ObjectInfo, ObjectRecord);
			CommitObjectRecord(InternalIndex, ObjectRecord);
	
			++WrittenCount;
		}
		else
		{
			bWroteAllDestroyedObjects = false;
			break;
		}
	}

	// Write Header
	{
		FNetBitStreamWriteScope WriteScope(Writer, HeaderPos);
		Writer.WriteBits(WrittenCount, 16);
	}

	WriteContext.bHasDestroyedObjectsToSend = !bWroteAllDestroyedObjects;

	return WrittenCount;
}

bool FReplicationWriter::CanSendObject(uint32 InternalIndex) const
{
	const FReplicationInfo& Info = GetReplicationInfo(InternalIndex);
	const EReplicatedObjectState State = Info.GetState();

	// Currently we do wait for CreateConfirmation before sending more data
	// We might want to change this and allow "bombing" creation info until we get confirmation to minimize latency
	// We also prevent objects from being transmitted if they are waiting on destroy/tear-off confirmation or cancelling destroy.
	if (State == EReplicatedObjectState::WaitOnCreateConfirmation || State == EReplicatedObjectState::WaitOnDestroyConfirmation || State == EReplicatedObjectState::CancelPendingDestroy)
	{
		return false;
	}

	// Don't send more recent state that could arrive before the huge state. We only need to check the parent.
	if (IsActiveHugeObject(InternalIndex))
	{
		if (!Info.IsSubObject)
		{
			return false;
		}
	}

	if (Info.HasDirtySubObjects)
	{
		for (FInternalNetRefIndex SubObjectInternalIndex : NetRefHandleManager->GetSubObjects(InternalIndex))
		{
			if (!CanSendObject(SubObjectInternalIndex))
			{
				return false;
			}
		}
	}

	// Currently we enforce a strict dependency on the state of initial dependent objects unless they are already serialized in the same packet
	if (NetRefHandleManager->GetObjectsWithDependentObjectsInternalIndices().GetBit(InternalIndex))
	{
		for (const FDependentObjectInfo DependentObjectInfo : NetRefHandleManager->GetDependentObjectInfos(InternalIndex))
		{
			const FInternalNetRefIndex DependentInternalIndex = DependentObjectInfo.NetRefIndex;

			// If the dependent object already has been written in this packet and is not part of a huge object we do not need to do any further checks.
			// Note: To avoid waiting for ack of huge dependent object we could remove the special scheduling of dependent actors and instead handle this when we write the batch
			if (WriteContext.ObjectsWrittenThisPacket.GetBit(DependentInternalIndex) && !IsActiveHugeObject(DependentInternalIndex))
			{
				continue;
			}

			const FReplicationInfo& DependentReplicationInfo = GetReplicationInfo(DependentInternalIndex);
			if (IsInitialState(DependentReplicationInfo.GetState()))
			{
				// if we cannot send the initial dependent object we must wait until we can.
				if (!CanSendObject(DependentInternalIndex))
				{
					UE_LOG(LogIris, Verbose, TEXT("ReplicationWriter: Cannot send internal index (%u) due to waiting on init dependency internal index (%d)"), InternalIndex, DependentInternalIndex);
					return false;
				}

				// if the dependent object are scheduled before parent and did not fit in this packet, we cannot write the parent either and have to wait until creation is confirmed
				if ((DependentObjectInfo.SchedulingHint == EDependentObjectSchedulingHint::ScheduleBeforeParent) && ObjectsWithDirtyChanges.GetBit(DependentInternalIndex))
				{
					UE_LOG(LogIris, Verbose, TEXT("ReplicationWriter: Cannot send internal index (%u) due to waiting on ScheduleBefore dependency internal index (%d)"), InternalIndex, DependentInternalIndex);
					return false;
				}
			}
		}
	}

	return true;
}

void FReplicationWriter::SerializeObjectStateDelta(FNetSerializationContext& Context, uint32 InternalIndex, const FReplicationInfo& Info, const FNetRefHandleManager::FReplicatedObjectData& ObjectData, const uint8* ReplicatedObjectStateBuffer, FDeltaCompressionBaseline& CurrentBaseline, uint32 CreatedBaselineIndex)
{
	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	// Write baseline info
	Writer.WriteBits(Info.LastAckedBaselineIndex, FDeltaCompressionBaselineManager::BaselineIndexBitCount);
	if (Info.LastAckedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
	{
		// Verify assumptions made about new baselineindices
		check(CurrentBaseline.IsValid());
		check(CreatedBaselineIndex == FDeltaCompressionBaselineManager::InvalidBaselineIndex || CreatedBaselineIndex == (Info.LastAckedBaselineIndex + 1) % FDeltaCompressionBaselineManager::MaxBaselineCount);

		// Do we want to store a new baseline?
		Writer.WriteBool(CreatedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex);

		UE_NET_TRACE_SCOPE(DeltaCompressed, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

		FReplicationProtocolOperations::SerializeWithMaskDelta(Context, Info.GetChangeMaskStoragePointer(), ReplicatedObjectStateBuffer, CurrentBaseline.StateBuffer, ObjectData.Protocol);
	}
	else
	{
		// if we do not have a valid LastAckedBaselineIndex we need to write the full CreatedBaselineIndex
		Writer.WriteBits(CreatedBaselineIndex, FDeltaCompressionBaselineManager::BaselineIndexBitCount);

		// $IRIS: $TODO: Consider Delta compressing against default state
		// Write non delta compressed state
		FReplicationProtocolOperations::SerializeWithMask(Context, Info.GetChangeMaskStoragePointer(), ReplicatedObjectStateBuffer, ObjectData.Protocol);	
	}
}

FReplicationWriter::EWriteObjectStatus FReplicationWriter::WriteObjectAndSubObjects(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags, FBatchInfo& OutBatchInfo)
{
	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
	
	FReplicationInfo& Info = GetReplicationInfo(InternalIndex);
	const EReplicatedObjectState State = Info.GetState();

	// As an object might still have subobjects pending destroy in the list of subobjects 
	if (State == EReplicatedObjectState::Invalid || !ensureMsgf(State > EReplicatedObjectState::Invalid && State < EReplicatedObjectState::PendingDestroy, TEXT("Unsupported state %s"), ToCStr(LexToString(State))))
	{
		return EWriteObjectStatus::InvalidState;
	}

	// If this object or anything else included in the batch did not write any data we will rollback any data written for the object
	FNetBitStreamRollbackScope ObjectRollbackScope(Writer);

	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
	const FNetRefHandle NetRefHandle = ObjectData.RefHandle;

	IRIS_PROFILER_PROTOCOL_NAME(ObjectData.Protocol?ObjectData.Protocol->DebugName->Name:TEXT("NoProtocol"));

#if UE_NET_TRACE_ENABLED
	FNetRefHandle NetRefHandleForTraceScope = NetRefHandle;
	if (WriteObjectFlags & EWriteObjectFlag::WriteObjectFlag_HugeObject)
	{
		const FInternalNetRefIndex HugeObjectInternalIndex = HugeObjectSendQueue.GetRootObjectInternalIndexForTrace();
		if (HugeObjectInternalIndex != FNetRefHandleManager::InvalidInternalIndex)
		{
			NetRefHandleForTraceScope = NetRefHandleManager->GetReplicatedObjectDataNoCheck(HugeObjectInternalIndex).RefHandle;
		}
	}
	UE_NET_TRACE_OBJECT_SCOPE(NetRefHandleForTraceScope, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
#endif

	// We only need to write batch info for root objects
	const bool bWriteBatchInfo = !Info.IsSubObject;
	uint32 InitialStateHeaderPos = 0U;
	const uint32 NumBitsUsedForBatchSize = Parameters.NumBitsUsedForBatchSize;

	// This is the beginning of what we treat as a batch on the receiving end
	if (bWriteBatchInfo)
	{
		// Write bit indicating that we are not a destruction info.
		const bool bIsDestructionInfo = false;
		Writer.WriteBool(bIsDestructionInfo);

	#if UE_NET_USE_READER_WRITER_SENTINEL
		WriteSentinelBits(&Writer, 8);
	#endif

		// A batch starts with (RefHandleId | BatchSize | bHasBatchObjectData | bHasExports)
		// We write the header up front, and then we seek back and update relevant info is the object + subobjects is successfully serialized along with necessary exports

		// We send the Index of the handle to the remote end
		// $IRIS: $TODO: consider sending the internal index instead to save bits and only send handle when we create the object, https://jira.it.epicgames.com/browse/UE-127373
		WriteNetRefHandleId(Context, NetRefHandle);

		InitialStateHeaderPos = Writer.GetPosBits();
		{
			UE_NET_TRACE_SCOPE(BatchSize, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
			Writer.WriteBits(0U, NumBitsUsedForBatchSize);
		}

		// Did we write serialize any data related to batch owner
		Writer.WriteBool(false);

		// If the batch has exports, they are at the end of the batch
		// We handle this on the reading side to avoid rewriting the entire object to insert exports up front.	
		Writer.WriteBool(false);
	}
	
	// Create a temporary batch entry. We don't want to push it to the batch info unless we're successful.
	FBatchObjectInfo BatchEntry = {};

	uint32 CreatedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	FDeltaCompressionBaseline CurrentBaseline;

	// We need to release created baseline if we fail to commit anything to batchrecord
	ON_SCOPE_EXIT
	{
		if (CreatedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
		{
			UE_LOG_REPLICATIONWRITER_CONN(TEXT("Destroy cancelled baseline %u for ( InternalIndex: %u )"), CreatedBaselineIndex, InternalIndex);
			BaselineManager->DestroyBaseline(Parameters.ConnectionId, InternalIndex, CreatedBaselineIndex);
		}
	};

	// Only write data for the object if we have data to write
	uint8* ReplicatedObjectStateBuffer = NetRefHandleManager->GetReplicatedObjectStateBufferNoCheck(InternalIndex);

	const bool bIsInitialState = IsInitialState(State);

	// Filter out changemasks that are not supposed to be replicated to this connection
	const bool bNeedToFilterChangeMask = (bIsInitialState || Info.HasDirtyChangeMask) && Info.HasChangemaskFilter;
	if (bNeedToFilterChangeMask)
	{
		ApplyFilterToChangeMask(OutBatchInfo.ParentInternalIndex, InternalIndex, Info, ObjectData.Protocol, ReplicatedObjectStateBuffer, bIsInitialState);
#if UE_NET_IRIS_CSV_STATS
		if (!bIsInitialState && Info.HasDirtyChangeMask)
		{
			WriteContext.Stats.AddNumberOfReplicatedObjectStatesMaskedOut(1U);
		}
#endif
	}

	const bool bIsObjectIndexForAttachment = IsObjectIndexForOOBAttachment(InternalIndex);
	const bool bHasState = (bIsInitialState || Info.HasDirtyChangeMask) && !!(WriteObjectFlags & EWriteObjectFlag::WriteObjectFlag_State);
	const bool bHasAttachments = (Info.HasAttachments || bIsObjectIndexForAttachment);
	const bool bWriteAttachments = bHasAttachments && !!(WriteObjectFlags & EWriteObjectFlag::WriteObjectFlag_Attachments);
	BatchEntry.bHasUnsentAttachments = bHasAttachments;

	// Check if we must defer tearoff until after flush
	const bool bSentTearOff = Info.TearOff && (GetFlushStatus(InternalIndex, Info, uint32(Info.FlushFlags | EFlushFlags::FlushFlags_FlushTornOffSubObjects)) == EFlushFlags::FlushFlags_None);

	Context.SetIsInitState(bIsInitialState);

	if (bHasState | bWriteAttachments | bSentTearOff | Info.SubObjectPendingDestroy)
	{
		// Only need to write the handle if this is a subobject
		if (Info.IsSubObject)
		{
			// We send the Index of the handle to the remote end
			// $IRIS: $TODO: consider sending the internal index instead to save bits and only send handle when we create the object, https://jira.it.epicgames.com/browse/UE-127373
			UE_NET_TRACE_SCOPE(SubObjectHandle, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
			WriteNetRefHandleId(Context, NetRefHandle);
		}

		// Store position of destroy header bits
		const uint32 ReplicatedDestroyHeaderBitPos = Writer.GetPosBits();

		// We only need to write this for actual replicated objects
		const bool bWriteReplicatedDestroyHeader = !bIsObjectIndexForAttachment;
		if (bWriteReplicatedDestroyHeader)
		{
			// Write destroy header bits, we always want to write the same number of bits to be able to update the header afterwards when we know what data made it into the packet
			Writer.WriteBits(0U, ReplicatedDestroyHeaderFlags_BitCount);
		}

		if (Writer.WriteBool(bHasState))
		{
#if UE_NET_USE_READER_WRITER_SENTINEL
			WriteSentinelBits(&Writer, 8);
#endif

			BatchEntry.bSentState = 1;

			// If the last transmitted baseline is acknowledged we can request a new baseline to be stored for the current state, we cannot compress against it until it has been acknowledged
			if (Info.IsDeltaCompressionEnabled)
			{
				// Lookup current baseline that we should compress against
				if (Info.LastAckedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
				{
					CurrentBaseline = BaselineManager->GetBaseline(Parameters.ConnectionId, InternalIndex, Info.LastAckedBaselineIndex);

					// If we cannot find the baseline it has become invalidated, if that is the case we must invalidate all tracking and request a new baseline to be created
					if (!CurrentBaseline.IsValid())
					{
						InvalidateBaseline(InternalIndex, Info);
					}
				}

				if (Info.PendingBaselineIndex == FDeltaCompressionBaselineManager::InvalidBaselineIndex)
				{
					// For new objects we start with baselineindex 0
					const uint32 NextBaselineIndex = bIsInitialState ? 0U : (Info.LastAckedBaselineIndex + 1U) % FDeltaCompressionBaselineManager::MaxBaselineCount;
					FDeltaCompressionBaseline NewBaseline = BaselineManager->CreateBaseline(Parameters.ConnectionId, InternalIndex, NextBaselineIndex);				
					if (NewBaseline.IsValid())
					{
						CreatedBaselineIndex = NextBaselineIndex;

						// $IRIS: $TODO: Currently due to how repnotifies are implemented we might have to write an extra changemask when sending a new baseline to avoid extra calls to repnotifies
						// Modify changemask to include any data we have in flight to ensure baseline integrity on receiving end
						if (PatchupObjectChangeMaskWithInflightChanges(InternalIndex, Info))
						{
							// Mask off changemasks that may have been disabled due to conditionals.
							ApplyFilterToChangeMask(OutBatchInfo.ParentInternalIndex, InternalIndex, Info, ObjectData.Protocol, ReplicatedObjectStateBuffer, bIsInitialState);
						}

						UE_LOG_REPLICATIONWRITER_CONN(TEXT("Created new baseline %u for ( InternalIndex: %u )"), CreatedBaselineIndex, InternalIndex);
					}
				}
			}

			// $TODO: Consider rewriting the FReplicationProtocolOperations::SerializeWithMask() methods to accept the changemask passed in the Context rather setting it up again later
			FNetBitArrayView ChangeMask = MakeNetBitArrayView(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
			Context.SetChangeMask(&ChangeMask);

			// Collect potential exports and append them to the list of pending exports to be exported with the batch
			CollectAndAppendExports(Context, ReplicatedObjectStateBuffer, ObjectData.Protocol);
			
			// Role downgrade
			{
				FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
				InternalContext->bDowngradeAutonomousProxyRole = (Context.GetLocalConnectionId() != ReplicationFiltering->GetOwningConnection(InternalIndex));
			}

			if (Writer.WriteBool(bIsInitialState))
			{
				// Creation Info
				{
					UE_NET_TRACE_SCOPE(CreationInfo, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

					// Warn if we cannot replicate this object
					if (!ObjectData.InstanceProtocol)
					{
						UE_LOG_REPLICATIONWRITER_WARNING(TEXT("Failed to replicate ( InternalIndex: %u ) %s, ProtocolName: %s, Currently we do not support creating a remote instance when the instance has been detached."), InternalIndex, *NetRefHandle.ToString(), ToCStr(ObjectData.Protocol->DebugName));
						return EWriteObjectStatus::NoInstanceProtocol;
					}

					if (Writer.WriteBool(Info.IsDeltaCompressionEnabled))
					{
						// As we might fail to create a baseline for initial state we need to include it here.
						Writer.WriteBits(CreatedBaselineIndex, FDeltaCompressionBaselineManager::BaselineIndexBitCount);
					}

					const bool bIsDestructionInfo = (Info.IsDestructionInfo == 1U);					
					FReplicationBridgeSerializationContext BridgeContext(Context, Parameters.ConnectionId, bIsDestructionInfo);

					bool bWriteSuccess = false;
					if (Info.IsDestructionInfo)
					{
						bWriteSuccess = ReplicationBridge->CallWriteNetRefHandleDestructionInfo(BridgeContext, NetRefHandle);
					}
					else
					{
						bWriteSuccess = ReplicationBridge->CallWriteNetRefHandleCreationInfo(BridgeContext, NetRefHandle);
					}

					// We need to send creation info, so if we fail we skip this object for now
					if (!bWriteSuccess)
					{
						if (!Context.HasErrorOrOverflow())
						{
							// Unforced error, treat it as we have no instance and cannot create this object but we can continue with other objects
							return EWriteObjectStatus::NoInstanceProtocol;
						}
						else
						{
							return Context.HasError() ? EWriteObjectStatus::Error : EWriteObjectStatus::BitStreamOverflow;
						}
					}
				}
				// Serialize initial state data for this object using delta compression against default state
				FReplicationProtocolOperations::SerializeInitialStateWithMask(Context, Info.GetChangeMaskStoragePointer(), ReplicatedObjectStateBuffer, ObjectData.Protocol);

				UE_NET_IRIS_STATS_ADD_BITS_WRITTEN_AND_COUNT_FOR_OBJECT(Context.GetNetStatsContext(), Writer.GetPosBits() - ObjectRollbackScope.GetStartPos(), WriteCreationInfo, InternalIndex);
			}
			else
			{
				if (Info.IsDeltaCompressionEnabled)
				{
					SerializeObjectStateDelta(Context, InternalIndex, Info, ObjectData, ReplicatedObjectStateBuffer, CurrentBaseline, CreatedBaselineIndex);
				}
				else
				{
					// Serialize state data for this object
					FReplicationProtocolOperations::SerializeWithMask(Context, Info.GetChangeMaskStoragePointer(), ReplicatedObjectStateBuffer, ObjectData.Protocol);
				}
			}
#if UE_NET_USE_READER_WRITER_SENTINEL
			WriteSentinelBits(&Writer, 8);
#endif
		}

		{
			const uint32 HasAttachmentsWritePos = Writer.GetPosBits();
			Writer.WriteBool(bWriteAttachments);
			if (Writer.IsOverflown())
			{
				return EWriteObjectStatus::BitStreamOverflow;
			}

			if (bWriteAttachments)
			{
				FNetBitStreamWriter AttachmentWriter = Writer.CreateSubstream();
				FNetSerializationContext AttachmentContext = Context.MakeSubContext(&AttachmentWriter);
				BatchEntry.AttachmentType = ENetObjectAttachmentType::Normal;
				if (bIsObjectIndexForAttachment)
				{
					BatchEntry.AttachmentType = (WriteObjectFlags & EWriteObjectFlag::WriteObjectFlag_HugeObject ? ENetObjectAttachmentType::HugeObject : ENetObjectAttachmentType::OutOfBand);
					AttachmentWriter.WriteBool(BatchEntry.AttachmentType == ENetObjectAttachmentType::HugeObject);
				}

				const EAttachmentWriteStatus AttachmentWriteStatus = Attachments.Serialize(AttachmentContext, BatchEntry.AttachmentType, InternalIndex, NetRefHandle, BatchEntry.AttachmentRecord, BatchEntry.bHasUnsentAttachments);
				if (BatchEntry.AttachmentType == ENetObjectAttachmentType::HugeObject)
				{
					if (AttachmentWriteStatus == EAttachmentWriteStatus::ReliableWindowFull)
					{
						HugeObjectSendQueue.Stats.StartStallTime = FPlatformTime::Cycles64();
					}
					else
					{
						// Clear stall time now that we were theoretically able to send something.
						HugeObjectSendQueue.Stats.StartStallTime = 0;
					}
				}

				// If we didn't manage to fit any attachments then clear the HasAttachments bool in the packet
				if (AttachmentWriter.GetPosBits() == 0 || AttachmentWriter.IsOverflown())
				{
					BatchEntry.bSentAttachments = 0;

					const uint32 BitsThatWasAvailableForAttachements = Writer.GetBitsLeft();

					Writer.DiscardSubstream(AttachmentWriter);
					{
						FNetBitStreamWriteScope HasAttachmentsWriteScope(Writer, HasAttachmentsWritePos);
						Writer.WriteBool(false);
					}

					Info.HasAttachments = BatchEntry.bHasUnsentAttachments;
					
					// If we should have had enough space to write a attachment, the attachment + exports must be huge and we need to fall back on using the huge object path
					const uint32 SplitThreshold = PartialNetObjectAttachmentHandler->GetConfig()->GetBitCountSplitThreshold() * 2;
					const bool bFallbackToHugeObjectPath = BatchEntry.AttachmentType != ENetObjectAttachmentType::HugeObject && BatchEntry.bHasUnsentAttachments && (BitsThatWasAvailableForAttachements >= SplitThreshold);
					if (bFallbackToHugeObjectPath)
					{
						UE_LOG(LogIris, Verbose, TEXT("Failed to write huge attachment for object %s ( InternalIndex: %u ), forcing fallback on hugeobject for attachments"), *NetRefHandle.ToString(), InternalIndex);
						Writer.DoOverflow();
					}
					else if (!(BatchEntry.bSentState || bSentTearOff || Info.SubObjectPendingDestroy || Info.HasDirtySubObjects || bWriteBatchInfo))
					{
						// If we didn't send state and didn't send any attachments let's rollback
						ObjectRollbackScope.Rollback();
					}
				}
				else
				{
					BatchEntry.bSentAttachments = 1;

					Writer.CommitSubstream(AttachmentWriter);

					// Update the HasAttachments info based on this object batch failing. If the batch is a success we update again.
					Info.HasAttachments = Attachments.HasUnsentAttachments(BatchEntry.AttachmentType, InternalIndex);
				}
			}
		}

		if (Writer.IsOverflown())
		{
			UE_NET_IRIS_STATS_ADD_BITS_WRITTEN_FOR_OBJECT_AS_WASTE(Context.GetNetStatsContext(), Writer.GetPosBits() - ObjectRollbackScope.GetStartPos(), Write, InternalIndex);
			return EWriteObjectStatus::BitStreamOverflow;
		}

		if (bWriteReplicatedDestroyHeader)
		{
			// Rewrite destroy header if necessary	
			if (bSentTearOff || Info.SubObjectPendingDestroy)
			{
				uint32 ReplicatedDestroyHeaderFlags = 0U;

				// TearOff			
				ReplicatedDestroyHeaderFlags |= bSentTearOff ? ReplicatedDestroyHeaderFlags_TearOff : ReplicatedDestroyHeaderFlags_None;

				// Write SubObject destroy
				if (Info.SubObjectPendingDestroy)
				{
					ReplicatedDestroyHeaderFlags |= ReplicatedDestroyHeaderFlags_EndReplication;
					const bool bShouldDestroyInstance = ObjectData.RefHandle.IsDynamic() || NetRefHandleManager->GetIsDestroyedStartupObject(InternalIndex);					
					ReplicatedDestroyHeaderFlags |= bShouldDestroyInstance ? ReplicatedDestroyHeaderFlags_DestroyInstance : ReplicatedDestroyHeaderFlags_None;
				}

				FNetBitStreamWriteScope WriteScope(Writer, ReplicatedDestroyHeaderBitPos);
				Writer.WriteBits(ReplicatedDestroyHeaderFlags, ReplicatedDestroyHeaderFlags_BitCount);
			}
			else if (bWriteBatchInfo && !(BatchEntry.bSentState || BatchEntry.bSentAttachments))
			{
				// No need for the destroy header as we did not write any data at all for the batch.
				Writer.Seek(ReplicatedDestroyHeaderBitPos);
			}
		}
	}

	// Success so far. Fill in batch entry. Keep index to update info later as the array can resize.
	const int ParentBatchEntryIndex = OutBatchInfo.ObjectInfos.Num();
	{
		FBatchObjectInfo& FinalBatchEntry = OutBatchInfo.ObjectInfos.Emplace_GetRef();
		FinalBatchEntry = MoveTemp(BatchEntry);

		FinalBatchEntry.bIsInitialState = bIsInitialState;
		FinalBatchEntry.InternalIndex = InternalIndex;
		FinalBatchEntry.bHasDirtySubObjects = false;
		FinalBatchEntry.bSentTearOff = bSentTearOff;
		FinalBatchEntry.bSentDestroySubObject = Info.SubObjectPendingDestroy;
		FinalBatchEntry.NewBaselineIndex = CreatedBaselineIndex;
		if (InternalIndex != ObjectIndexForOOBAttachment)
		{
			// Mark this object as written this tick to avoid sending it multiple times
			WriteContext.ObjectsWrittenThisPacket.SetBit(InternalIndex);
		}
	}

	// Reset CreatedBaselineIndex to avoid it being released on scope exit
	CreatedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;

	// Write dirty sub objects
	const uint32 SubObjectStartPos = Writer.GetPosBits();
	uint32 SubObjectsWrittenBits = 0U;
	if (Info.HasDirtySubObjects && !Info.IsSubObject)
	{
		bool bHasDirtySubObjects = false;
		
		FReplicationConditionals::FSubObjectsToReplicateArray SubObjectsToReplicate;
		ReplicationConditionals->GetSubObjectsToReplicate(Parameters.ConnectionId, InternalIndex, SubObjectsToReplicate);		

		for (FInternalNetRefIndex SubObjectInternalIndex : SubObjectsToReplicate)
		{
			if (!ObjectsWithDirtyChanges.GetBit(SubObjectInternalIndex))
			{
				continue;
			}

			const int BatchObjectInfoCount = OutBatchInfo.ObjectInfos.Num();
			EWriteObjectStatus SubObjectWriteStatus = WriteObjectAndSubObjects(Context, SubObjectInternalIndex, WriteObjectFlags, OutBatchInfo);
			if (!IsWriteObjectSuccess(SubObjectWriteStatus))
			{
				// SubObject will rollback on fail (and report its own waste) but we as we will rollback successfully written subobjects it is better to at least report it with the owner.
				UE_NET_IRIS_STATS_ADD_BITS_WRITTEN_FOR_OBJECT_AS_WASTE(Context.GetNetStatsContext(), Writer.GetPosBits() - ObjectRollbackScope.GetStartPos(), Write, InternalIndex);
				return SubObjectWriteStatus;
			}

			// There are success statuses where no object info is added. In such case we shouldn't read from it.
			if (OutBatchInfo.ObjectInfos.Num() > BatchObjectInfoCount)
			{
				const FBatchObjectInfo& SubObjectEntry = OutBatchInfo.ObjectInfos.Last();
				bHasDirtySubObjects |= SubObjectEntry.bHasDirtySubObjects || SubObjectEntry.bHasUnsentAttachments;
			}
		}

		SubObjectsWrittenBits = Writer.GetPosBits() - SubObjectStartPos;

		// Update parent batch info
		{
			FBatchObjectInfo& ParentBatchEntry = OutBatchInfo.ObjectInfos[ParentBatchEntryIndex];
			ParentBatchEntry.bHasDirtySubObjects |= bHasDirtySubObjects;
		}
	}

	// ObjectBatch ends here
	// We include the size of the data written so we can skip it if needed.
	if (OutBatchInfo.ParentInternalIndex == InternalIndex)
	{
		FBatchObjectInfo& ParentBatchEntry = OutBatchInfo.ObjectInfos[ParentBatchEntryIndex];

		const uint32 WrittenBitsInBatch = (Writer.GetPosBits() - InitialStateHeaderPos) - NumBitsUsedForBatchSize;
		
		const bool bWroteData = (ParentBatchEntry.bSentState || ParentBatchEntry.bSentAttachments || bSentTearOff || Info.SubObjectPendingDestroy);
		if (bWroteData || (SubObjectsWrittenBits != 0U))
		{
			const FObjectReferenceCache::EWriteExportsResult WriteExportResult = ObjectReferenceCache->WritePendingExports(Context, InternalIndex);

			if (WriteExportResult == FObjectReferenceCache::EWriteExportsResult::BitStreamOverflow)
			{
				// If we fail to write exports, we fail the entire object
				UE_NET_IRIS_STATS_ADD_BITS_WRITTEN_FOR_OBJECT_AS_WASTE(Context.GetNetStatsContext(), Writer.GetPosBits() - ObjectRollbackScope.GetStartPos(), Write, InternalIndex);
				return EWriteObjectStatus::BitStreamOverflow;	
			}

			const bool bWroteExports = WriteExportResult == FObjectReferenceCache::EWriteExportsResult::WroteExports;

			// Update header
			if (ensure(bWriteBatchInfo))
			{
				FNetBitStreamWriteScope SizeScope(Writer, InitialStateHeaderPos);
				Writer.WriteBits(WrittenBitsInBatch, NumBitsUsedForBatchSize);
				Writer.WriteBool(bWroteData);
				Writer.WriteBool(bWroteExports);
			}

			ParentBatchEntry.bSentBatchData = 1U;

			UE_NET_IRIS_STATS_ADD_BITS_WRITTEN_FOR_OBJECT(Context.GetNetStatsContext(), (Writer.GetPosBits() - ObjectRollbackScope.GetStartPos()) - SubObjectsWrittenBits, Write, InternalIndex);
		}
		// If we did not write any data we rollback any written headers and report a success
		else
		{
			// if we or our subobjects did not write any data, rollback and forget about everything
			ObjectRollbackScope.Rollback();
		}	
	}

	return EWriteObjectStatus::Success;
}

FReplicationWriter::EWriteObjectStatus FReplicationWriter::WriteObjectInBatch(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags, FBatchInfo& OutBatchInfo)
{
	UE_NET_IRIS_STATS_TIMER(Timer, Context.GetNetStatsContext());

	// Reset pending exports
	FNetExportContext* ExportContext = Context.GetExportContext();
	if (ExportContext)
	{
		ExportContext->ClearPendingExports();
	}

	// Write parent object and subobjects
	const EWriteObjectStatus WriteObjectStatus = WriteObjectAndSubObjects(Context, InternalIndex, WriteObjectFlags, OutBatchInfo);
	if (!IsWriteObjectSuccess(WriteObjectStatus))
	{
		UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT_AS_WASTE(Timer, Write, InternalIndex);
		return WriteObjectStatus;
	}

	UE_NET_IRIS_STATS_ADD_TIME_AND_COUNT_FOR_OBJECT(Timer, Write, InternalIndex);

	// Include dependent objects as separate batch, (for hugeobjects they will be included as they are written to a separate bitstream)
	{
		const uint32 OldBatchInfoParentInternalIndex = OutBatchInfo.ParentInternalIndex;
		for (const FDependentObjectInfo DependentObjectInfo : NetRefHandleManager->GetDependentObjectInfos(InternalIndex))
		{
			const FInternalNetRefIndex DependentInternalIndex = DependentObjectInfo.NetRefIndex;
			const bool bIsDependentInitialState = IsInitialState(GetReplicationInfo(DependentInternalIndex).GetState());
			if (bIsDependentInitialState && !WriteContext.ObjectsWrittenThisPacket.GetBit(DependentInternalIndex))
			{
				UE_NET_TRACE_SCOPE(DependentObjectData, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);

				OutBatchInfo.ParentInternalIndex = DependentInternalIndex;
				EWriteObjectStatus DependentObjectWriteStatus = WriteObjectInBatch(Context, DependentInternalIndex, WriteObjectFlags, OutBatchInfo);
				if (!IsWriteObjectSuccess(DependentObjectWriteStatus))
				{
					// Restore ParentInternalIndex
					OutBatchInfo.ParentInternalIndex = OldBatchInfoParentInternalIndex;
					return DependentObjectWriteStatus;
				}
			}
		}

		// Restore ParentInternalIndex
		OutBatchInfo.ParentInternalIndex = OldBatchInfoParentInternalIndex;
	}

	return EWriteObjectStatus::Success;
}

int FReplicationWriter::PrepareAndSendHugeObjectPayload(FNetSerializationContext& Context, uint32 InternalIndex)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_PrepareAndSendHugeObjectPayload);

	// Sanity check
	if (HugeObjectSendQueue.IsFull() || HugeObjectSendQueue.IsObjectInQueue(InternalIndex, false))
	{
		ensureMsgf(false, TEXT("HugeObjectSendQueue should not be full or already transmitting. ( InternalIndex: %u )"), InternalIndex);
		return 0;
	}

	typedef uint32 HugeObjectStorageType;
	const uint32 BitsPerStorageWord = sizeof(HugeObjectStorageType) * 8;

	TArray<HugeObjectStorageType> HugeObjectPayload;
	HugeObjectPayload.AddUninitialized(static_cast<int32>(PartialNetObjectAttachmentHandler->GetConfig()->GetTotalMaxPayloadBitCount() + (BitsPerStorageWord - 1U))/BitsPerStorageWord);

	// Setup a special context for the huge object serialization.
	FNetBitStreamWriter HugeObjectWriter;
	const uint32 MaxHugeObjectPayLoadBytes = HugeObjectPayload.Num() * sizeof(HugeObjectStorageType);
	HugeObjectWriter.InitBytes(HugeObjectPayload.GetData(), MaxHugeObjectPayLoadBytes);
	FNetSerializationContext HugeObjectSerializationContext = Context.MakeSubContext(&HugeObjectWriter);

#if UE_NET_TRACE_ENABLED
	if (!HugeObjectSendQueue.TraceCollector)
	{
		HugeObjectSendQueue.TraceCollector = UE_NET_TRACE_CREATE_COLLECTOR(ENetTraceVerbosity::Trace);
	}
	else
	{
		HugeObjectSendQueue.TraceCollector->Reset();
	}
	HugeObjectSerializationContext.SetTraceCollector(HugeObjectSendQueue.TraceCollector);
#endif

	// Huge object header needed for the receiving side to be able to process this correctly.
	FNetObjectBlob::FHeader HugeObjectHeader = {};
	const uint32 HeaderPos = HugeObjectWriter.GetPosBits();
	FNetObjectBlob::SerializeHeader(HugeObjectSerializationContext, HugeObjectHeader);
	const uint32 PastHeaderPos = HugeObjectWriter.GetPosBits();

	FHugeObjectContext HugeObjectContext;

	FBatchInfo BatchInfo;
	BatchInfo.Type = EBatchInfoType::Internal;
	BatchInfo.ParentInternalIndex = InternalIndex;
	uint32 WriteObjectFlags = WriteObjectFlag_State;
	// Get the creation going as quickly as possible.
	if (!Context.IsInitState())
	{
		WriteObjectFlags |= WriteObjectFlag_Attachments;
	}
	
	// Push new ExportContext for the hugeobject-batch as we cannot share exports with an OOB object
	{
		HugeObjectContext.BatchExports.Reset();
		FNetExports::FExportScope ExportScope = NetExports->MakeExportScope(HugeObjectSerializationContext, HugeObjectContext.BatchExports);

		UE_NET_TRACE_SCOPE(HugeObjectState, *HugeObjectSerializationContext.GetBitStreamWriter(), HugeObjectSerializationContext.GetTraceCollector(), ENetTraceVerbosity::Trace);

		// We can encounter other errors than bitstream overflow now that we've got a really large buffer to write to.
		const EWriteObjectStatus WriteHugeObjectStatus = WriteObjectInBatch(HugeObjectSerializationContext, InternalIndex, WriteObjectFlags, BatchInfo);

		// If we cannot fit the object in the largest supported buffer then we will never fit the object.
		if (WriteHugeObjectStatus == EWriteObjectStatus::BitStreamOverflow)
		{
			// Cleanup data from batch
			HandleObjectBatchFailure(WriteHugeObjectStatus, BatchInfo, WriteBitStreamInfo);

			const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);
			UE_LOG(LogIris, Error, TEXT("Unable to fit object %s ( InternalIndex: %u ) %s in maximum combined payload of %u bytes. Connection %u will be disconnected."), *ObjectData.RefHandle.ToString(), InternalIndex, ToCStr(ObjectData.Protocol ? ObjectData.Protocol->DebugName : nullptr), MaxHugeObjectPayLoadBytes, Context.GetLocalConnectionId());
			ensure(false);

			Context.SetError(NetError_ObjectStateTooLarge);
			return -1;
		}

		// If we encounter some other error we can try sending a smaller object in the meantime.
		if (!IsWriteObjectSuccess(WriteHugeObjectStatus))
		{
			// Cleanup data from batch
			HandleObjectBatchFailure(WriteHugeObjectStatus, BatchInfo, WriteBitStreamInfo);

			UE_LOG(LogIris, Verbose, TEXT("Problem writing huge object ( InternalIndex: %u ). WriteObjectStatus: %u. Trying smaller object."), InternalIndex, unsigned(WriteHugeObjectStatus));
			return 0;
		}
	}

	if (HugeObjectSendQueue.IsEmpty())
	{
		HugeObjectSendQueue.Stats.StartSendingTime = FPlatformTime::Cycles64();
	}

	HugeObjectContext.RootObjectInternalIndex = InternalIndex;

	// Store batch record for later processing once the whole state is acked.
	HandleObjectBatchSuccess(BatchInfo, HugeObjectContext.BatchRecord);
	// We want to track the number of Batches
	HugeObjectHeader.ObjectCount = HugeObjectContext.BatchRecord.BatchCount;

	// Write huge object header
	{
		FNetBitStreamWriteScope WriteScope(HugeObjectWriter, HeaderPos);
		FNetObjectBlob::SerializeHeader(HugeObjectSerializationContext, HugeObjectHeader);
	}

	HugeObjectWriter.CommitWrites();

	// Create a NetObjectBlob from the temporary buffer and split it into multiple smaller pieces.
	const uint32 PayLoadBitCount = HugeObjectWriter.GetPosBits();
	const uint32 StorageWordsWritten = (PayLoadBitCount + (BitsPerStorageWord - 1))/BitsPerStorageWord;

	check(StorageWordsWritten <= (uint32)HugeObjectPayload.Num());

	TArrayView<HugeObjectStorageType> PayloadView(HugeObjectPayload.GetData(), StorageWordsWritten);
	TRefCountPtr<FNetObjectBlob> NetObjectBlob = NetObjectBlobHandler->CreateNetObjectBlob(PayloadView, PayLoadBitCount);
	TArray<TRefCountPtr<FNetBlob>> PartialNetBlobs;
	const bool bSplitSuccess = PartialNetObjectAttachmentHandler->SplitRawDataNetBlob(TRefCountPtr<FRawDataNetBlob>(NetObjectBlob.GetReference()), PartialNetBlobs, HugeObjectSendQueue.DebugName);
	if (!bSplitSuccess)
	{
		UE_LOG(LogIris, Error, TEXT("Unable to split huge object ( InternalIndex: %u ) payload. Connection %u will be disconnected."), InternalIndex, Context.GetLocalConnectionId());
		Context.SetError(NetError_ObjectStateTooLarge);
		return -1;
	}

	// Enqueue attachments
	const bool bEnqueueSuccess = Attachments.Enqueue(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment, MakeArrayView(PartialNetBlobs.GetData(), PartialNetBlobs.Num()));
	check(bEnqueueSuccess);
	if (!bEnqueueSuccess)
	{
		UE_LOG(LogIris, Error, TEXT("Unable to enqueue huge object attachments ( InternalIndex: %u ). Connection %u will be disconnected."), InternalIndex, Context.GetLocalConnectionId());
		Context.SetError(GNetError_InternalError);
		return -1;
	}

	// Add huge object to queue
	HugeObjectContext.Blobs = MoveTemp(PartialNetBlobs);
	const bool bHugeObjectWasEnqueued = HugeObjectSendQueue.EnqueueHugeObject(HugeObjectContext);
	check(bHugeObjectWasEnqueued);
	if (!bHugeObjectWasEnqueued)
	{
		UE_LOG(LogIris, Error, TEXT("Unable to enqueue huge object ( InternalIndex: %u ). Connection %u will be disconnected."), InternalIndex, Context.GetLocalConnectionId());
		Context.SetError(GNetError_InternalError);
		return -1;
	}

	// Write huge object attachment(s)
	{
		FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
		UE_NET_TRACE_SCOPE(Batch, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		FNetBitStreamRollbackScope RollbackScope(Writer);
		
		FBatchInfo HugeObjectBatchInfo;
		HugeObjectBatchInfo.Type = EBatchInfoType::HugeObject;
		HugeObjectBatchInfo.ParentInternalIndex = FNetRefHandleManager::InvalidInternalIndex;
		const uint32 WriteHugeObjectFlags = WriteObjectFlag_Attachments | WriteObjectFlag_HugeObject;
		const EWriteObjectStatus HugeObjectStatus = WriteObjectInBatch(Context, ObjectIndexForOOBAttachment, WriteHugeObjectFlags, HugeObjectBatchInfo);
		if (!IsWriteObjectSuccess(HugeObjectStatus))
		{
			// Need to call this in order to cleanup data associated with batch
			HandleObjectBatchFailure(HugeObjectStatus, HugeObjectBatchInfo, WriteBitStreamInfo);

			ensureMsgf(HugeObjectStatus == EWriteObjectStatus::BitStreamOverflow, TEXT("Expected split payload to not be able to generate other errors than overflow. Got %u"), unsigned(HugeObjectStatus));
			// It's unexpected, but not a critical error, if no part of the payload could be sent.
			// We do expect a smaller object to be sent though so that's why 0 is returned.

			// Mark the context so that we can try to send the huge object in the next packet if we are allowed
			WriteContext.bHasHugeObjectToSend = 1;

			// Try to fit a smaller object.
			return 0;
		}

		FBatchRecord BatchRecord;
		HandleObjectBatchSuccess(HugeObjectBatchInfo, BatchRecord);
		CommitBatchRecord(BatchRecord);

		// If all chunks did not make it into the packet (expected) mark the context so that we can try to send the huge object in the next packet if we are allowed
		const bool bHasHugeObjectToSend = Attachments.HasUnsentAttachments(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment);
		WriteContext.bHasHugeObjectToSend = bHasHugeObjectToSend;
		if (!bHasHugeObjectToSend)
		{
			HugeObjectSendQueue.Stats.EndSendingTime = FPlatformTime::Cycles64();
		}

		return 1;
	}

	// Should not get here.
	return -1;
}

int FReplicationWriter::WriteObjectBatch(FNetSerializationContext& Context, uint32 InternalIndex, uint32 WriteObjectFlags)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_WriteObjectBatch);

	// If this is a destruction info we treat it differently and just write the information required to destruct the object
	if (GetReplicationInfo(InternalIndex).IsDestructionInfo)
	{
		return WriteDestructionInfo(Context, InternalIndex);
	}

	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	// Batch successful writes and commit them as a atomic batch.
	// It is a fail if we fail to write any subobject with dirty state.
	// It is also not ok to skip over creation header- if we do then the entire batch needs to be delayed.

	// Write object and subobjects. Try #1 - send state and attachments.
	{
		UE_NET_TRACE_SCOPE(Batch, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		FNetBitStreamRollbackScope RollbackScope(Writer);
		FNetExportRollbackScope ExportRollbackScope(Context);

		WriteBitStreamInfo.BatchStartPos = Writer.GetPosBits();
		FBatchInfo BatchInfo;
		BatchInfo.Type = (InternalIndex == ObjectIndexForOOBAttachment ? (WriteObjectFlags & WriteObjectFlag_HugeObject ? EBatchInfoType::HugeObject : EBatchInfoType::OOBAttachment) : EBatchInfoType::Object);
		BatchInfo.ParentInternalIndex = InternalIndex;

		// Write an object and its subobjects. If object has dependent objects pending creation we currently write them as well as an individual batch.
		const EWriteObjectStatus WriteObjectStatus = WriteObjectInBatch(Context, InternalIndex, WriteObjectFlags, BatchInfo);

		if (IsWriteObjectSuccess(WriteObjectStatus))
		{
			FBatchRecord BatchRecord;
			const int WrittenObjectCount = HandleObjectBatchSuccess(BatchInfo, BatchRecord);

			// As a single batch also might include dependent objects which are treated as separate batches on the receiving end we need to account for this when tracking the written batch count.
			WriteContext.WrittenBatchCount += BatchRecord.BatchCount;

			CommitBatchRecord(BatchRecord);
			return WrittenObjectCount;
		}

		const EWriteObjectRetryMode WriteRetryMode = HandleObjectBatchFailure(WriteObjectStatus, BatchInfo, WriteBitStreamInfo);

		// Regardless of the reason for fail we should rollback anything written.
		RollbackScope.Rollback();

		// Rollback exported references that was exported as part of the batch we just rolled back
		ExportRollbackScope.Rollback();

		switch (WriteRetryMode)
		{
			case EWriteObjectRetryMode::Abort:
			{
				return -1;
			}

			case EWriteObjectRetryMode::TrySmallObject:
			{
				++WriteContext.FailedToWriteSmallObjectCount;
				return 0;
			}

			case EWriteObjectRetryMode::SplitHugeObject:
			{
				break;
			}

			default:
			{
				check(false);
			}
		}
	}

	// Try #2 - Object will be serialized to a temporary buffer of maximum supported size and split into multiple chunks.
	{
		const int SendHugeObjectStatus = PrepareAndSendHugeObjectPayload(Context, InternalIndex);

		// If the huge object wrote data it will be tracked as a single batch
		if (SendHugeObjectStatus == 1)
		{
			++WriteContext.WrittenBatchCount;
		}
				
		return SendHugeObjectStatus;
	}
}

int FReplicationWriter::WriteDestructionInfo(FNetSerializationContext& Context, uint32 InternalIndex)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_WriteDestructionInfo);

	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
	UE_NET_TRACE_SCOPE(Batch, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	// Rollback for entire batch
	FNetBitStreamRollbackScope Rollback(Writer);
	FNetExportRollbackScope ExportRollbackScope(Context);

	// Only write data for the object if we have data to write
	const FNetRefHandleManager::FReplicatedObjectData& ObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(InternalIndex);

	// Special case for static objects that should be destroyed on the client but we have not replicated
	Writer.WriteBool(true);

	constexpr bool bIsDestructionInfo = true;
	FReplicationBridgeSerializationContext BridgeContext(Context, Parameters.ConnectionId, bIsDestructionInfo);

	// Push ForceInlineExportScope to inline exports instead of writing exports later.
	FForceInlineExportScope ForceInlineExportScope(Context.GetInternalContext());
	if (!ReplicationBridge->CallWriteNetRefHandleDestructionInfo(BridgeContext, ObjectData.RefHandle))
	{
		// Trigger Rollback
		Writer.DoOverflow();

		return 0;
	}

#if UE_NET_USE_READER_WRITER_SENTINEL
	{
		UE_NET_TRACE_SCOPE(Sentinel, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		WriteSentinelBits(&Writer, 8);
	}
#endif

	// Push record
	if (!Writer.IsOverflown())
	{
		// We did write the initial state, change the state to WaitOnCreateConfirmation
		SetState(InternalIndex, EReplicatedObjectState::WaitOnCreateConfirmation);

		FReplicationInfo& Info = GetReplicationInfo(InternalIndex);

		FBatchObjectInfo ObjectInfo = {};
		ObjectInfo.InternalIndex = InternalIndex;
		FObjectRecord ObjectRecord;
		CreateObjectRecord(nullptr, Info, ObjectInfo, ObjectRecord);
		CommitObjectRecord(InternalIndex, ObjectRecord);

		Info.HasDirtyChangeMask = 0U;
		Info.HasDirtySubObjects = 0U;
		Info.HasAttachments = 0U;

		ObjectsWithDirtyChanges.ClearBit(InternalIndex);

		// Reset scheduling priority
		SchedulingPriorities[InternalIndex] = 0.0f;

		WriteContext.Stats.AddNumberOfReplicatedDestructionInfos(1U);

		// We count this as an object batch
		++WriteContext.WrittenBatchCount;
	}

	return Writer.IsOverflown() ? -1 :  1;
}

uint32 FReplicationWriter::WriteOOBAttachments(FNetSerializationContext& Context)
{
	uint32 WrittenObjectCount = 0U;

	if (WriteContext.WriteMode == EDataStreamWriteMode::PostTickDispatch)
	{
		if (WriteContext.bHasOOBAttachmentsToSend && CanSendObject(ObjectIndexForOOBAttachment))
		{
			IRIS_PROFILER_SCOPE(FReplicationWriter_WriteOOBAttachments);
			const int32 Result = WriteObjectBatch(Context, ObjectIndexForOOBAttachment, WriteObjectFlag_Attachments);
			if (Result == -1)
			{
				return WrittenObjectCount;
			}

			WriteContext.bHasOOBAttachmentsToSend = Attachments.HasUnsentUnreliableAttachments(ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment);
			WrittenObjectCount += Result;
		}
	}
	else
	{
		if (WriteContext.bHasHugeObjectToSend)
		{
			IRIS_PROFILER_SCOPE(FReplicationWriter_WriteHugeObjectAttachments);
			const int32 Result = WriteObjectBatch(Context, ObjectIndexForOOBAttachment, WriteObjectFlag_Attachments | WriteObjectFlag_HugeObject);
			if (Result == -1)
			{
				return WrittenObjectCount;
			}

			const bool bHasHugeObjectToSend = Attachments.HasUnsentAttachments(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment);
			WriteContext.bHasHugeObjectToSend = bHasHugeObjectToSend;
			if (!bHasHugeObjectToSend)
			{
				HugeObjectSendQueue.Stats.EndSendingTime = FPlatformTime::Cycles64();
			}

			WrittenObjectCount += Result;
		}

		if (WriteContext.bHasOOBAttachmentsToSend && CanSendObject(ObjectIndexForOOBAttachment))
		{
			IRIS_PROFILER_SCOPE(FReplicationWriter_WriteOOBAttachments);
			const int32 Result = WriteObjectBatch(Context, ObjectIndexForOOBAttachment, WriteObjectFlag_Attachments);
			if (Result == -1)
			{
				return WrittenObjectCount;
			}

			WriteContext.bHasOOBAttachmentsToSend = Attachments.HasUnsentAttachments(ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment);
			WrittenObjectCount += Result;
		}
	}

	return WrittenObjectCount;
}

uint32 FReplicationWriter::WriteObjects(FNetSerializationContext& Context)
{
	uint32 WrittenObjectCount = 0;
	
	const uint32 ObjectCount = WriteContext.ScheduledObjectCount;
	FScheduleObjectInfo* ObjectList = WriteContext.ScheduledObjectInfos;

	uint32 ObjectListIt = WriteContext.CurrentIndex;
	uint32 SortedCount = WriteContext.SortedObjectCount;

	bool bContinue = WriteContext.bHasUpdatedObjectsToSend;

	auto SendObjectFunction = [this, &Context, &WrittenObjectCount](FInternalNetRefIndex InternalIndex)
	{
		if (!this->WriteContext.ObjectsWrittenThisPacket.GetBit(InternalIndex) && this->CanSendObject(InternalIndex))
		{
			const int32 Result = this->WriteObjectBatch(Context, InternalIndex, WriteObjectFlag_State | WriteObjectFlag_Attachments);
			if (Result >= 0)
			{
				WrittenObjectCount += Result;
			}
			else
			{
				return false;
			}
		}
		return true;
	};
	

	while (bContinue && ObjectListIt < ObjectCount)
	{
		// Partial sort next batch
		if (ObjectListIt >= SortedCount)
		{
			SortedCount += SortScheduledObjects(ObjectList, ObjectCount, ObjectListIt);
		}

		for (;;)
		{
			// Try to send dependent objects
			while (WriteContext.DependentObjectsPendingSend.Num() > 0)
			{
				UE_NET_TRACE_SCOPE(DependentObjectData, *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);

				const uint32 InternalIndex = WriteContext.DependentObjectsPendingSend.Pop();
				checkSlow(InternalIndex != ObjectIndexForOOBAttachment);
				ensureMsgf(GetReplicationInfo(InternalIndex).GetState() != EReplicatedObjectState::Invalid, TEXT("DependentObject with InternalIndex %u is not in scope"), InternalIndex);
				if (!SendObjectFunction(InternalIndex))
				{
					// If we fail, we put the object back on the pending send stack and try again in the next packet of the batch
					// The reason for pop before using the index is that the SendObjectFunction will push new dependent objects on the stack
					WriteContext.DependentObjectsPendingSend.Push(InternalIndex);				
					bContinue = false;
					break;
				}
			}

			// Normal send, dependent objects will be pushed as needed
			if (ObjectListIt < SortedCount)
			{
				const uint32 InternalIndex = ObjectList[ObjectListIt].Index;

				checkSlow(InternalIndex != ObjectIndexForOOBAttachment);
				if (!SendObjectFunction(InternalIndex))
				{
					bContinue = false;
					break;
				}

				++ObjectListIt;
			}
			else
			{
				break;
			}
		}
	}

	// if we have more data to write, store state so that we can continue if we are allowed to write more data
	WriteContext.bHasUpdatedObjectsToSend = (ObjectListIt != ObjectCount) || (WriteContext.DependentObjectsPendingSend.Num() > 0U);
	WriteContext.CurrentIndex = ObjectListIt;
	WriteContext.SortedObjectCount = SortedCount;

	// Reset objects written this packet
	WriteContext.ObjectsWrittenThisPacket.Reset();

	return WrittenObjectCount;
}

int FReplicationWriter::HandleObjectBatchSuccess(const FBatchInfo& BatchInfo, FReplicationWriter::FBatchRecord& OutRecord)
{
	int WrittenObjectCount = 0;
	int WrittenBatchCount = 0;

	const bool bTrackObjectStats = BatchInfo.Type != EBatchInfoType::Internal;
	uint32 ObjectCount = 0;
	uint32 AttachmentCount = 0;
	uint32 DeltaCompressedObjectCount = 0;

	OutRecord.ObjectReplicationRecords.Reserve(BatchInfo.ObjectInfos.Num());
	for (const FBatchObjectInfo& BatchObjectInfo : BatchInfo.ObjectInfos)
	{
		//UE_LOG_REPLICATIONWRITER(TEXT("FReplicationWriter::Wrote Object with %s InitialState: %u"), *NetRefHandleManager->NetHandle.ToString(), bIsInitialState ? 1u : 0u);
		FReplicationInfo& Info = GetReplicationInfo(BatchObjectInfo.InternalIndex);

		// We did write the initial state, change the state to WaitOnCreateConfirmation
		if (BatchObjectInfo.bIsInitialState)
		{
			SetState(BatchObjectInfo.InternalIndex, EReplicatedObjectState::WaitOnCreateConfirmation);
		}
		else if (Info.TearOff)
		{
			if (BatchObjectInfo.bSentTearOff)
			{
				SetState(BatchObjectInfo.InternalIndex, EReplicatedObjectState::PendingTearOff);
				SetState(BatchObjectInfo.InternalIndex, EReplicatedObjectState::WaitOnDestroyConfirmation);
			}
			else
			{
				SetState(BatchObjectInfo.InternalIndex, EReplicatedObjectState::WaitOnFlush);
			}
		}
		else if (BatchObjectInfo.bSentDestroySubObject)
		{			
			SetState(BatchObjectInfo.InternalIndex, EReplicatedObjectState::WaitOnDestroyConfirmation);
		}

		// We're now committing to what we wrote so inform the attachments writer.
		if (BatchObjectInfo.AttachmentRecord.IsValid())
		{
			Attachments.CommitReplicationRecord(BatchObjectInfo.AttachmentType, BatchObjectInfo.InternalIndex, BatchObjectInfo.AttachmentRecord);
		}

		AttachmentCount += BatchObjectInfo.bSentAttachments;

		// Update transmission record.
		if (BatchObjectInfo.bSentState)
		{
			FNetBitArrayView ChangeMask(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount);
			FObjectRecord& ObjectRecord = OutRecord.ObjectReplicationRecords.AddDefaulted_GetRef();
			CreateObjectRecord(&ChangeMask, Info, BatchObjectInfo, ObjectRecord);

			// The object no longer has any dirty state, but may still have attachments that didn't fit
			ChangeMask.Reset();

			++ObjectCount;
			if (Info.LastAckedBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
			{
				++DeltaCompressedObjectCount;
			}
		}
		else if (BatchObjectInfo.AttachmentRecord.IsValid() || BatchObjectInfo.bSentTearOff || BatchObjectInfo.bSentDestroySubObject)
		{
			FObjectRecord& ObjectRecord = OutRecord.ObjectReplicationRecords.AddDefaulted_GetRef();
			CreateObjectRecord(nullptr, Info, BatchObjectInfo, ObjectRecord);
		}

		// Schedule rest of dependent objects for replication, note there is no guarantee that they will replicate in same packet
		for (const FDependentObjectInfo DependentObjectInfo : NetRefHandleManager->GetDependentObjectInfos(BatchObjectInfo.InternalIndex))
		{
			const FInternalNetRefIndex DependentInternalIndex = DependentObjectInfo.NetRefIndex;
			if (ObjectsWithDirtyChanges.GetBit(DependentInternalIndex) && !WriteContext.ObjectsWrittenThisPacket.GetBit(DependentInternalIndex))
			{
				WriteContext.DependentObjectsPendingSend.Push(DependentInternalIndex);
				// Bumping the scheduling priority here will make sure that they will be scheduled the next update if we are not allowed to replicate this frame
				SchedulingPriorities[DependentInternalIndex] = FMath::Max(SchedulingPriorities[BatchObjectInfo.InternalIndex], SchedulingPriorities[DependentInternalIndex]);
			}
		}			

		if (BatchObjectInfo.bSentState | BatchObjectInfo.bSentAttachments | BatchObjectInfo.bSentTearOff | BatchObjectInfo.bSentDestroySubObject)
		{
			++WrittenObjectCount;
		}

		WrittenBatchCount += BatchObjectInfo.bSentBatchData;

		Info.HasDirtyChangeMask = 0U;
		Info.HasDirtySubObjects = BatchObjectInfo.bHasDirtySubObjects;
		Info.HasAttachments = BatchObjectInfo.bHasUnsentAttachments;

		// Indicate that we are now waiting for a new baseline to be acknowledged
		if (BatchObjectInfo.bSentState && BatchObjectInfo.NewBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
		{
			Info.PendingBaselineIndex = BatchObjectInfo.NewBaselineIndex;
		}
		
		const bool bObjectIsStillDirty = BatchObjectInfo.bHasUnsentAttachments || BatchObjectInfo.bHasDirtySubObjects;
		ObjectsWithDirtyChanges.SetBitValue(BatchObjectInfo.InternalIndex, bObjectIsStillDirty);

		// Reset scheduling priority if everything was replicated
		if (!bObjectIsStillDirty)
		{
			SchedulingPriorities[BatchObjectInfo.InternalIndex] = 0.0f;
		}
	}

#if UE_NET_IRIS_CSV_STATS
	if (bTrackObjectStats)
	{
		FNetSendStats& NetStats = WriteContext.Stats;

		// We count RootObjects if anything is sent in an object batch, even if it's just subobjects or attachments. This is to mimic UReplicationGraph::ReplicateSingleActor stats.
		if (BatchInfo.Type == EBatchInfoType::Object)
		{
			if (ObjectCount || AttachmentCount)
			{
				NetStats.AddNumberOfReplicatedRootObjects(1U);
			}
		}
		NetStats.AddNumberOfReplicatedObjects(ObjectCount);
		NetStats.AddNumberOfDeltaCompressedReplicatedObjects(DeltaCompressedObjectCount);
	}
#endif

	OutRecord.BatchCount = WrittenBatchCount;

	return WrittenObjectCount;
}

FReplicationWriter::EWriteObjectRetryMode FReplicationWriter::HandleObjectBatchFailure(FReplicationWriter::EWriteObjectStatus WriteObjectStatus, const FBatchInfo& BatchInfo, const FReplicationWriter::FBitStreamInfo& BatchBitStreamInfo)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_HandleObjectBatchFailure);
	
	// Cleanup data stored in BatchInfo
	for (const FBatchObjectInfo& BatchObjectInfo : BatchInfo.ObjectInfos)
	{
		// If we did not end up using the baseline we need to release it
		if (BatchObjectInfo.bSentState && BatchObjectInfo.NewBaselineIndex != FDeltaCompressionBaselineManager::InvalidBaselineIndex)
		{
			BaselineManager->LostBaseline(Parameters.ConnectionId, BatchObjectInfo.InternalIndex, BatchObjectInfo.NewBaselineIndex);
		}
		
		if (BatchObjectInfo.InternalIndex != ObjectIndexForOOBAttachment)
		{
			// If we failed to write the batch and we wrote data for an object we need to mark it as not written, if we want to try again
			WriteContext.ObjectsWrittenThisPacket.ClearBit(BatchObjectInfo.InternalIndex);
		}
	}

	if (WriteObjectStatus == EWriteObjectStatus::NoInstanceProtocol || WriteObjectStatus == EWriteObjectStatus::InvalidOwner)
	{
		return EWriteObjectRetryMode::TrySmallObject;
	}

	const uint32 BitsLeft = BatchBitStreamInfo.ReplicationCapacity - (BatchBitStreamInfo.BatchStartPos - BatchBitStreamInfo.ReplicationStartPos);
	if (BitsLeft < Parameters.SmallObjectBitThreshold)
	{
		return EWriteObjectRetryMode::Abort;
	}

	// If there are more bits left than the split threshold we treat it as a huge object and proceed with splitting.
	// We expect at least one part of the payload to be sendable if there are more bits left than the split threshold.
	if (CanQueueHugeObject() && PartialNetObjectAttachmentHandler != nullptr)
	{
		const uint32 SplitThreshold = PartialNetObjectAttachmentHandler->GetConfig()->GetBitCountSplitThreshold();
		if (BitsLeft > SplitThreshold)
		{
			//UE_LOG_REPLICATIONWRITER(TEXT("FReplicationWriter::HandleObjectBatchFailure Failed to write object with ParentInternalIndex: %u EWriteObjectRetryMode::SplitHugeObject"), BatchInfo.ParentInternalIndex);
			return EWriteObjectRetryMode::SplitHugeObject;
		}
	}
	else
	{
		IRIS_PROFILER_SCOPE(FReplicationWriter_BlockedByHugeOBjectAlreadyBeingSent);
	}

	// If we are allowed to request more packets to write, we should abort once we failed to write a small object
	const uint32 MaxFailedSmallObjectCount = WriteContext.bCanWriteMoreData ? 1U : Parameters.MaxFailedSmallObjectCount;
	if (WriteContext.FailedToWriteSmallObjectCount >= MaxFailedSmallObjectCount)	
	{
		return EWriteObjectRetryMode::Abort;
	}

	// Default- try some more, hopefully smaller state, objects.
	//UE_LOG_REPLICATIONWRITER(TEXT("FReplicationWriter::HandleObjectBatchFailure Failed to write object with ParentInternalIndex: %u EWriteObjectRetryMode::TrySmallObject"), BatchInfo.ParentInternalIndex);
	return EWriteObjectRetryMode::TrySmallObject;
}

UDataStream::EWriteResult FReplicationWriter::BeginWrite(const UDataStream::FBeginWriteParameters& Params)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_PrepareWrite);

	// For now we do not support partial writes
	check(!WriteContext.bIsValid);

	if (!bReplicationEnabled)
	{
		return UDataStream::EWriteResult::NoData;
	}

	// Initialize context which can be used over multiple calls to WriteData
	WriteContext.bHasUpdatedObjectsToSend  = 0U;
	WriteContext.bHasDestroyedObjectsToSend = 0U;
	WriteContext.bHasHugeObjectToSend = 0U;
	WriteContext.bHasOOBAttachmentsToSend = 0U;
	WriteContext.ScheduledObjectCount = 0u;

	WriteContext.WriteMode = Params.WriteMode;

	// Setup for writing PostTickDispatch data, currently this is only writing unreliable OOBAttachments.
	if (WriteContext.WriteMode == EDataStreamWriteMode::PostTickDispatch)
	{
		const bool bHasUnsentOOBAttachments = Attachments.HasUnsentUnreliableAttachments(ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment);
		if (!bHasUnsentOOBAttachments)
		{
			return UDataStream::EWriteResult::NoData;
		}
		WriteContext.bHasOOBAttachmentsToSend = bHasUnsentOOBAttachments;
		WriteContext.bCanWriteMoreData = Params.bCanWriteMoreData;
	}
	else
	{
		// See if we have any work to do
		const bool bHasUpdatedObjectsToSend = ObjectsWithDirtyChanges.IsAnyBitSet();
		const bool bHasDestroyedObjectsToSend = ObjectsPendingDestroy.IsAnyBitSet();
		const bool bHasUnsentOOBAttachments = Attachments.HasUnsentAttachments(ENetObjectAttachmentType::OutOfBand, ObjectIndexForOOBAttachment);
		const bool bHasUnsentHugeObject = Attachments.HasUnsentAttachments(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment);

		// Nothing to send
		if (!(bHasUpdatedObjectsToSend | bHasDestroyedObjectsToSend | bHasUnsentOOBAttachments | bHasUnsentHugeObject))
		{
			return UDataStream::EWriteResult::NoData;
		}

		// Initialize context which can be used over multiple calls to WriteData
		WriteContext.bHasUpdatedObjectsToSend  = bHasUpdatedObjectsToSend | bHasUnsentOOBAttachments | bHasUnsentHugeObject;
		WriteContext.bHasDestroyedObjectsToSend = bHasDestroyedObjectsToSend;
		WriteContext.bHasHugeObjectToSend = bHasUnsentHugeObject;
		WriteContext.bHasOOBAttachmentsToSend = bHasUnsentOOBAttachments;

		// $IRIS TODO: LinearAllocator/ScratchPad?
		// Allocate space for indices to send
		// This should be allocated from frame temp allocator and be cleaned up end of frame, we might want this data to persist over multiple write calls but not over multiple frames 
		// https://jira.it.epicgames.com/browse/UE-127374	
		WriteContext.ScheduledObjectInfos = reinterpret_cast<FScheduleObjectInfo*>(FMemory::Malloc(sizeof(FScheduleObjectInfo) * Parameters.MaxActiveReplicatedObjectCount));
		WriteContext.ScheduledObjectCount = ScheduleObjects(WriteContext.ScheduledObjectInfos);
	}

	// Reset dependent object array
	WriteContext.DependentObjectsPendingSend.Reset();

	WriteContext.CurrentIndex = 0U;
	WriteContext.FailedToWriteSmallObjectCount = 0U;
	WriteContext.SortedObjectCount = 0U;
	WriteContext.NumWrittenPacketsInThisBatch = 0U;
	WriteContext.bCanWriteMoreData = Params.bCanWriteMoreData;

	// Clear net stats. Used for CVS and Network Insights stats.
	WriteContext.Stats.Reset();
	WriteContext.Stats.SetNumberOfRootObjectsScheduledForReplication(WriteContext.ScheduledObjectCount);

	WriteContext.bIsValid = true;

	return UDataStream::EWriteResult::HasMoreData;
}

void FReplicationWriter::EndWrite()
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_FinishWrite);

	if (WriteContext.bIsValid)
	{
#if UE_NET_IRIS_CSV_STATS
		// Update stats
		{
			FNetSendStats& Stats = WriteContext.Stats;
			if (!HugeObjectSendQueue.IsEmpty())
			{
				Stats.SetNumberOfActiveHugeObjects(HugeObjectSendQueue.NumRootObjectsInTransit());

				if (HugeObjectSendQueue.Stats.EndSendingTime != 0)
				{
					Stats.AddHugeObjectWaitingTime(FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - HugeObjectSendQueue.Stats.EndSendingTime));
				}
				if (HugeObjectSendQueue.Stats.StartStallTime != 0)
				{
					Stats.AddHugeObjectStallTime(FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - HugeObjectSendQueue.Stats.StartStallTime));
				}
			}

			FNetSendStats& TotalStats = Parameters.ReplicationSystem->GetReplicationSystemInternal()->GetSendStats();
			TotalStats.Accumulate(Stats);
		}
#endif

 #if UE_NET_ENABLE_REPLICATIONWRITER_LOG
		// See if we failed to write any objects
		if (const int32 NumPendingDependentObjects = WriteContext.DependentObjectsPendingSend.Num())
		{
			UE_LOG_REPLICATIONWRITER_WARNING(TEXT("FReplicationWriter::EndWrite() Has %d more dependent objects to write"), NumPendingDependentObjects);
		}
		
		if (const int32 NumPendingObjectsToWrite = WriteContext.ScheduledObjectCount - WriteContext.CurrentIndex)
		{
			UE_LOG_REPLICATIONWRITER_WARNING(TEXT("FReplicationWriter::EndWrite() Has %d more objects with dirty data"), NumPendingObjectsToWrite);
		}
#endif

		FMemory::Free(WriteContext.ScheduledObjectInfos);
		WriteContext.ScheduledObjectInfos = nullptr;
		WriteContext.bIsValid = false;
	}
}

bool FReplicationWriter::HasDataToSend(const FWriteContext& Context) const
{
	return WriteContext.bIsValid & (Context.bHasDestroyedObjectsToSend | Context.bHasUpdatedObjectsToSend | Context.bHasHugeObjectToSend | Context.bHasOOBAttachmentsToSend);
}

UDataStream::EWriteResult FReplicationWriter::Write(FNetSerializationContext& Context)
{
	IRIS_PROFILER_SCOPE(FReplicationWriter_Write);

	if (!HasDataToSend(WriteContext))
	{
		return UDataStream::EWriteResult::NoData;
	}

	// We have somethings in the WriteContext that must be reset each packet
	WriteContext.FailedToWriteSmallObjectCount = 0U;
	WriteContext.WrittenBatchCount = 0U;

	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	// Setup internal context
	FInternalNetSerializationContext InternalContext(Parameters.ReplicationSystem);
	Context.SetLocalConnectionId(Parameters.ConnectionId);
	Context.SetInternalContext(&InternalContext);
	Context.SetNetStatsContext(NetTypeStats->GetNetStatsContext());

	// Give some info for the case when we consider splitting a huge object.
	WriteBitStreamInfo.ReplicationStartPos = Writer.GetPosBits();
	WriteBitStreamInfo.ReplicationCapacity = Writer.GetBitsLeft();

	UE_NET_TRACE_SCOPE(ReplicationData, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
	
	FNetBitStreamRollbackScope Rollback(Writer);
	const uint32 HeaderPos = Writer.GetPosBits();

	uint32 WrittenObjectCount = 0;
	const uint32 OldReplicationInfoCount = ReplicationRecord.GetInfoCount();

	// Written batch count
	Writer.WriteBits(0U, 16);

	// Write timestamps etc? Or do we do this in header.
	// WriteReplicationFrameData();

	const uint32 WrittenObjectsPendingDestroyCount = WriteObjectsPendingDestroy(Context);
	WrittenObjectCount += WrittenObjectsPendingDestroyCount;

	// Only reason for overflow here is if we did not fit header
	if (Writer.IsOverflown())
	{
		return UDataStream::EWriteResult::NoData;
	}

	WrittenObjectCount += WriteOOBAttachments(Context);

	WrittenObjectCount += WriteObjects(Context);

	UDataStream::EWriteResult WriteResult = UDataStream::EWriteResult::Ok;

	// If we have more data to write, request more updates
	// $IRIS $TODO: When we have better control over bandwidth usage, introduce setting to only allow over-commit if we have a huge object or split rpc to send
	// https://jira.it.epicgames.com/browse/UE-127371
	if (HasDataToSend(WriteContext))
	{
		if (WriteContext.bHasHugeObjectToSend)
		{
			WriteResult = UDataStream::EWriteResult::HasMoreData;
		}
		else if (WriteContext.bCanWriteMoreData && ((int32)WriteContext.NumWrittenPacketsInThisBatch < GReplicationWriterMaxAllowedPacketsIfNotHugeObject))
		{
			WriteResult = UDataStream::EWriteResult::HasMoreData;
		}
		else
		{
			WriteResult = UDataStream::EWriteResult::Ok;
		}
	}
	
	if (!Writer.IsOverflown() && WrittenObjectCount > 0)
	{
		{
			// Seek back to HeaderPos and update the header
			FNetBitStreamWriteScope WriteScope(Writer, HeaderPos);
			const uint32 TotalWrittenBatchCount = WriteContext.WrittenBatchCount + WrittenObjectsPendingDestroyCount;
			Writer.WriteBits(TotalWrittenBatchCount, 16);
		}

		//UE_LOG_REPLICATIONWRITER(TEXT("FReplicationWriter::Write() Wrote %u Objects for ConnectionId:%u, ReplicationSystemId: %u."), WrittenObjectCount, Parameters.ConnectionId, Parameters.ReplicationSystem->GetId());	
	
		// Push record
		const uint16 ReplicationInfoCount = static_cast<uint16>(ReplicationRecord.GetInfoCount() - OldReplicationInfoCount);
		ReplicationRecord.PushRecord(ReplicationInfoCount);

#if UE_NET_VALIDATE_REPLICATION_RECORD
		check(s_ValidateReplicationRecord(&ReplicationRecord, Parameters.MaxActiveReplicatedObjectCount + 1U, false));
#endif

#if UE_NET_TRACE_ENABLED
		if (FNetTraceCollector* Collector = HugeObjectSendQueue.TraceCollector)
		{
			FNetTrace::FoldTraceCollector(Context.GetTraceCollector(), Collector, GetBitStreamPositionForNetTrace(Writer));
			Collector->Reset();
		}
#endif

		++WriteContext.NumWrittenPacketsInThisBatch;
	}
	else 
	{
		// Trigger rollback as we did not write any data
		Writer.DoOverflow();
		WriteResult = UDataStream::EWriteResult::NoData;
	}

	// Report packet stats
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationWriter.WrittenObjectCount, WrittenObjectCount, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationWriter.WrittenBatchCount, WriteContext.WrittenBatchCount, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationWriter.FailedToWriteSmallObjectCount, WriteContext.FailedToWriteSmallObjectCount, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationWriter.RemainingObjectsPendingWriteCount, WriteContext.ScheduledObjectCount - WriteContext.CurrentIndex, ENetTraceVerbosity::Trace);
	UE_NET_TRACE_PACKET_STATSCOUNTER(Parameters.ReplicationSystem->GetId(), Parameters.ConnectionId, ReplicationWriter.ScheduledObjectCount, WriteContext.ScheduledObjectCount, ENetTraceVerbosity::Trace);

	return WriteResult;
}

void FReplicationWriter::SetupReplicationInfoForAttachmentsToObjectsNotInScope()
{
	FReplicationInfo& Info = GetReplicationInfo(ObjectIndexForOOBAttachment);
	Info = FReplicationInfo();
	Info.State = static_cast<uint32>(EReplicatedObjectState::AttachmentToObjectNotInScope);
	ReplicationRecord.ResetList(ReplicatedObjectsRecordInfoLists[ObjectIndexForOOBAttachment]);
}

void FReplicationWriter::ApplyFilterToChangeMask(uint32 ParentInternalIndex, uint32 InternalIndex, FReplicationInfo& Info, const FReplicationProtocol* Protocol, const uint8* InternalStateBuffer, bool bIsInitialState)
{
	const uint32* ConditionalChangeMaskPointer = (EnumHasAnyFlags(Protocol->ProtocolTraits, EReplicationProtocolTraits::HasConditionalChangeMask) ? reinterpret_cast<const uint32*>(InternalStateBuffer + Protocol->GetConditionalChangeMaskOffset()) : static_cast<const uint32*>(nullptr));
	const bool bChangeMaskWasModified = ReplicationConditionals->ApplyConditionalsToChangeMask(Parameters.ConnectionId, bIsInitialState, ParentInternalIndex, InternalIndex, Info.GetChangeMaskStoragePointer(), ConditionalChangeMaskPointer, Protocol);
	if (bChangeMaskWasModified && !MakeNetBitArrayView(Info.GetChangeMaskStoragePointer(), Info.ChangeMaskBitCount).IsAnyBitSet())
	{
		Info.HasDirtyChangeMask = 0;
	}
}

void FReplicationWriter::InvalidateBaseline(uint32 InternalIndex, FReplicationInfo& Info)
{
	FReplicationRecord::FRecordInfoList& RecordInfoList = ReplicatedObjectsRecordInfoLists[InternalIndex];

	// Iterate over all data in flight for this object and mark any new baselines as invalid to avoid acking or nacking an invalidated baseline
	FReplicationRecord::FRecordInfo* CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(RecordInfoList.FirstRecordIndex);
	while (CurrentRecordInfo)
	{
		CurrentRecordInfo->NewBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
		CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(CurrentRecordInfo->NextIndex);
	};

	Info.LastAckedBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
	Info.PendingBaselineIndex = FDeltaCompressionBaselineManager::InvalidBaselineIndex;
}

bool FReplicationWriter::HasInFlightStateChanges(const FReplicationRecord::FRecordInfo* RecordInfo) const
{
	while (RecordInfo)
	{
		if (RecordInfo->HasChangeMask)
		{
			return true;
		}
		RecordInfo = ReplicationRecord.GetInfoForIndex(RecordInfo->NextIndex);
	};

	return false;
}

bool FReplicationWriter::HasInFlightStateChanges(uint32 InternalIndex, const FReplicationInfo& Info) const
{
	const FReplicationRecord::FRecordInfoList& RecordInfoList = ReplicatedObjectsRecordInfoLists[InternalIndex];
	const FReplicationRecord::FRecordInfo* CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(RecordInfoList.FirstRecordIndex);

	return HasInFlightStateChanges(CurrentRecordInfo);
}

bool FReplicationWriter::PatchupObjectChangeMaskWithInflightChanges(uint32 InternalIndex, FReplicationInfo& Info)
{
	bool bInFlightChangesAdded = false;

	const FReplicationRecord::FRecordInfoList& RecordInfoList = ReplicatedObjectsRecordInfoLists[InternalIndex];

	FNetBitArrayView ChangeMask = FChangeMaskUtil::MakeChangeMask(Info.ChangeMaskOrPtr, Info.ChangeMaskBitCount);

	// Iterate over all data in flight for this object and include any changes in flight to ensure atomicity when received
	// N.B. We don't check if this object is in huge object mode and check to see if any of these changes were part of that payload.
	const FReplicationRecord::FRecordInfo* CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(RecordInfoList.FirstRecordIndex);
	while (CurrentRecordInfo)
	{
		if (CurrentRecordInfo->HasChangeMask)
		{
			bInFlightChangesAdded = true;
			const FNetBitArrayView CurrentRecordInfoChangeMask(FChangeMaskUtil::MakeChangeMask(CurrentRecordInfo->ChangeMaskOrPtr, Info.ChangeMaskBitCount));
			ChangeMask.Combine(CurrentRecordInfoChangeMask, FNetBitArrayView::OrOp);
		}

		CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(CurrentRecordInfo->NextIndex);
	};

	return bInFlightChangesAdded;
}

void FReplicationWriter::SetNetExports(FNetExports& InNetExports)
{
	NetExports = &InNetExports;
}

bool FReplicationWriter::IsActiveHugeObject(uint32 InternalIndex) const
{
	constexpr bool bIncludeSubObjects = false;
	return HugeObjectSendQueue.IsObjectInQueue(InternalIndex, bIncludeSubObjects);
}

bool FReplicationWriter::IsObjectPartOfActiveHugeObject(uint32 InternalIndex) const
{
	constexpr bool bFullSearch = true;
	return HugeObjectSendQueue.IsObjectInQueue(InternalIndex, bFullSearch);
}

bool FReplicationWriter::CanQueueHugeObject() const
{
	if (HugeObjectSendQueue.IsFull())
	{
		return false;
	}

	// Check whether the reliable queue is full in which case there's no point in queueing additional huge objects.
	if (!Attachments.CanSendMoreReliableAttachments(ENetObjectAttachmentType::HugeObject, ObjectIndexForOOBAttachment))
	{
		return false;
	}

	return true;
}

void FReplicationWriter::FreeHugeObjectSendQueue()
{
	HugeObjectSendQueue.FreeContexts([this](const FHugeObjectContext& HugeObjectContext)
	{
		for (const FObjectRecord& ObjectRecord : HugeObjectContext.BatchRecord.ObjectReplicationRecords)
		{
			FReplicationInfo& ReplicationInfo = this->GetReplicationInfo(ObjectRecord.Record.Index);
			const uint32 ChangeMaskBitCount = ReplicationInfo.ChangeMaskBitCount;
			if (ObjectRecord.Record.HasChangeMask)
			{
				FChangeMaskStorageOrPointer::Free(ObjectRecord.Record.ChangeMaskOrPtr, ChangeMaskBitCount, s_DefaultChangeMaskAllocator);
			}
		}
	});
}

void FReplicationWriter::CollectAndAppendExports(FNetSerializationContext& Context, uint8* RESTRICT InternalBuffer, const FReplicationProtocol* Protocol) const
{
	FNetExportContext* ExportContext = Context.GetExportContext();
	if (!ExportContext)
	{
		return;
	}

	FNetReferenceCollector Collector(ENetReferenceCollectorTraits::OnlyCollectReferencesThatCanBeExported);
	FReplicationProtocolOperationsInternal::CollectReferences(Context, Collector, InternalBuffer, Protocol);

	for (const FNetReferenceCollector::FReferenceInfo& Info : MakeArrayView(Collector.GetCollectedReferences()))
	{
		ObjectReferenceCache->AddPendingExport(*ExportContext, Info.Reference);
	}
}

bool FReplicationWriter::IsWriteObjectSuccess(EWriteObjectStatus Status) const
{
	return (Status == EWriteObjectStatus::Success) | (Status == EWriteObjectStatus::InvalidState);
}

void FReplicationWriter::DiscardAllRecords()
{
	TReplicationRecordHelper Helper(ReplicatedObjects, ReplicatedObjectsRecordInfoLists, &ReplicationRecord);

	const uint32 RecordCount = ReplicationRecord.GetRecordCount();
	for (uint32 RecordIt = 0, RecordEndIt = RecordCount; RecordIt != RecordEndIt; ++RecordIt)
	{
		if (const uint32 RecordInfoCount = ReplicationRecord.PopRecord())
		{
			Helper.Process(RecordInfoCount,
				[this](const FReplicationRecord::FRecordInfo& RecordInfo, FReplicationInfo& Info, const FNetObjectAttachmentsWriter::FReliableReplicationRecord& AttachmentRecord)
				{
					HandleDiscardedRecord(RecordInfo, Info, AttachmentRecord);
				}
			);
		}
	}
}

// Do minimal work to free references and resources. We assume connection removal handling will be dealt with by respective subsystems,
// such as the DeltaCompressionBaselinemanager releasing baselines. 
void FReplicationWriter::StopAllReplication()
{
	if (ReplicatedObjects.Num() == 0)
	{
		return;
	}

	// Don't process special index.
	ReplicatedObjects[ObjectIndexForOOBAttachment].State = (uint8)EReplicatedObjectState::Invalid;

	// We cannot tell for sure which objects need processing so we check them all.
	int32 ReplicatedObjectsCount = ReplicatedObjects.Num();
	for (int32 InternalIndex = 0; InternalIndex < ReplicatedObjectsCount; InternalIndex++)
	{
		const FReplicationInfo& Info = GetReplicationInfo(InternalIndex);

		if (Info.GetState() == EReplicatedObjectState::Invalid)
		{
			continue;
		}

		// Free allocated ChangeMask (if it is allocated)
		FChangeMaskStorageOrPointer::Free(Info.ChangeMaskOrPtr, Info.ChangeMaskBitCount, s_DefaultChangeMaskAllocator);

		// Release object reference
		NetRefHandleManager->ReleaseNetObjectRef(InternalIndex);
	}
}

void FReplicationWriter::MarkObjectDirty(FInternalNetRefIndex InternalIndex, const char* Caller)
{
	if (bValidateObjectsWithDirtyChanges)
	{
		const FReplicationInfo& ObjectInfo = ReplicatedObjects[InternalIndex];
		if (!ensureMsgf(ObjectInfo.GetState() != EReplicatedObjectState::Invalid && ObjectInfo.GetState() < EReplicatedObjectState::PendingDestroy, TEXT("Object ( InternalIndex: %u ) with Invalid state marked dirty. Caller: %hs"), InternalIndex, Caller))
		{
			return;
		}
	}

	ObjectsWithDirtyChanges.SetBit(InternalIndex);
}

void FReplicationWriter::OnLargestIndexIncrease(uint32 InternalIndex)
{
	ReplicatedObjects.SetNumZeroed(InternalIndex);
	ReplicatedObjectsRecordInfoLists.SetNumZeroed(InternalIndex);
	SchedulingPriorities.SetNumZeroed(InternalIndex);
}


FReplicationWriter::FHugeObjectContext::FHugeObjectContext() = default;

FReplicationWriter::FHugeObjectContext::~FHugeObjectContext() = default;

// HugeObjectSendQueue implementation
FReplicationWriter::FHugeObjectSendQueue::FHugeObjectSendQueue()
: DebugName(CreatePersistentNetDebugName(TEXT("HugeObjectState"), UE_ARRAY_COUNT(TEXT("HugeObjectState"))))
{
#if UE_NET_TRACE_ENABLED
	DebugName->DebugNameId = FNetTrace::TraceName(DebugName->Name);
#endif
}

FReplicationWriter::FHugeObjectSendQueue::~FHugeObjectSendQueue()
{
	UE_NET_TRACE_DESTROY_COLLECTOR(TraceCollector);
	TraceCollector = nullptr;
}

// TODO: If reliable queue is full should we keep on filling up?
bool FReplicationWriter::FHugeObjectSendQueue::IsFull() const
{
	const int32 QueueSize = FPlatformMath::Max<int32>(GReplicationWriterMaxHugeObjectsInTransit, 1);
	return RootObjectsInTransit.Num() > QueueSize;
}

bool FReplicationWriter::FHugeObjectSendQueue::IsEmpty() const
{
	return RootObjectsInTransit.IsEmpty();
}

uint32 FReplicationWriter::FHugeObjectSendQueue::NumRootObjectsInTransit() const
{
	return static_cast<uint32>(RootObjectsInTransit.Num());
}

bool FReplicationWriter::FHugeObjectSendQueue::EnqueueHugeObject(const FHugeObjectContext& Context)
{
	if (IsFull())
	{
		return false;
	}

	if (RootObjectsInTransit.Find(Context.RootObjectInternalIndex))
	{
		ensureMsgf(false, TEXT("An object that is already in the huge object queue should not try replicating again ( InternalIndex: %u )"), Context.RootObjectInternalIndex);
		return false;
	}

	RootObjectsInTransit.Add(Context.RootObjectInternalIndex);
	// Note: Lists don't have methods to perform moving of an element.
	SendContexts.AddTail(Context);
	return true;
}

// Returns true if the object is a huge object root object or part of any huge object's payload. The latter is an expensive operation.
bool FReplicationWriter::FHugeObjectSendQueue::IsObjectInQueue(FInternalNetRefIndex ObjectIndex, bool bFullSearch) const
{
	if (IsEmpty())
	{
		return false;
	}

	if (RootObjectsInTransit.Find(ObjectIndex))
	{
		return true;
	}

	if (!bFullSearch)
	{
		return false;
	}

	for (const FHugeObjectContext& Context : SendContexts)
	{
		const FBatchRecord& BatchRecord = Context.BatchRecord;
		for (const FObjectRecord& ObjectRecord : MakeArrayView(BatchRecord.ObjectReplicationRecords.GetData(), BatchRecord.ObjectReplicationRecords.Num()))
		{
			if (ObjectIndex == ObjectRecord.Record.Index)
			{
				return true;
			}
		}
	}

	return false;
}

 FInternalNetRefIndex FReplicationWriter::FHugeObjectSendQueue::GetRootObjectInternalIndexForTrace() const
 {
	 const TDoubleLinkedList<FHugeObjectContext>::TDoubleLinkedListNode* TailNode = SendContexts.GetTail();
	 if (TailNode)
	 {
		 return TailNode->GetValue().RootObjectInternalIndex;
	 }

	return FNetRefHandleManager::InvalidInternalIndex;
 }

void FReplicationWriter::FHugeObjectSendQueue::AckObjects(TFunctionRef<void (const FHugeObjectContext& Context)> AckHugeObject)
{
	for (TDoubleLinkedList<FHugeObjectContext>::TDoubleLinkedListNode* Node = SendContexts.GetHead(), *NextNode = nullptr; Node != nullptr; Node = NextNode)
	{
		NextNode = Node->GetNextNode();

		FHugeObjectContext& Context = Node->GetValue();

		// Iterate over the blobs backwards to break out of the loop as quickly as possible.
		bool bObjectIsAcked = true;
		for (TRefCountPtr<FNetBlob>& Blob : ReverseIterate(Context.Blobs))
		{
			const uint32 RefCount = Blob.GetRefCount();		
			if (RefCount > 1)
			{
				bObjectIsAcked = false;
				break;
			}
			else if (RefCount == 1)
			{
				// We no longer need to keep this blob around as we're the only thing referencing it.
				Blob.SafeRelease();
			}
		}

		if (!bObjectIsAcked)
		{
			// As clients deliver hugeobjects parts in order we cannot ack later objects until previous ones have been fully acked.
			break;
		}

		AckHugeObject(Context);

		// Remove from fast lookup set.
		RootObjectsInTransit.Remove(Context.RootObjectInternalIndex);

		// Remove from queue.
		SendContexts.RemoveNode(Node);

		ensure(SendContexts.IsEmpty() == RootObjectsInTransit.IsEmpty());
	}

	if (RootObjectsInTransit.IsEmpty())
	{
		Stats = FStats();
	}
}

void FReplicationWriter::FHugeObjectSendQueue::FreeContexts(TFunctionRef<void (const FHugeObjectContext& Context)> FreeHugeObject)
{
	for (const FHugeObjectContext& Context : SendContexts)
	{
		FreeHugeObject(Context);
	}

	SendContexts.Empty();
	RootObjectsInTransit.Empty();
}

}
