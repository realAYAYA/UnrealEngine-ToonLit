// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlob.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlobHandlerManager.h"
#include "Iris/ReplicationSystem/NetRefHandleManager.h" // For FInternalNetRefIndex
#include "UObject/StrongObjectPtr.h"

class UNetBlobHandler;
class UNetObjectBlobHandler;
class UPartialNetObjectAttachmentHandler;
class UPartialNetObjectAttachmentHandlerConfig;
class UNetRPCHandler;
namespace UE::Net
{
	class FNetObjectReference;
	namespace Private
	{
		class FNetRefHandleManager;
		class FObjectReferenceCache;
		class FReplicationConnections;
	}
}

namespace UE::Net::Private
{

struct FNetBlobManagerInitParams
{
	UReplicationSystem* ReplicationSystem = nullptr;
	bool bSendAttachmentsWithObject = false;
};


class FNetBlobManager
{
public:
	FNetBlobManager();
	
	void Init(FNetBlobManagerInitParams& InitParams);

	bool RegisterNetBlobHandler(UNetBlobHandler* Handler);

	bool QueueNetObjectAttachment(uint32 ConnectionId, const FNetObjectReference& TargetRef, const TRefCountPtr<FNetObjectAttachment>& Attachment);
	bool SendRPC(const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters);
	bool SendRPC(uint32 ConnectionId, const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters);

	enum class EProcessMode 
	{
		ProcessObjectsGoingOutOfScope,
		ProcessObjectsInScope,
	};

	void ProcessNetObjectAttachmentSendQueue(EProcessMode ProcessMode);
	void ResetNetObjectAttachmentSendQueue();

	FNetBlobHandlerManager& GetNetBlobHandlerManager() { return BlobHandlerManager; }
	const FNetBlobHandlerManager& GetNetBlobHandlerManager() const { return BlobHandlerManager; }

	const UPartialNetObjectAttachmentHandler* GetPartialNetObjectAttachmentHandler() const { return PartialNetObjectAttachmentHandler.Get(); }
	const UNetObjectBlobHandler* GetNetObjectBlobHandler() const { return NetObjectBlobHandler.Get(); }

	// Connection handling
	void AddConnection(uint32 ConnectionId);
	void RemoveConnection(uint32 ConnectionId);

private:
	void RegisterDefaultHandlers();

	bool GetOwnerAndSubObjectIndicesFromHandle(FNetRefHandle RefHandle, FInternalNetRefIndex& OutOwnerIndex, FInternalNetRefIndex& OutSubObjectIndex);

	class FNetObjectAttachmentSendQueue
	{
	public:
		FNetObjectAttachmentSendQueue();

		void Init(FNetBlobManager* Manager);

		// Unicast
		void Enqueue(uint32 ConnectionId, FInternalNetRefIndex OwnerIndex, FInternalNetRefIndex SubObjectIndex, const TRefCountPtr<FNetObjectAttachment>& Attachment);

		// Multicast
		void Enqueue(FInternalNetRefIndex OwnerIndex, FInternalNetRefIndex SubObjectIndex, const TRefCountPtr<FNetObjectAttachment>& Attachment);

		void PrepareProcessQueue(FReplicationConnections* InConnections, const FNetRefHandleManager* InNetRefHandleManager);
		void ProcessQueue(EProcessMode ProcessMode);
		void ResetProcessQueue();
	
	private:
		struct FNetObjectAttachmentQueueEntry
		{
			uint32 ConnectionId;
			FInternalNetRefIndex OwnerIndex;
			FInternalNetRefIndex SubObjectIndex;
			TRefCountPtr<FNetObjectAttachment> Attachment;
		};
		typedef TArray<FNetObjectAttachmentQueueEntry> FQueue;

		bool PreSerializeAndSplitNetBlob(uint32 ConnectionId, const TRefCountPtr<FNetObjectAttachment>& Attachment, TArray<TRefCountPtr<FNetBlob>>& OutPartialNetBlobs) const;

		FNetBlobManager* Manager;
		FQueue AttachmentQueue;
		bool bHasMulticastAttachments;

		struct FProcessQueueContext
		{
			FNetBitArray AttachmentsToObjectsGoingOutOfScope;
			FNetBitArray AttachmentsToObjectsInScope;

			TArray<uint32> ConnectionIds;
			FReplicationConnections* Connections = nullptr;
			const FNetRefHandleManager* NetRefHandleManager = nullptr;

			void Reset()
			{
				Connections = nullptr;
				NetRefHandleManager = nullptr;
			}

			bool IsValid() const { return NetRefHandleManager != nullptr; }
		};
		FProcessQueueContext ProcessContext;
	};


	FNetBlobHandlerManager BlobHandlerManager;
	FNetObjectAttachmentSendQueue AttachmentSendQueue;
	TStrongObjectPtr<UNetRPCHandler> RPCHandler;
	TStrongObjectPtr<UPartialNetObjectAttachmentHandler> PartialNetObjectAttachmentHandler;
	TStrongObjectPtr<UNetObjectBlobHandler> NetObjectBlobHandler;

	UReplicationSystem* ReplicationSystem;
	FObjectReferenceCache* ObjectReferenceCache;
	FReplicationConnections* Connections;
	const UPartialNetObjectAttachmentHandlerConfig* PartialNetObjectAttachmentHandlerConfig;
	const FNetRefHandleManager* NetRefHandleManager;
	bool bIsServer;
	bool bSendAttachmentsWithObject;
};

}
