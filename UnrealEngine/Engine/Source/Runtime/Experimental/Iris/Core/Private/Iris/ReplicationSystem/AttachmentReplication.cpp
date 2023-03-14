// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/AttachmentReplication.h"
#include "HAL/IConsoleManager.h"
#include "Iris/Core/IrisLog.h"
#include "Net/Core/Trace/NetTrace.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlobAssembler.h"
#include "Iris/ReplicationSystem/NetBlob/SequentialPartialNetBlobHandler.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetExportContext.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"

namespace UE::Net::Private
{

namespace AttachmentReplicationCVars
{
	static int32 UnreliableRPCQueueSize = 10;
	FAutoConsoleVariableRef CVarUnreliableRPCQueueSize(TEXT("net.UnreliableRPCQueueSize"), UnreliableRPCQueueSize, TEXT("Maximum number of unreliable RPCs queued per object. If more RPCs are queued then older ones will be dropped."));

	static int32 ReliableRPCQueueSize = 4096;
	FAutoConsoleVariableRef CVarReliableRPCQueueSize(TEXT("net.ReliableRPCQueueSize"), ReliableRPCQueueSize, TEXT("Maximum number of reliable RPCs queued per object. This is in addition to the 256 that are in the send window. This is to support very large RPCs that are split into smaller pieces."));

	static int32 ClientToServerUnreliableRPCQueueSize = 16;
	FAutoConsoleVariableRef CVarClientToServerUnreliableRPCQueueSize(TEXT("net.ClientToServerUnreliableRPCQueueSize"), ClientToServerUnreliableRPCQueueSize, TEXT( "Maximum number of unreliable RPCs queued for sending from the client to the server. If more RPCs are queued then older ones will be dropped."));

	static int32 MaxSimultaneousObjectsWithRPCs = 4096;
	FAutoConsoleVariableRef CVarMaxSimultaneousObjectsWithRPCs(TEXT("net.MaxSimultaneousObjectsWithRPCs"), MaxSimultaneousObjectsWithRPCs, TEXT("Maximum number of objects that can have unsent RPCs at the same time. "));
}

static const FName NetError_UnreliableQueueFull("Unreliable attachment queue full");
static const FName NetError_UnsupportedNetBlob("Unsupported NetBlob type");
static const FName NetError_TooManyObjectsWithFunctionCalls("Too many objects being targeted by RPCs at the same time.");

// SendQueue and Writer
class FNetObjectAttachmentSendQueue::FReliableSendQueue
{
public:
	FReliableSendQueue()
	: MaxPreQueueCount(AttachmentReplicationCVars::ReliableRPCQueueSize)
	{
	}

	bool HasUnsentBlobs() const
	{
		return PreQueue.Num() > 0 || ReliableQueue.HasUnsentBlobs();
	}

	bool CanSendBlobs() const
	{
		return ReliableQueue.HasUnsentBlobs();
	}

	bool IsSafeToDestroy() const
	{
		return ReliableQueue.IsSafeToDestroy();
	}

	bool IsAllSentAndAcked() const
	{
		return PreQueue.Num() == 0 && ReliableQueue.IsAllSentAndAcked();
	}

	uint32 GetUnsentBlobCount() const
	{
		return ReliableQueue.GetUnsentBlobCount() + static_cast<uint32>(PreQueue.Num());
	}

	bool Enqueue(TArrayView<const TRefCountPtr<FNetBlob>> Attachments)
	{
		// If we have blobs in the pre queue then continue adding to that.
		const uint32 PreQueueCount = static_cast<uint32>(PreQueue.Num());
		const uint32 AttachmentCount = static_cast<uint32>(Attachments.Num());

		// Assume that we won't fit all the attachments unless they can fit in the pre-queue.
		if (AttachmentCount + PreQueueCount > MaxPreQueueCount)
		{
			return false;
		}

		uint32 QueuedCount = 0;
		if (PreQueueCount == 0)
		{
			for (const TRefCountPtr<FNetBlob>& Attachment : Attachments)
			{
				if (!ReliableQueue.Enqueue(Attachment))
				{
					break;
				}

				++QueuedCount;
			}
		}

		const uint32 AddToPreQueueCount = AttachmentCount - QueuedCount;
		if (AddToPreQueueCount > 0)
		{
			PreQueue.Append(Attachments.GetData() + QueuedCount, AddToPreQueueCount);
		}

		return true;
	}

