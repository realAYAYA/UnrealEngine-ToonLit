// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/ReliableNetBlobQueue.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlobHandlerManager.h"
#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetExportContext.h"

namespace UE::Net::Private
{

static const FName NetError_ReliableQueueFull("Reliable attachment queue full");
static const FName NetError_InvalidSequence("Invalid sequence number");

// ReliableNetBlobQueue
FReliableNetBlobQueue::FReliableNetBlobQueue()
: Sent{}
, Acked{}
, FirstSeq(0)
, LastSeq(0)
, UnsentBlobCount(0)
{
}

FReliableNetBlobQueue::~FReliableNetBlobQueue()
{
}

// If everything is sent and acked and the first sequence index is zero then it's safe to destroy this instance.
bool FReliableNetBlobQueue::IsSafeToDestroy() const
{
	if (FirstSeq != LastSeq)
	{
		return false;
	}

	if (SequenceToIndex(FirstSeq) != 0)
	{
		return false;
	}

	return true;
}

uint32 FReliableNetBlobQueue::Serialize(FNetSerializationContext& Context, FReliableNetBlobQueue::ReplicationRecord& OutRecord)
{
	FNetHandle InvalidNetHandle;
	return SerializeInternal(Context, InvalidNetHandle, OutRecord, false);
}

uint32 FReliableNetBlobQueue::SerializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle, FReliableNetBlobQueue::ReplicationRecord& OutRecord)
{
	return SerializeInternal(Context, NetHandle, OutRecord, true);
}

uint32 FReliableNetBlobQueue::SerializeInternal(FNetSerializationContext& Context, FNetHandle NetHandle, FReliableNetBlobQueue::ReplicationRecord& OutRecord, const bool bSerializeWithObject)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	const FObjectReferenceCache* ObjectReferenceCache = Context.GetInternalContext()->ObjectReferenceCache;

	uint32 PrevHasMoreBlobsWritePos = 0;
	uint32 SerializedCount = 0;
	uint32 PrevWrittenSeq = ~0U;
	uint32 WrittenSeq[2] = {~0U, ~0U};
	uint32 WrittenCount[2] = {0U, 0U};
	uint32 WrittenIndex = 0;
	for (uint32 Seq = FirstSeq, EndSeq = LastSeq; Seq < EndSeq; ++Seq)
	{
		const uint32 Index = SequenceToIndex(Seq);
		if (IsIndexSent(Index))
		{
			continue;
		}

		// Disjoint sequence handling
		// First in sequence?
		if (WrittenSeq[WrittenIndex] == ~0U)
		{
			WrittenSeq[WrittenIndex] = Seq;
		}
		// If broken sequence start the next one.
		else if (PrevWrittenSeq + 1U != Seq)
		{
			++WrittenIndex;
			// Our ReplicationRecord can't hold a lot of info.
			if (WrittenIndex >= UE_ARRAY_COUNT(WrittenCount))
			{
				break;
			}
			WrittenSeq[WrittenIndex] = Seq;
		}

		FNetBitStreamRollbackScope RollbackScope(*Writer);
		FNetExportRollbackScope ExportRollbackScope(Context);

		const TRefCountPtr<FNetBlob>& Attachment = NetBlobs[Index];

		// If this sequence is disjoint from the previous sequence we need to serialize the full index.
		// It's important that the sequence number is sent first so we can validate it before receiving exports and payload.
		if (Writer->WriteBool(Seq != PrevWrittenSeq + 1U))
		{
			Writer->WriteBits(Index, IndexBitCount);
		}

		// If we have exports, write them now, if attachment is rolled back we will roll back any written exports as well.
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

		const uint32 HasMoreBlobsWritePos = Writer->GetPosBits();
		Writer->WriteBool(false); // Don't know yet if there's more to come

		if (Writer->IsOverflown())
		{
			ExportRollbackScope.Rollback();
			break;
		}
		// We did manage to serialize yet another blob
		else
		{
			if (SerializedCount > 0)
			{
				FNetBitStreamWriteScope WriteScope(*Writer, PrevHasMoreBlobsWritePos);
				Writer->WriteBool(true);
			}

			++SerializedCount;

			// Our ReplicationRecord currently only holds two sequences with number and count, one byte each.
			++WrittenCount[WrittenIndex];
			if (WrittenCount[WrittenIndex] == 255)
			{
				++WrittenIndex;
				if (WrittenIndex >= UE_ARRAY_COUNT(WrittenCount))
				{
					break;
				}
			}

			PrevWrittenSeq = Seq;
			PrevHasMoreBlobsWritePos = HasMoreBlobsWritePos;
		}
	}

	//
	const ReplicationRecord Record = ((WrittenSeq[0] & 255) << 24U) | ((WrittenCount[0] & 255) << 16U) | ((WrittenSeq[1] & 255) << 8U) | ((WrittenCount[1] & 255) << 0U);
	OutRecord = (SerializedCount ? Record : FReliableNetBlobQueue::InvalidReplicationRecord);

	return SerializedCount;
}

