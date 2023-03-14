// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetBlobManager.h"
#include "HAL/IConsoleManager.h"
#include "Iris/ReplicationSystem/NetBlob/NetObjectBlobHandler.h"
#include "Iris/ReplicationSystem/NetBlob/NetRPCHandler.h"
#include "Iris/ReplicationSystem/NetBlob/PartialNetObjectAttachmentHandler.h"
#include "Iris/ReplicationSystem/ReplicationConnections.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/Core/IrisLog.h"

namespace UE::Net::Private
{

static TAutoConsoleVariable<int32> CVarEnableIrisRPCs(TEXT("net.Iris.EnableRPCs"), 1, TEXT( "If > 0 let Iris replicate and execute RPCs."));

FNetBlobManager::FNetBlobManager()
: ReplicationSystem(nullptr)
, ObjectReferenceCache(nullptr)
, Connections(nullptr)
, PartialNetObjectAttachmentHandlerConfig(nullptr)
, NetHandleManager(nullptr)
, bIsServer(false)
, bSendAttachmentsWithObject(false)
{
}
	
void FNetBlobManager::Init(FNetBlobManagerInitParams& InitParams)
{
	BlobHandlerManager.Init();

	ReplicationSystem = InitParams.ReplicationSystem;
	Connections = &InitParams.ReplicationSystem->GetReplicationSystemInternal()->GetConnections();
	NetHandleManager = &InitParams.ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();
	ObjectReferenceCache = &InitParams.ReplicationSystem->GetReplicationSystemInternal()->GetObjectReferenceCache();
	bIsServer = ReplicationSystem->IsServer();
	bSendAttachmentsWithObject = InitParams.bSendAttachmentsWithObject; 

	RegisterDefaultHandlers();

	AttachmentSendQueue.Init(this);
}

bool FNetBlobManager::RegisterNetBlobHandler(UNetBlobHandler* Handler)
{
	return BlobHandlerManager.RegisterHandler(Handler);
}

bool FNetBlobManager::QueueNetObjectAttachment(uint32 ConnectionId, const FNetObjectReference& TargetRef, const TRefCountPtr<FNetObjectAttachment>& Attachment)
{
	if (!Attachment.IsValid())
	{
		return false;
	}

	if (!Connections->IsValidConnection(ConnectionId))
	{
		UE_LOG(LogIris, Warning, TEXT("Dropping attachment to invalid connection %u."), ConnectionId);
		return true;
	}

	FInternalNetHandle OwnerIndex = 0;
	FInternalNetHandle SubObjectIndex = 0;
	const FNetObjectReference& NetObjectReference = Attachment->GetNetObjectReference();

	const FNetObjectReference OwnerOrSubObjectReference = TargetRef;	
	FNetObjectReference OwnerReference = OwnerOrSubObjectReference;

	bool bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerOrSubObjectReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
	if (!bCanSendRpc)
	{
		// If the TargetRef is a valid reference but not replicated, then the owner must be used instead
		if (OwnerOrSubObjectReference.IsValid())
		{
			// See if we can find a replicated handle
			OwnerReference = ObjectReferenceCache->GetReplicatedOuter(TargetRef);
			bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
		}

		if (!bCanSendRpc)
		{
			UE_LOG(LogIris, Warning, TEXT("QueueNetObjectAttachment %s Failed. Unable to resolve object reference %s."), *NetObjectReference.ToString(), *OwnerReference.ToString());
			return false;
		}
	}

	Attachment->SetNetObjectReference(OwnerReference, OwnerOrSubObjectReference);
	AttachmentSendQueue.Enqueue(ConnectionId, OwnerIndex, SubObjectIndex, Attachment);
	return true;
}

bool FNetBlobManager::SendRPC(const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters)
{
	if (CVarEnableIrisRPCs.GetValueOnGameThread() <= 0)
	{
		return false;
	}

	UNetRPCHandler* Handler = RPCHandler.Get();
	if (Handler == nullptr)
	{
		return false;
	}

	// May the RPC be sent?
	if ((Function->FunctionFlags & (bIsServer ? (FUNC_NetClient | FUNC_NetMulticast) : FUNC_NetServer)) == 0)
	{
		checkf(false, TEXT("Trying to call RPC %s in the wrong direction."), ToCStr(Function->GetName()));
		return true;
	}

	// Check if there are any connections at all
	const FNetBitArray& ValidConnections = Connections->GetValidConnections();
	if (ValidConnections.FindFirstOne() == FNetBitArray::InvalidIndex)
	{
		return true;
	}

	FInternalNetHandle OwnerIndex = 0;
	FInternalNetHandle SubObjectIndex = 0;

	const FNetObjectReference OwnerOrSubObjectReference = ObjectReferenceCache->GetOrCreateObjectReference(SubObject ? SubObject : Object);	
	FNetObjectReference OwnerReference = OwnerOrSubObjectReference;

	bool bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerOrSubObjectReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
	if (!bCanSendRpc)
	{
		// If the subobject is a valid reference but not replicated, then the owner must be used instead
		if (OwnerOrSubObjectReference.IsValid() && SubObject)
		{
			OwnerReference = ObjectReferenceCache->GetOrCreateObjectReference(Object);
			bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
		}

		if (!bCanSendRpc)
		{
			UE_LOG(LogIris, Warning, TEXT("SendRPC %s for %s Failed. Unable to resolve object reference %s."), ToCStr(Function->GetName()), ToCStr(Object->GetName()), ToCStr(OwnerReference.ToString()));
			return false;
		}
	}

	const TRefCountPtr<FNetRPC>& RPC = Handler->CreateRPC(OwnerReference, Function, Parameters);
	if (!RPC.IsValid())
	{
		return true;
	}

	RPC->SetNetObjectReference(OwnerReference, OwnerOrSubObjectReference);
	AttachmentSendQueue.Enqueue(OwnerIndex, SubObjectIndex, reinterpret_cast<const TRefCountPtr<FNetObjectAttachment>&>(RPC));
	return true;
}

bool FNetBlobManager::SendRPC(uint32 ConnectionId, const UObject* Object, const UObject* SubObject, const UFunction* Function, const void* Parameters)
{
	if (CVarEnableIrisRPCs.GetValueOnGameThread() <= 0)
	{
		return false;
	}

	UNetRPCHandler* Handler = RPCHandler.Get();
	if (Handler == nullptr)
	{
		return false;
	}

	if ((Function->FunctionFlags & (bIsServer ? (FUNC_NetClient | FUNC_NetMulticast) : FUNC_NetServer)) == 0)
	{
		checkf(false, TEXT("Trying to call RPC %s in the wrong direction."), ToCStr(Function->GetName()));
		return true;
	}

	if (!Connections->IsValidConnection(ConnectionId))
	{
		UE_LOG(LogIris, Warning, TEXT("Trying to call RPC on non-existing connection %u."), ConnectionId);
		return true;
	}

	FInternalNetHandle OwnerIndex = 0;
	FInternalNetHandle SubObjectIndex = 0;

	const FNetObjectReference OwnerOrSubObjectReference = ObjectReferenceCache->GetOrCreateObjectReference(SubObject ? SubObject : Object);	
	FNetObjectReference OwnerReference = OwnerOrSubObjectReference;

	bool bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerOrSubObjectReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
	if (!bCanSendRpc)
	{
		// If the subobject is a valid reference but  not replicated, then the owner must be used instead
		if (OwnerOrSubObjectReference.IsValid() && SubObject)
		{
			OwnerReference = ObjectReferenceCache->GetOrCreateObjectReference(Object);
			bCanSendRpc = GetOwnerAndSubObjectIndicesFromHandle(OwnerReference.GetRefHandle(), OwnerIndex, SubObjectIndex);
		}

		if (!bCanSendRpc)
		{
			UE_LOG(LogIris, Warning, TEXT("SendRPC %s for %s Failed. Unable to resolve object reference %s."), ToCStr(Function->GetName()), ToCStr(Object->GetName()), ToCStr(OwnerReference.ToString()));
			return false;
		}
	}

	const TRefCountPtr<FNetRPC>& RPC = Handler->CreateRPC(OwnerReference, Function, Parameters);
	if (!RPC.IsValid())
	{
		UE_LOG(LogIris, Warning, TEXT("Unable to create RPC for function %s."), ToCStr(Function->GetName()));
		return true;
	}

	RPC->SetNetObjectReference(OwnerReference, OwnerOrSubObjectReference);
	AttachmentSendQueue.Enqueue(ConnectionId, OwnerIndex, SubObjectIndex, reinterpret_cast<const TRefCountPtr<FNetObjectAttachment>&>(RPC));
	return true;
}

bool FNetBlobManager::GetOwnerAndSubObjectIndicesFromHandle(FNetHandle NetHandle, FInternalNetHandle& OutOwnerIndex, FInternalNetHandle& OutSubObjectIndex)
{
	if (!NetHandle.IsValid())
	{
		return false;
	}

	const FInternalNetHandle ObjectIndex = NetHandleManager->GetInternalIndex(NetHandle);
	if (ObjectIndex == FNetHandleManager::InvalidInternalIndex)
	{
		return false;
	}

	const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
	if (ObjectData.SubObjectRootIndex != FNetHandleManager::InvalidInternalIndex)
	{
		OutOwnerIndex = ObjectData.SubObjectRootIndex;
		OutSubObjectIndex = ObjectIndex;
	}
	else
	{
		OutOwnerIndex = ObjectIndex;
		OutSubObjectIndex = FNetHandleManager::InvalidInternalIndex;
	}

	return true;
}

void FNetBlobManager::ProcessNetObjectAttachmentSendQueue()
{
	AttachmentSendQueue.ProcessQueue(Connections);
}

void FNetBlobManager::AddConnection(uint32 ConnectionId)
{
	BlobHandlerManager.AddConnection(ConnectionId);
}

void FNetBlobManager::RemoveConnection(uint32 ConnectionId)
{
	BlobHandlerManager.RemoveConnection(ConnectionId);
}

void FNetBlobManager::RegisterDefaultHandlers()
{
	// NetRPCHandler
	{
		RPCHandler = TStrongObjectPtr<UNetRPCHandler>(NewObject<UNetRPCHandler>());
		RPCHandler->Init(*ReplicationSystem);
		if (!RegisterNetBlobHandler(RPCHandler.Get()))
		{
#if !WITH_AUTOMATION_WORKER
			checkf(false, TEXT("%s"), TEXT("Unable to register RPC handler. RPCs cannot be sent or received."));
#endif
			RPCHandler.Reset();
		}
	}

	// PartialNetObjectAttachmentHandler
	{
		PartialNetObjectAttachmentHandlerConfig = GetDefault<UPartialNetObjectAttachmentHandlerConfig>();
		FPartialNetObjectAttachmentHandlerInitParams InitParams = {};
		InitParams.ReplicationSystem  = ReplicationSystem;
		InitParams.Config = PartialNetObjectAttachmentHandlerConfig;
		PartialNetObjectAttachmentHandler = TStrongObjectPtr<UPartialNetObjectAttachmentHandler>(NewObject<UPartialNetObjectAttachmentHandler>());
		PartialNetObjectAttachmentHandler->Init(InitParams);
		if (!RegisterNetBlobHandler(PartialNetObjectAttachmentHandler.Get()))
		{
#if !WITH_AUTOMATION_WORKER
			checkf(false, TEXT("%s"), TEXT("Unable to register PartialNetObjectAttachment handler. Attachments cannot be split."));
#endif
			PartialNetObjectAttachmentHandler.Reset();
		}
	}

	// NetObjectBlobHandler
	{
		NetObjectBlobHandler = TStrongObjectPtr<UNetObjectBlobHandler>(NewObject<UNetObjectBlobHandler>());
		if (!RegisterNetBlobHandler(NetObjectBlobHandler.Get()))
		{
#if !WITH_AUTOMATION_WORKER
			checkf(false, TEXT("%s"), TEXT("Unable to register NetObjectBlobHandler handler. Replicated objects cannot be split."));
#endif
			NetObjectBlobHandler.Reset();
		}
	}
}

// FNetObjectAttachmentQueue
FNetBlobManager::FNetObjectAttachmentSendQueue::FNetObjectAttachmentSendQueue()
: Manager(nullptr)
, bHasMulticastAttachments(false)
{
}

void FNetBlobManager::FNetObjectAttachmentSendQueue::Init(FNetBlobManager* InManager)
{
	Manager = InManager;
}

void FNetBlobManager::FNetObjectAttachmentSendQueue::Enqueue(uint32 ConnectionId, FInternalNetHandle OwnerIndex, FInternalNetHandle SubObjectIndex, const TRefCountPtr<FNetObjectAttachment>& Attachment)
{
	FNetObjectAttachmentQueueEntry& QueueEntry = AttachmentQueue.AddDefaulted_GetRef();
	QueueEntry.ConnectionId = ConnectionId;
	QueueEntry.OwnerIndex = OwnerIndex;
	QueueEntry.SubObjectIndex = SubObjectIndex;
	QueueEntry.Attachment = Attachment;
}

void FNetBlobManager::FNetObjectAttachmentSendQueue::Enqueue(FInternalNetHandle OwnerIndex, FInternalNetHandle SubObjectIndex, const TRefCountPtr<FNetObjectAttachment>& Attachment)
{
	FNetObjectAttachmentQueueEntry& QueueEntry = AttachmentQueue.AddDefaulted_GetRef();
	QueueEntry.ConnectionId = 0;
	QueueEntry.OwnerIndex = OwnerIndex;
	QueueEntry.SubObjectIndex = SubObjectIndex;
	QueueEntry.Attachment = Attachment;

	bHasMulticastAttachments = true;
}

void FNetBlobManager::FNetObjectAttachmentSendQueue::ProcessQueue(FReplicationConnections* InConnections)
{
	if (AttachmentQueue.Num() <= 0)
	{
		return;
	}

	const FNetBitArray& ValidConnections = InConnections->GetValidConnections();
	const FNetBitArrayView ReplicatingConnections = MakeNetBitArrayView(ValidConnections);

	uint32* ConnectionIds = nullptr;
	uint32 ConnectionCount = 0;

	if (bHasMulticastAttachments)
	{
		ConnectionIds = static_cast<uint32*>(FMemory_Alloca(ReplicatingConnections.GetNumBits() * 4));
		CA_ASSUME(ConnectionIds != nullptr);
		ReplicatingConnections.ForAllSetBits([ConnectionIds, &ConnectionCount](uint32 Bit) { ConnectionIds[ConnectionCount++] = Bit; });
	}

	TArray<TRefCountPtr<FNetBlob>> PartialNetBlobs;
	for (const FNetObjectAttachmentQueueEntry& Entry : MakeArrayView(AttachmentQueue))
	{
		PartialNetBlobs.Reset();

		const TRefCountPtr<FNetObjectAttachment>& Attachment = Entry.Attachment;
		const FReplicationStateDescriptor* ReplicationStateDescriptor = Attachment->GetReplicationStateDescriptor();

		const bool bMulticast = Entry.ConnectionId == 0;
		const bool bHasConnectionSpecificSerialization = ReplicationStateDescriptor && EnumHasAnyFlags(ReplicationStateDescriptor->Traits, EReplicationStateTraits::HasConnectionSpecificSerialization);

		if (!(bMulticast && bHasConnectionSpecificSerialization) && !PreSerializeAndSplitNetBlob(Entry.ConnectionId, Attachment, PartialNetBlobs))
		{
			checkf(false, TEXT("Unable to split %s NetObjectAttachment."), (EnumHasAnyFlags(Attachment->GetCreationInfo().Flags, ENetBlobFlags::Reliable) ? TEXT("reliable") : TEXT("unreliable")));
			continue;
		}

		const bool bIsMultiPartAttachment = (PartialNetBlobs.Num() > 1);
		TArrayView<const TRefCountPtr<FNetBlob>> AttachmentsView = MakeArrayView(PartialNetBlobs);
		if (bMulticast)
		{
			if (bIsMultiPartAttachment)
			{
				UE_LOG(LogIris, Warning, TEXT("Splitting multicast net object attachment %s."), ReplicationStateDescriptor != nullptr && ReplicationStateDescriptor->DebugName ? ReplicationStateDescriptor->DebugName->Name : TEXT("Unknown"));
			}

			const FNetBlobCreationInfo& BlobCreationInfo = Attachment->GetCreationInfo();
			const bool bIsReliableRPC = EnumHasAnyFlags(BlobCreationInfo.Flags, ENetBlobFlags::Reliable);
			for (uint32 ConnectionId : MakeArrayView(ConnectionIds, ConnectionCount))
			{
				// Objects won't be prioritized until there's a view so let's avoid queuing multicast attachments.
				if (!bIsReliableRPC && InConnections->GetReplicationView(ConnectionId).Views.Num() <= 0)
				{
					continue;
				}

				if (bHasConnectionSpecificSerialization)
				{
					PartialNetBlobs.Reset();
					if (!PreSerializeAndSplitNetBlob(ConnectionId, Attachment, PartialNetBlobs))
					{
						checkf(false, TEXT("Unable to split %s NetObjectAttachment with connection specific serialization."), (EnumHasAnyFlags(Attachment->GetCreationInfo().Flags, ENetBlobFlags::Reliable) ? TEXT("reliable") : TEXT("unreliable")));
						continue;
					}

					AttachmentsView = MakeArrayView(PartialNetBlobs);
				}

				FReplicationConnection* Connection = InConnections->GetConnection(ConnectionId);
				// We're only iterating over valid connections so the Connection pointer must be valid.
				Connection->ReplicationWriter->QueueNetObjectAttachments(Entry.OwnerIndex, Entry.SubObjectIndex, AttachmentsView);
			}
		}
		else
		{
			if (FReplicationConnection* Connection = InConnections->GetConnection(Entry.ConnectionId))
			{
				Connection->ReplicationWriter->QueueNetObjectAttachments(Entry.OwnerIndex, Entry.SubObjectIndex, AttachmentsView);
			}
		}
	}

	// Clear queue
	AttachmentQueue.Reset();
	bHasMulticastAttachments = false;
}

bool FNetBlobManager::FNetObjectAttachmentSendQueue::PreSerializeAndSplitNetBlob(uint32 ConnectionId, const TRefCountPtr<FNetObjectAttachment>& Attachment, TArray<TRefCountPtr<FNetBlob>>& OutPartialNetBlobs) const
{
	if (!Manager->PartialNetObjectAttachmentHandler.IsValid())
	{
		OutPartialNetBlobs.Add(reinterpret_cast<const TRefCountPtr<FNetBlob>&>(Attachment));
		return true;
	}

	return Manager->PartialNetObjectAttachmentHandler->PreSerializeAndSplitNetBlob(ConnectionId, Attachment, OutPartialNetBlobs, Manager->bSendAttachmentsWithObject);
}

}