	uint32 Serialize(FNetSerializationContext& Context, FNetHandle NetHandle, FReliableNetBlobQueue::ReplicationRecord& OutRecord)
	{
		if (NetHandle.IsValid())
		{
			return ReliableQueue.SerializeWithObject(Context, NetHandle, OutRecord);
		}
		else
		{
			return ReliableQueue.Serialize(Context, OutRecord);
		}
	}

	void ProcessPacketDeliveryStatus(EPacketDeliveryStatus Status, FReliableNetBlobQueue::ReplicationRecord Record)
	{
		ReliableQueue.ProcessPacketDeliveryStatus(Status, Record);
		if (Status == EPacketDeliveryStatus::Delivered && Record != FReliableNetBlobQueue::InvalidReplicationRecord)
		{
			PopulateQueueFromPreQueue();
		}
	}

	void CommitReplicationRecord(FReliableNetBlobQueue::ReplicationRecord Record)
	{
		ReliableQueue.CommitReplicationRecord(Record);
	}

private:
	void PopulateQueueFromPreQueue()
	{
		const uint32 AttachmentCount = static_cast<uint32>(PreQueue.Num());
		if (AttachmentCount == 0)
		{
			return;
		}

		// Similar to enqueueing, but without all the checks as we've already passed them.
		TArray<TRefCountPtr<FNetBlob>> PrevPreQueue = MoveTemp(PreQueue);
		TArrayView<const TRefCountPtr<FNetBlob>> Attachments = MakeArrayView(PrevPreQueue);

		uint32 QueuedCount = 0;
		for (const TRefCountPtr<FNetBlob>& Attachment : Attachments)
		{
			if (!ReliableQueue.Enqueue(Attachment))
			{
				break;
			}

			++QueuedCount;
		}

		const uint32 AddToPreQueueCount = AttachmentCount - QueuedCount;
		if (AddToPreQueueCount > 0)
		{
			PreQueue.Append(Attachments.GetData() + QueuedCount, AddToPreQueueCount);
		}
	}