void FReliableNetBlobQueue::CommitReplicationRecord(FReliableNetBlobQueue::ReplicationRecord Record)
{
	uint32 RecordSeq[2] = {(Record >> 24U) & 255, (Record >> 8U) & 255U};
	uint32 RecordCount[2] = {(Record >> 16U) & 255, (Record >> 0U) & 255U};

	UnsentBlobCount -= RecordCount[0] + RecordCount[1];
	for (const uint32 Index : {0U, 1U})
	{
		for (uint32 Seq = RecordSeq[Index], EndSeq = Seq + RecordCount[Index]; Seq != EndSeq; ++Seq)
		{
			SetSequenceIsSent(Seq);
		}
	}
}

uint32 FReliableNetBlobQueue::Deserialize(FNetSerializationContext& Context)
{
	FNetHandle InvalidNetHandle;
	return DeserializeInternal(Context, InvalidNetHandle, false);
}

uint32 FReliableNetBlobQueue::DeserializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle)
{
	return DeserializeInternal(Context, NetHandle, true);
}

uint32 FReliableNetBlobQueue::DeserializeInternal(FNetSerializationContext& Context, FNetHandle NetHandle, const bool bSerializeWithObject)
{
	INetBlobReceiver* BlobReceiver = Context.GetNetBlobReceiver();
	checkSlow(BlobReceiver != nullptr);

	FNetBitStreamReader* Reader = Context.GetBitStreamReader();
	FObjectReferenceCache* ObjectReferenceCache = Context.GetInternalContext()->ObjectReferenceCache;

	uint32 Index = ~0U;
	uint32 DeserializedCount = 0;
	bool bHasMoreBlobs = false;
	do
	{
		if (const bool bIsNewSequence = Reader->ReadBool())
		{
			Index = Reader->ReadBits(IndexBitCount);
		}
		else
		{
			Index = (Index + 1) % MaxUnackedBlobCount;
		}

		uint32 ReceivedSeq = FirstSeq - (FirstSeq % MaxUnackedBlobCount) + Index;
		if (ReceivedSeq < FirstSeq)
		{
			ReceivedSeq += MaxUnackedBlobCount;
		}

		if (!IsValidReceiveSequence(ReceivedSeq) || IsIndexAcked(Index))
		{
			UE_LOG(LogIris, Warning, TEXT("Invalid reliable sequence number. ReceivedSeq: %u FirstSeq: %u LastSeq: %u IsAcked: %u"), ReceivedSeq, FirstSeq, LastSeq, unsigned(IsIndexAcked(Index)));
			Context.SetError(NetError_InvalidSequence);

			return DeserializedCount;
		}

		// If we have exports, import them now.
		ObjectReferenceCache->ReadExports(Context);

		FNetBlobCreationInfo CreationInfo;
		FNetBlob::DeserializeCreationInfo(Context, CreationInfo);
		CreationInfo.Flags = CreationInfo.Flags | ENetBlobFlags::Reliable;
		const TRefCountPtr<FNetBlob>& Blob = BlobReceiver->CreateNetBlob(CreationInfo);
		if (!Blob.IsValid())
		{
			UE_LOG(LogIris, Warning, TEXT("%s"), TEXT("Unable to create blob."));
			Context.SetError(GNetError_UnsupportedNetBlob);

			return DeserializedCount;
		}

		if (bSerializeWithObject)
		{
			Blob->DeserializeWithObject(Context, NetHandle);
		}
		else
		{
			Blob->Deserialize(Context);
		}
		
		bHasMoreBlobs = Reader->ReadBool();

		// Break if something went wrong with deserialization.
		if (Context.HasErrorOrOverflow())
		{
			UE_LOG(LogIris, Warning, TEXT("Failed to deserialize reliable attachments for %s"), *NetHandle.ToString());
			break;
		}

		NetBlobs[Index] = Blob;
		LastSeq = FMath::Max(LastSeq, ReceivedSeq + 1U);
		SetIndexIsAcked(Index);

		++DeserializedCount;
	} while (bHasMoreBlobs);

	return DeserializedCount;
}

