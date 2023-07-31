// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/Misc/ResizableCircularQueue.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlob.h"
#include "Iris/ReplicationSystem/NetBlob/ReliableNetBlobQueue.h"
#include "Containers/Map.h"

namespace UE::Net::Private
{
	class FNetBlobHandlerManager;
	class FNetObjectAttachmentsReader;
	class FNetObjectAttachmentsWriter;
}

namespace UE::Net::Private
{

enum class ENetObjectAttachmentType : uint32
{
	// For normal attachments a valid ObjectIndex is required.
	Normal,

	// For special attachments like OutOfBand and HugeObject the ObjectIndex is assumed to be zero.
	OutOfBand,
	HugeObject,

	// For internal use only
	InternalCount
};

enum class EAttachmentWriteStatus : unsigned
{
	/** At least one attachment could be sent. */
	Success,
	/** There were no attachments to write. */
	NoAttachments,
	/** Due to the reliable window size being full no reliable attachments could be sent. */
	ReliableWindowFull,
	/** Writing caused bitstream overflow. */
	BitstreamOverflow,
};

class FNetObjectAttachmentSendQueue
{
public:
	typedef uint64 ReplicationRecord;
	static constexpr uint64 InvalidReplicationRecord = uint64(0);

public:
	FNetObjectAttachmentSendQueue();
	~FNetObjectAttachmentSendQueue();

	bool Enqueue(TArrayView<const TRefCountPtr<FNetBlob>> Attachments);

	void DropUnreliable(bool &bOutHasUnsentAttachments);

	bool HasUnsent() const;

	bool IsSafeToDestroy() const;

	bool IsAllSentAndAcked() const;

	void SetUnreliableQueueCapacity(uint32 QueueCapacity);

private:
	friend FNetObjectAttachmentsWriter;
	class FReliableSendQueue;

	struct FInternalRecord
	{
		union
		{
			struct
			{
				uint32 UnreliableRecord;
				FReliableNetBlobQueue::ReplicationRecord ReliableRecord;
			};
			ReplicationRecord CombinedRecord;
		};

		FInternalRecord() { CombinedRecord = InvalidReplicationRecord; };
	};

	EAttachmentWriteStatus Serialize(FNetSerializationContext& Context, FNetHandle NetHandle, ReplicationRecord& OutRecord, bool& bOutHasUnprocessedAttachments);
	uint32 SerializeReliable(FNetSerializationContext& Context, FNetHandle NetHandle, FReliableNetBlobQueue::ReplicationRecord& OutRecord);
	uint32 SerializeUnreliable(FNetSerializationContext& Context, FNetHandle NetHandle, uint32& OutRecord);

	void CommitReplicationRecord(ReplicationRecord Record);

	void OnPacketDelivered(ReplicationRecord Record);
	void OnPacketLost(ReplicationRecord Record);

	FReliableSendQueue* ReliableQueue;
	TResizableCircularQueue<TRefCountPtr<FNetBlob>> UnreliableQueue;
	uint32 MaxUnreliableCount;
};

class FNetObjectAttachmentsWriter
{
public:
	typedef FNetObjectAttachmentSendQueue::ReplicationRecord ReplicationRecord;

public:
	bool Enqueue(ENetObjectAttachmentType Type, uint32 ObjectIndex, TArrayView<const TRefCountPtr<FNetBlob>> Attachments);

	bool HasUnsentAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;
	// Whether all queued attachments have been sent and that all reliable ones have been acked.
	bool IsAllSentAndAcked(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;
	// Whether the queue can be destroyed without causing issues if more attachments are queued to this instance.
	bool IsSafeToDestroy(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;

	void DropAllAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex);
	void DropUnreliableAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex, bool& bOutHasUnsentAttachments);

	EAttachmentWriteStatus Serialize(FNetSerializationContext& Context, ENetObjectAttachmentType Type, uint32 ObjectIndex, FNetHandle NetHandle, ReplicationRecord& OutRecord, bool& bOutHasUnsentAttachments);