	FReliableNetBlobQueue ReliableQueue;
	TArray<TRefCountPtr<FNetBlob>> PreQueue;
	const uint32 MaxPreQueueCount;
};

FNetObjectAttachmentSendQueue::FNetObjectAttachmentSendQueue()
: ReliableQueue(nullptr)
, MaxUnreliableCount(AttachmentReplicationCVars::UnreliableRPCQueueSize)
{
}

FNetObjectAttachmentSendQueue::~FNetObjectAttachmentSendQueue()
{
	delete ReliableQueue;
}

bool FNetObjectAttachmentSendQueue::Enqueue(TArrayView<const TRefCountPtr<FNetBlob>> Attachments)
{
	const FNetBlobCreationInfo& CreationInfo = Attachments[0]->GetCreationInfo();
	if (EnumHasAnyFlags(CreationInfo.Flags, ENetBlobFlags::Reliable))
	{
		if (ReliableQueue == nullptr)
		{
			ReliableQueue = new FReliableSendQueue();
		}

		return ReliableQueue->Enqueue(Attachments);
	}
	else
	{
		const uint32 TotalCountNeeded = UnreliableQueue.Count() + Attachments.Num();
		if (TotalCountNeeded > MaxUnreliableCount)
		{
			UE_LOG(LogIris, Verbose, TEXT("Dropping old RPC due to too many unreliable Attachments: %d max: %u"), UnreliableQueue.Count(), MaxUnreliableCount);
			UnreliableQueue.PopNoCheck(TotalCountNeeded - MaxUnreliableCount);
		}

		for (const TRefCountPtr<FNetBlob>& Attachment : Attachments)
		{
			UnreliableQueue.Enqueue(Attachment);
		}
	}

	return true;
}

void FNetObjectAttachmentSendQueue::DropUnreliable(bool& bOutHasUnsent)
{
	UnreliableQueue.Empty();
	bOutHasUnsent = (ReliableQueue != nullptr && ReliableQueue->HasUnsentBlobs());
}

bool FNetObjectAttachmentSendQueue::HasUnsent() const
{
	return !UnreliableQueue.IsEmpty() || (ReliableQueue != nullptr && ReliableQueue->HasUnsentBlobs());
}

bool FNetObjectAttachmentSendQueue::IsAllSentAndAcked() const
{
	return UnreliableQueue.IsEmpty() && (ReliableQueue == nullptr || ReliableQueue->IsAllSentAndAcked());
}

bool FNetObjectAttachmentSendQueue::IsSafeToDestroy() const
{
	return UnreliableQueue.IsEmpty() && (ReliableQueue == nullptr || ReliableQueue->IsSafeToDestroy());
}

void FNetObjectAttachmentSendQueue::SetUnreliableQueueCapacity(uint32 QueueCapacity)
{
	// MaxUnreliableCount is what prevents the queue from growing too large.
	MaxUnreliableCount = QueueCapacity;
	
	const uint32 UnreliableCount = UnreliableQueue.Count();
	if (QueueCapacity >= UnreliableCount)
	{
		return;
	}

	const uint32 DropCount = UnreliableCount - QueueCapacity;
	UE_LOG(LogIris, Warning, TEXT("Dropping %u attachments due to change in unreliable queue capacity to %u"), DropCount, QueueCapacity);
	UnreliableQueue.PopNoCheck(DropCount);
}

EAttachmentWriteStatus FNetObjectAttachmentSendQueue::Serialize(FNetSerializationContext& Context, FNetHandle NetHandle, FNetObjectAttachmentSendQueue::ReplicationRecord& OutRecord, bool& bOutHasUnsentAttachments)
{
	FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();

	FInternalRecord ReplicationRecord;
	// This count is the total number of unsent reliable blobs. If the reliable window is full no blobs can be sentuntil some have been acked.
	const uint32 UnsentReliableCount = (ReliableQueue != nullptr ? ReliableQueue->GetUnsentBlobCount() : 0U);
	const bool bCanSendReliableAttachments = UnsentReliableCount > 0 && ReliableQueue->CanSendBlobs();
	const bool bHasUnreliableAttachments = !UnreliableQueue.IsEmpty();
	if (!(bCanSendReliableAttachments || bHasUnreliableAttachments))
	{
		// Ideally we shouldn't get here, but we can handle it. Important to overflow to report a soft error.
		Writer.DoOverflow();
		OutRecord = ReplicationRecord.CombinedRecord;
		bOutHasUnsentAttachments = false;

		if (UnsentReliableCount > 0)
		{
			// We want to send reliable blobs but the window is full.
			return EAttachmentWriteStatus::ReliableWindowFull;
		}
		else
		{
			return EAttachmentWriteStatus::NoAttachments;
		}
	}

	UE_NET_TRACE_SCOPE(RPCs, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	Writer.WriteBool(bCanSendReliableAttachments);
	const uint32 HasUnreliableAttachmentsWritePos = Writer.GetPosBits();
	Writer.WriteBool(bHasUnreliableAttachments);

	if (Writer.IsOverflown())
	{
		OutRecord = ReplicationRecord.CombinedRecord;
		bOutHasUnsentAttachments = true;
		return EAttachmentWriteStatus::BitstreamOverflow;
	}

	uint32 SerializedReliableCount = 0;
	if (bCanSendReliableAttachments)
	{
		UE_NET_TRACE_SCOPE(Reliable, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		SerializedReliableCount = SerializeReliable(Context, NetHandle, ReplicationRecord.ReliableRecord);
		// If we couldn't fit any reliable attachments then don't even try unreliable
		if (SerializedReliableCount == 0)
		{
			Writer.DoOverflow();
			OutRecord = ReplicationRecord.CombinedRecord;
			bOutHasUnsentAttachments = true;
			return EAttachmentWriteStatus::BitstreamOverflow;
		}
	}

	// Assume success and change when necessary.
	EAttachmentWriteStatus WriteStatus = EAttachmentWriteStatus::Success;

	uint32 SerializedUnreliableCount = 0;
	if (bHasUnreliableAttachments)
	{
		UE_NET_TRACE_SCOPE(Unreliable, Writer, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		SerializedUnreliableCount = SerializeUnreliable(Context, NetHandle, ReplicationRecord.UnreliableRecord);
		if (SerializedUnreliableCount == 0)
		{
			// If we didn't manage to send anything then inform the caller of this via overflowing the bitstream
			if (SerializedReliableCount == 0)
			{
				Writer.DoOverflow();
				// Even if we couldn't send reliable attachments due to the window being full we still probably wouldn't have fit any, so return the overflow status.
				WriteStatus = EAttachmentWriteStatus::BitstreamOverflow;
			}
			// Patch up information previously saying there were unreliable attachments because we couldn't fit any in the bitstream
			else
			{
				FNetBitStreamWriteScope WriteScope(Writer, HasUnreliableAttachmentsWritePos);
				Writer.WriteBool(false);
			}
		}
	}

	if (WriteStatus == EAttachmentWriteStatus::Success)
	{
		// Override WriteStatus with ReliableWindowFull in case we fit one or more unreliable attachments but weren't able to write any reliable ones.
		if (UnsentReliableCount > 0 && !bCanSendReliableAttachments)
		{
			WriteStatus = EAttachmentWriteStatus::ReliableWindowFull;
		}
	}

	OutRecord = ReplicationRecord.CombinedRecord;
	bOutHasUnsentAttachments = (SerializedReliableCount < UnsentReliableCount) || (SerializedUnreliableCount < UnreliableQueue.Count());
	return WriteStatus;
}

uint32 FNetObjectAttachmentSendQueue::SerializeReliable(FNetSerializationContext& Context, FNetHandle NetHandle, FReliableNetBlobQueue::ReplicationRecord& OutRecord)
{
	return ReliableQueue->Serialize(Context, NetHandle, OutRecord);
}

uint32 FNetObjectAttachmentSendQueue::SerializeUnreliable(FNetSerializationContext& Context, FNetHandle NetHandle, uint32& OutRecord)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	const FObjectReferenceCache* ObjectReferenceCache = Context.GetInternalContext()->ObjectReferenceCache;

	uint32 SerializedUnreliableCount = 0;
	uint32 PrevHasMoreAttachmentsWritePos = 0;
	const bool bSerializeWithObject = NetHandle.IsValid();
	for (uint32 AttachmentIt = 0, AttachmentEndIt = UnreliableQueue.Count(); AttachmentIt != AttachmentEndIt; ++AttachmentIt)
	{
		FNetBitStreamRollbackScope RollbackScope(*Writer);
		FNetExportRollbackScope ExportScope(Context);

		const TRefCountPtr<FNetBlob>& Attachment = UnreliableQueue.PeekAtOffsetNoCheck(AttachmentIt);

		// Write exports, if attachment does not fit we will rollback exports as well.
		ObjectReferenceCache->WriteExports(Context, Attachment->CallGetExports());

		Attachment->SerializeCreationInfo(Context, Attachment->GetCreationInfo());
		if (bSerializeWithObject)
		{
			Attachment->SerializeWithObject(Context, NetHandle);
		}
		else
		{
			Attachment->Serialize(Context);
		}

		const uint32 HasMoreAttachmentsWritePos = Writer->GetPosBits();
		const bool bHasMoreAttachments = AttachmentIt + 1 != AttachmentEndIt;
		Writer->WriteBool(bHasMoreAttachments);

		if (Writer->IsOverflown())
		{
			// Rollback exports
			ExportScope.Rollback();

			// We need to rollback before we update the continuation bit
			RollbackScope.Rollback();
			if (SerializedUnreliableCount > 0)
			{
				FNetBitStreamWriteScope WriteScope(*Writer, PrevHasMoreAttachmentsWritePos);
				Writer->WriteBool(false);
			}

			break;
		}
		else
		{
			++SerializedUnreliableCount;
			PrevHasMoreAttachmentsWritePos = HasMoreAttachmentsWritePos;
		}
	}

	OutRecord = SerializedUnreliableCount;
	return SerializedUnreliableCount;
}

void FNetObjectAttachmentSendQueue::CommitReplicationRecord(FNetObjectAttachmentSendQueue::ReplicationRecord Record)
{
	FInternalRecord InternalRecord;
	InternalRecord.CombinedRecord = Record;
	if (InternalRecord.UnreliableRecord)
	{
		UnreliableQueue.PopNoCheck(InternalRecord.UnreliableRecord);
	}
	if (InternalRecord.ReliableRecord)
	{
		ReliableQueue->CommitReplicationRecord(InternalRecord.ReliableRecord);
	}
}

void FNetObjectAttachmentSendQueue::OnPacketDelivered(FNetObjectAttachmentSendQueue::ReplicationRecord Record)
{
	if (ReliableQueue == nullptr)
	{
		return;
	}

	FInternalRecord InternalRecord;
	InternalRecord.CombinedRecord = Record;
	ReliableQueue->ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Delivered, InternalRecord.ReliableRecord);
}

void FNetObjectAttachmentSendQueue::OnPacketLost(FNetObjectAttachmentSendQueue::ReplicationRecord Record)
{
	if (ReliableQueue == nullptr)
	{
		return;
	}

	FInternalRecord InternalRecord;
	InternalRecord.CombinedRecord = Record;
	ReliableQueue->ProcessPacketDeliveryStatus(EPacketDeliveryStatus::Lost, InternalRecord.ReliableRecord);
}

bool FNetObjectAttachmentsWriter::Enqueue(ENetObjectAttachmentType Type, uint32 ObjectIndex, TArrayView<const TRefCountPtr<FNetBlob>> Attachments)
{
	FNetObjectAttachmentSendQueue* Queue = GetOrCreateQueue(Type, ObjectIndex);
	if (!ensureMsgf(Queue != nullptr, TEXT("Too many objects being targeted by Attachments simultaneously: %d"), ObjectToQueue.Num()))
	{
		return false;
	}

	return Queue->Enqueue(Attachments);
}

bool FNetObjectAttachmentsWriter::HasUnsentAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	const FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return false;
	}

