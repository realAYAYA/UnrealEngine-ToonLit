// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientReplicationDataCollector.h"

#include "Replication/Data/ReplicationStream.h"
#include "Replication/Formats/IObjectReplicationFormat.h"
#include "Replication/IConcertClientReplicationBridge.h"
#include "Replication/ReplicationPropertyFilter.h"
#include "Replication/Data/ObjectIds.h"

#include "Algo/RemoveIf.h"
#include "Misc/EBreakBehavior.h"

namespace UE::ConcertSyncClient::Replication
{
	FClientReplicationDataCollector::FClientReplicationDataCollector(
		IConcertClientReplicationBridge* InReplicationBridge,
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> InReplicationFormat,
		FGetClientStreams InGetStreamsDelegate,
		const FGuid& InClientId
		)
		: Bridge(InReplicationBridge)
		, ReplicationFormat(MoveTemp(InReplicationFormat))
		, GetStreamsDelegate(MoveTemp(InGetStreamsDelegate))
		, ClientId(InClientId)
	{
		check(GetStreamsDelegate.IsBound());
		Bridge->OnObjectDiscovered().AddRaw(this, &FClientReplicationDataCollector::StartTrackingObject);
		Bridge->OnObjectHidden().AddRaw(this, &FClientReplicationDataCollector::StopTrackingObject);
	}

	FClientReplicationDataCollector::~FClientReplicationDataCollector()
	{
		Bridge->OnObjectDiscovered().RemoveAll(this);
		Bridge->OnObjectHidden().RemoveAll(this);

		for (const TPair<FSoftObjectPath, TArray<FObjectInfo>>& Pair : ObjectsToReplicate)
		{
			Bridge->PopTrackedObjects({ Pair.Key });
		}
	}
	
	void FClientReplicationDataCollector::AddReplicatedObjectStreams(const FSoftObjectPath& Object, TArrayView<const FGuid> AddedStreams)
	{
		TArray<FObjectInfo>& ReplicatedObjectInfo = ObjectsToReplicate.FindOrAdd(Object);
		const bool bIsNewReplicatedObject = ReplicatedObjectInfo.IsEmpty();
		
		const TArray<FConcertReplicationStream>& RegisteredStreams = *GetStreamsDelegate.Execute();
		ReplicatedObjectInfo.Reserve(ReplicatedObjectInfo.Num() + AddedStreams.Num());
		for (const FGuid& StreamId : AddedStreams)
		{
			const FConcertReplicationStream* Stream = RegisteredStreams.FindByPredicate([&StreamId](const FConcertReplicationStream& Stream)
			{
				return Stream.BaseDescription.Identifier == StreamId;
			});
			const FConcertReplicatedObjectInfo* ObjectInfo = Stream ? Stream->BaseDescription.ReplicationMap.ReplicatedObjects.Find(Object) : nullptr;
			if (ensureAlwaysMsgf(ObjectInfo, TEXT("Client's registered streams cache is out of sync")))
			{
				ReplicatedObjectInfo.Add({ StreamId, ObjectInfo->PropertySelection });
			}
		}
		
		// FClientReplicationDataCollector should not push Object more than once because each push increments an internal counter
		if (bIsNewReplicatedObject)
		{
			Bridge->PushTrackedObjects({ Object });
		}
	}

	void FClientReplicationDataCollector::RemoveReplicatedObjectStreams(const FSoftObjectPath& Object, TArrayView<const FGuid> RemovedStreams)
	{
		TArray<FObjectInfo>* ReplicatedObjectInfo = ObjectsToReplicate.Find(Object);
		if (!ReplicatedObjectInfo)
		{
			// This object is not being replicated
			return;
		}

		ReplicatedObjectInfo->SetNum(Algo::RemoveIf(*ReplicatedObjectInfo, [&RemovedStreams](const FObjectInfo& ObjectInfo)
		{
			return RemovedStreams.Contains(ObjectInfo.StreamId);
		}));

		if (ReplicatedObjectInfo->IsEmpty())
		{
			ObjectsToReplicate.Remove(Object);
			Bridge->PopTrackedObjects({ Object });
		}
	}

	void FClientReplicationDataCollector::OnObjectStreamModified(const FSoftObjectPath& Object, TArrayView<const FGuid> PutStreams)
	{
		TArray<FObjectInfo>* ReplicatedObjectInfo = ObjectsToReplicate.Find(Object);
		if (!ReplicatedObjectInfo)
		{
			// This object is not being replicated
			return;
		}

		const TArray<FConcertReplicationStream>& RegisteredStreams = *GetStreamsDelegate.Execute();
		for (const FGuid& StreamId : PutStreams)
		{
			const FConcertReplicationStream* Stream = RegisteredStreams.FindByPredicate([&StreamId](const FConcertReplicationStream& Stream)
			{
				return Stream.BaseDescription.Identifier == StreamId;
			});
			const FConcertReplicatedObjectInfo* ObjectInfo = Stream ? Stream->BaseDescription.ReplicationMap.ReplicatedObjects.Find(Object) : nullptr;
			if (!ensureAlwaysMsgf(ObjectInfo, TEXT("Client's registered streams cache is out of sync")))
			{
				continue;
			}

			// The PutObject request either just created ObjectInfo or updated it.
			FObjectInfo* ReplicatedObject = ReplicatedObjectInfo->FindByPredicate([&](const FObjectInfo& Existing)
			{
				return Existing.StreamId == StreamId;
			});
			if (ReplicatedObject)
			{
				// PutObject updated existing object in stream
				ReplicatedObject->SelectedProperties = ObjectInfo->PropertySelection;
			}
			else
			{
				// PutObject added object to stream
				ReplicatedObjectInfo->Add({ StreamId, ObjectInfo->PropertySelection });
			}
		}
	}

