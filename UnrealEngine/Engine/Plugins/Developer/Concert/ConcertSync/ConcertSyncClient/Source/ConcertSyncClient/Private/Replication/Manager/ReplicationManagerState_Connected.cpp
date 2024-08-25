// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerState_Connected.h"

#include "ConcertLogGlobal.h"
#include "ConcertTransportMessages.h"
#include "IConcertSession.h"
#include "ReplicationManagerState_Disconnected.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/Formats/FullObjectFormat.h"
#include "Replication/Messages/Handshake.h"
#include "Replication/Processing/ClientReplicationDataCollector.h"
#include "Replication/Processing/ObjectReplicationApplierProcessor.h"
#include "Replication/Processing/ObjectReplicationReceiver.h"

#include "Algo/RemoveIf.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::ConcertSyncClient::Replication
{
	TAutoConsoleVariable<bool> CVarSimulateAuthorityTimeouts(
		TEXT("Concert.Replication.SimulateAuthorityTimeouts"),
		false,
		TEXT("Whether the client should pretend that authority requests timed out instead of sending to the server.")
		);
	TAutoConsoleVariable<bool> CVarSimulateQueryTimeouts(
		TEXT("Concert.Replication.SimulateQueryTimeouts"),
		false,
		TEXT("Whether the client should pretend that query requests timed out instead of sending to the server.")
		);
	TAutoConsoleVariable<bool> CVarSimulateStreamChangeTimeouts(
		TEXT("Concert.Replication.SimulateStreamChangeTimeouts"),
		false,
		TEXT("Whether the client should pretend that stream change requests timed out instead of sending to the server.")
		);

	TAutoConsoleVariable<bool> CVarSimulateAuthorityRejection(
		TEXT("Concert.Replication.SimulateAuthorityRejection"),
		false,
		TEXT("Whether the client should pretend that authority change requests were rejected.")
		);

	TAutoConsoleVariable<bool> CVarLogStreamRequestsAndResponsesOnClient(
		TEXT("Concert.Replication.LogStreamRequestsAndResponsesOnClient"),
		false,
		TEXT("Whether to log changes to streams.")
		);
	TAutoConsoleVariable<bool> CVarLogAuthorityRequestsAndResponsesOnClient(
		TEXT("Concert.Replication.LogAuthorityRequestsAndResponsesOnClient"),
		false,
		TEXT("Whether to log changes to authority.")
		);

	namespace Private
	{
		template<typename TMessage>
		static void LogNetworkMessage(const TAutoConsoleVariable<bool>& ShouldLog, const TMessage& Message)
		{
			if (ShouldLog.GetValueOnAnyThread())
			{
				FString JsonString;
				FJsonObjectConverter::UStructToJsonObjectString(TMessage::StaticStruct(), &Message, JsonString, 0, 0);
				UE_LOG(LogConcert, Log, TEXT("%s\n%s"), *TMessage::StaticStruct()->GetName(), *JsonString);
			}
		}
	}
	
	FReplicationManagerState_Connected::FReplicationManagerState_Connected(
		TSharedRef<IConcertClientSession> LiveSession,
		IConcertClientReplicationBridge* ReplicationBridge,
		TArray<FConcertReplicationStream> StreamDescriptions,
		FReplicationManager& Owner
		)
		: FReplicationManagerState(Owner)
		, LiveSession(LiveSession)
		, ReplicationBridge(ReplicationBridge)
		, RegisteredStreams(MoveTemp(StreamDescriptions))
		// TODO DP: Use config to determine which replication format to use
		, ReplicationFormat(MakeShared<ConcertSyncCore::FFullObjectFormat>())
		, ReplicationDataSource(MakeShared<FClientReplicationDataCollector>(
			ReplicationBridge,
			ReplicationFormat,
			FClientReplicationDataCollector::FGetClientStreams::CreateLambda([this]()
			{
				return &RegisteredStreams;
			}),
			LiveSession->GetSessionClientEndpointId()
			))
		, Sender(
			ConcertSyncCore::FGetObjectFrequencySettings::CreateRaw(this, &FReplicationManagerState_Connected::GetObjectFrequencySettings),
			LiveSession->GetSessionServerEndpointId(), LiveSession, ReplicationDataSource
			)
		, ReceivedDataCache(MakeShared<ConcertSyncCore::FObjectReplicationCache>(ReplicationFormat))
		, Receiver(MakeShared<ConcertSyncCore::FObjectReplicationReceiver>(LiveSession, ReceivedDataCache))
		, ReceivedReplicationQueuer(FClientReplicationDataQueuer::Make(ReplicationBridge, ReceivedDataCache))
		, ReplicationApplier(MakeShared<FObjectReplicationApplierProcessor>(ReplicationBridge, ReplicationFormat, ReceivedReplicationQueuer))
	{}

	FReplicationManagerState_Connected::~FReplicationManagerState_Connected()
	{
		// Technically not needed due to AddSP but let's be nice and clean up after ourselves
		LiveSession->OnTick().RemoveAll(this);
	}

	TFuture<FJoinReplicatedSessionResult> FReplicationManagerState_Connected::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		UE_LOG(LogConcert, Warning, TEXT("JoinReplicationSession requested while already in a session"));
		return MakeFulfilledPromise<FJoinReplicatedSessionResult>(FJoinReplicatedSessionResult{ EJoinReplicationErrorCode::AlreadyInSession }).GetFuture();
	}

	void FReplicationManagerState_Connected::LeaveReplicationSession()
	{
		LiveSession->SendCustomEvent(FConcertReplication_LeaveEvent{}, LiveSession->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		ChangeState(MakeShared<FReplicationManagerState_Disconnected>(LiveSession, ReplicationBridge, GetOwner()));
	}

	IConcertClientReplicationManager::EStreamEnumerationResult FReplicationManagerState_Connected::ForEachRegisteredStream(
		TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback
		) const
	{
		for (const FConcertReplicationStream& Stream : RegisteredStreams)
		{
			if (Callback(Stream) == EBreakBehavior::Break)
			{
				break;
			}
		}
		return EStreamEnumerationResult::Iterated;
	}

	TFuture<FConcertReplication_ChangeAuthority_Response> FReplicationManagerState_Connected::RequestAuthorityChange(FConcertReplication_ChangeAuthority_Request Args)
	{
		if (CVarSimulateAuthorityTimeouts.GetValueOnGameThread())
		{
			return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(FConcertReplication_ChangeAuthority_Response{ EReplicationResponseErrorCode::Timeout }).GetFuture();
		}
		if (CVarSimulateAuthorityRejection.GetValueOnGameThread())
		{
			return RejectAll(MoveTemp(Args));
		}
		
		// Stop replicating removed objects right now: the server will remove authority after processing this request.
		// At that point, it will log errors for receiving replication data from a client without authority.
		HandleReleasingReplicatedObjects(Args);

		Private::LogNetworkMessage(CVarLogAuthorityRequestsAndResponsesOnClient, Args);
		return LiveSession->SendCustomRequest<FConcertReplication_ChangeAuthority_Request, FConcertReplication_ChangeAuthority_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([WeakThis = TWeakPtr<FReplicationManagerState_Connected>(SharedThis(this)), Args](FConcertReplication_ChangeAuthority_Response&& Response) mutable
			{
				Private::LogNetworkMessage(CVarLogAuthorityRequestsAndResponsesOnClient, Response);
				
				if (const TSharedPtr<FReplicationManagerState_Connected> ThisPin = WeakThis.Pin()
					; ThisPin && Response.ErrorCode == EReplicationResponseErrorCode::Handled)
				{
					ThisPin->UpdateReplicatedObjectsAfterAuthorityChange(MoveTemp(Args), Response);
				}
				else if (ThisPin && Response.ErrorCode == EReplicationResponseErrorCode::Timeout)
				{
					// HandleReleasingReplicatedObjects caused Args.ReleaseAuthority to stop being replicated. Revert.
					ThisPin->RevertReleasingReplicatedObjects(Args);
				}

				return FConcertReplication_ChangeAuthority_Response { MoveTemp(Response) };
			});
	}

	TFuture<FConcertReplication_QueryReplicationInfo_Response> FReplicationManagerState_Connected::QueryClientInfo(FConcertReplication_QueryReplicationInfo_Request Args)
	{
		if (CVarSimulateQueryTimeouts.GetValueOnGameThread())
		{
			return MakeFulfilledPromise<FConcertReplication_QueryReplicationInfo_Response>(FConcertReplication_QueryReplicationInfo_Response{ EReplicationResponseErrorCode::Timeout }).GetFuture();
		}
		
		if (EnumHasAllFlags(Args.QueryFlags, EConcertQueryClientStreamFlags::SkipAuthority | EConcertQueryClientStreamFlags::SkipStreamInfo | EConcertQueryClientStreamFlags::SkipFrequency ))
		{
			UE_LOG(LogConcert, Warning, TEXT("Request QueryClientInfo is pointless because SkipAuthority and SkipStreamInfo are both set. Returning immediately..."));
			return MakeFulfilledPromise<FConcertReplication_QueryReplicationInfo_Response>().GetFuture();
		}
		
		return LiveSession->SendCustomRequest<FConcertReplication_QueryReplicationInfo_Request, FConcertReplication_QueryReplicationInfo_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([](FConcertReplication_QueryReplicationInfo_Response&& Response)
			{
				return FConcertReplication_QueryReplicationInfo_Response { MoveTemp(Response) };
			});
	}

	TFuture<FConcertReplication_ChangeStream_Response> FReplicationManagerState_Connected::ChangeStream(FConcertReplication_ChangeStream_Request Args)
	{
		if (CVarSimulateStreamChangeTimeouts.GetValueOnGameThread())
		{
			return MakeFulfilledPromise<FConcertReplication_ChangeStream_Response>(FConcertReplication_ChangeStream_Response{ EReplicationResponseErrorCode::Timeout }).GetFuture();
		}
		
		// Stop replicating removed objects right now: the server will remove authority after processing this request.
		// At that point, it will log errors for receiving replication data from a client without authority.
		HandleRemovingReplicatedObjects(Args);
		
		Private::LogNetworkMessage(CVarLogStreamRequestsAndResponsesOnClient, Args);
		return LiveSession->SendCustomRequest<FConcertReplication_ChangeStream_Request, FConcertReplication_ChangeStream_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([WeakThis = TWeakPtr<FReplicationManagerState_Connected>(SharedThis(this)), Args](FConcertReplication_ChangeStream_Response&& Response)
			{
				Private::LogNetworkMessage(CVarLogStreamRequestsAndResponsesOnClient, Response);

				const TSharedPtr<FReplicationManagerState_Connected> ThisPin = WeakThis.Pin();
				if (ThisPin && Response.IsSuccess())
				{
					ThisPin->UpdateReplicatedObjectsAfterStreamChange(Args, Response);
				}
				else if (ThisPin && Response.ErrorCode == EReplicationResponseErrorCode::Timeout)
				{
					// HandleRemovingReplicatedObjects caused Request.ObjectsToRemove to stop being replicated. Revert.
					ThisPin->RevertRemovingReplicatedObjects(Args);
				}
				
				return FConcertReplication_ChangeStream_Response { MoveTemp(Response) };
			});
	}

	IConcertClientReplicationManager::EAuthorityEnumerationResult FReplicationManagerState_Connected::ForEachClientOwnedObject(
		TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback
		) const
	{
		TSet<FGuid> Result;
		int32 ExpectedNumStreams = 0;
		ReplicationDataSource->ForEachOwnedObject([this, &Callback, &Result, &ExpectedNumStreams](const FSoftObjectPath& ObjectPath)
		{
			// Reuse TSet (if possible) for a slightly better memory footprint
			ExpectedNumStreams = FMath::Max(ExpectedNumStreams, Result.Num());
			Result.Empty(Result.Num());
			
			ReplicationDataSource->AppendOwningStreamsForObject(ObjectPath, Result);
			return Callback(ObjectPath, MoveTemp(Result));
		});
		return EAuthorityEnumerationResult::Iterated;
	}

	TSet<FGuid> FReplicationManagerState_Connected::GetClientOwnedStreamsForObject(
		const FSoftObjectPath& ObjectPath
		) const
	{
		TSet<FGuid> Result;
		ReplicationDataSource->AppendOwningStreamsForObject(ObjectPath, Result);
		return Result;
	}

	void FReplicationManagerState_Connected::OnEnterState()
	{
		LiveSession->OnTick().AddSP(this, &FReplicationManagerState_Connected::Tick);
	}

	void FReplicationManagerState_Connected::Tick(IConcertClientSession& Session, float DeltaTime)
	{
		// TODO UE-190714: We should set a time budget for the client so ticking does not cause frame spikes
		const ConcertSyncCore::FProcessObjectsParams Params { DeltaTime };
		Sender.ProcessObjects(Params);
		ReplicationApplier->ProcessObjects(Params);
	}

	void FReplicationManagerState_Connected::UpdateReplicatedObjectsAfterStreamChange(const FConcertReplication_ChangeStream_Request& Request, const FConcertReplication_ChangeStream_Response& Response)
	{
		OnPreStreamsChangedDelegate.Broadcast(Request, { Response });
		ON_SCOPE_EXIT{ OnPostStreamsChangedDelegate.Broadcast(); };
		
		// Build RegisteredStreams while RegisteredStreams has the old, unupdated state
		TMap<FSoftObjectPath, TArray<FGuid>> BundledModifiedObjects;
		for (const TPair<FConcertObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& PutObject : Request.ObjectsToPut)
		{
			const FConcertObjectInStreamID ObjectInfo = PutObject.Key;
			const FSoftObjectPath Object = ObjectInfo.Object;
			const FGuid StreamId = ObjectInfo.StreamId;
			
			const FConcertReplicationStream* StreamDescription = RegisteredStreams.FindByPredicate([&StreamId](const FConcertReplicationStream& Stream)
			{
				return Stream.BaseDescription.Identifier == StreamId;
			});
			// ReplicationDataSource only cares about inflight objects.
			// Newly added objects inflight because the client must first request authority.
			const bool bWasAddedByRequest = !ensure(StreamDescription) || !StreamDescription->BaseDescription.ReplicationMap.ReplicatedObjects.Contains(Object);
			if (!bWasAddedByRequest)
			{
				BundledModifiedObjects.FindOrAdd(Object).Add(StreamId);
			}
		}

		// The local cache must be updated before calling OnObjectStreamModified
		ConcertSyncCore::Replication::ChangeStreamUtils::ApplyValidatedRequest(Request, RegisteredStreams);
		
		for (const TPair<FSoftObjectPath, TArray<FGuid>>& ModifiedObject : BundledModifiedObjects)
		{
			ReplicationDataSource->OnObjectStreamModified(ModifiedObject.Key, ModifiedObject.Value);
		}
	}

	namespace Private
	{
		static void ForEachObjectRemovedFromStreams(const FConcertReplication_ChangeStream_Request& Request, TFunctionRef<void(const FSoftObjectPath& ObjectPath, const TArray<FGuid>& Streams)> Callback)
		{
			TMap<FSoftObjectPath, TArray<FGuid>> BundledRemovedObjects;
			for (const FConcertObjectInStreamID& RemovedObject : Request.ObjectsToRemove)
			{
				BundledRemovedObjects.FindOrAdd(RemovedObject.Object).Add(RemovedObject.StreamId);
			}
			for (const TPair<FSoftObjectPath, TArray<FGuid>>& RemovedObjectInfo : BundledRemovedObjects)
			{
				Callback(RemovedObjectInfo.Key, RemovedObjectInfo.Value);
			}
		}
	}

	void FReplicationManagerState_Connected::HandleRemovingReplicatedObjects(const FConcertReplication_ChangeStream_Request& Request) const
	{
		Private::ForEachObjectRemovedFromStreams(Request, [this](const FSoftObjectPath& Object, const TArray<FGuid>& RemovedStreams)
		{
			ReplicationDataSource->RemoveReplicatedObjectStreams(Object, RemovedStreams);
		});
	}

	void FReplicationManagerState_Connected::RevertRemovingReplicatedObjects(const FConcertReplication_ChangeStream_Request& Request) const
	{
		Private::ForEachObjectRemovedFromStreams(Request, [this](const FSoftObjectPath& Object, const TArray<FGuid>& RemovedStreams)
		{
			ReplicationDataSource->AddReplicatedObjectStreams(Object, RemovedStreams);
		});
	}

	void FReplicationManagerState_Connected::UpdateReplicatedObjectsAfterAuthorityChange(FConcertReplication_ChangeAuthority_Request&& Request, const FConcertReplication_ChangeAuthority_Response& Response) const
	{
		OnPreAuthorityChangedDelegate.Broadcast(Request, { Response });
		ON_SCOPE_EXIT{ OnPostAuthorityChangedDelegate.Broadcast(); };
		
		for (TPair<FSoftObjectPath, FConcertStreamArray>& TakeAuthority : Request.TakeAuthority)
		{
			const FSoftObjectPath& ReplicatedObject = TakeAuthority.Key;
			// Request will be discarded so ...
			FConcertStreamArray& ReplicatedStreams = TakeAuthority.Value;
			
			UE_CLOG(ReplicatedStreams.StreamIds.IsEmpty(), LogConcert, Warning, TEXT("Your FConcertReplication_ChangeAuthority_Request::TakeAuthority request contained empty stream ID array for object %s"), *ReplicatedObject.ToString());
			const FConcertStreamArray* RejectedStreams = Response.RejectedObjects.Find(ReplicatedObject);
			if (RejectedStreams)
			{
				// ... reuse the memory
				ReplicatedStreams.StreamIds.SetNum(Algo::RemoveIf(ReplicatedStreams.StreamIds, [RejectedStreams](const FGuid& Stream)
				{
					return RejectedStreams->StreamIds.Contains(Stream);
				}));
			}

			const bool bWasFullyRejected = ReplicatedStreams.StreamIds.IsEmpty(); 
			if (!bWasFullyRejected)
			{
				ReplicationDataSource->AddReplicatedObjectStreams(TakeAuthority.Key, ReplicatedStreams.StreamIds);
			}
		}
	}

	void FReplicationManagerState_Connected::HandleReleasingReplicatedObjects(const FConcertReplication_ChangeAuthority_Request& Request) const
	{
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& ReleaseAuthority : Request.ReleaseAuthority)
		{
			ReplicationDataSource->RemoveReplicatedObjectStreams(ReleaseAuthority.Key, ReleaseAuthority.Value.StreamIds);
		}
	}

	void FReplicationManagerState_Connected::RevertReleasingReplicatedObjects(const FConcertReplication_ChangeAuthority_Request& Request) const
	{
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& ReleaseAuthority : Request.ReleaseAuthority)
		{
			ReplicationDataSource->AddReplicatedObjectStreams(ReleaseAuthority.Key, ReleaseAuthority.Value.StreamIds);
		}
	}

	FConcertObjectReplicationSettings FReplicationManagerState_Connected::GetObjectFrequencySettings(const FConcertReplicatedObjectId& Object) const
	{
		const FConcertReplicationStream* Stream = RegisteredStreams.FindByPredicate([&Object](const FConcertReplicationStream& Description)
		{
			return Description.BaseDescription.Identifier == Object.StreamId;
		});
		
		if (!ensureMsgf(Stream, TEXT("Caller of GetObjectFrequencySettings is trying to send an object that is not registered with the client")))
		{
			UE_LOG(LogConcert, Warning, TEXT("Requested frequency settings for unknown stream %s and object %s"), *Object.StreamId.ToString(), *Object.Object.ToString());
			return {};
		}
		
		return Stream->BaseDescription.FrequencySettings.GetSettingsFor(Object.Object);
	}
}