	return Queue->HasUnsent();
}

bool FNetObjectAttachmentsWriter::IsAllSentAndAcked(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	const FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return false;
	}

	return Queue->IsAllSentAndAcked();
}

bool FNetObjectAttachmentsWriter::IsSafeToDestroy(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	const FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return true;
	}

	return Queue->IsSafeToDestroy();
}

void FNetObjectAttachmentsWriter::DropAllAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	ObjectToQueue.Remove(ObjectIndex);
}

void FNetObjectAttachmentsWriter::DropUnreliableAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex, bool &bOutHasUnsentAttachments)
{
	FNetObjectAttachmentSendQueue* Queue = GetQueue(ENetObjectAttachmentType::Normal, ObjectIndex);
	if (Queue == nullptr)
	{
		bOutHasUnsentAttachments = false;
		return;
	}

	Queue->DropUnreliable(bOutHasUnsentAttachments);
	if (!bOutHasUnsentAttachments && Queue->IsSafeToDestroy())
	{
		ObjectToQueue.Remove(ObjectIndex);
	}
}

EAttachmentWriteStatus FNetObjectAttachmentsWriter::Serialize(FNetSerializationContext& Context, ENetObjectAttachmentType Type, uint32 ObjectIndex, const FNetHandle NetHandle,  FNetObjectAttachmentsWriter::ReplicationRecord& OutRecord, bool& bOutHasUnsentAttachments)
{
	FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	// If this ensure fires we have bad logic for keeping track of whether there are attachments or not
	if (!ensure(Queue != nullptr))
	{
		OutRecord = 0;
		bOutHasUnsentAttachments = false;
		return EAttachmentWriteStatus::NoAttachments;
	}

	return Queue->Serialize(Context, NetHandle, OutRecord, bOutHasUnsentAttachments);
}