	void FClientReplicationDataCollector::ForEachOwnedObject(
		TFunctionRef<EBreakBehavior(const FSoftObjectPath&)> Callback
		) const
	{
		for (const TPair<FSoftObjectPath, TArray<FObjectInfo>>& Pair : ObjectsToReplicate)
		{
			if (Callback(Pair.Key) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	void FClientReplicationDataCollector::AppendOwningStreamsForObject(
		const FSoftObjectPath& ObjectPath,
		TSet<FGuid>& Paths
		) const
	{
		if (const TArray<FObjectInfo>* ObjectInfo = ObjectsToReplicate.Find(ObjectPath))
		{
			Algo::Transform(*ObjectInfo, Paths, [](const FObjectInfo& ObjectInfo)
			{
				return ObjectInfo.StreamId;
			});
		}
	}

	void FClientReplicationDataCollector::ForEachPendingObject(TFunctionRef<void(const FConcertReplicatedObjectId&)> ProcessItemFunc) const
	{
		for (const TPair<FSoftObjectPath, TArray<FObjectInfo>>& Pair : ObjectsToReplicate)
		{
			for (const FObjectInfo& ObjectInfo : Pair.Value)
			{
				// The bridge has the object cached. We do not cache the object ourselves!
				// Funky Unreal flows can cause the object to be renamed out from under us and replaced by a different instance (just by using UObject::Rename()).
				// The bridge is aware of these flows and FindObjectIfAvailable will catch them.
				// If FindObjectIfAvailable fails to resolve, OnObjectHidden() is triggered if the object was previously visible.
				const UObject* Object = Bridge->FindObjectIfAvailable(Pair.Key);
				
				const FSoftObjectPath ObjectPath = Object;
				if (Object && ensureMsgf(ObjectPath == Object, TEXT("Sanity check: the bridge gave us an object with a different path!")))
				{
					ProcessItemFunc(FConcertReplicatedObjectId {
						FConcertObjectInStreamID{ ObjectInfo.StreamId, Object },
						ClientId
					});
				}
			}
		}
	}

	bool FClientReplicationDataCollector::ExtractReplicationDataForObject(
		const FConcertReplicatedObjectId& ObjectToProcess,
		TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable,
		TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable
		)
	{
		const TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(ObjectToProcess.Object);
		// ExtractReplicationDataForObject is supposed to be called in response to ForEachPendingObject... so either the call was invalid or ForEachPendingObject lied
		if (!ensure(ObjectInfos))
		{
			return {};
		}

		// The properties to be replicated depend on the stream: search for it.
		const int32 StreamIndex = ObjectInfos->IndexOfByPredicate([&ObjectToProcess](const FObjectInfo& ObjectInfo){ return ObjectInfo.StreamId == ObjectToProcess.StreamId; });
		// Same logic as above: if the stream is not found, then either the call is invalid or ForEachPendingObject lied
		if (!ensure(ObjectInfos->IsValidIndex(StreamIndex)))
		{
			return {};
		}

		UObject* Object = Bridge->FindObjectIfAvailable(ObjectToProcess.Object);
		// Finally... ask the bridge to resolve the object for us. The bridge has the object cached and handles the object getting renamed, etc.
		// This should resolve. If it does not: same logic as above - either this call is invalid or ForEachPendingObject lied.
		if (!ensure(Object))
		{
			return false;
		}

		const FObjectInfo& ObjectInfo = (*ObjectInfos)[StreamIndex];
		ConcertSyncCore::FReplicationPropertyFilter Filter(ObjectInfo.SelectedProperties);
		TOptional<FConcertSessionSerializedPayload> Payload = ReplicationFormat->CreateReplicationEvent(
			*Object,
			[&Filter](const FArchiveSerializedPropertyChain* Chain, const FProperty& Property)
			{
				return Filter.ShouldSerializeProperty(Chain, Property);
			}
		);
		if (Payload)
		{
			ProcessMoveable(MoveTemp(*Payload));
		}
		return true;
	}

	void FClientReplicationDataCollector::StartTrackingObject(UObject& Object)
	{
		if (TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(&Object))
		{
			++NumTrackedObjects;
		}
	}

	void FClientReplicationDataCollector::StopTrackingObject(const FSoftObjectPath& ObjectPath)
	{
		if (TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(ObjectPath))
		{
			--NumTrackedObjects;
		}
	}
}