	void CommitReplicationRecord(ENetObjectAttachmentType Type, uint32 ObjectIndex, ReplicationRecord Record);

	void OnPacketDelivered(ENetObjectAttachmentType Type, uint32 ObjectIndex, ReplicationRecord Record);
	void OnPacketLost(ENetObjectAttachmentType Type, uint32 ObjectIndex, ReplicationRecord Record);

private:
	bool NetBlobMightNeedSplitting(const TRefCountPtr<FNetObjectAttachment>& Attachment) const;

	FNetObjectAttachmentSendQueue* GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex);
	const FNetObjectAttachmentSendQueue* GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;

	FNetObjectAttachmentSendQueue* GetOrCreateQueue(ENetObjectAttachmentType Reason, uint32 ObjectIndex);

	TMap<uint32, FNetObjectAttachmentSendQueue> ObjectToQueue;
	TUniquePtr<FNetObjectAttachmentSendQueue> SpecialQueues[uint32(ENetObjectAttachmentType::InternalCount)];
};

class FNetObjectAttachmentReceiveQueue
{
public:
	FNetObjectAttachmentReceiveQueue();
	~FNetObjectAttachmentReceiveQueue();

	void SetPartialNetBlobType(FNetBlobType InPartialNetBlobType) { PartialNetBlobType = InPartialNetBlobType; }

	bool IsSafeToDestroy() const;
	bool HasUnprocessed() const;

	const TRefCountPtr<FNetBlob>* PeekReliable() const;
	void PopReliable();

	const TRefCountPtr<FNetBlob>* PeekUnreliable() const;
	void PopUnreliable();

	void SetUnreliableQueueCapacity(uint32 QueueCapacity);

private:
	friend FNetObjectAttachmentsReader;
	class FDeferredProcessingQueue;

	bool IsDeferredProcessingQueueEmpty() const;
	bool IsDeferredProcessingQueueSafeToDestroy() const;
	bool HasDeferredProcessingQueueUnprocessed() const;
	bool IsPartialNetBlob(const TRefCountPtr<FNetBlob>& Blob) const;

	void Deserialize(FNetSerializationContext& Context, FNetHandle NetHandle);
	uint32 DeserializeReliable(FNetSerializationContext& Context, FNetHandle NetHandle);
	uint32 DeserializeUnreliable(FNetSerializationContext& Context, FNetHandle NetHandle);

	TResizableCircularQueue<TRefCountPtr<FNetBlob>> UnreliableQueue;
	FReliableNetBlobQueue* ReliableQueue;
	FDeferredProcessingQueue* DeferredProcessingQueue;
	uint32 MaxUnreliableCount;
	FNetBlobType PartialNetBlobType;
};

class FNetObjectAttachmentsReader
{
public:
	FNetObjectAttachmentsReader();
	~FNetObjectAttachmentsReader();

	void SetPartialNetBlobType(FNetBlobType InPartialNetBlobType) { PartialNetBlobType = InPartialNetBlobType; }

	bool HasUnprocessedAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;

	void DropAllAttachments(ENetObjectAttachmentType Type, uint32 ObjectIndex);

	void Deserialize(FNetSerializationContext& Context, ENetObjectAttachmentType Type, uint32 ObjectIndex, FNetHandle NetHandle);

	FNetObjectAttachmentReceiveQueue* GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex);

private:
	const FNetObjectAttachmentReceiveQueue* GetQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex) const;

	FNetObjectAttachmentReceiveQueue* GetOrCreateQueue(ENetObjectAttachmentType Type, uint32 ObjectIndex);

	TMap<uint32, FNetObjectAttachmentReceiveQueue> ObjectToQueue;
	TUniquePtr<FNetObjectAttachmentReceiveQueue> SpecialQueues[uint32(ENetObjectAttachmentType::InternalCount)];
	FNetBlobType PartialNetBlobType;
};

}