void FNetObjectAttachmentsWriter::CommitReplicationRecord(ENetObjectAttachmentType Type, uint32 ObjectIndex, FNetObjectAttachmentsWriter::ReplicationRecord Record)
{
	FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	Queue->CommitReplicationRecord(Record);
}

void FNetObjectAttachmentsWriter::OnPacketDelivered(ENetObjectAttachmentType Type, uint32 ObjectIndex, FNetObjectAttachmentsWriter::ReplicationRecord Record)
{
	FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return;
	}

	Queue->OnPacketDelivered(Record);
}

void FNetObjectAttachmentsWriter::OnPacketLost(ENetObjectAttachmentType Type, uint32 ObjectIndex, ReplicationRecord Record)
{
	FNetObjectAttachmentSendQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return;
	}

	Queue->OnPacketLost(Record);
}

FNetObjectAttachmentSendQueue* FNetObjectAttachmentsWriter::GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	return Type == ENetObjectAttachmentType::Normal ? ObjectToQueue.Find(ObjectIndex) : SpecialQueues[uint32(Type)].Get();
}

const FNetObjectAttachmentSendQueue* FNetObjectAttachmentsWriter::GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	return Type == ENetObjectAttachmentType::Normal ? ObjectToQueue.Find(ObjectIndex) : SpecialQueues[uint32(Type)].Get();
}

FNetObjectAttachmentSendQueue* FNetObjectAttachmentsWriter::GetOrCreateQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	if (Type != ENetObjectAttachmentType::Normal)
	{
		checkSlow(ObjectIndex == 0U);
		TUniquePtr<FNetObjectAttachmentSendQueue>& QueuePtr = SpecialQueues[uint32(Type)];
		if (FNetObjectAttachmentSendQueue* Queue = QueuePtr.Get())
		{
			return Queue;
		}

		QueuePtr = MakeUnique<FNetObjectAttachmentSendQueue>();

		if (Type == ENetObjectAttachmentType::OutOfBand)
		{
			QueuePtr->SetUnreliableQueueCapacity(AttachmentReplicationCVars::ClientToServerUnreliableRPCQueueSize);
		}

		return QueuePtr.Get();
	}
	else
	{
		FNetObjectAttachmentSendQueue* Queue = ObjectToQueue.Find(ObjectIndex);
		if (Queue != nullptr)
		{
			return Queue;
		}

		if (ObjectToQueue.Num() >= AttachmentReplicationCVars::MaxSimultaneousObjectsWithRPCs)
		{
			return nullptr;
		}

		Queue = &ObjectToQueue.Add(ObjectIndex);

		return Queue;
	}
}