bool FReliableNetBlobQueue::Enqueue(const TRefCountPtr<FNetBlob>& Blob)
{
	if (IsFull())
	{
		return false;
	}

	++UnsentBlobCount;

	const uint32 Index = SequenceToIndex(LastSeq);
	++LastSeq;

	NetBlobs[Index] = Blob;
	return true;
}

const TRefCountPtr<FNetBlob>* FReliableNetBlobQueue::Peek() const
{
	const uint32 Index = SequenceToIndex(FirstSeq);
	if (IsIndexAcked(Index))
	{
		return &NetBlobs[Index];
	}

	return nullptr;
}

void FReliableNetBlobQueue::Pop()
{
	const uint32 Index = SequenceToIndex(FirstSeq);
	checkSlow(IsIndexAcked(Index) && (FirstSeq < LastSeq));
	NetBlobs[Index].SafeRelease();
	ClearIndexIsAcked(Index);
	++FirstSeq;

	// $TODO. For a memory optimization one can change the storage implementation and free the memory here if everything is acked.
}

void FReliableNetBlobQueue::ProcessPacketDeliveryStatus(EPacketDeliveryStatus Status, FReliableNetBlobQueue::ReplicationRecord Record)
{
	switch (Status)
	{
	case EPacketDeliveryStatus::Delivered:
	{
		OnPacketDelivered(Record);
		break;
	}
	case EPacketDeliveryStatus::Lost:
	{
		OnPacketDropped(Record);
		break;
	}
	default:
	{
		break;
	}
	}
}

void FReliableNetBlobQueue::OnPacketDelivered(FReliableNetBlobQueue::ReplicationRecord Record)
{
	uint32 RecordSeq[2] = {(Record >> 24U) & 255, (Record >> 8U) & 255U};
	uint32 RecordCount[2] = {(Record >> 16U) & 255, (Record >> 0U) & 255U};

	// Mark blobs as acked
	for (const uint32 Index : {0U, 1U})
	{
		for (uint32 Seq = RecordSeq[Index], EndSeq = Seq + RecordCount[Index]; Seq != EndSeq; ++Seq)
		{
			SetSequenceIsAcked(Seq);
		}
	}

	// Remove acked blobs, allowing for more blobs to be added to the queue.
	PopInOrderAckedBlobs();
}

void FReliableNetBlobQueue::OnPacketDropped(FReliableNetBlobQueue::ReplicationRecord Record)
{
	uint32 RecordSeq[2] = {(Record >> 24U) & 255, (Record >> 8U) & 255U};
	uint32 RecordCount[2] = {(Record >> 16U) & 255, (Record >> 0U) & 255U};

	// Mark blobs as unsent
	UnsentBlobCount += RecordCount[0] + RecordCount[1];
	for (const uint32 Index : {0U, 1U})
	{
		for (uint32 Seq = RecordSeq[Index], EndSeq = Seq + RecordCount[Index]; Seq != EndSeq; ++Seq)
		{
			ClearSequenceIsSent(Seq);
		}
	}
}

void FReliableNetBlobQueue::PopInOrderAckedBlobs()
{
	for (; FirstSeq != LastSeq; ++FirstSeq)
	{
		const uint32 Index = SequenceToIndex(FirstSeq);
		if (!IsIndexAcked(Index))
		{
			return;
		}

		NetBlobs[Index].SafeRelease();
		ClearIndexIsAcked(Index);
		ClearIndexIsSent(Index);
	}

	// $TODO. For a memory optimization one can change the storage implementation and free the memory here if everything is acked.
}

}