// ReceiveQueue and Reader
class FNetObjectAttachmentReceiveQueue::FDeferredProcessingQueue
{
public:
	bool IsEmpty() const { return Queue.IsEmpty(); }
	bool HasUnprocessed() const { return !Queue.IsEmpty(); }
	bool IsSafeToDestroy() const { return Queue.IsEmpty() && !NetBlobAssembler.IsValid(); }

	void Enqueue(FNetSerializationContext& Context, FNetHandle NetHandle, const TRefCountPtr<FNetBlob>& NetBlob, bool bIsPartialNetBlob)
	{
		if (bIsPartialNetBlob)
		{
			if (!NetBlobAssembler.IsValid())
			{
				NetBlobAssembler = MakeUnique<FNetBlobAssembler>();
			}

			NetBlobAssembler->AddPartialNetBlob(Context, NetHandle, reinterpret_cast<const TRefCountPtr<FPartialNetBlob>&>(NetBlob));
			if (NetBlobAssembler->IsReadyToAssemble())
			{
				const TRefCountPtr<FNetBlob>& AssembledBlob = NetBlobAssembler->Assemble(Context);
				if (AssembledBlob.IsValid())
				{
					Queue.Enqueue(AssembledBlob);
				}
				NetBlobAssembler.Reset();
			}
		}
		else
		{
			Queue.Enqueue(NetBlob);
		}
	}

	const TRefCountPtr<FNetBlob>* Peek() const
	{
		return &Queue.Peek();
	}

	void Pop()
	{
		return Queue.Pop();
	}

private:
	TUniquePtr<FNetBlobAssembler> NetBlobAssembler;
	TResizableCircularQueue<TRefCountPtr<FNetBlob>> Queue;
};

FNetObjectAttachmentReceiveQueue::FNetObjectAttachmentReceiveQueue()
: ReliableQueue(nullptr)
, DeferredProcessingQueue(nullptr)
, MaxUnreliableCount(AttachmentReplicationCVars::UnreliableRPCQueueSize)
, PartialNetBlobType(InvalidNetBlobType)
{
}

FNetObjectAttachmentReceiveQueue::~FNetObjectAttachmentReceiveQueue()
{
	delete ReliableQueue;
	delete DeferredProcessingQueue;
}

bool FNetObjectAttachmentReceiveQueue::IsSafeToDestroy() const
{
	return UnreliableQueue.IsEmpty() && IsDeferredProcessingQueueSafeToDestroy() && (ReliableQueue == nullptr || ReliableQueue->IsSafeToDestroy());
}

bool FNetObjectAttachmentReceiveQueue::HasUnprocessed() const
{
	return !UnreliableQueue.IsEmpty() || HasDeferredProcessingQueueUnprocessed();
}

const TRefCountPtr<FNetBlob>* FNetObjectAttachmentReceiveQueue::PeekReliable() const
{
	if (IsDeferredProcessingQueueEmpty())
	{
		return nullptr;
	}
	else
	{
		return DeferredProcessingQueue->Peek();
	}
}

void FNetObjectAttachmentReceiveQueue::PopReliable()
{
	DeferredProcessingQueue->Pop();
}

const TRefCountPtr<FNetBlob>* FNetObjectAttachmentReceiveQueue::PeekUnreliable() const
{
	if (UnreliableQueue.IsEmpty())
	{
		return nullptr;
	}
	else
	{
		return &UnreliableQueue.Peek();
	}
}

void FNetObjectAttachmentReceiveQueue::PopUnreliable()
{
	UnreliableQueue.Pop();
}

void FNetObjectAttachmentReceiveQueue::SetUnreliableQueueCapacity(uint32 QueueCapacity)
{
	MaxUnreliableCount = QueueCapacity;
	
	const uint32 UnreliableCount = UnreliableQueue.Count();
	if (QueueCapacity >= UnreliableCount)
	{
		return;
	}

	const uint32 DropCount = UnreliableCount - QueueCapacity;
	UE_LOG(LogIris, Warning, TEXT("Dropping %u attachments to due to change in unreliable queue capacity to %u"), DropCount, QueueCapacity);
	UnreliableQueue.Pop(DropCount);
}

bool FNetObjectAttachmentReceiveQueue::IsDeferredProcessingQueueEmpty() const
{
	return DeferredProcessingQueue == nullptr || DeferredProcessingQueue->IsEmpty();
}

bool FNetObjectAttachmentReceiveQueue::IsDeferredProcessingQueueSafeToDestroy() const
{
	return DeferredProcessingQueue == nullptr || DeferredProcessingQueue->IsSafeToDestroy();
}

bool FNetObjectAttachmentReceiveQueue::HasDeferredProcessingQueueUnprocessed() const
{
	return DeferredProcessingQueue != nullptr && DeferredProcessingQueue->HasUnprocessed();
}

bool FNetObjectAttachmentReceiveQueue::IsPartialNetBlob(const TRefCountPtr<FNetBlob>& Blob) const
{
	return Blob.IsValid() && Blob->GetCreationInfo().Type == PartialNetBlobType;
}

void FNetObjectAttachmentReceiveQueue::Deserialize(FNetSerializationContext& Context, FNetHandle NetHandle)
{
	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	UE_NET_TRACE_SCOPE(RPCs, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);

	const bool bHasReliableAttachments = Reader.ReadBool();
	const bool bHasUnreliableAttachments = Reader.ReadBool();

	if (Reader.IsOverflown())
	{
		return;
	}

	if (bHasReliableAttachments)
	{
		UE_NET_TRACE_SCOPE(Reliable, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		DeserializeReliable(Context, NetHandle);
		if (Context.HasErrorOrOverflow())
		{
			return;
		}
	}

	if (bHasUnreliableAttachments)
	{
		UE_NET_TRACE_SCOPE(Unreliable, Reader, Context.GetTraceCollector(), ENetTraceVerbosity::Trace);
		DeserializeUnreliable(Context, NetHandle);
		if (Context.HasErrorOrOverflow())
		{
			return;
		}
	}
}

uint32 FNetObjectAttachmentReceiveQueue::DeserializeReliable(FNetSerializationContext& Context, FNetHandle NetHandle)
{
	if (ReliableQueue == nullptr)
	{
		ReliableQueue = new FReliableNetBlobQueue();
		checkSlow(DeferredProcessingQueue == nullptr);
		DeferredProcessingQueue = new FDeferredProcessingQueue();
	}

	uint32 DeserializedReliableCount = 0;
	if (NetHandle.IsValid())
	{
		DeserializedReliableCount = ReliableQueue->DeserializeWithObject(Context, NetHandle);
	}
	else
	{
		DeserializedReliableCount = ReliableQueue->Deserialize(Context);
	}

	if (Context.HasErrorOrOverflow())
	{
		return DeserializedReliableCount;
	}

	while (const TRefCountPtr<FNetBlob>* Attachment = ReliableQueue->Peek())
	{
		DeferredProcessingQueue->Enqueue(Context, NetHandle, *Attachment, IsPartialNetBlob(*Attachment));
		ReliableQueue->Pop();

		if (Context.HasErrorOrOverflow())
		{
			return DeserializedReliableCount;
		}
	}

	return DeserializedReliableCount;
}

uint32 FNetObjectAttachmentReceiveQueue::DeserializeUnreliable(FNetSerializationContext& Context, FNetHandle NetHandle)
{
	INetBlobReceiver* BlobReceiver = Context.GetNetBlobReceiver();
	checkSlow(BlobReceiver != nullptr);

	FNetBitStreamReader& Reader = *Context.GetBitStreamReader();

	uint32 DeserializedUnreliableCount = 0;
	const bool bDeserializeWithObject = NetHandle.IsValid();
	bool bHasMoreAttachments = false;
	do
	{
		// If the unreliable queue overflows this could be a carefully crafted malicious packet.
		if (UnreliableQueue.Count() == MaxUnreliableCount)
		{
			UE_LOG(LogIris, Error, TEXT("Unreliable queue is full for %s"), *NetHandle.ToString());
			Context.SetError(NetError_UnreliableQueueFull);
			break;
		}

		Context.GetInternalContext()->ObjectReferenceCache->ReadExports(Context);

		FNetBlobCreationInfo CreationInfo;
		FNetBlob::DeserializeCreationInfo(Context, CreationInfo);
		const TRefCountPtr<FNetBlob>& Attachment = BlobReceiver->CreateNetBlob(CreationInfo);
		if (!Attachment.IsValid())
		{
			Context.SetError(GNetError_UnsupportedNetBlob);
			break;
		}

		if (bDeserializeWithObject)
		{
			Attachment->DeserializeWithObject(Context, NetHandle);
		}
		else
		{
			Attachment->Deserialize(Context);
		}
		
		bHasMoreAttachments = Reader.ReadBool();

		// Break if something went wrong with deserialization.
		if (Context.HasErrorOrOverflow())
		{
			UE_LOG(LogIris, Error, TEXT("Failed to deserialize unreliable attachments for %s"), *NetHandle.ToString());
			break;
		}

		UnreliableQueue.Enqueue(Attachment);
		++DeserializedUnreliableCount;
	} while (bHasMoreAttachments);

	return DeserializedUnreliableCount;
}

FNetObjectAttachmentsReader::FNetObjectAttachmentsReader()
: PartialNetBlobType(InvalidNetBlobType)
{
}

FNetObjectAttachmentsReader::~FNetObjectAttachmentsReader()
{
}

bool FNetObjectAttachmentsReader::HasUnprocessedAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	const FNetObjectAttachmentReceiveQueue* Queue = GetQueue(Type, ObjectIndex);
	if (Queue == nullptr)
	{
		return false;
	}

	return Queue->HasUnprocessed();
}

void FNetObjectAttachmentsReader::DropAllAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	ObjectToQueue.Remove(ObjectIndex);
}

void FNetObjectAttachmentsReader::Deserialize(FNetSerializationContext& Context, ENetObjectAttachmentType Type, uint32 ObjectIndex, FNetHandle NetHandle)
{
	FNetObjectAttachmentReceiveQueue* Queue = GetOrCreateQueue(Type, ObjectIndex);
	if (!ensure(Queue != nullptr))
	{
		Context.SetError(NetError_TooManyObjectsWithFunctionCalls);
		return;
	}

	Queue->Deserialize(Context, NetHandle);
}

FNetObjectAttachmentReceiveQueue* FNetObjectAttachmentsReader::GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	return Type == ENetObjectAttachmentType::Normal ? ObjectToQueue.Find(ObjectIndex) : SpecialQueues[uint32(Type)].Get();
}

const FNetObjectAttachmentReceiveQueue* FNetObjectAttachmentsReader::GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex) const
{
	return Type == ENetObjectAttachmentType::Normal ? ObjectToQueue.Find(ObjectIndex) : SpecialQueues[uint32(Type)].Get();
}

FNetObjectAttachmentReceiveQueue* FNetObjectAttachmentsReader::GetOrCreateQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex)
{
	if (Type != ENetObjectAttachmentType::Normal)
	{
		checkSlow(ObjectIndex == 0U);
		TUniquePtr<FNetObjectAttachmentReceiveQueue>& QueuePtr = SpecialQueues[uint32(Type)];
		FNetObjectAttachmentReceiveQueue* Queue = QueuePtr.Get();
		if (Queue != nullptr)
		{
			return Queue;
		}

		QueuePtr = MakeUnique<FNetObjectAttachmentReceiveQueue>();
		Queue = QueuePtr.Get();
		Queue->SetPartialNetBlobType(PartialNetBlobType);

		if (Type == ENetObjectAttachmentType::OutOfBand)
		{
			Queue->SetUnreliableQueueCapacity(AttachmentReplicationCVars::ClientToServerUnreliableRPCQueueSize);
		}

		return Queue;
	}
	else
	{
		FNetObjectAttachmentReceiveQueue* Queue = ObjectToQueue.Find(ObjectIndex);
		if (Queue != nullptr)
		{
			return Queue;
		}

		if (ObjectToQueue.Num() >= AttachmentReplicationCVars::MaxSimultaneousObjectsWithRPCs)
		{
			return nullptr;
		}

		Queue = &ObjectToQueue.Add(ObjectIndex);
		Queue->SetPartialNetBlobType(PartialNetBlobType);

		return Queue;
	}


	FNetObjectAttachmentReceiveQueue* Queue = ObjectToQueue.Find(ObjectIndex);
	if (Queue != nullptr)
	{
		return Queue;
	}

	if (ObjectToQueue.Num() >= AttachmentReplicationCVars::MaxSimultaneousObjectsWithRPCs)
	{
		return nullptr;
	}

	Queue = &ObjectToQueue.Add(ObjectIndex);
	if (ObjectIndex == 0)
	{
		Queue->SetUnreliableQueueCapacity(AttachmentReplicationCVars::ClientToServerUnreliableRPCQueueSize);
	}

	return Queue;
}

}
